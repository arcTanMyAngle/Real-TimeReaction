#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "data/database.hpp"

namespace {
Database make_db() {
    Database db(":memory:");
    db.create_schema();
    return db;
}
}

TEST_CASE("Database: create_schema is idempotent") {
    Database db(":memory:");
    REQUIRE_NOTHROW(db.create_schema());
    REQUIRE_NOTHROW(db.create_schema());
}

TEST_CASE("Database: insert_player returns a positive id") {
    Database db = make_db();
    int id = db.insert_player("Alice");
    REQUIRE(id > 0);
}

TEST_CASE("Database: get_or_create_player returns the same id on second call") {
    Database db = make_db();
    int first  = db.get_or_create_player("Bob");
    int second = db.get_or_create_player("Bob");
    REQUIRE(first > 0);
    REQUIRE(first == second);

    auto players = db.get_all_players();
    REQUIRE(players.size() == 1);
    REQUIRE(players[0].name == "Bob");
}

TEST_CASE("Database: insert_session returns positive id; close_session succeeds") {
    Database db = make_db();
    int pid = db.get_or_create_player("Solo");

    Session s;
    s.player1_id = pid;
    s.mode       = "single";
    s.started_at = Database::now_iso();
    int sid = db.insert_session(s);
    REQUIRE(sid > 0);

    REQUIRE_NOTHROW(db.close_session(sid));
}

TEST_CASE("Database: insert_trial round-trips through get_session_trials") {
    Database db = make_db();
    int pid = db.get_or_create_player("Solo");

    Session s;
    s.player1_id = pid;
    s.mode       = "single";
    s.started_at = Database::now_iso();
    int sid = db.insert_session(s);

    Trial t;
    t.session_id           = sid;
    t.trial_number         = 1;
    t.stimulus_type        = "circle";
    t.stimulus_color       = "green";
    t.player               = 1;
    t.stimulus_onset_epoch = 1234.5;
    t.reaction_time_ms     = 287.5f;
    t.false_start          = false;
    t.response_epoch       = 1234.8;
    int tid = db.insert_trial(t);
    REQUIRE(tid > 0);

    auto trials = db.get_session_trials(sid);
    REQUIRE(trials.size() == 1);
    const Trial& got = trials[0];
    REQUIRE(got.session_id == sid);
    REQUIRE(got.trial_number == 1);
    REQUIRE(got.stimulus_type == "circle");
    REQUIRE(got.stimulus_color == "green");
    REQUIRE(got.player == 1);
    REQUIRE(got.reaction_time_ms == Catch::Approx(287.5f));
    REQUIRE(got.false_start == false);
    REQUIRE_FALSE(is_miss(got));
}

TEST_CASE("Database: get_player_trials filters by the player's sessions") {
    Database db = make_db();
    int p1 = db.get_or_create_player("Alice");
    int p2 = db.get_or_create_player("Bob");

    auto make_session = [&](int pid) {
        Session s;
        s.player1_id = pid;
        s.mode       = "single";
        s.started_at = Database::now_iso();
        return db.insert_session(s);
    };
    int sidA = make_session(p1);
    int sidB = make_session(p2);

    auto make_trial = [&](int sid, int n) {
        Trial t;
        t.session_id     = sid;
        t.trial_number   = n;
        t.stimulus_type  = "square";
        t.stimulus_color = "red";
        t.player         = 1;
        db.insert_trial(t);
    };
    make_trial(sidA, 1);
    make_trial(sidA, 2);
    make_trial(sidB, 1);

    auto alice = db.get_player_trials(p1);
    auto bob   = db.get_player_trials(p2);
    REQUIRE(alice.size() == 2);
    REQUIRE(bob.size() == 1);
    for (const auto& t : alice) REQUIRE(t.session_id == sidA);
    for (const auto& t : bob)   REQUIRE(t.session_id == sidB);
}
