#pragma once
#include <chrono>
#include <stdexcept>

class ReactionTimer {
public:
    void  start();           // throws if already running
    float stop();            // returns ms; throws if not running
    void  reset() noexcept;
    bool  is_running() const noexcept { return running_; }

private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start_time_;
    bool              running_ = false;
};
