#include <catch2/catch_test_macros.hpp>
#include <thread>
#include "core/timer.hpp"

TEST_CASE("Timer: stop before start throws") {
    ReactionTimer t;
    REQUIRE_THROWS_AS(t.stop(), std::runtime_error);
}
TEST_CASE("Timer: double start throws") {
    ReactionTimer t;
    t.start();
    REQUIRE_THROWS_AS(t.start(), std::runtime_error);
}
TEST_CASE("Timer: 100ms sleep within tolerance") {
    ReactionTimer t;
    t.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    float ms = t.stop();
    REQUIRE(ms >= 85.0f);
    REQUIRE(ms <= 130.0f);
}
TEST_CASE("Timer: reset allows restart") {
    ReactionTimer t;
    t.start();
    t.reset();
    REQUIRE_NOTHROW(t.start());
}
