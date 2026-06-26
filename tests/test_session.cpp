#include <catch2/catch_test_macros.hpp>
#include "core/session.hpp"
#include "data/database.hpp"

namespace {
// Each test gets its own in-memory DB so trials don't bleed across cases.
struct Fixture {
    Database db{":memory:"};
    Fixture() { db.create_schema(); }
    GameSession single() { return GameSession("single", {"P1"}, db); }
};
} // namespace

TEST_CASE("Session: initial state is Idle") {
    Fixture f;
    auto s = f.single();
    REQUIRE(s.get_state() == SessionState::Idle);
}

TEST_CASE("Session: start() transitions to Countdown") {
    Fixture f;
    auto s = f.single();
    s.start();
    REQUIRE(s.get_state() == SessionState::Countdown);
}

TEST_CASE("Session: tick past COUNTDOWN_S reaches Waiting") {
    Fixture f;
    auto s = f.single();
    s.start();
    s.tick(0.0f, {});                       // latch countdown start at t=0
    auto st = s.tick(GameSession::COUNTDOWN_S + 0.1f, {});
    REQUIRE(st == SessionState::Waiting);
}

TEST_CASE("Session: active player false start keeps Waiting and records it") {
    Fixture f;
    auto s = f.single();
    s.start();
    s.tick(0.0f, {});
    s.tick(GameSession::COUNTDOWN_S + 0.1f, {});  // → Waiting (phase_start here)
    const float t = GameSession::COUNTDOWN_S + 0.2f;
    auto st = s.tick(t, {SDLK_SPACE});            // player 1 jumps the gun
    REQUIRE(st == SessionState::Waiting);
    REQUIRE_FALSE(s.get_results().empty());
    REQUIRE(s.get_results().back().false_start == true);
    auto lr = s.get_last_result();
    REQUIRE(lr.has_value());
    REQUIRE(lr->false_start == true);
}

TEST_CASE("Session: inactive player key during Waiting is ignored") {
    Fixture f;
    auto s = f.single();
    s.start();
    s.tick(0.0f, {});
    s.tick(GameSession::COUNTDOWN_S + 0.1f, {});  // → Waiting
    const std::size_t before = s.get_results().size();
    auto st = s.tick(GameSession::COUNTDOWN_S + 0.2f, {SDLK_RETURN});  // player 2 key
    REQUIRE(st == SessionState::Waiting);
    REQUIRE(s.get_results().size() == before);  // no false start recorded
}

TEST_CASE("Session: tick past ISI_MAX reaches StimulusActive") {
    Fixture f;
    auto s = f.single();
    s.start();
    s.tick(0.0f, {});
    const float waiting_start = GameSession::COUNTDOWN_S + 0.1f;
    s.tick(waiting_start, {});                                   // → Waiting
    auto st = s.tick(waiting_start + GameSession::ISI_MAX_S + 0.1f, {});
    REQUIRE(st == SessionState::StimulusActive);
}

TEST_CASE("Session: valid keypress during StimulusActive records a reaction time") {
    Fixture f;
    auto s = f.single();
    s.start();
    s.tick(0.0f, {});
    const float waiting_start = GameSession::COUNTDOWN_S + 0.1f;
    s.tick(waiting_start, {});
    const float stim = waiting_start + GameSession::ISI_MAX_S + 0.1f;
    s.tick(stim, {});                              // → StimulusActive (phase_start = stim)
    auto st = s.tick(stim + 0.2f, {SDLK_SPACE});   // 200 ms reaction
    REQUIRE(st == SessionState::Collecting);
    auto lr = s.get_last_result();
    REQUIRE(lr.has_value());
    REQUIRE(lr->reaction_time_ms > 0.0f);
    REQUIRE_FALSE(s.get_results().empty());
    REQUIRE_FALSE(is_miss(s.get_results().back()));
}

TEST_CASE("Session: timeout during StimulusActive records a miss") {
    Fixture f;
    auto s = f.single();
    s.start();
    s.tick(0.0f, {});
    const float waiting_start = GameSession::COUNTDOWN_S + 0.1f;
    s.tick(waiting_start, {});
    const float stim = waiting_start + GameSession::ISI_MAX_S + 0.1f;
    s.tick(stim, {});                                                  // → StimulusActive
    auto st = s.tick(stim + GameSession::TIMEOUT_MS / 1000.0f + 0.1f, {});  // no key
    REQUIRE(st == SessionState::Collecting);
    REQUIRE_FALSE(s.get_results().empty());
    REQUIRE(is_miss(s.get_results().back()));
}

TEST_CASE("Session: completing TRIALS_PER_PLAYER trials reaches Complete") {
    Fixture f;
    auto s = f.single();
    s.start();
    float t = 0.0f;
    s.tick(t, {});  // latch countdown

    int guard = 0;
    while (s.get_state() != SessionState::Complete && guard++ < 1000) {
        std::vector<SDL_Keycode> keys;
        if (s.get_state() == SessionState::StimulusActive) {
            t += 0.2f;            // respond 200 ms after the stimulus
            keys.push_back(SDLK_SPACE);
        } else {
            t += 6.0f;            // long enough to clear countdown / ISI / feedback
        }
        s.tick(t, keys);
    }

    REQUIRE(s.get_state() == SessionState::Complete);

    int completed = 0;
    for (const auto& tr : s.get_results())
        if (!tr.false_start) ++completed;
    REQUIRE(completed == GameSession::TRIALS_PER_PLAYER);
}
