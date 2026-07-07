#include "ui/screens/game_screen.hpp"
#include "ui/colors.hpp"

#include "imgui.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

namespace {

float now_seconds() { return static_cast<float>(SDL_GetTicks64()) / 1000.0f; }

ImVec4 stimulus_color_vec(const std::string& name) {
    if (name == "red")    return ImVec4(0.86f, 0.20f, 0.20f, 1.0f);
    if (name == "green")  return ImVec4(0.20f, 0.78f, 0.31f, 1.0f);
    if (name == "blue")   return ImVec4(0.24f, 0.51f, 0.86f, 1.0f);
    if (name == "yellow") return ImVec4(0.86f, 0.78f, 0.12f, 1.0f);
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

const char* state_label(SessionState s) {
    switch (s) {
        case SessionState::Idle:           return "Ready";
        case SessionState::Countdown:      return "Get ready...";
        case SessionState::Waiting:        return "Wait for it...";
        case SessionState::StimulusActive: return "GO!";
        case SessionState::Collecting:     return "Result";
        case SessionState::Complete:       return "Complete";
    }
    return "";
}

} // namespace

GameScreen::GameScreen(Database& db,
                       const std::vector<std::string>& player_names,
                       const std::string& mode,
                       const std::string& game_mode)
    : db_(db),
      mode_(mode),
      game_mode_(game_mode),
      player_names_(player_names),
      session_(game_mode, player_names, db),
      camera_(Display::CAMERA_W, Display::CAMERA_H) {
    total_trials_ =
        GameSession::TRIALS_PER_PLAYER * (mode == "two_player" ? 2 : 1);

    // Camera-feed centre (window-relative) for the flying performance label.
    const bool two = (mode == "two_player");
    camera_center_ = two ? ImVec2(640.0f, 210.0f) : ImVec2(360.0f, 360.0f);

    if (!player_names_.empty())
        personal_best_ms_ =
            db_.get_personal_best(db_.get_or_create_player(player_names_[0]));

    session_.start();
    camera_.open();  // false → black panel; never fatal
}

GameScreen::~GameScreen() = default;

void GameScreen::handle_event(const SDL_Event& e) {
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) {
            result_.transition = true;
            result_.next       = ScreenResult::Next::Menu;
        } else if (!e.key.repeat) {
            keys_this_frame_.push_back(e.key.keysym.sym);
        }
    }
}

void GameScreen::update(float dt) {
    camera_.update();
    const SessionState st = session_.tick(now_seconds(), keys_this_frame_);
    keys_this_frame_.clear();

    process_feel(st);
    effects_.update(dt);

    if (false_start_flash_timer_ > 0.0f)
        false_start_flash_timer_ -= dt;

    if (st == SessionState::Complete && !result_.transition) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Results;
    }
}

// Audio + visual feedback driven off state transitions and newly appended trials.
void GameScreen::process_feel(SessionState st) {
    if (prev_state_ != SessionState::StimulusActive &&
        st == SessionState::StimulusActive)
        audio_.play(AudioEngine::Sound::Stimulus);
    prev_state_ = st;

    const auto& results = session_.get_results();
    if (results.size() <= seen_results_) return;

    const Trial& tr     = results.back();
    const int    player = tr.player;
    if (tr.false_start) {
        audio_.play(AudioEngine::Sound::FalseStart);
        effects_.flash.trigger(Colors::DANGER);
        effects_.label.trigger(0.0f, camera_center_, true);
        false_start_flash_timer_ = 0.6f;
        flash_name_ = player_name(player);
    } else if (is_miss(tr)) {
        audio_.play(AudioEngine::Sound::Miss);
        effects_.label.trigger(-1.0f, camera_center_);
    } else {
        const float rt = tr.reaction_time_ms;
        audio_.play(AudioEngine::Sound::Hit);
        effects_.flash.trigger(player == 1 ? Colors::P1 : Colors::P2);
        effects_.label.trigger(rt, camera_center_);
        if (rt > 0.0f && (personal_best_ms_ < 0.0f || rt < personal_best_ms_)) {
            audio_.play(AudioEngine::Sound::NewRecord);
            effects_.record.trigger();
            personal_best_ms_ = rt;
        }
    }
    seen_results_ = results.size();
}

