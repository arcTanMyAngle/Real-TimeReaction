#include "ui/screens/menu_screen.hpp"
#include "ui/colors.hpp"

#include "imgui.h"
#include <cstring>

MenuScreen::MenuScreen(Database& db) : db_(db) {}

void MenuScreen::handle_event(const SDL_Event& /*e*/) {}

void MenuScreen::update(float /*dt*/) {}

ScreenResult MenuScreen::get_result() const { return result_; }

void MenuScreen::render() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(Display::W),
                                    static_cast<float>(Display::H)));
    ImGui::Begin("##menu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize);

    render_title();
    ImGui::Dummy(ImVec2(0, 24));

    // Centre a fixed-width column.
    const float panel_w = 460.0f;
    const float indent  = (ImGui::GetWindowWidth() - panel_w) * 0.5f;
    ImGui::Indent(indent);
    ImGui::BeginGroup();
    ImGui::PushItemWidth(panel_w);

    render_mode_buttons();
    ImGui::Dummy(ImVec2(0, 12));
    render_name_inputs();
    ImGui::Dummy(ImVec2(0, 24));
    render_start_button();
    ImGui::Dummy(ImVec2(0, 8));
    render_analytics_button();

    ImGui::PopItemWidth();
    ImGui::EndGroup();
    ImGui::Unindent(indent);

    ImGui::End();
}

void MenuScreen::render_title() {
    const char* title = "REAL-TIME REACTION";
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::ACCENT);
    ImFont* font = ImGui::GetFont();
    const float big = font->FontSize * 2.4f;
    const ImVec2 sz = font->CalcTextSizeA(big, FLT_MAX, 0.0f, title);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - sz.x) * 0.5f);
    ImGui::SetCursorPosY(64.0f);
    ImGui::GetWindowDrawList()->AddText(
        font, big, ImGui::GetCursorScreenPos(),
        ImGui::GetColorU32(Colors::ACCENT), title);
    ImGui::Dummy(sz);
    ImGui::PopStyleColor();

    const char* sub = "Reaction Time Trainer";
    const ImVec2 ssz = ImGui::CalcTextSize(sub);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ssz.x) * 0.5f);
    ImGui::TextColored(Colors::MUTED, "%s", sub);
}

void MenuScreen::render_mode_buttons() {
    const ImVec2 btn(220.0f, 40.0f);

    auto mode_button = [&](const char* label, bool active) -> bool {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, Colors::ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, Colors::BG);
        }
        const bool clicked = ImGui::Button(label, btn);
        if (active) ImGui::PopStyleColor(2);
        return clicked;
    };

    if (mode_button("Single Player", !two_player_)) two_player_ = false;
    ImGui::SameLine();
    if (mode_button("Two Player", two_player_)) two_player_ = true;
}

void MenuScreen::render_name_inputs() {
    ImGui::TextColored(Colors::TEXT, "Player 1");
    ImGui::InputTextWithHint("##p1", "enter name", p1_name_, sizeof(p1_name_));

    if (two_player_) {
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::TextColored(Colors::TEXT, "Player 2");
        ImGui::InputTextWithHint("##p2", "enter name", p2_name_, sizeof(p2_name_));
    }
}

void MenuScreen::render_start_button() {
    const bool p1_ok = std::strlen(p1_name_) > 0;
    const bool p2_ok = !two_player_ || std::strlen(p2_name_) > 0;
    const bool can_start = p1_ok && p2_ok;

    ImGui::BeginDisabled(!can_start);
    ImGui::PushStyleColor(ImGuiCol_Button, Colors::ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::BG);
    if (ImGui::Button("START", ImVec2(-FLT_MIN, 48.0f)) && can_start) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Game;
        result_.mode       = two_player_ ? "two_player" : "single";
        result_.player_names.clear();
        result_.player_names.emplace_back(p1_name_);
        if (two_player_) result_.player_names.emplace_back(p2_name_);
    }
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
}

void MenuScreen::render_analytics_button() {
    if (ImGui::Button("Analytics", ImVec2(-FLT_MIN, 36.0f))) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Analytics;
    }
}
