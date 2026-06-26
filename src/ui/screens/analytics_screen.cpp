#include "ui/screens/analytics_screen.hpp"
#include "ui/colors.hpp"

#include "imgui.h"
#include "implot.h"
#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

std::string fmt_ms(float v) {
    if (v < 0.0f) return "-";  // default ImGui font has no em-dash glyph
    char b[32];
    std::snprintf(b, sizeof(b), "%.0f ms", v);
    return b;
}

} // namespace

AnalyticsScreen::AnalyticsScreen(Database& db, int initial_player_id)
    : db_(db) {
    all_players_ = db_.get_all_players();
    leaders_     = Analytics::get_leaderboard(db_);

    if (!all_players_.empty()) {
        if (initial_player_id >= 0) {
            for (std::size_t i = 0; i < all_players_.size(); ++i)
                if (all_players_[i].id == initial_player_id) {
                    selected_idx_ = static_cast<int>(i);
                    break;
                }
        }
        load_player_data(current_player_id());
    }
}

int AnalyticsScreen::current_player_id() const {
    if (all_players_.empty()) return -1;
    return all_players_[selected_idx_].id;
}

void AnalyticsScreen::handle_event(const SDL_Event& e) {
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Menu;
    }
}

void AnalyticsScreen::update(float dt) {
    if (toast_timer_ > 0.0f)
        toast_timer_ -= dt;
}

ScreenResult AnalyticsScreen::get_result() const { return result_; }

void AnalyticsScreen::show_toast(const std::string& msg) {
    toast_msg_   = msg;
    toast_timer_ = 2.5f;
}

void AnalyticsScreen::load_player_data(int player_id) {
    const std::vector<Trial> trials = db_.get_player_trials(player_id);
    stats_        = Analytics::compute_stats(trials);
    sessions_     = Analytics::get_player_sessions(db_, player_id);
    rt_hist_data_ = Analytics::valid_rts(trials);
    leaders_      = Analytics::get_leaderboard(db_);

    std::vector<std::vector<Trial>> session_trials;
    session_trials.reserve(sessions_.size());
    for (const auto& s : sessions_)
        session_trials.push_back(s.trials);

    trend_y_ = Analytics::session_trend(session_trials);
    trend_x_.resize(trend_y_.size());
    for (std::size_t i = 0; i < trend_x_.size(); ++i)
        trend_x_[i] = static_cast<float>(i + 1);
}

void AnalyticsScreen::render() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(Display::W),
                                    static_cast<float>(Display::H)));
    ImGui::Begin("##analytics", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize);

    ImGui::PushStyleColor(ImGuiCol_Text, Colors::ACCENT);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextUnformatted("ANALYTICS");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 40);
    render_player_selector();
    ImGui::Separator();

    if (all_players_.empty()) {
        ImGui::Dummy(ImVec2(0, 40));
        ImGui::TextColored(Colors::MUTED,
                           "No players yet. Play a session to see analytics.");
        ImGui::Dummy(ImVec2(0, 24));
        if (ImGui::Button("Main Menu", ImVec2(200, 40))) {
            result_.transition = true;
            result_.next       = ScreenResult::Next::Menu;
        }
        render_toast();
        ImGui::End();
        return;
    }

    render_stats_bar();
    ImGui::Separator();
    render_charts();
    ImGui::Separator();
    render_session_table();
    ImGui::Separator();
    render_leaderboard();
    ImGui::Separator();
    render_export_buttons();

    render_toast();
    ImGui::End();
}

void AnalyticsScreen::render_player_selector() {
    if (all_players_.empty()) {
        ImGui::TextColored(Colors::MUTED, "(no players)");
        return;
    }
    const int prev = selected_idx_;
    const int n    = static_cast<int>(all_players_.size());

    if (ImGui::ArrowButton("##prev", ImGuiDir_Left))
        selected_idx_ = (selected_idx_ - 1 + n) % n;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::P1);
    ImGui::Text("%s", all_players_[selected_idx_].name.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::ArrowButton("##next", ImGuiDir_Right))
        selected_idx_ = (selected_idx_ + 1) % n;

    if (selected_idx_ != prev)
        load_player_data(current_player_id());
}

void AnalyticsScreen::render_stats_bar() {
    auto cell = [](const char* label, const std::string& value) {
        ImGui::BeginGroup();
        ImGui::TextColored(Colors::MUTED, "%s", label);
        ImGui::TextColored(Colors::TEXT, "%s", value.c_str());
        ImGui::EndGroup();
    };

    cell("Count", std::to_string(stats_.count));
    ImGui::SameLine(0, 40); cell("Mean", fmt_ms(stats_.mean_ms));
    ImGui::SameLine(0, 40); cell("Median", fmt_ms(stats_.median_ms));
    ImGui::SameLine(0, 40); cell("Best", fmt_ms(stats_.best_ms));
    ImGui::SameLine(0, 40); cell("Worst", fmt_ms(stats_.worst_ms));
    ImGui::SameLine(0, 40); cell("Std Dev", fmt_ms(stats_.stdev_ms));
    ImGui::SameLine(0, 40); cell("Misses", std::to_string(stats_.misses));
    ImGui::SameLine(0, 40); cell("False starts", std::to_string(stats_.false_starts));
}

