# reActivation — C++ Project Context

## What This Is
**reActivation** — a reaction time measurement app for 1–2 local players. A visual stimulus appears on screen;
players respond via keyboard. The app measures stimulus-onset to keypress delta, persists
all trials to SQLite, and displays analytics with charts.

## Stack
| Layer | Library | Source |
|---|---|---|
| Language | C++17 | — |
| Build | CMake 3.20+ | system |
| Window / Input | SDL2 | system package |
| OpenGL loader | GLEW | system package |
| UI + Draw | Dear ImGui 1.90+ | FetchContent |
| Charts | ImPlot 0.16+ | FetchContent |
| Camera | OpenCV 4.x (videoio, imgproc, core) | system package |
| Database | SQLiteCpp 3.x | FetchContent |
| SQLite3 | via SQLiteCpp | bundled |
| Testing | Catch2 v3 | FetchContent |

## Target Directory Structure
```
real-time-reaction/
├── CMakeLists.txt
├── CLAUDE.md
├── .gitignore
├── data/                          # gitignored — runtime only
│   ├── reaction.db
│   └── exports/
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── timer.hpp / timer.cpp
│   │   ├── detection.hpp / detection.cpp
│   │   └── session.hpp / session.cpp
│   ├── data/
│   │   ├── models.hpp
│   │   ├── database.hpp / database.cpp
│   │   └── analytics.hpp / analytics.cpp
│   └── ui/
│       ├── screen.hpp             # IScreen interface
│       ├── renderer.hpp / renderer.cpp
│       ├── camera_texture.hpp / camera_texture.cpp
│       ├── colors.hpp             # ImVec4 constants
│       └── screens/
│           ├── menu_screen.hpp / menu_screen.cpp
│           ├── game_screen.hpp / game_screen.cpp
│           ├── results_screen.hpp / results_screen.cpp
│           └── analytics_screen.hpp / analytics_screen.cpp
└── tests/
    ├── CMakeLists.txt
    ├── test_database.cpp
    ├── test_timer.cpp
    ├── test_session.cpp
    ├── test_two_player.cpp        # Phase 3
    └── test_analytics.cpp         # Phase 4
```

## Design Decisions — Immutable
1. **Keyboard is the response mechanism.** `SDLK_SPACE` = Player 1. `SDLK_RETURN` = Player 2. Not motion.
2. **Motion detection = presence/idle only.** `DetectionEngine` returns data structs. It never draws.
3. **Camera pipeline = OpenCV Mat → OpenGL texture.** Conversion happens only in `CameraTexture`. Nowhere else.
4. **All DB access via `Database` class.** No raw sqlite3 calls outside `database.cpp`.
5. **All rendering via ImGui.** No SDL2 direct drawing. No cv2.imshow. Stimuli drawn via `ImDrawList`.
6. **Screen management via `IScreen` polymorphism.** `main.cpp` owns a `unique_ptr<IScreen>` and swaps it.

## Build Commands
```bash
# Configure (first time)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --parallel

# Run
./build/rtr

# Test
cd build && ctest --output-on-failure
```

## System Dependency Install
```bash
# Ubuntu / Debian
sudo apt install libsdl2-dev libglew-dev libopencv-dev libsqlite3-dev

# macOS (Homebrew)
brew install sdl2 glew opencv sqlite3

# Windows — use vcpkg
vcpkg install sdl2 glew opencv4 sqlite3
# then: cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
```

## Color Palette — `src/ui/colors.hpp`
```cpp
#pragma once
#include "imgui.h"

namespace Colors {
    inline constexpr ImVec4 BG      {0.059f, 0.059f, 0.078f, 1.0f}; // #0F0F14
    inline constexpr ImVec4 PANEL   {0.110f, 0.110f, 0.149f, 1.0f}; // #1C1C26
    inline constexpr ImVec4 ACCENT  {0.369f, 0.918f, 0.824f, 1.0f}; // #5EEACC
    inline constexpr ImVec4 P1      {0.388f, 0.400f, 0.945f, 1.0f}; // #6366F1
    inline constexpr ImVec4 P2      {0.976f, 0.451f, 0.086f, 1.0f}; // #F97316
    inline constexpr ImVec4 TEXT    {0.945f, 0.957f, 0.976f, 1.0f}; // #F1F5F9
    inline constexpr ImVec4 MUTED   {0.392f, 0.455f, 0.545f, 1.0f}; // #64748B
    inline constexpr ImVec4 SUCCESS {0.133f, 0.773f, 0.369f, 1.0f}; // #22C55E
    inline constexpr ImVec4 WARNING {0.918f, 0.702f, 0.031f, 1.0f}; // #EAB308
    inline constexpr ImVec4 DANGER  {0.937f, 0.267f, 0.267f, 1.0f}; // #EF4444
}
```

## Display Constants
```cpp
constexpr int   WINDOW_W  = 1280;
constexpr int   WINDOW_H  = 720;
constexpr int   CAMERA_W  = 640;
constexpr int   CAMERA_H  = 480;
constexpr float TARGET_FPS = 60.0f;
```

## ImGui Style Notes
- Push `ImGuiStyleVar_WindowRounding` to 8.0f globally
- Push `ImGuiStyleVar_FrameRounding` to 6.0f globally
- Use `ImGui::PushStyleColor` / `PopStyleColor` for per-widget color overrides
- All full-screen windows: `ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize`
- Set window position/size to cover full viewport each frame before `ImGui::Begin()`
