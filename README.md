# Real-Time Reaction:
Webcam-based Application that measures user reaction time through motion detection. The system combines Python's ease of use with C++'s performance capabilities to provide accurate reaction time measurements.
# Requirements
- Python 3.9
- OpenCV
- NumPy
- Webcam
## System Architecture 
This application is built using a hybrid approach:
- Core timing logic implemented in C++ for microsecond precision
- Python frontend for webcam handling and user interface
- OpenCV for efficient motion detection and image processing
## Prerequisites

### Essential Software
- Python 3.9 or later
- Visual Studio 2019 or later with C++ Desktop Development workload
   - Make sure to install "Desktop development with C++" workload
   - Include "Windows 10 SDK" and "MSVC v142" components
- Functioning webcam

## Installation

python -m venv venv
venv\Scripts\activate
or
source venv/bin/activate



## Install required Python packages:
pip install -r requirements.txt

## Build C++ components:
python setup.py build_ext --inplace

## Running the Application
Start the reaction time tester:
python test_full_system.py
# Controls

'+' key: Increase motion detection sensitivity

'-' key: Decrease motion detection sensitivity

'q' key: Quit application

# What to Expect

The application opens two windows:

Main window showing your webcam feed with stimulus overlays

Motion detection window displaying movement analysis

Random shapes will appear at random intervals

Move quickly when you see a shape appear

Your reaction time will be measured and displayed

Final statistics show at the end of your session


# Real-Time Reaction

A C++17 reaction-time trainer for one or two local players. A visual stimulus
(circle / square / cross, in one of four colours) appears over a live camera
panel; players respond on the keyboard. The app measures stimulus-onset → keypress
latency, persists every trial to SQLite, and shows per-player analytics with
in-app charts.

> Built with SDL2 + OpenGL + Dear ImGui (UI), ImPlot (charts), OpenCV (camera),
> and SQLiteCpp (storage). See [CLAUDE.md](CLAUDE.md) for the full design spec.

## Features

- **Single-player** and **two-player** modes (`SPACE` = Player 1, `ENTER` = Player 2).
- Per-trial timing with a high-resolution timer; **false-start** detection and a
  2 s **timeout → MISS**.
- Live webcam panel (OpenCV → OpenGL texture); falls back to a black panel if no
  camera is present — it never crashes.
- **Results screen** with per-player average / best / worst / misses, a winner
  banner in two-player mode, and a colour-coded trial log.
- **Analytics screen**: player selector, full stats (mean, median, std-dev, p10/p90),
  an ImPlot reaction-time **histogram** (with mean/median markers), a
  session-by-session **trend line**, a session-history table, a **leaderboard**
  (min. 5 valid trials to qualify), and **CSV export** to `data/exports/`.
- All trials/sessions/players persisted in a local SQLite database (`data/reaction.db`).

## Controls

| Key | Action |
|---|---|
| `SPACE` | Player 1 response |
| `ENTER` | Player 2 response |
| `ESC` | Back to the menu |

## Requirements

- **CMake** 3.20+
- A **C++17** compiler (MSVC / VS 2022 on Windows; GCC/Clang elsewhere)
- Native libraries: **SDL2, GLEW, OpenGL, OpenCV 4** (core/videoio/imgproc), **SQLite3**
- Fetched automatically at configure time (no manual install): Dear ImGui, ImPlot,
  SQLiteCpp, Catch2

### Installing the native dependencies

```bash
# Ubuntu / Debian
sudo apt install cmake libsdl2-dev libglew-dev libopencv-dev libsqlite3-dev

# macOS (Homebrew)
brew install cmake sdl2 glew opencv sqlite3

# Windows (vcpkg)
vcpkg install sdl2 glew opencv4 sqlite3
```

## Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# ...or, on Windows with vcpkg:
cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows

# Build
cmake --build build --config Debug --parallel
```

## Run

```bash
# Linux / macOS
./build/rtr

# Windows (multi-config)
./build/Debug/rtr.exe
```

The app creates `data/reaction.db` and `data/exports/` next to the executable on
first run.

## Test

```bash
cd build
ctest -C Debug --output-on-failure
```

34 tests cover the timer, database layer, session state machine, two-player rules,
winner determination, and the analytics computation / CSV export.

## Project layout

```
.
├── CMakeLists.txt              # build: find_package + FetchContent
├── CLAUDE.md                   # design spec / decisions
├── src/
│   ├── main.cpp                # SDL/ImGui loop, screen swapping
│   ├── core/                   # timer, motion detection, game session state machine
│   ├── data/                   # models, SQLite database, analytics + CSV export
│   └── ui/                     # renderer, camera texture, colours
│       └── screens/            # menu, game, results, analytics
├── tests/                      # Catch2 test suites
└── data/                       # runtime DB + CSV exports (gitignored)
```

## Architecture notes

- **Keyboard is the response mechanism** — motion detection is presence/idle only.
- All rendering goes through **Dear ImGui**; stimuli are drawn with `ImDrawList`.
- All database access lives in the **`Database`** class (no raw SQLite elsewhere).
- Screens implement a common **`IScreen`** interface; `main.cpp` owns a single
  `unique_ptr<IScreen>` and swaps it on transitions.

This project was built in four phases (foundation → UI/game loop → two-player →
analytics); the per-phase specs are in `PHASE_1.md` … `PHASE_4.md`.
