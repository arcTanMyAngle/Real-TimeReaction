#pragma once
#include "ui/screen.hpp"
#include "data/database.hpp"
#include "data/models.hpp"
#include "core/app_state.hpp"

class MenuScreen : public IScreen {
public:
    // `state` is owned by main and persists across screens; the menu pre-fills
    // from it and writes the chosen names/mode back (+ to the DB) on START.
    MenuScreen(Database& db, AppState& state);

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

private:
    Database&  db_;
    AppState&  state_;
    char     p1_name_[64]   = {};
    char     p2_name_[64]   = {};
    bool     two_player_    = false;
    GameMode selected_mode_ = GameMode::Classic;
    ScreenResult result_;

    bool race_selected() const { return selected_mode_ == GameMode::Race; }

    void render_title();
    void render_name_inputs();
    void render_game_mode_grid();      // CLASSIC / RACE / BLITZ / SURVIVAL
    void render_player_count_toggle(); // Single / Two (hidden for Race)
    void render_start_button();
    void render_analytics_button();
};
