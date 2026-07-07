#pragma once
#include <string>
#include <optional>

// Game modes (Phase 6). Defined in the data layer so both the session and the
// UI screens can reference it without pulling in SDL/Database headers.
enum class GameMode { Classic, Race, Blitz, Survival };

inline GameMode mode_from_string(const std::string& s) {
    if (s == "race")     return GameMode::Race;
    if (s == "blitz")    return GameMode::Blitz;
    if (s == "survival") return GameMode::Survival;
    return GameMode::Classic;  // "classic", "single", "two_player", anything else
}

inline const char* mode_to_string(GameMode m) {
    switch (m) {
        case GameMode::Race:     return "race";
        case GameMode::Blitz:    return "blitz";
        case GameMode::Survival: return "survival";
        default:                 return "classic";
    }
}

struct Player {
    int         id   = 0;
    std::string name;
    std::string created_at;  // ISO 8601
};

struct Session {
    int         id           = 0;
    int         player1_id   = 0;
    int         player2_id   = 0;   // 0 = single player
    std::string mode;               // "single" | "two_player"
    std::string started_at;
    std::string completed_at;
    std::string notes;
    std::string game_mode = "classic"; // "classic"|"race"|"blitz"|"survival"
};

struct Trial {
    int         id                   = 0;
    int         session_id           = 0;
    int         trial_number         = 0;
    std::string stimulus_type;       // "circle" | "square" | "cross"
    std::string stimulus_color;      // "red" | "green" | "blue" | "yellow"
    int         player               = 1;   // 1 or 2
    double      stimulus_onset_epoch = 0.0; // std::time(nullptr) at display
    float       reaction_time_ms     = -1.0f; // -1 = miss/timeout
    bool        false_start          = false;
    double      response_epoch       = 0.0;
    int         round_winner         = 0;   // 0=no contest, 1=P1, 2=P2 (Race)
    int         streak_at_time       = 0;   // live streak count at completion
};

// Sentinel — use instead of optional to avoid nullable floats in hot path
inline bool is_miss(const Trial& t) { return t.reaction_time_ms < 0.0f; }
