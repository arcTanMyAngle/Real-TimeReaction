#include "app_state.hpp"

AppState load_app_state(Database& db) {
    AppState s;
    const std::string p1 = db.get_setting("player1");
    const std::string p2 = db.get_setting("player2");
    if (!p1.empty())            s.player_names.push_back(p1);
    if (!p2.empty())            s.player_names.push_back(p2);
    s.game_mode  = db.get_setting("game_mode", "classic");
    s.two_player = db.get_setting("two_player", "0") == "1";
    return s;
}

void save_app_state(Database& db, const AppState& state) {
    db.set_setting("player1", state.player1());
    db.set_setting("player2", state.player2());
    db.set_setting("game_mode", state.game_mode);
    db.set_setting("two_player", state.two_player ? "1" : "0");
}
