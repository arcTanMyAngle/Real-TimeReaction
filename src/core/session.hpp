#pragma once
#include <string>
#include <vector>
#include <optional>
#include <SDL2/SDL_keycode.h>
#include "data/models.hpp"
#include "data/database.hpp"

enum class SessionState {
    Idle,
    Countdown,       // 3-second count before first trial
    Waiting,         // inter-stimulus interval (random 2–5 s)
    StimulusActive,  // stimulus visible; awaiting keypress
    Collecting,      // result recorded; 800 ms feedback display
    Complete
};

struct TrialInfo {
    std::string stimulus_type;
    std::string stimulus_color;
    int         player       = 1;
    int         trial_number = 0;
    int         total_trials = 0;
};

struct LastResult {
    float reaction_time_ms = -1.0f;  // -1 = miss
    bool  false_start      = false;
    int   player           = 1;
};

class GameSession {
public:
    static constexpr int   TRIALS_PER_PLAYER = 10;
    static constexpr float ISI_MIN_S         = 2.0f;
    static constexpr float ISI_MAX_S         = 5.0f;
    static constexpr float TIMEOUT_MS        = 2000.0f;
    static constexpr float FEEDBACK_MS       = 800.0f;
    static constexpr float COUNTDOWN_S       = 3.0f;

    GameSession(const std::string& mode,
                const std::vector<std::string>& player_names,
                Database& db);

    void         start();  // registers players, inserts session row, → Countdown
    SessionState tick(float current_time_s, const std::vector<SDL_Keycode>& keys);

    SessionState               get_state()        const noexcept;
    int                        get_active_player() const noexcept;
    std::optional<TrialInfo>   get_current_trial() const;
    std::optional<LastResult>  get_last_result()   const;
    float                      get_countdown_remaining(float now_s) const;
    const std::vector<Trial>&  get_results()       const;

private:
    std::string              mode_;
    std::vector<std::string> player_names_;
    Database&                db_;

    SessionState     state_      = SessionState::Idle;
    int              session_id_ = 0;
    std::vector<int> player_ids_;
    std::vector<Trial> trials_;

    Trial* current_trial_ = nullptr;  // points into trials_ or nullptr
    int    trial_counter_ = 0;
    int    total_trials_  = 0;

    float phase_start_s_  = 0.0f;
    float isi_duration_s_ = 0.0f;

    std::optional<LastResult> last_result_;

    // ── Implementation-only state (not part of the public contract) ──────────
    std::string cur_type_;            // stimulus selected for the current trial
    std::string cur_color_;
    int         active_player_      = 1;
    bool        phase_initialized_  = false;  // first tick latches phase_start_s_

    // Helpers
    int  current_player() const noexcept;
    bool key_is_valid(SDL_Keycode key) const noexcept;
    void enter_waiting(float t);
    void enter_stimulus(float t);
    void enter_collecting(float t);
    void record_response(float elapsed_ms);
    void record_timeout();
    void record_false_start(float t, SDL_Keycode key);
    void complete();

    static std::string random_stimulus_type();
    static std::string random_stimulus_color();
};
