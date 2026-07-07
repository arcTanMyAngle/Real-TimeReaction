#include "ui/screens/results_screen.hpp"
#include "ui/colors.hpp"

#include "imgui.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

ResultsScreen::ResultsScreen(const std::vector<Trial>& trials,
                             const std::vector<std::string>& player_names,
                             GameMode mode, int max_streak)
    : trials_(trials),
      player_names_(player_names),
      game_mode_(mode),
      max_streak_(max_streak) {}

void ResultsScreen::handle_event(const SDL_Event& e) {
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Menu;
    }
}

void ResultsScreen::update(float /*dt*/) {}

ScreenResult ResultsScreen::get_result() const { return result_; }

ResultsScreen::Stats ResultsScreen::compute_stats(int player) const {
    Stats s;
    float sum = 0.0f;
    for (const auto& t : trials_) {
        if (t.player != player) continue;
        if (t.false_start) { ++s.false_starts; continue; }
        ++s.count;
        if (is_miss(t)) {
            ++s.misses;
            continue;
        }
        sum += t.reaction_time_ms;
        if (s.best_ms < 0.0f || t.reaction_time_ms < s.best_ms)
            s.best_ms = t.reaction_time_ms;
        if (t.reaction_time_ms > s.worst_ms)
            s.worst_ms = t.reaction_time_ms;
    }
    const int hits = s.count - s.misses;
    if (hits > 0) s.avg_ms = sum / static_cast<float>(hits);
    return s;
}

ImVec4 ResultsScreen::rt_color(float ms) const {
    if (ms < 0.0f)    return Colors::MUTED;
    if (ms < 250.0f)  return Colors::SUCCESS;
    if (ms <= 400.0f) return Colors::WARNING;
    return Colors::DANGER;
}

float ResultsScreen::player_avg(int player) const {
    float sum = 0.0f;
    int   n   = 0;
    for (const auto& t : trials_)
        if (t.player == player && !t.false_start && !is_miss(t)) {
            sum += t.reaction_time_ms;
            ++n;
        }
    return n > 0 ? sum / static_cast<float>(n) : -1.0f;
}

bool ResultsScreen::is_two_player() const {
    if (player_names_.size() > 1) return true;
    for (const auto& t : trials_)
        if (t.player == 2) return true;
    return false;
}

char ResultsScreen::compute_rank(float mean_ms, int misses, int total) const {
    if (mean_ms < 0.0f || total == 0) return 'D';
    // Penalize misses: each miss adds 40ms to the effective average.
    const float adj = mean_ms + (misses * 40.0f);
    if (adj < 200.0f) return 'S';
    if (adj < 250.0f) return 'A';
    if (adj < 300.0f) return 'B';
    if (adj < 350.0f) return 'C';
    return 'D';
}

ImVec4 ResultsScreen::rank_color(char r) const {
    switch (r) {
        case 'S': return ImVec4{1.0f, 0.84f, 0.0f, 1.0f};  // gold
        case 'A': return Colors::SUCCESS;
        case 'B': return Colors::ACCENT;
        case 'C': return Colors::WARNING;
        default:  return Colors::DANGER;
    }
}

std::string ResultsScreen::determine_winner() const {
    const float a1 = player_avg(1);
    const float a2 = player_avg(2);
    if (a1 < 0 && a2 < 0) return "NO_DATA";
    if (a1 < 0) return "P2";
    if (a2 < 0) return "P1";
    if (std::abs(a1 - a2) < 5.0f) return "TIE";
    return (a1 < a2) ? "P1" : "P2";
}

void ResultsScreen::render() {
    begin_fullscreen("##results", /*scrollable=*/true);

    ImGui::PushStyleColor(ImGuiCol_Text, Colors::ACCENT);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextUnformatted(is_two_player() ? "SESSION RESULTS" : "RESULTS");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 12));

    if (is_two_player()) {
        render_mode_header();
        render_two_player_stats();
        ImGui::Dummy(ImVec2(0, 8));
        render_winner_banner();
        ImVec4 scol = max_streak_ >= 5 ? Colors::SUCCESS
                    : max_streak_ >= 3 ? Colors::ACCENT
                                       : Colors::MUTED;
        ImGui::TextColored(scol, "Max Streak: %d  (under 300 ms)", max_streak_);
    } else {
        render_rank_display();
        render_mode_header();
        render_stats_row(compute_stats(1), 1);
    }

    ImGui::Dummy(ImVec2(0, 12));
    render_trial_log();
    ImGui::Dummy(ImVec2(0, 12));
    render_action_buttons();

    ImGui::End();
}

