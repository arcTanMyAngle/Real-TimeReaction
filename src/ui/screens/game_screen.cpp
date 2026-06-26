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
                       const std::string& mode)
    : db_(db),
      mode_(mode),
      player_names_(player_names),
      session_(mode, player_names, db),
      camera_(Display::CAMERA_W, Display::CAMERA_H) {
    total_trials_ =
        GameSession::TRIALS_PER_PLAYER * (mode == "two_player" ? 2 : 1);
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

    // Detect a newly recorded false start (results grew and the new tail is one)
    // and trigger the 0.6s flash with the offending player's name.
    const auto& results = session_.get_results();
    if (results.size() > seen_results_) {
        if (!results.empty() && results.back().false_start) {
            false_start_flash_timer_ = 0.6f;
            flash_name_ = player_name(results.back().player);
        }
        seen_results_ = results.size();
    }
    if (false_start_flash_timer_ > 0.0f)
        false_start_flash_timer_ -= dt;

    if (st == SessionState::Complete && !result_.transition) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Results;
    }
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
}

void GameScreen::render_single_player_layout() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(Display::W),
                                    static_cast<float>(Display::H)));
    ImGui::Begin("##game", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize);

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

    dl->AddRect(panel_min, panel_max, ImGui::GetColorU32(Colors::ACCENT), 8.0f,
                0, 2.0f);
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

    int trial_number = 0;
    if (auto info = session_.get_current_trial())
        trial_number = info->trial_number;
    else if (st == SessionState::Complete)
        trial_number = total_trials_;
    ImGui::TextColored(Colors::MUTED, "Trial");
    ImGui::Text("%d / %d", std::min(trial_number, total_trials_), total_trials_);

    ImGui::Dummy(ImVec2(0, 16));

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
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(Display::W),
                                    static_cast<float>(Display::H)));
    ImGui::Begin("##game2p", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize);

    const int active = session_.get_active_player();
    const auto last  = session_.get_last_result();
    const float panel_w = 320.0f;
    const float full_h  = static_cast<float>(Display::H);

    render_player_panel(ImVec2(0, 0), ImVec2(panel_w, full_h), 1,
                        player_name(1), active == 1, last);
    render_player_panel(ImVec2(Display::W - panel_w, 0),
                        ImVec2(panel_w, full_h), 2, player_name(2), active == 2,
                        last);

    // Camera 400x300, centred, y=60.
    const ImVec2 cam_size(400.0f, 300.0f);
    const ImVec2 cam_pos((Display::W - cam_size.x) * 0.5f, 60.0f);
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

    // Camera border colour matches the active player.
    ImGui::GetWindowDrawList()->AddRect(
        cam_min, cam_max, ImGui::GetColorU32(player_color(active)), 8.0f, 0, 3.0f);

    render_progress_bar(ImVec2(cam_pos.x, cam_pos.y + cam_size.y + 24.0f),
                        ImVec2(cam_size.x, 20.0f));

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
