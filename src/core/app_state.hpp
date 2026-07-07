#pragma once
#include <string>
#include <vector>
#include "data/database.hpp"

// Holds the current user/session selection so it survives screen transitions
// (e.g. returning to the menu) and app restarts. One name per player.
struct AppState {
    std::vector<std::string> player_names;        // [p1] or [p1, p2]
    std::string              game_mode  = "classic";
    bool                     two_player = false;

    std::string player1() const {
        return player_names.empty() ? "" : player_names[0];
    }
    std::string player2() const {
        return player_names.size() > 1 ? player_names[1] : "";
    }
};

// Persisted via the Database `settings` key/value table.
AppState load_app_state(Database& db);
void     save_app_state(Database& db, const AppState& state);
