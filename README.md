# reActivation

A C++17 reaction-time trainer and **game** for one or two local players. A visual
stimulus (circle / square / cross, in one of four colours) appears over a live
camera panel; players respond on the keyboard. The app measures stimulus-onset →
keypress latency, reacts with procedural audio and screen effects, persists every
trial to SQLite, and shows per-player analytics with in-app charts.

> Built with SDL2 + OpenGL + Dear ImGui (UI), ImPlot (charts), OpenCV (camera),
> SDL_mixer (procedural audio), and SQLiteCpp (storage). See [CLAUDE.md](CLAUDE.md)
> for the full design spec.

## Game modes

| Mode | Players | Ends when | Score |
|---|---|---|---|
| **Classic** | 1 or 2 (alternating) | a fixed number of trials | lowest average reaction time |
| **Race** | 2 (simultaneous) | a fixed number of rounds | most rounds won — both keys live, first press wins |
| **Blitz** | 1 or 2 (alternating) | a 30-second timer | most valid presses |
| **Survival** | 1 or 2 (alternating) | lives reach 0 | trials survived (a miss or false start costs a life) |

## Features

- **Four game modes** (above), selectable from the menu; Race forces two players,
  the others toggle single / two-player.
- **Feel layer** — procedurally generated audio (stimulus beep, hit, miss,
  false-start buzzer, new-record arpeggio; zero audio files), a screen flash on
  press, per-trial performance labels (`GODLIKE` / `ELITE` / … / `TOO SLOW`),
  a **personal-best** detector with a `NEW RECORD` flash, a breathing camera
  border, and a subtle CRT scanline overlay.
- **Live streak counter** (consecutive sub-300 ms hits) in the HUD, and a large
  **session rank** (S / A / B / C / D, with a miss penalty) on the results screen.
- Per-trial timing with a high-resolution timer; **false-start** detection and a
  2 s **timeout → MISS**.
- **Persistent session state** — one name per player, plus the last mode, are
  remembered across screens and app restarts, so you don't retype your name.
- Live webcam panel (OpenCV → OpenGL texture); falls back to a black panel if no
  camera is present — it never crashes. Audio is likewise silent-safe with no
  audio device.
- **Resizable, high-DPI** window (crisp on scaled displays; won't shrink below the
  design resolution).
- **Results screen** with per-player average / best / worst / misses, mode-specific
  headers (round score / valid-press count / trials survived), a winner banner in
  two-player mode, max-streak, and a colour-coded trial log.
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

In **Race** mode both keys are live every round — first valid press wins.

## Requirements

- **CMake** 3.20+
- A **C++17** compiler (MSVC / VS 2022 on Windows; GCC/Clang elsewhere)
- Native libraries: **SDL2**, **SDL2_mixer**, **GLEW**, **OpenGL**, **OpenCV 4**
  (core/videoio/imgproc), **SQLite3**
- Fetched automatically at configure time (no manual install): Dear ImGui, ImPlot,
  SQLiteCpp, Catch2

### Installing the native dependencies

```bash
# Ubuntu / Debian
sudo apt install cmake libsdl2-dev libsdl2-mixer-dev libglew-dev libopencv-dev libsqlite3-dev

# macOS (Homebrew)
brew install cmake sdl2 sdl2_mixer glew opencv sqlite3

# Windows (vcpkg)
vcpkg install sdl2 sdl2-mixer glew opencv4 sqlite3
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
first run. The executable target is named `rtr`; the window title is
**reActivation**.

## Test

```bash
cd build
ctest -C Debug --output-on-failure
```

51 tests cover the timer, database layer (incl. settings + personal-best), the
session state machine, two-player rules, the four game modes (Race / Blitz /
Survival / streak / rank), winner determination, procedural audio, and the
analytics computation / CSV export.

## Project layout

```
.
├── CMakeLists.txt              # build: find_package + FetchContent
├── CLAUDE.md                   # design spec / decisions
├── src/
│   ├── main.cpp                # SDL/ImGui loop, screen swapping, AppState
│   ├── audio/                  # procedural audio engine (SDL_mixer, WAV synthesis)
│   ├── core/                   # timer, motion detection, session state machine,
│   │                           #   game modes/streak, AppState (session persistence)
│   ├── data/                   # models, SQLite database + settings, analytics/CSV
│   └── ui/                     # renderer, camera texture, colours, effects
│       └── screens/            # menu, game, results, analytics
├── tests/                      # Catch2 test suites
└── data/                       # runtime DB + CSV exports (gitignored)
```

## Architecture notes

- **Keyboard is the response mechanism** — motion detection is presence/idle only.
- All rendering goes through **Dear ImGui**; stimuli and effects are drawn with
  `ImDrawList`. Screens fill the live viewport, so the window is resizable.
- **`GameSession`** owns the state machine and per-mode logic. Its constructor
  takes the game mode as a string (`classic`/`race`/`blitz`/`survival`, and still
  accepts legacy `single`/`two_player` → Classic); player count is derived from
  the number of names.
- **Audio is procedural** — `AudioEngine` synthesises every sound as 16-bit PCM
  at startup and wraps it in an in-memory WAV; no asset files, and it degrades
  silently when there is no audio device.
- All database access lives in the **`Database`** class (no raw SQLite elsewhere),
  including a `settings` key/value table used by **`AppState`** to persist the
  current user/session across restarts.
- Screens implement a common **`IScreen`** interface; `main.cpp` owns a single
  `unique_ptr<IScreen>` and swaps it on transitions.

This project was built in six phases (foundation → UI/game loop → two-player →
analytics → feel layer/audio → game modes); the per-phase specs are in
`PHASE_1.md` … `PHASE_6.md`, and the prompts that drove each phase are in
[PROMPTS.md](PROMPTS.md).
