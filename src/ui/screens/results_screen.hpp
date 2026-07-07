#pragma once
#include "ui/screen.hpp"
#include "data/models.hpp"

#include "imgui.h"
#include <string>
#include <vector>

class ResultsScreen : public IScreen {
public:
    ResultsScreen(const std::vector<Trial>& trials,
                  const std::vector<std::string>& player_names,
                  GameMode mode = GameMode::Classic,
                  int max_streak = 0);

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

    // Public so tests can assert it directly.
    // Returns "P1" | "P2" | "TIE" | "NO_DATA".
    std::string determine_winner() const;

    // Public for tests. Session rank from the player's mean RT + miss penalty.
    char   compute_rank(float mean_ms, int misses, int total) const;
    ImVec4 rank_color(char r) const;

private:
    std::vector<Trial>       trials_;
    std::vector<std::string> player_names_;
    GameMode                 game_mode_  = GameMode::Classic;
    int                      max_streak_ = 0;
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

    void render_rank_display();
    void render_mode_header();
    void render_stats_row(const Stats& s, int player);
    void render_two_player_stats();
    void render_winner_banner();
    void render_trial_log();
    void render_action_buttons();
};
