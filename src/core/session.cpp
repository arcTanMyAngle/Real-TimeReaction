#include "session.hpp"

#include <algorithm>
#include <chrono>
#include <random>

namespace {

double now_epoch() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

std::mt19937& rng() {
    static thread_local std::mt19937 engine{std::random_device{}()};
    return engine;
}

int key_to_player(SDL_Keycode k) noexcept {
    if (k == SDLK_SPACE)  return 1;
    if (k == SDLK_RETURN) return 2;
    return 0;
}

} // namespace

GameSession::GameSession(const std::string& mode,
                         const std::vector<std::string>& player_names,
                         Database& db)
    : mode_(mode),
      game_mode_(mode_from_string(mode)),  // "single"/"two_player" → Classic
      player_names_(player_names),
      db_(db) {}

void GameSession::start() {
    player_ids_.clear();
    for (const auto& name : player_names_)
        player_ids_.push_back(db_.get_or_create_player(name));

    // Player count is derived from the names, not the mode string (Race forces
    // two players; Classic/Blitz/Survival may be one or two).
    const int  nplayers = static_cast<int>(player_names_.size());
    const bool two      = nplayers > 1;

    Session s;
    s.player1_id = player_ids_.empty() ? 0 : player_ids_[0];
    s.player2_id = player_ids_.size() > 1 ? player_ids_[1] : 0;
    s.mode       = two ? "two_player" : "single";  // satisfies DB CHECK constraint
    s.game_mode  = mode_to_string(game_mode_);
    s.started_at = Database::now_iso();
    session_id_  = db_.insert_session(s);

    // Race = fixed rounds (one trial sequence, both keys live); others scale with
    // player count. Blitz/Survival have no trial ceiling (timer / lives end them).
    if (game_mode_ == GameMode::Race)
        total_trials_ = TRIALS_PER_PLAYER;
    else
        total_trials_ = TRIALS_PER_PLAYER * (two ? 2 : 1);

    trials_.clear();
    trials_.reserve(512);  // generous: keeps current_trial_ pointers stable
    trial_counter_     = 0;
    current_trial_     = nullptr;
    phase_initialized_ = false;
    active_player_     = 1;
    last_result_.reset();

    // Reset Phase 6 mode state.
    lives_[0] = lives_[1] = SURVIVAL_LIVES;
    time_remaining_  = BLITZ_DURATION_S;
    last_tick_init_  = false;
    current_streak_  = 0;
    max_streak_      = 0;
    rounds_won_[0]   = rounds_won_[1] = 0;
    round_settled_   = false;

    state_ = SessionState::Countdown;
}

SessionState GameSession::tick(float current_time_s,
                               const std::vector<SDL_Keycode>& keys) {
    if (state_ == SessionState::Idle || state_ == SessionState::Complete)
        return state_;

    if (!phase_initialized_) {
        phase_start_s_     = current_time_s;
        phase_initialized_ = true;
    }

    // Blitz countdown — decrement by the clamped-positive wall-clock delta so the
    // timer is robust to non-monotonic test clocks. Runs in every active state.
    if (game_mode_ == GameMode::Blitz) {
        if (!last_tick_init_) {
            last_tick_time_s_ = current_time_s;
            last_tick_init_   = true;
        }
        float delta = current_time_s - last_tick_time_s_;
        if (delta < 0.0f) delta = 0.0f;
        last_tick_time_s_ = current_time_s;
        time_remaining_ -= delta;
        if (time_remaining_ <= 0.0f) {
            time_remaining_ = 0.0f;
            complete();
            return state_;
        }
    }

    const float elapsed_ms = (current_time_s - phase_start_s_) * 1000.0f;

    switch (state_) {
        case SessionState::Countdown: {
            if (elapsed_ms >= COUNTDOWN_S * 1000.0f)
                enter_waiting(current_time_s);
            break;
        }

        case SessionState::Waiting: {
            // Active player jumping the gun → false start, restart the wait.
            for (SDL_Keycode k : keys) {
                if (key_is_valid(k)) {
                    record_false_start(current_time_s, k);
                    return state_;  // already back in Waiting
                }
            }
            if (elapsed_ms >= isi_duration_s_ * 1000.0f)
                enter_stimulus(current_time_s);
            break;
        }

        case SessionState::StimulusActive: {
            if (game_mode_ == GameMode::Race) {
                // Both keys live; first valid press wins the round.
                for (SDL_Keycode k : keys) {
                    if (round_settled_) break;
                    if (k == SDLK_SPACE) {
                        if (current_trial_) current_trial_->round_winner = 1;
                        ++rounds_won_[0];
                        record_response(elapsed_ms, /*responder=*/1);
                        round_settled_ = true;
                    } else if (k == SDLK_RETURN) {
                        if (current_trial_) current_trial_->round_winner = 2;
                        ++rounds_won_[1];
                        record_response(elapsed_ms, /*responder=*/2);
                        round_settled_ = true;
                    }
                }
                if (round_settled_) {
                    enter_collecting(current_time_s);
                } else if (elapsed_ms >= TIMEOUT_MS) {
                    record_timeout();
                    if (state_ != SessionState::Complete)
                        enter_collecting(current_time_s);
                }
                break;
            }

            bool responded = false;
            for (SDL_Keycode k : keys) {
                if (key_is_valid(k)) {
                    record_response(elapsed_ms);
                    enter_collecting(current_time_s);
                    responded = true;
                    break;
                }
            }
            if (!responded && elapsed_ms >= TIMEOUT_MS) {
                record_timeout();
                if (state_ != SessionState::Complete)
                    enter_collecting(current_time_s);
            }
            break;
        }

        case SessionState::Collecting: {
            if (elapsed_ms >= FEEDBACK_MS) {
                // Classic/Race end on a fixed trial count; Blitz ends on the
                // timer and Survival on lives, so they always continue here.
                const bool fixed =
                    (game_mode_ == GameMode::Classic ||
                     game_mode_ == GameMode::Race);
                if (fixed && trial_counter_ >= total_trials_)
                    complete();
                else
                    enter_waiting(current_time_s);
            }
            break;
        }

        default:
            break;
    }

    return state_;
}

