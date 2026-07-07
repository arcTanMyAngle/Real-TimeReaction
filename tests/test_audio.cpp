#include <catch2/catch_test_macros.hpp>
#include "audio/audio_engine.hpp"

TEST_CASE("AudioEngine constructs without crash") {
    REQUIRE_NOTHROW(AudioEngine());
}

TEST_CASE("AudioEngine::play does not throw when unavailable") {
    AudioEngine ae;
    // available() may be false in CI (no audio device) — play must still be safe.
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::Stimulus));
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::Hit));
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::FalseStart));
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::Miss));
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::NewRecord));
}