void AnalyticsScreen::render_charts() {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float w     = (avail - 12.0f) * 0.5f;
    const ImVec2 plot_size(w, 220.0f);

    if (ImPlot::BeginPlot("Reaction Time Distribution", plot_size,
                          ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes("ms", "count");
        if (!rt_hist_data_.empty()) {
            ImPlot::SetNextFillStyle(Colors::P1, 0.8f);
            ImPlot::PlotHistogram("RT", rt_hist_data_.data(),
                                  static_cast<int>(rt_hist_data_.size()), 20,
                                  1.0, ImPlotRange(0, 1000));
            const double ymax = static_cast<double>(rt_hist_data_.size());
            if (stats_.mean_ms > 0) {
                ImPlot::SetNextLineStyle(Colors::ACCENT, 2.0f);
                double xv[2] = {stats_.mean_ms, stats_.mean_ms};
                double yv[2] = {0, ymax};
                ImPlot::PlotLine("Mean", xv, yv, 2);
            }
            if (stats_.median_ms > 0) {
                ImPlot::SetNextLineStyle(Colors::WARNING, 2.0f);
                double xv[2] = {stats_.median_ms, stats_.median_ms};
                double yv[2] = {0, ymax};
                ImPlot::PlotLine("Median", xv, yv, 2);
            }
        }
        ImPlot::EndPlot();
    }

    ImGui::SameLine();

    if (ImPlot::BeginPlot("Session Improvement", plot_size)) {
        ImPlot::SetupAxes("session", "avg ms");
        if (!trend_x_.empty()) {
            ImPlot::SetNextLineStyle(Colors::ACCENT, 2.0f);
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5);
            ImPlot::PlotLine("Avg RT (ms)", trend_x_.data(), trend_y_.data(),
                             static_cast<int>(trend_x_.size()));
        }
        ImPlot::EndPlot();
    }
}

void AnalyticsScreen::render_session_table() {
    ImGui::TextColored(Colors::MUTED, "Session history");
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("##sessions", 5, flags, ImVec2(0, 130))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Date");
        ImGui::TableSetupColumn("Mode");
        ImGui::TableSetupColumn("Trials");
        ImGui::TableSetupColumn("Avg RT");
        ImGui::TableHeadersRow();

        int n = 1;
        for (const auto& s : sessions_) {
            const Analytics::Stats st = Analytics::compute_stats(s.trials);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%d", n++);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(s.started_at.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(s.mode.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", static_cast<int>(s.trials.size()));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(fmt_ms(st.mean_ms).c_str());
        }
        ImGui::EndTable();
    }
}

void AnalyticsScreen::render_leaderboard() {
    ImGui::TextColored(Colors::MUTED, "Leaderboard (top 5)");
    const int me = current_player_id();
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("##leaders", 4, flags)) {
        ImGui::TableSetupColumn("Rank");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Avg RT");
        ImGui::TableSetupColumn("Trials");
        ImGui::TableHeadersRow();

        const int shown = std::min<int>(5, static_cast<int>(leaders_.size()));
        for (int i = 0; i < shown; ++i) {
            const auto& l = leaders_[i];
            const bool mine = (l.player_id == me);
            ImGui::TableNextRow();
            const ImVec4 col = mine ? Colors::ACCENT : Colors::TEXT;
            ImGui::TableNextColumn(); ImGui::TextColored(col, "%d", i + 1);
            ImGui::TableNextColumn(); ImGui::TextColored(col, "%s", l.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%s", fmt_ms(l.mean_ms).c_str());
            ImGui::TableNextColumn(); ImGui::TextColored(col, "%d", l.trial_count);
        }
        if (shown == 0) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(Colors::MUTED, "Need 5+ valid trials to rank.");
        }
        ImGui::EndTable();
    }
}

void AnalyticsScreen::render_export_buttons() {
    ImGui::PushStyleColor(ImGuiCol_Button, Colors::ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::BG);
    if (ImGui::Button("EXPORT CSV", ImVec2(200, 44))) {
        const int pid = current_player_id();
        const auto trials = db_.get_player_trials(pid);
        const auto path = Analytics::export_trials_csv(
            trials, all_players_[selected_idx_].name);
        show_toast("Saved to " + path.string());
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    if (ImGui::Button("MAIN MENU", ImVec2(200, 44))) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Menu;
    }
}

void AnalyticsScreen::render_toast() {
    if (toast_timer_ <= 0.0f)
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 ts = ImGui::CalcTextSize(toast_msg_.c_str());
    const float pad = 12.0f;
    const ImVec2 br(Display::W - 24.0f, Display::H - 24.0f);
    const ImVec2 tl(br.x - ts.x - pad * 2, br.y - ts.y - pad * 2);
    dl->AddRectFilled(tl, br, ImGui::GetColorU32(Colors::PANEL), 6.0f);
    dl->AddRect(tl, br, ImGui::GetColorU32(Colors::ACCENT), 6.0f);
    dl->AddText(ImVec2(tl.x + pad, tl.y + pad),
                ImGui::GetColorU32(Colors::TEXT), toast_msg_.c_str());
}
