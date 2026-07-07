#pragma once
#include <SDL2/SDL_mixer.h>
#include <array>
#include <cstdint>
#include <vector>

// Generates all sounds procedurally at startup — zero audio files required.
// Safe to construct with no audio device: available() reports false and play()
// becomes a no-op rather than crashing (CI / SDL_AUDIODRIVER=dummy).
class AudioEngine {
public:
    enum class Sound {
        Stimulus,    // high beep — attention-getter
        Hit,         // rising tone — satisfying correct press
        FalseStart,  // buzzer — harsh penalty
        Miss,        // descending tone — failure
        NewRecord    // ascending arpeggio — personal best beaten
    };
    static constexpr int SOUND_COUNT = 5;

    AudioEngine();
    ~AudioEngine();

    // No copy
    AudioEngine(const AudioEngine&)            = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    void play(Sound s);
    bool available() const noexcept { return available_; }

private:
    bool available_ = false;
    bool audio_subsystem_owned_ = false;  // we initialised SDL_INIT_AUDIO
    std::array<Mix_Chunk*, SOUND_COUNT> chunks_ = {};

    // Generate one channel of 16-bit PCM samples
    std::vector<int16_t> gen_sine(float freq, float duration_s,
                                  float amplitude = 0.70f) const;
    std::vector<int16_t> gen_sweep(float f0, float f1, float duration_s,
                                   float amplitude = 0.70f) const;
    std::vector<int16_t> gen_sawtooth(float freq, float duration_s,
                                      float amplitude = 0.55f) const;
    std::vector<int16_t> gen_arpeggio(const std::vector<float>& freqs,
                                      float note_dur, float amplitude = 0.65f) const;

    // Wrap raw PCM in a WAV container and load as Mix_Chunk
    Mix_Chunk* make_chunk(const std::vector<int16_t>& samples,
                          int sample_rate = 44100) const;
};
