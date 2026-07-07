#include "ui/screens/menu_screen.hpp"
#include "ui/colors.hpp"

#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float MENU_PANEL_W = 460.0f;

float menu_scale() {
    const float sx = ImGui::GetWindowWidth() / static_cast<float>(Display::W);
    const float sy = ImGui::GetWindowHeight() / static_cast<float>(Display::H);
    return std::clamp(std::min(sx, sy), 1.0f, 1.55f);
}

float scaled(float value) {
    return std::floor(value * menu_scale());
}

float menu_panel_w() {
    const float max_width = ImGui::GetWindowWidth() - scaled(96.0f);
    return std::floor(std::min(MENU_PANEL_W * menu_scale(), max_width));
}

float centered_x(float width) {
    const float x = (ImGui::GetWindowWidth() - width) * 0.5f;
    return std::floor(std::max(0.0f, x));
}

void center_next_item(float width) {
    ImGui::SetCursorPosX(centered_x(width));
}

void centered_text(const char* text, const ImVec4& color, float within_width = 0.0f) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    if (within_width > 0.0f) {
        const float inner_x = std::max(0.0f, (within_width - size.x) * 0.5f);
        ImGui::SetCursorPosX(centered_x(within_width) + inner_x);
    } else {
        center_next_item(size.x);
    }
    ImGui::TextColored(color, "%s", text);
}

ImFont* font_at(int index) {
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    return atlas->Fonts.Size > index && atlas->Fonts[index] ? atlas->Fonts[index] : ImGui::GetFont();
}

ImFont* menu_body_font() {
    const float scale = menu_scale();
    if (scale >= 1.35f) return font_at(2);
    if (scale >= 1.15f) return font_at(1);
    return font_at(0);
}

ImFont* menu_title_font() {
    return menu_scale() >= 1.30f ? font_at(4) : font_at(3);
}

} // namespace

MenuScreen::MenuScreen(Database& db, AppState& state)
    : db_(db), state_(state) {
    // Pre-fill from the saved session state (one name per player).
    two_player_    = state_.two_player;
    selected_mode_ = mode_from_string(state_.game_mode);

    const std::string p1 = state_.player1();
    const std::string p2 = state_.player2();
    std::strncpy(p1_name_, p1.c_str(), sizeof(p1_name_) - 1);
    std::strncpy(p2_name_, p2.c_str(), sizeof(p2_name_) - 1);
}

void MenuScreen::handle_event(const SDL_Event& /*e*/) {}

void MenuScreen::update(float /*dt*/) {}

ScreenResult MenuScreen::get_result() const { return result_; }

void MenuScreen::render() {
    begin_fullscreen("##menu");

    const float scale = menu_scale();
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushFont(menu_body_font());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(style.ItemSpacing.x * scale,
                               style.ItemSpacing.y * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x * scale,
                               style.FramePadding.y * scale));

    render_title();
    ImGui::Dummy(ImVec2(0, scaled(24.0f)));

    render_game_mode_grid();
    ImGui::Dummy(ImVec2(0, scaled(12.0f)));
    render_player_count_toggle();
    ImGui::Dummy(ImVec2(0, scaled(12.0f)));
    render_name_inputs();
    ImGui::Dummy(ImVec2(0, scaled(24.0f)));
    render_start_button();
    ImGui::Dummy(ImVec2(0, scaled(8.0f)));
    render_analytics_button();

    ImGui::PopStyleVar(2);
    ImGui::PopFont();
    ImGui::End();
}

void MenuScreen::render_title() {
    const char* title = "REACTIVATION";
    ImGui::SetCursorPosY(scaled(64.0f));
    ImGui::PushFont(menu_title_font());
    centered_text(title, Colors::ACCENT);
    ImGui::PopFont();

    const char* sub = "Reaction Time Trainer";
    centered_text(sub, Colors::MUTED);
}

