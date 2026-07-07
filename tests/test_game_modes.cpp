#include <catch2/catch_test_macros.hpp>
#include <SDL2/SDL_keycode.h>
#include "core/session.hpp"
#include "data/database.hpp"
#include "ui/screens/results_screen.hpp"

// ── Helpers ──────────────────────────────────────────────────────────────────
// A persistent monotonic clock. The spec's advance() reset t=0 on every call,
// which produces non-monotonic time and flaky ISI/Stimulus transitions; carrying
// the clock across calls (and polling for a state rather than over-advancing a
// fixed span) keeps these tests deterministic.
namespace {

struct Clock { float t = 0.0f; };

Database make_db() {
    Database db(":memory:");
    db.create_schema();
    return db;
}

SessionState advance(GameSession& s, Clock& c, float secs, float step = 0.016f) {
    SessionState st = s.get_state();
    const float target = c.t + secs;
    while (c.t < target) {
        st = s.tick(c.t, {});
        c.t += step;
    }
    return st;
}

// Poll forward until the desired state is observed (or timeout elapses).
SessionState advance_until(GameSession& s, Clock& c, SessionState want,
                           float timeout = 20.0f, float step = 0.016f) {
    SessionState st = s.get_state();
    const float limit = c.t + timeout;
    while (st != want && c.t < limit) {
        st = s.tick(c.t, {});
        c.t += step;
    }
    return st;
}

// Press a key `delay` seconds after the current clock time.
SessionState press(GameSession& s, Clock& c, SDL_Keycode k, float delay = 0.2f) {
    c.t += delay;
    return s.tick(c.t, {k});
}

} // namespace

// ── Race ─────────────────────────────────────────────────────────────────────
TEST_CASE("Race: both keys valid simultaneously in StimulusActive") {
    auto db = make_db();
    GameSession s("race", {"A", "B"}, db);
    s.start();
    Clock c;
    REQUIRE(advance_until(s, c, SessionState::StimulusActive) ==
            SessionState::StimulusActive);
    // P2 key should win the round.
    s.tick(c.t, {SDLK_RETURN});
    REQUIRE(s.get_state() == SessionState::Collecting);
    REQUIRE(s.get_rounds_won(2) == 1);
    REQUIRE(s.get_rounds_won(1) == 0);
}

TEST_CASE("Race: winner's reaction is attributed to the presser") {
    auto db = make_db();
    GameSession s("race", {"A", "B"}, db);
    s.start();
    Clock c;
    advance_until(s, c, SessionState::StimulusActive);
    s.tick(c.t, {SDLK_RETURN});  // P2 presses
    auto lr = s.get_last_result();
    REQUIRE(lr.has_value());
    REQUIRE(lr->player == 2);              // not the always-active player 1
    REQUIRE(lr->reaction_time_ms >= 0.0f); // a real reaction, not a miss
}

TEST_CASE("Race: second key after first is ignored") {
    auto db = make_db();
    GameSession s("race", {"A", "B"}, db);
    s.start();
    Clock c;
    advance_until(s, c, SessionState::StimulusActive);
    s.tick(c.t, {SDLK_SPACE});   // P1 wins
    REQUIRE(s.get_rounds_won(1) == 1);
    s.tick(c.t, {SDLK_RETURN});  // already in Collecting → no-op
    REQUIRE(s.get_rounds_won(2) == 0);
}

// ── Blitz ────────────────────────────────────────────────────────────────────
TEST_CASE("Blitz: transitions to Complete when time runs out") {
    auto db = make_db();
    GameSession s("blitz", {"A"}, db);
    s.start();
    Clock c;
    advance(s, c, GameSession::COUNTDOWN_S + 0.1f);
    SessionState st = advance(s, c, 31.0f, 0.1f);  // past BLITZ_DURATION_S=30
    REQUIRE(st == SessionState::Complete);
}

TEST_CASE("Blitz: time_remaining decrements") {
    auto db = make_db();
    GameSession s("blitz", {"A"}, db);
    s.start();
    Clock c;
    advance(s, c, GameSession::COUNTDOWN_S + 1.0f, 0.1f);
    REQUIRE(s.get_time_remaining() < GameSession::BLITZ_DURATION_S);
}

// ── Survival ─────────────────────────────────────────────────────────────────
TEST_CASE("Survival: miss removes a life") {
    auto db = make_db();
    GameSession s("survival", {"A"}, db);
    s.start();
    REQUIRE(s.get_lives(1) == GameSession::SURVIVAL_LIVES);
    Clock c;
    advance_until(s, c, SessionState::StimulusActive);
    advance(s, c, GameSession::TIMEOUT_MS / 1000.0f + 0.1f);  // timeout, no press
    REQUIRE(s.get_lives(1) == GameSession::SURVIVAL_LIVES - 1);
}

TEST_CASE("Survival: Complete when lives reach 0") {
    auto db = make_db();
    GameSession s("survival", {"A"}, db);
    s.start();
    Clock c;
    for (int i = 0; i < GameSession::SURVIVAL_LIVES; ++i) {
        advance_until(s, c, SessionState::StimulusActive);
        advance(s, c, GameSession::TIMEOUT_MS / 1000.0f + 0.2f);   // miss
        advance(s, c, GameSession::FEEDBACK_MS / 1000.0f + 0.1f);  // feedback
    }
    REQUIRE(s.get_state() == SessionState::Complete);
}

// ── Streak ───────────────────────────────────────────────────────────────────
TEST_CASE("Streak: increments on fast valid press") {
    auto db = make_db();
    GameSession s("classic", {"A"}, db);
    s.start();
    Clock c;
    advance_until(s, c, SessionState::StimulusActive);
    press(s, c, SDLK_SPACE);  // ~200 ms → fast
    REQUIRE(s.get_current_streak() == 1);
    REQUIRE(s.get_max_streak() == 1);
}

TEST_CASE("Streak: resets on miss") {
    auto db = make_db();
    GameSession s("classic", {"A"}, db);
    s.start();
    Clock c;
    advance_until(s, c, SessionState::StimulusActive);
    press(s, c, SDLK_SPACE);  // streak = 1
    advance(s, c, GameSession::FEEDBACK_MS / 1000.0f + 0.1f);
    advance_until(s, c, SessionState::StimulusActive);
    advance(s, c, GameSession::TIMEOUT_MS / 1000.0f + 0.1f);  // timeout
    REQUIRE(s.get_current_streak() == 0);
    REQUIRE(s.get_max_streak() == 1);  // max preserved
}

// ── Session Rank ─────────────────────────────────────────────────────────────
TEST_CASE("Rank: S for mean < 200ms with no misses") {
    ResultsScreen rs({}, {"A"}, GameMode::Classic, 0);
    REQUIRE(rs.compute_rank(190.0f, 0, 10) == 'S');
}

TEST_CASE("Rank: penalizes misses") {
    ResultsScreen rs({}, {"A"}, GameMode::Classic, 0);
    // 190ms mean + 2 misses (2*40=80) = 270ms adjusted → B
    REQUIRE(rs.compute_rank(190.0f, 2, 10) == 'B');
}

TEST_CASE("Rank: D for empty results") {
    ResultsScreen rs({}, {"A"}, GameMode::Classic, 0);
    REQUIRE(rs.compute_rank(-1.0f, 0, 0) == 'D');
}
