#pragma once
#include <chrono>
#include <stdexcept>

class HighPrecisionTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    bool is_running;

public:
    HighPrecisionTimer() : is_running(false) {}

    void start() {
        if (is_running) {
            throw std::runtime_error("Timer is already running");
        }
        start_time = std::chrono::high_resolution_clock::now();
        is_running = true;
    }

    double stop() {
        if (!is_running) {
            throw std::runtime_error("Timer is not running");
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        is_running = false;
        return std::chrono::duration<double, std::milli>(
            end_time - start_time).count();
    }
};