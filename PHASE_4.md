# Phase 4: Analytics & Export

## Goal
Persistent analytics screen with per-player stats, ImPlot charts, and CSV export.
No external plotting library — ImPlot handles all in-app charts natively.

## Prerequisites
- Phase 3 Acceptance Criteria all passing
- `cd build && ctest --output-on-failure` fully passing
- ImPlot already linked (it was in CMakeLists.txt from Phase 1)

---

## Deliverable 1 — `src/data/analytics.hpp` + `analytics.cpp`

Pure computation. No ImGui, no ImPlot, no file I/O in the header.

```cpp
// analytics.hpp
#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include "models.hpp"
#include "database.hpp"

namespace Analytics {

// ── Stats ────────────────────────────────────────────────────────────────────

struct Stats {
    int   count        = 0;    // valid trials (not false_start, not miss)
    int   misses       = 0;    // reaction_time_ms == -1, not false_start
    int   false_starts = 0;
    float mean_ms      = -1.0f;
    float median_ms    = -1.0f;
    float stdev_ms     = -1.0f;  // -1 if count < 2
    float best_ms      = -1.0f;
    float worst_ms     = -1.0f;
    float p10_ms       = -1.0f;
    float p90_ms       = -1.0f;
};

// Returns only trials that are valid (not false_start, not miss)
std::vector<float> valid_rts(const std::vector<Trial>& trials, int player = 0);
// player=0 means all players

Stats compute_stats(const std::vector<Trial>& trials, int player = 0);

// Per-session average RTs for trend line. -1 if session had no valid trials.
std::vector<float> session_trend(
    const std::vector<std::vector<Trial>>& sessions_trials,
    int player = 0);

// ── DB Queries ────────────────────────────────────────────────────────────────

struct SessionSummary {
    int         session_id  = 0;
    std::string started_at;
    std::string mode;
    std::vector<Trial> trials;
};

std::vector<SessionSummary> get_player_sessions(Database& db, int player_id);

struct LeaderEntry {
    std::string name;
    float       mean_ms     = -1.0f;
    int         trial_count = 0;
    int         player_id   = 0;
};

// Top 10 players by mean RT. Minimum 5 valid trials to qualify.
std::vector<LeaderEntry> get_leaderboard(Database& db, int limit = 10);

// ── Export ────────────────────────────────────────────────────────────────────

namespace fs = std::filesystem;
fs::path export_dir();   // returns "data/exports", creates if needed

// Writes CSV. Returns path written.
fs::path export_trials_csv(const std::vector<Trial>& trials,
                            const std::string& player_name);

} // namespace Analytics
```

### Implementation notes for `analytics.cpp`:

`valid_rts`: filter `!t.false_start && !is_miss(t)` and optionally `t.player == player`.

`compute_stats`:
- `mean_ms`: `std::accumulate / count`
- `median_ms`: sort copy; `sorted[count/2]` (even count: average of two midpoints)
- `stdev_ms`: sample std dev — `sqrt(sum of (x-mean)^2 / (count-1))`
- `p10_ms` / `p90_ms`: `sorted[static_cast<int>(count * 0.10)]` / `* 0.90`
- All `-1.0f` if count == 0; `stdev_ms = -1.0f` if count < 2

`export_trials_csv`:
- Filename: `reaction_{player_name}_{YYYYMMDD_HHMMSS}.csv`
- Columns: `id,session_id,trial_number,player,stimulus_type,stimulus_color,reaction_time_ms,false_start,stimulus_onset_epoch`
- `reaction_time_ms` cell = "MISS" if `is_miss(t)`, "FALSE_START" if `t.false_start`
- Use `<fstream>` + `<ctime>` — no third-party

---

## Deliverable 2 — `src/ui/screens/analytics_screen.hpp` + `analytics_screen.cpp`

```cpp
class AnalyticsScreen : public IScreen {
public:
    explicit AnalyticsScreen(Database& db, int initial_player_id = -1);

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

private:
    Database&  db_;
    ScreenResult result_;

    // Player selector
    std::vector<Player>  all_players_;
    int                  selected_idx_  = 0;

    // Loaded data for selected player
    Analytics::Stats                        stats_;
    std::vector<Analytics::SessionSummary>  sessions_;
    std::vector<Analytics::LeaderEntry>     leaders_;
    std::vector<float>                      rt_hist_data_;  // for ImPlot histogram
    std::vector<float>                      trend_x_;       // session indices
    std::vector<float>                      trend_y_;       // avg ms per session

    // UI state
    int   table_scroll_  = 0;
    float toast_timer_   = 0.0f;
    std::string toast_msg_;

    void load_player_data(int player_id);
    void render_player_selector();
    void render_stats_bar();
    void render_charts();           // ImPlot histogram + trend
    void render_session_table();
    void render_leaderboard();
    void render_export_buttons();
    void render_toast();

    void show_toast(const std::string& msg);
};
```

