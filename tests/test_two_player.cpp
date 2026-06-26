#include <catch2/catch_test_macros.hpp>
#include <SDL2/SDL_keycode.h>
#include "core/session.hpp"
#include "data/database.hpp"
#include "ui/screens/results_screen.hpp"

// Helper: advance session time by ticking with empty keys
static SessionState advance(GameSession& s, float seconds,
                            float step = 0.016f) {
    float t = 0;
    SessionState st = s.get_state();
    while (t < seconds) {
        st = s.tick(t, {});
        t += step;
    }
    return st;
}

TEST_CASE("Two-player: trials alternate P1/P2") {
    Database db(":memory:"); db.create_schema();
    GameSession s("two_player", {"Alice","Bob"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);
    REQUIRE(s.get_active_player() == 1);  // first trial is P1
}

TEST_CASE("Two-player: inactive player key ignored during Waiting") {
    Database db(":memory:"); db.create_schema();
    GameSession s("two_player", {"Alice","Bob"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    // P2 key during P1's Waiting — should be ignored
    s.tick(GameSession::COUNTDOWN_S + 0.2f, {SDLK_RETURN});
    REQUIRE(s.get_state() == SessionState::Waiting);
    bool any_false_start = false;
    for (auto& t : s.get_results())
        if (t.false_start) any_false_start = true;
    REQUIRE_FALSE(any_false_start);
}

TEST_CASE("Two-player: active player false start recorded") {
    Database db(":memory:"); db.create_schema();
    GameSession s("two_player", {"Alice","Bob"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    s.tick(GameSession::COUNTDOWN_S + 0.2f, {SDLK_SPACE});  // P1 false start
    bool found = false;
    for (auto& t : s.get_results())
        if (t.false_start) found = true;
    REQUIRE(found);
}

TEST_CASE("Two-player: inactive player key ignored during StimulusActive") {
    Database db(":memory:"); db.create_schema();
    GameSession s("two_player", {"Alice","Bob"}, db);
    s.start();
    // Drive a monotonic clock until the stimulus appears (ISI is random, so we
    // observe the state each tick rather than over-advancing into a timeout).
    float t = 0.0f;
    SessionState st = s.get_state();
    while (st != SessionState::StimulusActive && t < 15.0f) {
        st = s.tick(t, {});
        t += 0.016f;
    }
    REQUIRE(st == SessionState::StimulusActive);
    REQUIRE(s.get_active_player() == 1);
    // P2 key during P1's stimulus — ignored, state unchanged (no timeout yet).
    t += 0.016f;
    st = s.tick(t, {SDLK_RETURN});
    REQUIRE(st == SessionState::StimulusActive);
}

TEST_CASE("Winner determination: basic") {
    // Build trial list manually for ResultsScreen test
    std::vector<Trial> trials;
    for (int i = 0; i < 5; ++i) {
        Trial t; t.player=1; t.reaction_time_ms=200.0f; t.false_start=false;
        trials.push_back(t);
    }
    for (int i = 0; i < 5; ++i) {
        Trial t; t.player=2; t.reaction_time_ms=250.0f; t.false_start=false;
        trials.push_back(t);
    }
    ResultsScreen rs(trials, {"Alice","Bob"});
    REQUIRE(rs.determine_winner() == "P1");
}

TEST_CASE("Winner determination: tie within 5ms") {
    std::vector<Trial> trials;
    for (int i=0; i<5; ++i) {
        Trial t1; t1.player=1; t1.reaction_time_ms=200.0f; t1.false_start=false;
        Trial t2; t2.player=2; t2.reaction_time_ms=203.0f; t2.false_start=false;
        trials.push_back(t1); trials.push_back(t2);
    }
    ResultsScreen rs(trials, {"A","B"});
    REQUIRE(rs.determine_winner() == "TIE");
}

TEST_CASE("Winner determination: all misses for P1") {
    std::vector<Trial> trials;
    for (int i=0; i<5; ++i) {
        Trial t1; t1.player=1; t1.reaction_time_ms=-1.0f; t1.false_start=false;
        Trial t2; t2.player=2; t2.reaction_time_ms=250.0f; t2.false_start=false;
        trials.push_back(t1); trials.push_back(t2);
    }
    ResultsScreen rs(trials, {"A","B"});
    REQUIRE(rs.determine_winner() == "P2");
}