ScreenResult GameScreen::get_result() const { return result_; }

ImVec4 GameScreen::player_color(int player) {
    return player == 2 ? Colors::P2 : Colors::P1;
}

std::string GameScreen::player_name(int player) const {
    const int idx = player - 1;
    if (idx >= 0 && idx < static_cast<int>(player_names_.size()) &&
        !player_names_[idx].empty())
        return player_names_[idx];
    return "Player " + std::to_string(player);
}

void GameScreen::render() {
    if (mode_ == "two_player")
        render_two_player_layout();
    else
        render_single_player_layout();

    render_false_start_flash();

    // Phase 5 feel layer drawn on top of everything — cover the live window.
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    effects_.render_foreground(static_cast<int>(disp.x), static_cast<int>(disp.y));
    effects_.label.render();
}

void GameScreen::render_single_player_layout() {
    begin_fullscreen("##game");

    render_camera_panel(ImVec2(40, 120),
                        ImVec2(static_cast<float>(Display::CAMERA_W),
                               static_cast<float>(Display::CAMERA_H)));
    render_hud_panel(ImVec2(720, 120), ImVec2(Display::W - 760.0f, 480.0f));
    render_progress_bar(ImVec2(40, Display::H - 48.0f),
                        ImVec2(Display::W - 80.0f, 20.0f));

    ImGui::End();
}

void GameScreen::render_camera_panel(ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorPos(pos);
    ImGui::Image(reinterpret_cast<ImTextureID>(
                     static_cast<intptr_t>(camera_.texture_id())),
                 size);
    const ImVec2 panel_min = ImGui::GetItemRectMin();
    const ImVec2 panel_max = ImGui::GetItemRectMax();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const SessionState st = session_.get_state();

    if (st == SessionState::Countdown) {
        render_countdown_overlay(panel_min, size,
                                 session_.get_countdown_remaining(now_seconds()));
    } else if (st == SessionState::StimulusActive) {
        if (auto info = session_.get_current_trial())
            render_stimulus(panel_min, size, *info);
    } else if (st == SessionState::Collecting) {
        if (auto lr = session_.get_last_result(); lr && lr->false_start) {
            dl->AddRectFilled(panel_min, panel_max,
                              ImGui::GetColorU32(ImVec4(Colors::DANGER.x,
                                                        Colors::DANGER.y,
                                                        Colors::DANGER.z, 0.45f)));
            const char* msg = "FALSE START";
            ImFont* font = ImGui::GetFont();
            const float fs = font->FontSize * 2.0f;
            const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, msg);
            dl->AddText(font, fs,
                        ImVec2(panel_min.x + (size.x - tsz.x) * 0.5f,
                               panel_min.y + (size.y - tsz.y) * 0.5f),
                        ImGui::GetColorU32(Colors::TEXT), msg);
        }
    }

    const ImVec4 bcol =
        effects_.border.border_color(st, session_.get_active_player());
    const float bth = effects_.border.border_thickness(st);
    dl->AddRect(panel_min, panel_max, ImGui::GetColorU32(bcol), 8.0f, 0, bth);
}

void GameScreen::render_stimulus(ImVec2 camera_pos, ImVec2 camera_size,
                                 const TrialInfo& info) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center(camera_pos.x + camera_size.x * 0.5f,
                        camera_pos.y + camera_size.y * 0.5f);
    const float radius = std::min(camera_size.x, camera_size.y) * 0.18f;
    draw_stimulus_shape(dl, info.stimulus_type, info.stimulus_color, center,
                        radius);
}

