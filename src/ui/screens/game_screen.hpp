#pragma once
#include "ui/screen.hpp"
#include "data/database.hpp"
#include "core/session.hpp"
#include "ui/camera_texture.hpp"

#include "imgui.h"
#include <string>
#include <vector>

class GameScreen : public IScreen {
public:
    GameScreen(Database& db, const std::vector<std::string>& player_names,
               const std::string& mode);
    ~GameScreen();

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

    // main.cpp reads these before switching to the Results screen.
    const std::vector<Trial>& get_results() const { return session_.get_results(); }

private:
    Database&                db_;
    std::string              mode_;
    std::vector<std::string> player_names_;
    GameSession              session_;
    CameraTexture            camera_;
    ScreenResult             result_;
    int                      total_trials_ = 0;

    std::vector<SDL_Keycode> keys_this_frame_;

    // False-start flash (Phase 3): set to 0.6s when a new false start appears.
    float       false_start_flash_timer_ = 0.0f;
    std::string flash_name_;
    std::size_t seen_results_ = 0;

    void render_single_player_layout();
    void render_two_player_layout();
    void render_player_panel(ImVec2 pos, ImVec2 size, int player,
                             const std::string& name, bool is_active,
                             const std::optional<LastResult>& last);
    void render_false_start_flash();

    void render_camera_panel(ImVec2 pos, ImVec2 size);
    void render_stimulus(ImVec2 camera_pos, ImVec2 camera_size,
                         const TrialInfo& info);
    void render_countdown_overlay(ImVec2 pos, ImVec2 size, float remaining_s);
    void render_hud_panel(ImVec2 pos, ImVec2 size);
    void render_progress_bar(ImVec2 pos, ImVec2 size);
    void draw_stimulus_shape(ImDrawList* dl, const std::string& type,
                             const std::string& color, ImVec2 center, float radius);

    // Active-player accent: P1 / P2 colour.
    static ImVec4 player_color(int player);
    std::string   player_name(int player) const;
};
