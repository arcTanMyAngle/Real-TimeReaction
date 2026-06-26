#pragma once
#include "ui/screen.hpp"
#include "data/database.hpp"
#include "data/analytics.hpp"

#include <string>
#include <vector>

class AnalyticsScreen : public IScreen {
public:
    explicit AnalyticsScreen(Database& db, int initial_player_id = -1);

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

private:
    Database&  db_;
    ScreenResult result_;

    // Player selector
    std::vector<Player>  all_players_;
    int                  selected_idx_  = 0;

    // Loaded data for selected player
    Analytics::Stats                        stats_;
    std::vector<Analytics::SessionSummary>  sessions_;
    std::vector<Analytics::LeaderEntry>     leaders_;
    std::vector<float>                      rt_hist_data_;  // for ImPlot histogram
    std::vector<float>                      trend_x_;       // session indices
    std::vector<float>                      trend_y_;       // avg ms per session

    // UI state
    int   table_scroll_  = 0;
    float toast_timer_   = 0.0f;
    std::string toast_msg_;

    void load_player_data(int player_id);
    void render_player_selector();
    void render_stats_bar();
    void render_charts();           // ImPlot histogram + trend
    void render_session_table();
    void render_leaderboard();
    void render_export_buttons();
    void render_toast();

    void show_toast(const std::string& msg);

    int  current_player_id() const;
};