void GameScreen::draw_stimulus_shape(ImDrawList* dl, const std::string& type,
                                     const std::string& color, ImVec2 center,
                                     float radius) {
    const ImU32 col = ImGui::ColorConvertFloat4ToU32(stimulus_color_vec(color));
    if (type == "circle") {
        dl->AddCircleFilled(center, radius, col, 64);
    } else if (type == "square") {
        dl->AddRectFilled(ImVec2(center.x - radius, center.y - radius),
                          ImVec2(center.x + radius, center.y + radius), col);
    } else {  // cross
        const float t = radius * 0.3f;
        dl->AddRectFilled(ImVec2(center.x - radius, center.y - t * 0.5f),
                          ImVec2(center.x + radius, center.y + t * 0.5f), col);
        dl->AddRectFilled(ImVec2(center.x - t * 0.5f, center.y - radius),
                          ImVec2(center.x + t * 0.5f, center.y + radius), col);
    }
}

void GameScreen::render_countdown_overlay(ImVec2 pos, ImVec2 size,
                                          float remaining_s) {
    const int n = std::max(1, static_cast<int>(std::ceil(remaining_s)));
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", n);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      ImGui::GetColorU32(ImVec4(0, 0, 0, 0.5f)));

    ImFont* font = ImGui::GetFont();
    const float fs = font->FontSize * 5.0f;
    const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, buf);
    dl->AddText(font, fs,
                ImVec2(pos.x + (size.x - tsz.x) * 0.5f,
                       pos.y + (size.y - tsz.y) * 0.5f),
                ImGui::GetColorU32(Colors::ACCENT), buf);
}

void GameScreen::render_hud_panel(ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorPos(pos);
    ImGui::BeginChild("##hud", size, true);

    const SessionState st = session_.get_state();
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::ACCENT);
    ImFont* font = ImGui::GetFont();
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted(state_label(st));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 16));

    render_mode_widgets();

    const GameMode gm = session_.get_mode();
    if (gm == GameMode::Classic || gm == GameMode::Race) {
        int trial_number = 0;
        if (auto info = session_.get_current_trial())
            trial_number = info->trial_number;
        else if (st == SessionState::Complete)
            trial_number = total_trials_;
        ImGui::TextColored(Colors::MUTED, "Trial");
        ImGui::Text("%d / %d", std::min(trial_number, total_trials_),
                    total_trials_);
        ImGui::Dummy(ImVec2(0, 16));
    }

    ImGui::TextColored(Colors::MUTED, "Last reaction");
    if (auto lr = session_.get_last_result()) {
        if (lr->false_start)
            ImGui::TextColored(Colors::DANGER, "False start");
        else if (lr->reaction_time_ms < 0.0f)
            ImGui::TextColored(Colors::WARNING, "Miss");
        else
            ImGui::Text("%.0f ms", lr->reaction_time_ms);
    } else {
        ImGui::TextColored(Colors::MUTED, "--");
    }

    ImGui::Dummy(ImVec2(0, 24));
    ImGui::TextColored(Colors::MUTED, "Press SPACE the moment the shape appears.");
    ImGui::TextColored(Colors::MUTED, "ESC to return to the menu.");
    (void)font;

    ImGui::EndChild();
}

void GameScreen::render_lives(ImVec2 pos, int lives, ImVec4 color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < GameSession::SURVIVAL_LIVES; ++i) {
        ImVec2 c = {pos.x + i * 32.0f, pos.y};
        const bool alive = (i < lives);
        dl->AddCircleFilled(c, 11.0f,
            alive ? ImGui::ColorConvertFloat4ToU32(color)
                  : IM_COL32(50, 50, 55, 255));
        dl->AddCircle(c, 11.0f, IM_COL32(100, 100, 110, 200), 0, 1.5f);
    }
}

