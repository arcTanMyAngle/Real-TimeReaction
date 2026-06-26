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
