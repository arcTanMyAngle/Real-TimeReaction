#include "session.hpp"

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
    : mode_(mode), player_names_(player_names), db_(db) {}

void GameSession::start() {
    player_ids_.clear();
    for (const auto& name : player_names_)
        player_ids_.push_back(db_.get_or_create_player(name));

    Session s;
    s.player1_id = player_ids_.empty() ? 0 : player_ids_[0];
    s.player2_id = player_ids_.size() > 1 ? player_ids_[1] : 0;
    s.mode       = mode_;
    s.started_at = Database::now_iso();
    session_id_  = db_.insert_session(s);

    const int nplayers = (mode_ == "two_player") ? 2 : 1;
    total_trials_ = TRIALS_PER_PLAYER * nplayers;

    trials_.clear();
    trials_.reserve(512);  // generous: keeps current_trial_ pointers stable
    trial_counter_     = 0;
    current_trial_     = nullptr;
    phase_initialized_ = false;
    active_player_     = 1;
    last_result_.reset();

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
                enter_collecting(current_time_s);
            }
            break;
        }

        case SessionState::Collecting: {
            if (elapsed_ms >= FEEDBACK_MS) {
                if (trial_counter_ >= total_trials_)
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

    std::uniform_real_distribution<float> dist(ISI_MIN_S, ISI_MAX_S);
    isi_duration_s_ = dist(rng());
}

void GameSession::enter_stimulus(float t) {
    state_         = SessionState::StimulusActive;
    phase_start_s_ = t;

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
void GameSession::record_response(float elapsed_ms) {
    if (current_trial_) {
        current_trial_->reaction_time_ms = elapsed_ms;
        current_trial_->response_epoch   = now_epoch();
        db_.insert_trial(*current_trial_);
    }
    last_result_ = LastResult{elapsed_ms, false, active_player_};
    ++trial_counter_;
}

void GameSession::record_timeout() {
    if (current_trial_) {
        current_trial_->reaction_time_ms = -1.0f;  // miss
        current_trial_->response_epoch   = now_epoch();
        db_.insert_trial(*current_trial_);
    }
    last_result_ = LastResult{-1.0f, false, active_player_};
    ++trial_counter_;
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
    trials_.push_back(tr);
    current_trial_ = nullptr;
    db_.insert_trial(tr);

    last_result_ = LastResult{-1.0f, true, active_player_};

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
    if (mode_ == "two_player")
        return (trial_counter_ % 2) + 1;
    return 1;
}

bool GameSession::key_is_valid(SDL_Keycode key) const noexcept {
    return key_to_player(key) == active_player_;
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
