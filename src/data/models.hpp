#pragma once
#include <string>
#include <optional>

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
};

// Sentinel — use instead of optional to avoid nullable floats in hot path
inline bool is_miss(const Trial& t) { return t.reaction_time_ms < 0.0f; }