// Streak counter (all modes) + mode-specific HUD. Emits at the current cursor
// inside whatever window/child is active.
void GameScreen::render_mode_widgets() {
    const GameMode gm  = session_.get_mode();
    const bool     two = (player_names_.size() > 1);
    const float    now = now_seconds();

    // ── Live streak (all modes) ──
    const int streak = session_.get_current_streak();
    if (streak >= 2) {
        float scale = 1.2f;
        if (streak >= 5) scale += 0.18f * std::sin(now * 9.0f);
        ImGui::SetWindowFontScale(scale);
        ImGui::TextColored(Colors::WARNING, "x%d STREAK", streak);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0, 8));
    }

    // ── Mode-specific ──
    if (gm == GameMode::Blitz) {
        const float t = session_.get_time_remaining();
        const ImVec4 col = (t > 15.0f) ? Colors::SUCCESS
                         : (t > 5.0f)  ? Colors::WARNING
                                       : Colors::DANGER;
        ImGui::TextColored(Colors::MUTED, "Time");
        float scale = 2.0f;
        if (t < 5.0f) scale += 0.25f * std::sin(now * 12.0f);
        ImGui::SetWindowFontScale(scale);
        ImGui::TextColored(col, "%.1fs", t);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0, 12));
    } else if (gm == GameMode::Survival) {
        ImGui::TextColored(Colors::MUTED, "Lives");
        ImGui::Dummy(ImVec2(0, 4));
        ImVec2 base = ImGui::GetCursorScreenPos();
        base.x += 12.0f; base.y += 12.0f;
        render_lives(base, session_.get_lives(1), Colors::P1);
        ImGui::Dummy(ImVec2(0, 28));
        if (two) {
            ImVec2 b2 = ImGui::GetCursorScreenPos();
            b2.x += 12.0f; b2.y += 12.0f;
            render_lives(b2, session_.get_lives(2), Colors::P2);
            ImGui::Dummy(ImVec2(0, 28));
        }
    } else if (gm == GameMode::Race) {
        const int w1 = session_.get_rounds_won(1);
        const int w2 = session_.get_rounds_won(2);
        ImGui::TextColored(Colors::MUTED, "Round score");
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(Colors::P1, "%s  %d", player_name(1).c_str(), w1);
        ImGui::SameLine();
        ImGui::TextColored(Colors::MUTED, " - ");
        ImGui::SameLine();
        ImGui::TextColored(Colors::P2, "%d  %s", w2, player_name(2).c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0, 12));
    }
}

void GameScreen::render_progress_bar(ImVec2 pos, ImVec2 size) {
    int completed = 0;
    for (const auto& t : session_.get_results())
        if (!t.false_start) ++completed;
    const float frac =
        total_trials_ > 0
            ? std::min(1.0f, static_cast<float>(completed) / total_trials_)
            : 0.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 scr = ImGui::GetWindowPos();
    const ImVec2 a(scr.x + pos.x, scr.y + pos.y);
    const ImVec2 b(a.x + size.x, a.y + size.y);
    dl->AddRectFilled(a, b, ImGui::GetColorU32(Colors::PANEL), 6.0f);
    dl->AddRectFilled(a, ImVec2(a.x + size.x * frac, b.y),
                      ImGui::GetColorU32(Colors::ACCENT), 6.0f);
}

