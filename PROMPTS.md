# Claude Code Prompts — Real-Time Reaction (C++)

## One-Time Setup

### 1. Place CLAUDE.md in the project root
Claude Code reads this file automatically. It contains the full stack, design
decisions, color constants, and build commands. Do this before any phase prompt.

### 2. Install system dependencies
```bash
# Ubuntu / Debian
sudo apt install cmake libsdl2-dev libglew-dev libopencv-dev libsqlite3-dev

# macOS
brew install cmake sdl2 glew opencv sqlite3

# Windows — run from vcpkg root
vcpkg install sdl2 glew opencv4 sqlite3
# Add to cmake: -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
```

### 3. Verify before Phase 1
```bash
cmake --version      # must be 3.20+
# Confirm sdl2, glew, opencv are findable:
cmake -B build_check -DCMAKE_BUILD_TYPE=Debug .  # expect no "not found" errors
rm -rf build_check
```

---

## Phase 1 Prompt

```
Read PHASE_1.md and CLAUDE.md fully before writing any code.

This is a C++17 project. CMakeLists.txt is the first deliverable — write it
completely before any source files.

Implement deliverables 1–7 in order.

After writing tests/, run:
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

Implement deliverables 1–8 in order. Write stub implementations
for analytics_screen.cpp (empty render, returns Next::Menu on any input)
so the project compiles — Phase 4 fills it in.

After completing the game loop:
  cmake --build build --parallel
  ./build/rtr

Then: cd build && ctest --output-on-failure

If no camera is available, CameraTexture must display a black panel — do not crash.
GLEW must be initialised before any ImGui OpenGL calls.
Do not write two-player UI (Phase 3).
Stop when all Acceptance Criteria in PHASE_2.md are met.
```

---

## Phase 3 Prompt

```
Read PHASE_3.md and CLAUDE.md fully before writing any code.

Phases 1 and 2 are complete. Extend without rewriting working code.

Implement in order:
  1. session.hpp/cpp — add get_active_player(), verify inactive key is silently ignored
  2. game_screen.hpp/cpp — add render_two_player_layout(); keep single-player path
  3. results_screen.hpp/cpp — add winner banner and two-player stats; make determine_winner() public
  4. tests/test_two_player.cpp — all 7 tests

cmake --build build --parallel
cd build && ctest --output-on-failure
./build/rtr   (test both single and two-player end-to-end)

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
  4. Verify menu Analytics button navigates correctly (likely already wired)

ImPlot is already linked. Include <implot.h> only in analytics_screen.cpp.
Use <filesystem> for export paths (C++17).
CSV export uses <fstream> only — no third-party.

cmake --build build --parallel
cd build && ctest --output-on-failure
./build/rtr   (trigger Analytics screen; test Export CSV button)

Stop when all Acceptance Criteria in PHASE_4.md are met.
```

---

## Troubleshooting Prompts

**Build fails — dependency not found:**
```
cmake -B build reports "[Library] not found". 
Check CLAUDE.md system install commands for my platform: [Ubuntu/macOS/Windows].
Fix only the CMakeLists.txt to locate the library. Do not change source files.
```

**Test fails after a deliverable:**
```
ctest --output-on-failure output is below. Fix the failing tests only.
Do not modify passing tests or unrelated source files.
Re-run ctest after the fix.
[paste output]
```

**Resuming after interruption:**
```
CLAUDE.md and PHASE_N.md are in the project root.
Run: cmake --build build --parallel && cd build && ctest --output-on-failure
Then continue from the first unmet Acceptance Criterion in PHASE_N.md.
Do not rewrite passing code.
```

**Segfault or crash at runtime:**
```
./build/rtr crashes with the following output: [paste].
Run under AddressSanitizer:
  cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
  cmake --build build_asan
  ./build_asan/rtr
Report the ASAN output and fix the root cause only.
```
