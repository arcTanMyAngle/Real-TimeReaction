#pragma once
#include "ui/screen.hpp"
#include "data/database.hpp"

class MenuScreen : public IScreen {
public:
    explicit MenuScreen(Database& db);

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

private:
    Database& db_;
    char  p1_name_[64] = {};
    char  p2_name_[64] = {};
    bool  two_player_  = false;
    ScreenResult result_;

    void render_title();
    void render_name_inputs();
    void render_mode_buttons();
    void render_start_button();
    void render_analytics_button();
};