void MenuScreen::render_game_mode_grid() {
    const float panel_w = menu_panel_w();
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 btn(std::floor((panel_w - gap * 3.0f) * 0.25f),
                     scaled(44.0f));

    auto mode_button = [&](const char* label, GameMode m) {
        const bool active = (selected_mode_ == m);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, Colors::ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, Colors::BG);
        }
        if (ImGui::Button(label, btn)) selected_mode_ = m;
        if (active) ImGui::PopStyleColor(2);
    };

    center_next_item(panel_w);
    ImGui::BeginGroup();
    mode_button("CLASSIC", GameMode::Classic);
    ImGui::SameLine();
    mode_button("RACE", GameMode::Race);
    ImGui::SameLine();
    mode_button("BLITZ", GameMode::Blitz);
    ImGui::SameLine();
    mode_button("SURVIVAL", GameMode::Survival);

    // Race requires two players; force the toggle on.
    if (race_selected()) two_player_ = true;

    const char* desc = "";
    switch (selected_mode_) {
        case GameMode::Classic:  desc = "Fixed trials. Lowest average wins."; break;
        case GameMode::Race:     desc = "Both keys live. First press wins each round."; break;
        case GameMode::Blitz:    desc = "30 seconds. Most valid presses wins."; break;
        case GameMode::Survival: desc = "Three lives. A miss or false start costs one."; break;
    }
    centered_text(desc, Colors::MUTED, panel_w);
    ImGui::EndGroup();
}

void MenuScreen::render_player_count_toggle() {
    const float panel_w = menu_panel_w();
    if (race_selected()) {
        centered_text("Race is two-player.", Colors::MUTED, panel_w);
        return;
    }

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 btn(std::floor((panel_w - gap) * 0.5f), scaled(40.0f));
    auto toggle = [&](const char* label, bool active) -> bool {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, Colors::ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, Colors::BG);
        }
        const bool clicked = ImGui::Button(label, btn);
        if (active) ImGui::PopStyleColor(2);
        return clicked;
    };

    center_next_item(panel_w);
    ImGui::BeginGroup();
    if (toggle("Single Player", !two_player_)) two_player_ = false;
    ImGui::SameLine();
    if (toggle("Two Player", two_player_)) two_player_ = true;
    ImGui::EndGroup();
}

void MenuScreen::render_name_inputs() {
    const float panel_w = menu_panel_w();
    center_next_item(panel_w);
    ImGui::BeginGroup();
    ImGui::PushItemWidth(panel_w);
    ImGui::TextColored(Colors::TEXT, "Player 1");
    ImGui::InputTextWithHint("##p1", "enter name", p1_name_, sizeof(p1_name_));

    if (two_player_) {
        ImGui::Dummy(ImVec2(0, scaled(8.0f)));
        ImGui::TextColored(Colors::TEXT, "Player 2");
        ImGui::InputTextWithHint("##p2", "enter name", p2_name_, sizeof(p2_name_));
    }
    ImGui::PopItemWidth();
    ImGui::EndGroup();
}

void MenuScreen::render_start_button() {
    const bool p1_ok = std::strlen(p1_name_) > 0;
    const bool p2_ok = !two_player_ || std::strlen(p2_name_) > 0;
    const bool can_start = p1_ok && p2_ok;
    const float panel_w = menu_panel_w();

    center_next_item(panel_w);
    ImGui::BeginDisabled(!can_start);
    ImGui::PushStyleColor(ImGuiCol_Button, Colors::ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::BG);
    if (ImGui::Button("START", ImVec2(panel_w, scaled(48.0f))) && can_start) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Game;
        result_.mode       = two_player_ ? "two_player" : "single";
        result_.game_mode  = mode_to_string(selected_mode_);
        result_.player_names.clear();
        result_.player_names.emplace_back(p1_name_);
        if (two_player_) result_.player_names.emplace_back(p2_name_);

        // Persist the user/session state so it survives navigation and restart.
        state_.player_names = result_.player_names;
        state_.game_mode    = result_.game_mode;
        state_.two_player   = two_player_;
        save_app_state(db_, state_);
    }
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
}

void MenuScreen::render_analytics_button() {
    const float panel_w = menu_panel_w();
    center_next_item(panel_w);
    if (ImGui::Button("Analytics", ImVec2(panel_w, scaled(36.0f)))) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Analytics;
    }
}
