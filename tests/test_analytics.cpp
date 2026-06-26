#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <fstream>
#include "data/analytics.hpp"
#include "data/database.hpp"

using Catch::Approx;

static Trial make_trial(int player, float rt, bool fs = false) {
    Trial t;
    t.player           = player;
    t.reaction_time_ms = rt;
    t.false_start      = fs;
    return t;
}

TEST_CASE("valid_rts excludes false starts and misses") {
    std::vector<Trial> trials = {
        make_trial(1, 200.0f),
        make_trial(1, -1.0f),           // miss
        make_trial(1, 300.0f, true),    // false start
        make_trial(1, 250.0f),
    };
    auto rts = Analytics::valid_rts(trials);
    REQUIRE(rts.size() == 2);
    REQUIRE(rts[0] == Approx(200.0f));
    REQUIRE(rts[1] == Approx(250.0f));
}

TEST_CASE("compute_stats basic") {
    std::vector<Trial> trials;
    for (float v : {200.0f, 220.0f, 240.0f, 260.0f, 280.0f})
        trials.push_back(make_trial(1, v));
    auto s = Analytics::compute_stats(trials);
    REQUIRE(s.count   == 5);
    REQUIRE(s.mean_ms == Approx(240.0f));
    REQUIRE(s.best_ms == Approx(200.0f));
    REQUIRE(s.worst_ms== Approx(280.0f));
    REQUIRE(s.misses  == 0);
}

TEST_CASE("compute_stats empty input") {
    auto s = Analytics::compute_stats({});
    REQUIRE(s.count   == 0);
    REQUIRE(s.mean_ms == Approx(-1.0f));
}

TEST_CASE("compute_stats excludes false starts from count and mean") {
    std::vector<Trial> trials = {
        make_trial(1, 200.0f),
        make_trial(1, 200.0f, true),    // false start — excluded
    };
    auto s = Analytics::compute_stats(trials);
    REQUIRE(s.count        == 1);
    REQUIRE(s.false_starts == 1);
    REQUIRE(s.mean_ms == Approx(200.0f));
}

TEST_CASE("compute_stats: misses counted separately, not in mean") {
    std::vector<Trial> trials = {
        make_trial(1, 200.0f),
        make_trial(1, -1.0f),           // miss
    };
    auto s = Analytics::compute_stats(trials);
    REQUIRE(s.count  == 1);
    REQUIRE(s.misses == 1);
    REQUIRE(s.mean_ms == Approx(200.0f));
}

TEST_CASE("session_trend two sessions") {
    std::vector<float> s1_rts = {200.0f, 300.0f};  // avg 250
    std::vector<float> s2_rts = {180.0f, 190.0f};  // avg 185
    std::vector<std::vector<Trial>> sessions(2);
    for (float v : s1_rts) sessions[0].push_back(make_trial(1, v));
    for (float v : s2_rts) sessions[1].push_back(make_trial(1, v));
    auto trend = Analytics::session_trend(sessions);
    REQUIRE(trend.size() == 2);
    REQUIRE(trend[0] == Approx(250.0f));
    REQUIRE(trend[1] == Approx(185.0f));
}

TEST_CASE("export_trials_csv creates file with correct headers", "[csv]") {
    std::vector<Trial> trials = { make_trial(1, 200.0f) };
    auto path = Analytics::export_trials_csv(trials, "TestPlayer");
    REQUIRE(std::filesystem::exists(path));
    std::ifstream f(path);
    std::string header;
    std::getline(f, header);
    REQUIRE(header.find("reaction_time_ms") != std::string::npos);
    REQUIRE(header.find("false_start") != std::string::npos);
    f.close();  // Windows can't remove a still-open file
    std::filesystem::remove(path);
}

TEST_CASE("get_leaderboard minimum 5 trials threshold") {
    Database db(":memory:"); db.create_schema();
    int pid = db.insert_player("Fast");
    Session s; s.player1_id = pid; s.mode = "single";
    s.started_at = Database::now_iso();
    int sid = db.insert_session(s);
    // Only 4 valid trials — should NOT appear in leaderboard
    for (int i = 0; i < 4; ++i) {
        Trial t; t.session_id = sid; t.trial_number = i+1;
        t.player = 1; t.stimulus_type = "circle";
        t.stimulus_color = "red"; t.stimulus_onset_epoch = 0;
        t.reaction_time_ms = 200.0f;
        db.insert_trial(t);
    }
    auto leaders = Analytics::get_leaderboard(db);
    bool found = false;
    for (auto& l : leaders) if (l.name == "Fast") found = true;
    REQUIRE_FALSE(found);
}