// ── State entry ──────────────────────────────────────────────────────────────
void GameSession::enter_waiting(float t) {
    state_         = SessionState::Waiting;
    phase_start_s_ = t;
    current_trial_ = nullptr;
    active_player_ = current_player();
    cur_type_      = random_stimulus_type();
    cur_color_     = random_stimulus_color();

    const float lo = (game_mode_ == GameMode::Blitz) ? BLITZ_ISI_MIN_S : ISI_MIN_S;
    const float hi = (game_mode_ == GameMode::Blitz) ? BLITZ_ISI_MAX_S : ISI_MAX_S;
    std::uniform_real_distribution<float> dist(lo, hi);
    isi_duration_s_ = dist(rng());
}

void GameSession::enter_stimulus(float t) {
    state_         = SessionState::StimulusActive;
    phase_start_s_ = t;
    round_settled_ = false;  // Race: new round, both keys live again

    Trial tr;
    tr.session_id           = session_id_;
    tr.trial_number         = trial_counter_ + 1;
    tr.stimulus_type        = cur_type_;
    tr.stimulus_color       = cur_color_;
    tr.player               = active_player_;
    tr.stimulus_onset_epoch = now_epoch();
    tr.reaction_time_ms     = -1.0f;
    tr.false_start          = false;
    trials_.push_back(tr);
    current_trial_ = &trials_.back();
}

void GameSession::enter_collecting(float t) {
    state_         = SessionState::Collecting;
    phase_start_s_ = t;
}

// ── Result recording ─────────────────────────────────────────────────────────
void GameSession::record_response(float elapsed_ms, int responder) {
    const int who = (responder > 0) ? responder : active_player_;
    if (current_trial_) {
        current_trial_->reaction_time_ms = elapsed_ms;
        current_trial_->response_epoch   = now_epoch();
        current_trial_->player           = who;  // Race: attribute to the presser
        update_streak(elapsed_ms, /*false_start=*/false, /*miss=*/false);
        db_.insert_trial(*current_trial_);
    }
    last_result_ = LastResult{elapsed_ms, false, who};
    ++trial_counter_;
}

void GameSession::record_timeout() {
    if (current_trial_) {
        current_trial_->reaction_time_ms = -1.0f;  // miss
        current_trial_->response_epoch   = now_epoch();
        update_streak(-1.0f, /*false_start=*/false, /*miss=*/true);
        db_.insert_trial(*current_trial_);
    }
    last_result_ = LastResult{-1.0f, false, active_player_};
    ++trial_counter_;
    lose_life_if_survival(active_player_);  // may complete() the session
}

