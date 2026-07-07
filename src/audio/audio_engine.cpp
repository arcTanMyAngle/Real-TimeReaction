#include "audio_engine.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr int   kSampleRate = 44100;
constexpr float kPi         = 3.14159265358979323846f;

// Apply a short linear attack/release envelope so tones don't click on/off.
void apply_envelope(std::vector<int16_t>& s) {
    const int n = static_cast<int>(s.size());
    const int edge = std::min(n / 2, kSampleRate / 200);  // ~5 ms ramps
    for (int i = 0; i < edge; ++i) {
        const float g = static_cast<float>(i) / static_cast<float>(edge);
        s[i]             = static_cast<int16_t>(s[i] * g);
        s[n - 1 - i]     = static_cast<int16_t>(s[n - 1 - i] * g);
    }
}
} // namespace

std::vector<int16_t> AudioEngine::gen_sine(float freq, float duration_s,
                                           float amplitude) const {
    const int n = static_cast<int>(duration_s * kSampleRate);
    std::vector<int16_t> out(n);
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kSampleRate;
        out[i] = static_cast<int16_t>(amplitude * 32767.0f *
                                      std::sin(2.0f * kPi * freq * t));
    }
    apply_envelope(out);
    return out;
}

std::vector<int16_t> AudioEngine::gen_sweep(float f0, float f1, float duration_s,
                                            float amplitude) const {
    const int n = static_cast<int>(duration_s * kSampleRate);
    std::vector<int16_t> out(n);
    float phase = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float frac = (n > 1) ? static_cast<float>(i) / (n - 1) : 0.0f;
        const float freq = f0 + (f1 - f0) * frac;
        phase += 2.0f * kPi * freq / kSampleRate;
        out[i] = static_cast<int16_t>(amplitude * 32767.0f * std::sin(phase));
    }
    apply_envelope(out);
    return out;
}

std::vector<int16_t> AudioEngine::gen_sawtooth(float freq, float duration_s,
                                               float amplitude) const {
    const int n = static_cast<int>(duration_s * kSampleRate);
    std::vector<int16_t> out(n);
    for (int i = 0; i < n; ++i) {
        const float t     = static_cast<float>(i) / kSampleRate;
        const float cycle = t * freq;
        const float saw   = 2.0f * (cycle - std::floor(cycle + 0.5f));  // -1..1
        out[i] = static_cast<int16_t>(amplitude * 32767.0f * saw);
    }
    apply_envelope(out);
    return out;
}

std::vector<int16_t> AudioEngine::gen_arpeggio(const std::vector<float>& freqs,
                                               float note_dur,
                                               float amplitude) const {
    std::vector<int16_t> out;
    for (float f : freqs) {
        std::vector<int16_t> note = gen_sine(f, note_dur, amplitude);
        out.insert(out.end(), note.begin(), note.end());
    }
    return out;
}

Mix_Chunk* AudioEngine::make_chunk(const std::vector<int16_t>& samples,
                                   int sample_rate) const {
    // Build minimal WAV in memory
    struct WavHdr {
        char     riff[4]     = {'R', 'I', 'F', 'F'};
        uint32_t chunk_sz    = 0;
        char     wave[4]     = {'W', 'A', 'V', 'E'};
        char     fmt[4]      = {'f', 'm', 't', ' '};
        uint32_t subchunk1   = 16;
        uint16_t audio_fmt   = 1;  // PCM
        uint16_t channels    = 1;
        uint32_t sample_rate = 0;
        uint32_t byte_rate   = 0;
        uint16_t block_align = 2;  // 1 channel * 16-bit
        uint16_t bits        = 16;
        char     data[4]     = {'d', 'a', 't', 'a'};
        uint32_t data_sz     = 0;
    };
    const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 2);
    WavHdr h;
    h.sample_rate = static_cast<uint32_t>(sample_rate);
    h.byte_rate   = static_cast<uint32_t>(sample_rate) * 2;
    h.data_sz     = data_bytes;
    h.chunk_sz    = 36 + data_bytes;

    std::vector<uint8_t> buf(sizeof(WavHdr) + data_bytes);
    std::memcpy(buf.data(), &h, sizeof(WavHdr));
    std::memcpy(buf.data() + sizeof(WavHdr), samples.data(), data_bytes);

    SDL_RWops* rw = SDL_RWFromMem(buf.data(), static_cast<int>(buf.size()));
    if (!rw) return nullptr;
    return Mix_LoadWAV_RW(rw, 1);  // 1 = freesrc
}

AudioEngine::AudioEngine() {
    // Audio subsystem may not have been initialised by the renderer.
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
            return;  // no audio device — stay unavailable, play() is a no-op
        audio_subsystem_owned_ = true;
    }

    if (Mix_OpenAudio(kSampleRate, AUDIO_S16SYS, 1, 512) < 0) {
        if (audio_subsystem_owned_) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            audio_subsystem_owned_ = false;
        }
        return;
    }

    chunks_[0] = make_chunk(gen_sine(880.0f, 0.080f));
    chunks_[1] = make_chunk(gen_sweep(440.0f, 660.0f, 0.120f));
    chunks_[2] = make_chunk(gen_sawtooth(220.0f, 0.160f));
    chunks_[3] = make_chunk(gen_sweep(330.0f, 165.0f, 0.220f));
    chunks_[4] = make_chunk(gen_arpeggio({523.f, 659.f, 784.f}, 0.090f));
    available_ = true;
}

AudioEngine::~AudioEngine() {
    if (available_) {
        for (Mix_Chunk* c : chunks_)
            if (c) Mix_FreeChunk(c);
        Mix_CloseAudio();
    }
    if (audio_subsystem_owned_)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioEngine::play(Sound s) {
    if (!available_) return;
    Mix_Chunk* c = chunks_[static_cast<int>(s)];
    if (c) Mix_PlayChannel(-1, c, 0);
}
