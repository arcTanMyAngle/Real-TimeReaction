# Claude Code Prompts — reActivation (C++)

## One-Time Setup

### 1. Place CLAUDE.md in the project root
Claude Code reads this file automatically for persistent project context.
Do this before any phase prompt.

### 2. Install system dependencies (Phases 1–4)
```bash
# Ubuntu / Debian
sudo apt install cmake libsdl2-dev libglew-dev libopencv-dev libsqlite3-dev

# macOS
brew install cmake sdl2 glew opencv sqlite3

# Windows — run from vcpkg root
vcpkg install sdl2 glew opencv4 sqlite3
# Add to cmake: -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
```

### 3. Install audio dependency (Phase 5+)
```bash
# Ubuntu
sudo apt install libsdl2-mixer-dev
# macOS
brew install sdl2_mixer
# Windows vcpkg
vcpkg install sdl2-mixer
```

### 4. Verify before Phase 1
```bash
cmake --version   # 3.20+
cmake -B build_check -DCMAKE_BUILD_TYPE=Debug .
rm -rf build_check
```

---

## Phase 1 Prompt

```
Read PHASE_1.md and CLAUDE.md fully before writing any code.

This is a C++17 project. Write CMakeLists.txt completely before any source files.
Implement deliverables 1–7 in order.

After writing tests/:
  cmake -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build --parallel
  cd build && ctest --output-on-failure

Fix any build errors before moving to the next deliverable.
Do not write any SDL2, ImGui, or OpenGL code.
Stop when all Acceptance Criteria in PHASE_1.md are met.
```

---

## Phase 2 Prompt

```
Read PHASE_2.md and CLAUDE.md fully before writing any code.
Phase 1 is complete and all tests pass.

Implement deliverables 1–8 in order.
Write a stub analytics_screen.cpp (empty render, returns Next::Menu on any input)
so the project compiles — Phase 4 fills it in.

After completing the game loop:
  cmake --build build --parallel && ./build/rtr

Then: cd build && ctest --output-on-failure

If no camera is available, CameraTexture must show a black panel — do not crash.
GLEW must be initialised before any ImGui OpenGL calls.
Do not write two-player UI.
Stop when all Acceptance Criteria in PHASE_2.md are met.
```

---

## Phase 3 Prompt

```
Read PHASE_3.md and CLAUDE.md fully before writing any code.
Phases 1–2 are complete. Extend without rewriting working code.

Implement in order:
  1. session.hpp/cpp — add get_active_player(), verify inactive key silently ignored
  2. game_screen.hpp/cpp — add render_two_player_layout(); keep single-player path
  3. results_screen.hpp/cpp — winner banner and two-player stats; make determine_winner() public
  4. tests/test_two_player.cpp — all 7 tests

cmake --build build --parallel
cd build && ctest --output-on-failure
./build/rtr   (test both single and two-player)

Stop when all Acceptance Criteria in PHASE_3.md are met.
```

---

## Phase 4 Prompt

```
Read PHASE_4.md and CLAUDE.md fully before writing any code.
Phases 1–3 are complete. Extend without breaking existing behaviour.

Implement in order:
  1. data/analytics.hpp + analytics.cpp — no ImGui/ImPlot includes here
  2. ui/screens/analytics_screen.hpp + analytics_screen.cpp — replace Phase 2 stub
  3. tests/test_analytics.cpp — all 8 tests
  4. Verify menu Analytics button navigates correctly

ImPlot is already linked. Include <implot.h> only in analytics_screen.cpp.
Use <filesystem> for export paths. CSV export uses <fstream> only.

cmake --build build --parallel
cd build && ctest --output-on-failure
./build/rtr   (trigger Analytics screen; test Export CSV button)

Stop when all Acceptance Criteria in PHASE_4.md are met.
```

---

## Phase 5 Prompt

```
Read PHASE_5.md and CLAUDE.md fully before writing any code.
Phases 1–4 are complete. This phase adds feel — no new game logic.

Implement in order:
  1. CMakeLists.txt — add SDL_mixer using the function pattern in PHASE_5.md
  2. src/audio/audio_engine.hpp + audio_engine.cpp — procedural WAV generation
  3. src/ui/effects.hpp + effects.cpp — flash, label, record flash, scanlines, border
  4. data/database.hpp + database.cpp — add get_personal_best()
  5. ui/screens/game_screen.hpp + game_screen.cpp — integrate audio_ and effects_
  6. Rename: window title, CMakeLists.txt project name, CLAUDE.md header
  7. tests/test_audio.cpp — 2 tests

cmake --build build --parallel
cd build && ctest --output-on-failure
./build/rtr   (play a session — verify sounds, flash, label, scanlines)

AudioEngine must not crash if no audio device is present (CI environment).
Use SDL_Init audio flag only inside AudioEngine constructor.
Stop when all Acceptance Criteria in PHASE_5.md are met.
```

---

## Phase 6 Prompt

```
Read PHASE_6.md and CLAUDE.md fully before writing any code.
Phases 1–5 are complete. This phase adds game modes and progression.

Implement in order:
  1. data/models.hpp — add round_winner, streak_at_time to Trial; game_mode to Session
  2. database.cpp — schema migration (ALTER TABLE, wrapped in try-catch)
  3. core/session.hpp + session.cpp — GameMode enum, Race/Blitz/Survival logic, streak
  4. ui/screens/menu_screen.hpp + menu_screen.cpp — mode selection grid
  5. ui/screens/game_screen.hpp + game_screen.cpp — mode-specific HUD
  6. ui/screens/results_screen.hpp + results_screen.cpp — rank display, streak, mode stats
  7. tests/test_game_modes.cpp — all 11 tests

cmake --build build --parallel
cd build && ctest --output-on-failure
./build/rtr   (test each mode: Classic, Race, Blitz, Survival)

Do not delete the existing reaction.db — the migration must not break it.
Stop when all Acceptance Criteria in PHASE_6.md are met.
```

---

## Troubleshooting Prompts

**Build fails — dependency not found:**
```
cmake -B build reports "[Library] not found".
Check CLAUDE.md system install commands for my platform: [Ubuntu/macOS/Windows].
Fix only the CMakeLists.txt. Do not change source files.
```

**Test fails after a deliverable:**
```
ctest --output-on-failure output is below. Fix the failing tests only.
Do not modify passing tests or unrelated source files. Re-run ctest after fix.
[paste output]
```

**Resuming after interruption:**
```
CLAUDE.md and PHASE_N.md are in the project root.
Run: cmake --build build --parallel && cd build && ctest --output-on-failure
Continue from the first unmet Acceptance Criterion in PHASE_N.md.
Do not rewrite passing code.
```

**Runtime crash:**
```
./build/rtr crashes. Run under AddressSanitizer:
  cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
  cmake --build build_asan && ./build_asan/rtr
Report ASAN output and fix the root cause only.
```

**No audio in Phase 5:**
```
AudioEngine::available() returns false. Check:
  1. SDL_mixer installed: pkg-config --libs SDL2_mixer should return output
  2. CMakeLists.txt link_sdl_mixer() called for rtr target
  3. Run with SDL_AUDIODRIVER=dummy ./build/rtr to confirm logic works without device
Do not change game logic — audio failure must be silent, not a crash.
```