void ResultsScreen::render_rank_display() {
    const Stats s     = compute_stats(1);
    const int   total = s.count;  // hits + misses (excludes false starts)
    const char  rank  = compute_rank(s.avg_ms, s.misses, total);
    const ImVec4 col  = rank_color(rank);

    const char* desc = "Keep Practicing";
    switch (rank) {
        case 'S': desc = "Superhuman";      break;
        case 'A': desc = "Elite";           break;
        case 'B': desc = "Solid";           break;
        case 'C': desc = "Average";         break;
        default:  desc = "Keep Practicing"; break;
    }

    ImGui::BeginChild("##rank", ImVec2(Display::W - 80.0f, 132.0f), true);
    const std::string letter = std::string("[ ") + rank + " ]";
    ImGui::SetWindowFontScale(3.4f);
    ImVec2 lsz = ImGui::CalcTextSize(letter.c_str());
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - lsz.x) * 0.5f);
    ImGui::TextColored(col, "%s", letter.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImVec2 dsz = ImGui::CalcTextSize(desc);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - dsz.x) * 0.5f);
    ImGui::TextColored(col, "%s", desc);
    ImGui::EndChild();

    ImVec4 scol = max_streak_ >= 5 ? Colors::SUCCESS
                : max_streak_ >= 3 ? Colors::ACCENT
                                   : Colors::MUTED;
    ImGui::TextColored(scol, "Max Streak: %d  (under 300 ms)", max_streak_);
    ImGui::Dummy(ImVec2(0, 8));
}

void ResultsScreen::render_mode_header() {
    if (game_mode_ == GameMode::Race) {
        int w1 = 0, w2 = 0;
        for (const auto& t : trials_) {
            if (t.round_winner == 1) ++w1;
            else if (t.round_winner == 2) ++w2;
        }
        const std::string n1 =
            (!player_names_.empty() && !player_names_[0].empty())
                ? player_names_[0] : "Player 1";
        const std::string n2 =
            (player_names_.size() > 1 && !player_names_[1].empty())
                ? player_names_[1] : "Player 2";
        ImGui::BeginChild("##racehdr", ImVec2(Display::W - 80.0f, 64.0f), true);
        ImGui::SetWindowFontScale(1.6f);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s  %d - %d  %s", n1.c_str(), w1, w2,
                      n2.c_str());
        ImVec2 sz = ImGui::CalcTextSize(buf);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - sz.x) * 0.5f);
        const ImVec4 col = (w1 == w2) ? Colors::WARNING
                         : (w1 > w2)  ? Colors::P1 : Colors::P2;
        ImGui::TextColored(col, "%s", buf);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild();
        ImGui::Dummy(ImVec2(0, 8));
    } else if (game_mode_ == GameMode::Blitz) {
        int valid = 0;
        for (const auto& t : trials_)
            if (!t.false_start && !is_miss(t)) ++valid;
        ImGui::TextColored(Colors::ACCENT, "Blitz score: %d valid presses", valid);
        ImGui::TextColored(Colors::MUTED, "Trials attempted: %d",
                           static_cast<int>(trials_.size()));
        ImGui::Dummy(ImVec2(0, 8));
    } else if (game_mode_ == GameMode::Survival) {
        int survived = 0;
        for (const auto& t : trials_)
            if (!t.false_start) ++survived;
        ImGui::TextColored(Colors::ACCENT, "Survived %d trials", survived);
        ImGui::Dummy(ImVec2(0, 8));
    }
}

void ResultsScreen::render_two_player_stats() {
    const float panel_w = (Display::W - 80.0f - 24.0f) * 0.5f;
    auto panel = [&](int player) {
        const Stats s = compute_stats(player);
        const std::string name =
            (player - 1 < static_cast<int>(player_names_.size()) &&
             !player_names_[player - 1].empty())
                ? player_names_[player - 1]
                : ("Player " + std::to_string(player));

        ImGui::BeginChild(("##tpstats" + std::to_string(player)).c_str(),
                          ImVec2(panel_w, 150.0f), true);
        ImGui::SetWindowFontScale(1.3f);
        ImGui::TextColored(player == 1 ? Colors::P1 : Colors::P2, "%s",
                           name.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();

        auto stat = [&](const char* label, float v) {
            ImGui::TextColored(Colors::MUTED, "%-7s", label);
            ImGui::SameLine(110);
            if (v < 0)
                ImGui::TextColored(Colors::MUTED, "--");
            else
                ImGui::TextColored(rt_color(v), "%.0f ms", v);
        };
        stat("Avg", s.avg_ms);
        stat("Best", s.best_ms);
        stat("Worst", s.worst_ms);
        ImGui::TextColored(Colors::MUTED, "Misses");
        ImGui::SameLine(110);
        ImGui::TextColored(s.misses ? Colors::WARNING : Colors::TEXT, "%d",
                           s.misses);
        ImGui::EndChild();
    };

    panel(1);
    ImGui::SameLine(0, 24);
    panel(2);
}

void ResultsScreen::render_winner_banner() {
    const std::string w = determine_winner();
    ImVec4 color = Colors::MUTED;
    std::string headline;
    std::string sub;

    if (w == "P1" || w == "P2") {
        const int win = (w == "P1") ? 1 : 2;
        color = (win == 1) ? Colors::P1 : Colors::P2;
        const std::string name =
            (win - 1 < static_cast<int>(player_names_.size()) &&
             !player_names_[win - 1].empty())
                ? player_names_[win - 1]
                : ("Player " + std::to_string(win));
        headline = "*** " + name + " WINS ***";
        const float diff = std::abs(player_avg(1) - player_avg(2));
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.0f ms faster on average", diff);
        sub = buf;
    } else if (w == "TIE") {
        color = Colors::WARNING;
        headline = "TOO CLOSE TO CALL";
    } else {  // NO_DATA
        color = Colors::MUTED;
        headline = "No completed trials";
    }

    ImGui::BeginChild("##winner", ImVec2(Display::W - 80.0f, 72.0f), true);
    ImGui::SetWindowFontScale(1.6f);
    const ImVec2 hsz = ImGui::CalcTextSize(headline.c_str());
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - hsz.x) * 0.5f);
    ImGui::TextColored(color, "%s", headline.c_str());
    ImGui::SetWindowFontScale(1.0f);
    if (!sub.empty()) {
        const ImVec2 ssz = ImGui::CalcTextSize(sub.c_str());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ssz.x) * 0.5f);
        ImGui::TextColored(Colors::MUTED, "%s", sub.c_str());
    }
    ImGui::EndChild();
}