// ── Two-player layout ────────────────────────────────────────────────────────
void GameScreen::render_two_player_layout() {
    begin_fullscreen("##game2p");

    const ImVec2 disp = ImGui::GetIO().DisplaySize;  // track window resize
    const int  active = session_.get_active_player();
    const bool race   = (session_.get_mode() == GameMode::Race);
    const auto last   = session_.get_last_result();
    const float panel_w = 320.0f;
    const float full_h  = disp.y;

    // In Race both players are live every round, so both panels read active.
    render_player_panel(ImVec2(0, 0), ImVec2(panel_w, full_h), 1,
                        player_name(1), race || active == 1, last);
    render_player_panel(ImVec2(disp.x - panel_w, 0),
                        ImVec2(panel_w, full_h), 2, player_name(2),
                        race || active == 2, last);

    // Camera 400x300, centred, y=60.
    const ImVec2 cam_size(400.0f, 300.0f);
    const ImVec2 cam_pos((disp.x - cam_size.x) * 0.5f, 60.0f);
    ImGui::SetCursorPos(cam_pos);
    ImGui::Image(reinterpret_cast<ImTextureID>(
                     static_cast<intptr_t>(camera_.texture_id())),
                 cam_size);
    const ImVec2 cam_min = ImGui::GetItemRectMin();
    const ImVec2 cam_max = ImGui::GetItemRectMax();

    const SessionState st = session_.get_state();
    if (st == SessionState::Countdown) {
        render_countdown_overlay(cam_min, cam_size,
                                 session_.get_countdown_remaining(now_seconds()));
    } else if (st == SessionState::StimulusActive) {
        if (auto info = session_.get_current_trial())
            render_stimulus(cam_min, cam_size, *info);
    }

    // Camera border breathes during Waiting; solid on stimulus (active player).
    const ImVec4 bcol = effects_.border.border_color(st, active);
    const float  bth  = effects_.border.border_thickness(st);
    ImGui::GetWindowDrawList()->AddRect(
        cam_min, cam_max, ImGui::GetColorU32(bcol), 8.0f, 0, bth);

    render_progress_bar(ImVec2(cam_pos.x, cam_pos.y + cam_size.y + 24.0f),
                        ImVec2(cam_size.x, 20.0f));

    // Mode HUD (streak / timer / lives / round score) centred below the camera.
    ImGui::SetCursorPos(ImVec2((disp.x - 360.0f) * 0.5f,
                               cam_pos.y + cam_size.y + 64.0f));
    ImGui::BeginChild("##modehud", ImVec2(360.0f, 200.0f), false);
    render_mode_widgets();
    ImGui::EndChild();

    ImGui::End();
}

void GameScreen::render_player_panel(ImVec2 pos, ImVec2 size, int player,
                                     const std::string& name, bool is_active,
                                     const std::optional<LastResult>& last) {
    ImGui::SetCursorPos(pos);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, is_active ? 3.0f : 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          is_active ? Colors::ACCENT : Colors::MUTED);
    ImGui::BeginChild((std::string("##panel") + std::to_string(player)).c_str(),
                      size, true);

    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextColored(player_color(player), "%s", name.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(0, 12));
    if (is_active) {
        ImGui::PushStyleColor(ImGuiCol_Button, Colors::ACCENT);
        ImGui::PushStyleColor(ImGuiCol_Text, Colors::BG);
        ImGui::Button("YOUR TURN");
        ImGui::PopStyleColor(2);
    } else {
        ImGui::TextColored(Colors::MUTED, "WAITING");
    }

    ImGui::Dummy(ImVec2(0, 24));
    ImGui::TextColored(Colors::MUTED, "Last reaction");
    ImGui::SetWindowFontScale(1.8f);
    if (last && last->player == player && !last->false_start) {
        if (last->reaction_time_ms < 0.0f)
            ImGui::TextColored(Colors::WARNING, "MISS");
        else
            ImGui::Text("%.0f ms", last->reaction_time_ms);
    } else {
        ImGui::TextColored(Colors::MUTED, "--");
    }
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(0, 24));
    ImGui::TextColored(Colors::MUTED, "Key");
    ImGui::Text("%s", player == 1 ? "[ SPACE ]" : "[ ENTER ]");

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void GameScreen::render_false_start_flash() {
    if (false_start_flash_timer_ <= 0.0f)
        return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0),
                      ImVec2(static_cast<float>(Display::W),
                             static_cast<float>(Display::H)),
                      IM_COL32(220, 50, 50, 120));

    const std::string msg = "FALSE START — " + flash_name_ + "!";
    ImFont* font = ImGui::GetFont();
    const float fs = font->FontSize * 2.2f;
    const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, msg.c_str());
    dl->AddText(font, fs,
                ImVec2((Display::W - tsz.x) * 0.5f, (Display::H - tsz.y) * 0.5f),
                IM_COL32(255, 255, 255, 255), msg.c_str());
}