void GameSession::record_false_start(float t, SDL_Keycode /*key*/) {
    Trial tr;
    tr.session_id           = session_id_;
    tr.trial_number         = trial_counter_ + 1;
    tr.stimulus_type        = cur_type_;
    tr.stimulus_color       = cur_color_;
    tr.player               = active_player_;
    tr.stimulus_onset_epoch = now_epoch();
    tr.reaction_time_ms     = -1.0f;
    tr.false_start          = true;
    tr.response_epoch       = now_epoch();

    // Streak breaks on a false start; record the (zeroed) streak on the trial.
    current_trial_ = &tr;
    update_streak(-1.0f, /*false_start=*/true, /*miss=*/false);
    current_trial_ = nullptr;

    trials_.push_back(tr);
    db_.insert_trial(tr);

    last_result_ = LastResult{-1.0f, true, active_player_};

    lose_life_if_survival(active_player_);
    if (state_ == SessionState::Complete)
        return;  // last life lost — don't restart the wait

    // Restart the inter-stimulus wait for the same trial number.
    enter_waiting(t);
}

void GameSession::complete() {
    state_ = SessionState::Complete;
    if (session_id_ > 0)
        db_.close_session(session_id_);
}

// ── Queries ──────────────────────────────────────────────────────────────────
SessionState GameSession::get_state() const noexcept { return state_; }

// Whose turn it is now. current_player() is derived from trial_counter_ so it
// reflects the upcoming trial's player (single-player → always 1).
int GameSession::get_active_player() const noexcept { return current_player(); }

std::optional<TrialInfo> GameSession::get_current_trial() const {
    if (state_ == SessionState::Waiting ||
        state_ == SessionState::StimulusActive ||
        state_ == SessionState::Collecting) {
        TrialInfo info;
        info.stimulus_type = cur_type_;
        info.stimulus_color = cur_color_;
        info.player        = active_player_;
        info.trial_number  = trial_counter_ + 1;
        info.total_trials  = total_trials_;
        return info;
    }
    return std::nullopt;
}

std::optional<LastResult> GameSession::get_last_result() const {
    return last_result_;
}

float GameSession::get_countdown_remaining(float now_s) const {
    if (state_ != SessionState::Countdown)
        return 0.0f;
    if (!phase_initialized_)
        return COUNTDOWN_S;
    const float remaining = COUNTDOWN_S - (now_s - phase_start_s_);
    return remaining > 0.0f ? remaining : 0.0f;
}

const std::vector<Trial>& GameSession::get_results() const { return trials_; }

// ── Helpers ──────────────────────────────────────────────────────────────────
int GameSession::current_player() const noexcept {
    if (game_mode_ == GameMode::Race)
        return 1;  // both keys live, no alternation
    if (player_names_.size() > 1)
        return (trial_counter_ % 2) + 1;
    return 1;
}

bool GameSession::key_is_valid(SDL_Keycode key) const noexcept {
    if (game_mode_ == GameMode::Race)
        return key == SDLK_SPACE || key == SDLK_RETURN;
    return key_to_player(key) == active_player_;
}

// ── Phase 6 helpers / accessors ──────────────────────────────────────────────
void GameSession::update_streak(float rt_ms, bool false_start, bool miss) {
    if (!false_start && !miss && rt_ms >= 0.0f && rt_ms <= STREAK_THRESHOLD_MS) {
        ++current_streak_;
        if (current_streak_ > max_streak_)
            max_streak_ = current_streak_;
    } else {
        current_streak_ = 0;
    }
    if (current_trial_)
        current_trial_->streak_at_time = current_streak_;
}

void GameSession::lose_life_if_survival(int player) {
    if (game_mode_ != GameMode::Survival)
        return;
    const int p = player - 1;
    if (p < 0 || p > 1)
        return;
    lives_[p] = std::max(0, lives_[p] - 1);
    if (lives_[p] == 0)
        complete();
}

int GameSession::get_lives(int player) const noexcept {
    const int p = player - 1;
    if (p < 0 || p > 1)
        return 0;
    return lives_[p];
}

float GameSession::get_time_remaining() const noexcept {
    return time_remaining_;
}

int GameSession::get_rounds_won(int player) const noexcept {
    const int p = player - 1;
    if (p < 0 || p > 1)
        return 0;
    return rounds_won_[p];
}

std::string GameSession::random_stimulus_type() {
    static const char* kTypes[] = {"circle", "square", "cross"};
    std::uniform_int_distribution<int> dist(0, 2);
    return kTypes[dist(rng())];
}

std::string GameSession::random_stimulus_color() {
    static const char* kColors[] = {"red", "green", "blue", "yellow"};
    std::uniform_int_distribution<int> dist(0, 3);
    return kColors[dist(rng())];
}