void ResultsScreen::render_stats_row(const Stats& s, int player) {
    const std::string name =
        (player - 1 < static_cast<int>(player_names_.size()) &&
         !player_names_[player - 1].empty())
            ? player_names_[player - 1]
            : ("Player " + std::to_string(player));

    ImGui::BeginChild(("##stats" + std::to_string(player)).c_str(),
                      ImVec2(Display::W - 80.0f, 96.0f), true);
    ImGui::TextColored(player == 1 ? Colors::P1 : Colors::P2, "%s", name.c_str());
    ImGui::Separator();

    auto cell = [](const char* label, const char* value, ImVec4 col) {
        ImGui::BeginGroup();
        ImGui::TextColored(Colors::MUTED, "%s", label);
        ImGui::TextColored(col, "%s", value);
        ImGui::EndGroup();
    };

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0f ms", s.avg_ms);
    cell("Average", s.avg_ms < 0 ? "--" : buf, rt_color(s.avg_ms));
    ImGui::SameLine(0, 48);
    std::snprintf(buf, sizeof(buf), "%.0f ms", s.best_ms);
    cell("Best", s.best_ms < 0 ? "--" : buf, rt_color(s.best_ms));
    ImGui::SameLine(0, 48);
    std::snprintf(buf, sizeof(buf), "%.0f ms", s.worst_ms);
    cell("Worst", s.worst_ms < 0 ? "--" : buf, rt_color(s.worst_ms));
    ImGui::SameLine(0, 48);
    std::snprintf(buf, sizeof(buf), "%d", s.misses);
    cell("Misses", buf, s.misses ? Colors::WARNING : Colors::TEXT);
    ImGui::SameLine(0, 48);
    std::snprintf(buf, sizeof(buf), "%d", s.false_starts);
    cell("False starts", buf, s.false_starts ? Colors::DANGER : Colors::TEXT);

    ImGui::EndChild();
}

void ResultsScreen::render_trial_log() {
    const bool two = is_two_player();
    ImGui::TextColored(Colors::MUTED, "Trial log");
    ImGui::BeginChild("##log", ImVec2(Display::W - 80.0f, 280.0f), true);
    for (const auto& t : trials_) {
        // Two-player: subtle per-player row tint spanning the log width.
        if (two) {
            ImVec4 tint = (t.player == 1) ? Colors::P1 : Colors::P2;
            tint.w = 0.15f;
            const ImVec2 rmin = ImGui::GetCursorScreenPos();
            const float  rw   = ImGui::GetContentRegionAvail().x;
            const float  rh   = ImGui::GetTextLineHeightWithSpacing();
            ImGui::GetWindowDrawList()->AddRectFilled(
                rmin, ImVec2(rmin.x + rw, rmin.y + rh),
                ImGui::GetColorU32(tint));
        }
        if (two) {
            ImGui::TextColored(t.player == 1 ? Colors::P1 : Colors::P2, "P%d",
                               t.player);
            ImGui::SameLine(36);
        }
        ImGui::Text("#%02d", t.trial_number);
        ImGui::SameLine(100);
        ImGui::TextColored(Colors::MUTED, "%s %s", t.stimulus_color.c_str(),
                           t.stimulus_type.c_str());
        ImGui::SameLine(280);
        if (t.false_start) {
            ImGui::TextColored(Colors::DANGER, "FALSE START");
        } else if (is_miss(t)) {
            ImGui::TextColored(Colors::WARNING, "MISS");
        } else {
            ImGui::TextColored(rt_color(t.reaction_time_ms), "%.0f ms",
                               t.reaction_time_ms);
        }
    }
    ImGui::EndChild();
}

void ResultsScreen::render_action_buttons() {
    ImGui::PushStyleColor(ImGuiCol_Button, Colors::ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::BG);
    if (ImGui::Button("PLAY AGAIN", ImVec2(220, 44))) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Menu;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    if (ImGui::Button("MAIN MENU", ImVec2(220, 44))) {
        result_.transition = true;
        result_.next       = ScreenResult::Next::Menu;
    }
}
