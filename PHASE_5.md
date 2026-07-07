# Phase 5: Feel Layer — Audio, Effects, reActivation

## Goal
Transform the game from functional to visceral. No new game logic — pure feel:
procedurally generated audio, per-trial performance labels, screen flash on press,
personal best detection, camera border pulse, CRT scanline overlay.
Rename the project to **reActivation** throughout.

## Prerequisites
- Phases 1–4 Acceptance Criteria all passing
- Install SDL_mixer:
```bash
# Ubuntu
sudo apt install libsdl2-mixer-dev
# macOS
brew install sdl2_mixer
# Windows vcpkg
vcpkg install sdl2-mixer
```

---

## Deliverable 1 — CMakeLists.txt Update

Add SDL_mixer. Replace the `find_package(SDL2 REQUIRED)` block with:

```cmake
find_package(SDL2 REQUIRED)

# SDL_mixer — try modern CMake config first, fall back to pkg-config
find_package(SDL2_mixer QUIET CONFIG)
if(NOT SDL2_mixer_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
endif()

# Helper to link SDL_mixer regardless of which path found it
function(link_sdl_mixer target)
    if(SDL2_mixer_FOUND)
        target_link_libraries(${target} PRIVATE SDL2_mixer::SDL2_mixer)
    else()
        target_include_directories(${target} PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
        target_link_libraries(${target} PRIVATE ${SDL2_MIXER_LIBRARIES})
    endif()
endfunction()
```

Call `link_sdl_mixer(rtr)` in the target block.

Add new source files to the `rtr` executable:
```cmake
src/audio/audio_engine.cpp
src/ui/effects.cpp
```

---

## Deliverable 2 — `src/audio/audio_engine.hpp` + `audio_engine.cpp`

Generates all sounds procedurally at startup — zero audio files required.

```cpp
// audio_engine.hpp
#pragma once
#include <SDL2/SDL_mixer.h>
#include <array>

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
    std::array<Mix_Chunk*, SOUND_COUNT> chunks_ = {};

    enum class WaveType { Sine, Sawtooth };

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
```

### Sound Specifications

| Sound | Generation | Params |
|---|---|---|
| Stimulus | Sine | 880 Hz, 80 ms |
| Hit | Sweep sine (rising) | 440 → 660 Hz, 120 ms |
| FalseStart | Sawtooth | 220 Hz, 160 ms |
| Miss | Sweep sine (falling) | 330 → 165 Hz, 220 ms |
| NewRecord | Arpeggio (3 notes) | C5(523)→E5(659)→G5(784), 90 ms/note |

### WAV wrapper (`make_chunk`):

```cpp
Mix_Chunk* AudioEngine::make_chunk(const std::vector<int16_t>& samples,
                                    int sample_rate) const {
    // Build minimal WAV in memory
    struct WavHdr {
        char     riff[4]    = {'R','I','F','F'};
        uint32_t chunk_sz;
        char     wave[4]    = {'W','A','V','E'};
        char     fmt[4]     = {'f','m','t',' '};
        uint32_t subchunk1  = 16;
        uint16_t audio_fmt  = 1;       // PCM
        uint16_t channels   = 1;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align = 2;      // 1 channel * 16-bit
        uint16_t bits        = 16;
        char     data[4]    = {'d','a','t','a'};
        uint32_t data_sz;
    };
    uint32_t data_bytes = (uint32_t)(samples.size() * 2);
    WavHdr h;
    h.sample_rate = sample_rate;
    h.byte_rate   = sample_rate * 2;
    h.data_sz     = data_bytes;
    h.chunk_sz    = 36 + data_bytes;

    std::vector<uint8_t> buf(sizeof(WavHdr) + data_bytes);
    std::memcpy(buf.data(), &h, sizeof(WavHdr));
    std::memcpy(buf.data() + sizeof(WavHdr), samples.data(), data_bytes);

    SDL_RWops* rw = SDL_RWFromMem(buf.data(), (int)buf.size());
    return Mix_LoadWAV_RW(rw, 1);  // 1 = freesrc
}
```

### AudioEngine constructor:
```cpp
AudioEngine::AudioEngine() {
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 1, 512) < 0) return;
    chunks_[0] = make_chunk(gen_sine(880.0f, 0.080f));
    chunks_[1] = make_chunk(gen_sweep(440.0f, 660.0f, 0.120f));
    chunks_[2] = make_chunk(gen_sawtooth(220.0f, 0.160f));
    chunks_[3] = make_chunk(gen_sweep(330.0f, 165.0f, 0.220f));
    chunks_[4] = make_chunk(gen_arpeggio({523.f, 659.f, 784.f}, 0.090f));
    available_ = true;
}
```

`AudioEngine::play(Sound s)`: `Mix_PlayChannel(-1, chunks_[(int)s], 0);` — silent if not available.

---

## Deliverable 3 — `src/ui/effects.hpp` + `effects.cpp`

