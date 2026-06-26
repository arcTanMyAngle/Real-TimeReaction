#include "timer.hpp"

void ReactionTimer::start() {
    if (running_)
        throw std::runtime_error("ReactionTimer::start() called while running");
    start_time_ = Clock::now();
    running_    = true;
}

float ReactionTimer::stop() {
    if (!running_)
        throw std::runtime_error("ReactionTimer::stop() called while not running");
    const auto elapsed = Clock::now() - start_time_;
    running_ = false;
    const auto us =
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    return static_cast<float>(us) / 1000.0f;
}

void ReactionTimer::reset() noexcept {
    running_ = false;
}