### Layout
```
┌──────────────────────────────────────────────────────────────┐
│  ANALYTICS          [< Alice >]  (player selector)           │
│──────────────────────────────────────────────────────────────│
│  STATS:  Count | Mean | Median | Best | Worst | StdDev       │
│  [values in one horizontal row with labels above]            │
│──────────────────────────────────────────────────────────────│
│  [Chart: Histogram 50% W]  │  [Chart: Trend line 50% W]      │
│  "Reaction Time Distribution"  "Session Improvement"         │
│──────────────────────────────────────────────────────────────│
│  SESSION HISTORY (scrollable table):                          │
│  # | Date | Mode | Trials | Avg RT                           │
│──────────────────────────────────────────────────────────────│
│  LEADERBOARD (top 5):                                        │
│  Rank | Name | Avg RT | Trials  [current player highlighted] │
│──────────────────────────────────────────────────────────────│
│  [ EXPORT CSV ]   [ MAIN MENU ]                              │
│  (toast: "Saved to data/exports/..." bottom-right, 2.5s)     │
└──────────────────────────────────────────────────────────────┘
```

### ImPlot usage:
```cpp
// Histogram
if (ImPlot::BeginPlot("Reaction Time Distribution",
                       ImVec2(-1, 220), ImPlotFlags_NoLegend)) {
    ImPlot::SetNextFillStyle(Colors::P1, 0.8f);
    ImPlot::PlotHistogram("RT", rt_hist_data_.data(),
                           (int)rt_hist_data_.size(), 20,
                           1.0, ImPlotRange(0, 1000));
    // vertical dashed lines at mean and median
    if (stats_.mean_ms > 0) {
        ImPlot::SetNextLineStyle(Colors::ACCENT, 2.0f);
        double xv[2] = {stats_.mean_ms, stats_.mean_ms};
        double yv[2] = {0, (double)rt_hist_data_.size()};
        ImPlot::PlotLine("Mean", xv, yv, 2);
    }
    ImPlot::EndPlot();
}

// Trend line
if (ImPlot::BeginPlot("Session Improvement", ImVec2(-1, 220))) {
    ImPlot::SetNextLineStyle(Colors::ACCENT, 2.0f);
    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5);
    ImPlot::PlotLine("Avg RT (ms)", trend_x_.data(),
                      trend_y_.data(), (int)trend_x_.size());
    ImPlot::EndPlot();
}
```

### Player Selector:
```cpp
// Left arrow / Right arrow or buttons to cycle selected_idx_
if (ImGui::ArrowButton("##prev", ImGuiDir_Left))
    selected_idx_ = (selected_idx_ - 1 + all_players_.size()) % all_players_.size();
ImGui::SameLine();
ImGui::Text("%s", all_players_[selected_idx_].name.c_str());
ImGui::SameLine();
if (ImGui::ArrowButton("##next", ImGuiDir_Right))
    selected_idx_ = (selected_idx_ + 1) % all_players_.size();
// On change: call load_player_data(all_players_[selected_idx_].id)
```

Toast:
```cpp
void AnalyticsScreen::show_toast(const std::string& msg) {
    toast_msg_   = msg;
    toast_timer_ = 2.5f;
}
// In render(), if toast_timer_ > 0:
// Use ImGui::GetForegroundDrawList to draw bottom-right text
```

---

## Deliverable 3 — Updated `src/ui/screens/menu_screen.cpp`

The Analytics button is already wired in Phase 2 (`Next::Analytics` transition).
Verify it calls `load_player_data` with the first player in DB if any exist,
or renders an empty state gracefully if DB has no players yet.

No code changes needed if Phase 2 was implemented correctly.
If the Analytics button is missing: add it below START in `render_action_buttons()`.

---

## Deliverable 4 — `tests/test_analytics.cpp`

Add to `tests/CMakeLists.txt`:
```cmake
target_sources(rtr_tests PRIVATE test_analytics.cpp)
```

```cpp
#include <catch2/catch_test_macros.hpp>
#include "data/analytics.hpp"
#include "data/database.hpp"

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
```

---

## Stats Definitions Reference

| Stat | Definition |
|---|---|
| count | valid trials only (not false_start, not miss) |
| misses | `reaction_time_ms == -1` AND `!false_start` |
| false_starts | `false_start == true` |
| mean_ms | `sum(valid_rts) / count` |
| median_ms | middle value of sorted valid_rts |
| stdev_ms | sample std dev; `-1` if count < 2 |
| best_ms | `min(valid_rts)` |
| worst_ms | `max(valid_rts)` |
| p10_ms | `sorted[floor(count × 0.10)]` |
| p90_ms | `sorted[floor(count × 0.90)]` |

---

## Acceptance Criteria
- [ ] "Analytics" button on menu navigates to AnalyticsScreen
- [ ] Empty state renders cleanly when no players exist in DB
- [ ] Player selector cycles all players; data reloads on change
- [ ] Stats bar shows all 6 stats; `-1` displayed as "—"
- [ ] Histogram renders with correct bin range (0–1000ms)
- [ ] Mean and median vertical lines visible on histogram
- [ ] Trend line shows session-by-session average (x = session number)
- [ ] Session history table scrollable; shows correct dates and averages
- [ ] Leaderboard shows top 5; current player row highlighted in ACCENT color
- [ ] EXPORT CSV creates file in `data/exports/`, toast appears for 2.5s
- [ ] Players with < 5 valid trials excluded from leaderboard
- [ ] All Phase 1–3 behaviour unchanged
- [ ] `cd build && ctest --output-on-failure` — all tests pass

## Out of Scope
- PDF or image export
- Per-stimulus-type breakdown
- Player profile deletion or rename
- Sound / audio
- Fullscreen toggle