```cpp
// effects.hpp
#pragma once
#include <string>
#include <imgui.h>
#include "colors.hpp"
#include "../core/session.hpp"

// ── Screen flash (single frame color flood) ──────────────────────────────────
struct ScreenFlash {
    float  timer    = 0.0f;
    ImVec4 color    = {};
    float  duration = 0.10f;
    float  max_alpha = 0.38f;

    void trigger(ImVec4 c);
    void update(float dt);
    void render(int w, int h);  // draws via GetForegroundDrawList
};

// ── Per-trial performance label (flies upward, fades) ────────────────────────
struct PerformanceLabel {
    float       timer    = 0.0f;
    float       duration = 1.4f;
    std::string text;
    ImVec4      color    = {};
    ImVec2      origin   = {};   // start position (center of camera feed)

    // rt_ms < 0 = miss; false_start overrides everything
    void trigger(float rt_ms, ImVec2 pos, bool is_false_start = false);
    void update(float dt);
    void render();  // transparent overlay window + SetWindowFontScale

    static const char* text_for(float rt_ms, bool false_start);
    static ImVec4      color_for(float rt_ms, bool false_start);

    bool is_active() const { return timer > 0.0f; }
};

// ── Personal best flash ───────────────────────────────────────────────────────
struct NewRecordFlash {
    float timer = 0.0f;
    void trigger() { timer = 2.2f; }
    void update(float dt);
    void render(ImVec2 center_pos);  // "★ NEW RECORD ★" in gold, pulsing
};

// ── Scanline overlay (CRT feel) ───────────────────────────────────────────────
struct ScanlineOverlay {
    float opacity = 0.07f;
    int   spacing = 4;
    void render(int w, int h);  // always-on; draws via GetForegroundDrawList
};

// ── Camera border pulse (breathes during Waiting) ────────────────────────────
struct CameraBorderPulse {
    float phase = 0.0f;

    void update(float dt);
    // Returns border color + thickness based on state and active player
    ImVec4 border_color(SessionState state, int active_player) const;
    float  border_thickness(SessionState state) const;
};

// ── All effects bundled for GameScreen ───────────────────────────────────────
struct GameEffects {
    ScreenFlash      flash;
    PerformanceLabel label;
    NewRecordFlash   record;
    ScanlineOverlay  scanlines;
    CameraBorderPulse border;

    void update(float dt);
    void render_foreground(int w, int h);  // flash + scanlines + record
    // label.render() called separately (needs camera-relative position)
};
```

### Performance Label Values

| Condition | Text | Color |
|---|---|---|
| false_start | `EARLY!` | `Colors::WARNING` |
| rt < 150ms | `GODLIKE` | `Colors::SUCCESS` (full bright) |
| rt < 200ms | `ELITE` | `Colors::SUCCESS` |
| rt < 250ms | `FAST` | `Colors::ACCENT` |
| rt < 300ms | `GOOD` | `Colors::WARNING` |
| rt < 400ms | `AVERAGE` | `Colors::MUTED` |
| rt >= 400ms | `SLOW` | `Colors::DANGER` |
| miss (rt<0) | `TOO SLOW` | `Colors::DANGER` |

### PerformanceLabel::render()
```cpp
void PerformanceLabel::render() {
    if (!is_active()) return;

    float progress = 1.0f - (timer / duration);        // 0→1 over lifetime
    float rise     = progress * 80.0f;                 // rises 80px
    float alpha    = (progress < 0.65f) ? 1.0f
                   : 1.0f - (progress - 0.65f) / 0.35f; // fade last 35%

    ImVec4 col = color;
    col.w *= alpha;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##perf_label", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    float scale = 2.4f - progress * 0.6f;  // 2.4x → 1.8x over lifetime
    ImGui::SetWindowFontScale(scale);
    ImGui::PushStyleColor(ImGuiCol_Text, col);

    // Position: origin.x centered, origin.y - rise
    ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
    ImGui::SetCursorPos({origin.x - text_size.x * 0.5f,
                         origin.y - rise - text_size.y * 0.5f});
    ImGui::TextUnformatted(text.c_str());

    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
}
```

### ScanlineOverlay::render()
```cpp
void ScanlineOverlay::render(int w, int h) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 line_col = IM_COL32(0, 0, 0, (int)(opacity * 255));
    for (int y = 0; y < h; y += spacing)
        dl->AddLine({0.0f, (float)y}, {(float)w, (float)y}, line_col);
}
```

### CameraBorderPulse
```cpp
ImVec4 CameraBorderPulse::border_color(SessionState state, int player) const {
    ImVec4 base = (player == 1) ? Colors::P1 : Colors::P2;
    if (state == SessionState::StimulusActive) {
        base.w = 1.0f;  // solid bright on stimulus
        return base;
    }
    if (state == SessionState::Waiting) {
        // breathe: 0.35 → 0.95
        float pulse = (std::sin(phase) + 1.0f) * 0.5f;
        base.w = 0.35f + pulse * 0.60f;
        return base;
    }
    base.w = 0.4f;
    return base;
}

float CameraBorderPulse::border_thickness(SessionState state) const {
    return (state == SessionState::StimulusActive) ? 4.0f : 2.0f;
}
```

---

## Deliverable 4 — `src/data/database.hpp` + `database.cpp` Update

