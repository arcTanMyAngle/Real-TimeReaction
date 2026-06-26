#pragma once
#include "ui/screen.hpp"
#include "data/models.hpp"

#include "imgui.h"
#include <string>
#include <vector>

class ResultsScreen : public IScreen {
public:
    ResultsScreen(const std::vector<Trial>& trials,
                  const std::vector<std::string>& player_names);

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

    // Public so tests can assert it directly.
    // Returns "P1" | "P2" | "TIE" | "NO_DATA".
    std::string determine_winner() const;

private:
    std::vector<Trial>       trials_;
    std::vector<std::string> player_names_;
    ScreenResult             result_;
    int                      scroll_offset_ = 0;

    struct Stats {
        int   count        = 0;
        int   misses       = 0;
        int   false_starts = 0;
        float avg_ms       = -1.0f;
        float best_ms      = -1.0f;
        float worst_ms     = -1.0f;
    };

    Stats  compute_stats(int player) const;
    ImVec4 rt_color(float ms) const;  // success/warning/danger thresholds
    float  player_avg(int player) const;
    bool   is_two_player() const;

    void render_stats_row(const Stats& s, int player);
    void render_two_player_stats();
    void render_winner_banner();
    void render_trial_log();
    void render_action_buttons();
};