Add personal best query:
```cpp
// Returns best reaction_time_ms across all valid prior sessions, or -1.0f if none
float get_personal_best(int player_id);
```

Implementation: SELECT MIN(reaction_time_ms) FROM trials
WHERE session_id IN (SELECT id FROM sessions WHERE player1_id=? OR player2_id=?)
AND false_start=0 AND reaction_time_ms >= 0

---

## Deliverable 5 — Updated `src/ui/screens/game_screen.hpp/cpp`

Add members:
```cpp
AudioEngine   audio_;
GameEffects   effects_;
float         personal_best_ms_ = -1.0f;   // loaded at construction
```

In constructor: `personal_best_ms_ = db_.get_personal_best(player1_id_);`

### Audio trigger points:
```cpp
// When state transitions to StimulusActive:
audio_.play(AudioEngine::Sound::Stimulus);

// On valid keypress (rt recorded):
audio_.play(AudioEngine::Sound::Hit);
effects_.flash.trigger(player == 1 ? Colors::P1 : Colors::P2);
effects_.label.trigger(rt_ms, camera_center_pos);

// If rt < personal_best and rt > 0:
audio_.play(AudioEngine::Sound::NewRecord);
effects_.record.trigger();
personal_best_ms_ = rt_ms;

// On false start:
audio_.play(AudioEngine::Sound::FalseStart);
effects_.flash.trigger(Colors::DANGER);
effects_.label.trigger(0.0f, camera_center_pos, true);  // false_start=true

// On timeout/miss:
audio_.play(AudioEngine::Sound::Miss);
effects_.label.trigger(-1.0f, camera_center_pos);
```

In `update(float dt)`:
```cpp
effects_.update(dt);
effects_.border.update(dt);
```

In `render()` — add at the end after all ImGui windows:
```cpp
effects_.render_foreground(Display::W, Display::H);
effects_.label.render();
```

Pass border color/thickness from `effects_.border` to the camera panel border draw call.

---

## Deliverable 6 — Rename to reActivation

**`src/ui/renderer.cpp`**: change window title from `"Real-Time Reaction"` to `"reActivation"`.

**`CLAUDE.md`**: update project name to **reActivation** in the header and overview.

**`CMakeLists.txt`**: change `project(RealTimeReaction ...)` to `project(reActivation ...)`.
Keep executable name as `rtr` (short, clean).

---

## Tests — `tests/test_audio.cpp`

Add to `tests/CMakeLists.txt`:
```cmake
target_sources(rtr_tests PRIVATE test_audio.cpp)
link_sdl_mixer(rtr_tests)
```

```cpp
#include <catch2/catch_test_macros.hpp>
#include "audio/audio_engine.hpp"

TEST_CASE("AudioEngine constructs without crash") {
    REQUIRE_NOTHROW(AudioEngine());
}

TEST_CASE("AudioEngine::play does not throw when unavailable") {
    AudioEngine ae;
    // available() may be false in CI (no audio device) — play must still be safe
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::Stimulus));
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::Hit));
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::FalseStart));
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::Miss));
    REQUIRE_NOTHROW(ae.play(AudioEngine::Sound::NewRecord));
}
```

Add to `tests/test_database.cpp`:
```cpp
TEST_CASE("get_personal_best returns -1 with no prior trials") {
    Database db(":memory:"); db.create_schema();
    int pid = db.insert_player("Ghost");
    REQUIRE(db.get_personal_best(pid) == Approx(-1.0f));
}

TEST_CASE("get_personal_best returns correct minimum") {
    Database db(":memory:"); db.create_schema();
    int pid = db.insert_player("Ghost");
    Session s; s.player1_id=pid; s.mode="single"; s.game_mode="classic";
    s.started_at = Database::now_iso();
    int sid = db.insert_session(s);
    for (float v : {300.0f, 180.0f, 250.0f}) {
        Trial t; t.session_id=sid; t.player=1; t.reaction_time_ms=v;
        t.stimulus_type="circle"; t.stimulus_color="red";
        t.stimulus_onset_epoch=0; t.false_start=false;
        db.insert_trial(t);
    }
    REQUIRE(db.get_personal_best(pid) == Approx(180.0f));
}
```

---

## Acceptance Criteria
- [ ] Window title reads "reActivation"
- [ ] A short beep plays when stimulus appears (or no crash if no audio device)
- [ ] A rising tone plays on correct keypress
- [ ] Buzzer plays on false start; descending tone plays on miss
- [ ] Arpeggio plays when personal best is beaten
- [ ] Performance label appears and flies upward after each trial
  - [ ] "GODLIKE" for < 150ms; "ELITE" for < 200ms; correct colors
  - [ ] "TOO SLOW" and "EARLY!" appear for miss and false start
- [ ] Screen flashes player color on correct press; red on false start
- [ ] "★ NEW RECORD ★" displays when personal best is beaten
- [ ] Camera border breathes during Waiting; goes solid on StimulusActive
- [ ] Subtle scanline overlay visible across full window
- [ ] `cd build && ctest --output-on-failure` — all tests pass

## Out of Scope
- New game modes (Phase 6)
- Font file loading (use SetWindowFontScale)
- Volume controls or audio settings