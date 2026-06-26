# Phase 1: Foundation — CMake, Data Layer, State Machine

## Goal
Working CMake build, SQLite data layer via SQLiteCpp, high-precision timer,
motion detection engine, game session state machine, and a compilable main stub.
No UI. All tests must pass.

## Prerequisites
Install system dependencies (see CLAUDE.md). Then verify:
```bash
cmake --version    # 3.20+
sdl2-config --version
pkg-config --modversion opencv4  # or opencv
```

---

## Deliverable 1 — `CMakeLists.txt` (root)

```cmake
cmake_minimum_required(VERSION 3.20)
project(RealTimeReaction VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── System packages ───────────────────────────────────────────────────────────
find_package(SDL2   REQUIRED)
find_package(GLEW   REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core videoio imgproc)
find_package(SQLite3 REQUIRED)

# ── FetchContent ─────────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
    GIT_SHALLOW    TRUE)

FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.16
    GIT_SHALLOW    TRUE)

FetchContent_Declare(sqlitecpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG        3.3.1
    GIT_SHALLOW    TRUE
    CMAKE_CACHE_ARGS
        -DSQLITECPP_RUN_CPPLINT:BOOL=OFF
        -DSQLITECPP_RUN_CPPCHECK:BOOL=OFF
        -DSQLITECPP_BUILD_TESTS:BOOL=OFF)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(imgui implot sqlitecpp Catch2)

# ── ImGui static library ─────────────────────────────────────────────────────
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2 GLEW::GLEW OpenGL::GL)

# ── ImPlot static library ────────────────────────────────────────────────────
add_library(implot_lib STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot_lib PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot_lib PUBLIC imgui_lib)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(rtr
    src/main.cpp
    src/core/timer.cpp
    src/core/detection.cpp
    src/core/session.cpp
    src/data/database.cpp
    src/data/analytics.cpp
    src/ui/renderer.cpp
    src/ui/camera_texture.cpp
    src/ui/screens/menu_screen.cpp
    src/ui/screens/game_screen.cpp
    src/ui/screens/results_screen.cpp
    src/ui/screens/analytics_screen.cpp
)
target_include_directories(rtr PRIVATE src/)
target_link_libraries(rtr PRIVATE
    imgui_lib implot_lib SQLiteCpp
    ${OpenCV_LIBS} SDL2::SDL2 GLEW::GLEW OpenGL::GL
)

# Create data/ dir at build time
add_custom_command(TARGET rtr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:rtr>/data/exports)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
```

### `tests/CMakeLists.txt`
```cmake
add_executable(rtr_tests
    test_database.cpp
    test_timer.cpp
    test_session.cpp
)
target_include_directories(rtr_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rtr_tests PRIVATE
    Catch2::Catch2WithMain SQLiteCpp ${OpenCV_LIBS}
)

include(Catch)
catch_discover_tests(rtr_tests)
```

### `.gitignore`
```
build/
data/reaction.db
data/exports/
.cache/
compile_commands.json
```

---

## Deliverable 2 — `src/data/models.hpp`

```cpp
#pragma once
#include <string>
#include <optional>

struct Player {
    int         id   = 0;
    std::string name;
    std::string created_at;  // ISO 8601
};

struct Session {
    int         id           = 0;
    int         player1_id   = 0;
    int         player2_id   = 0;   // 0 = single player
    std::string mode;               // "single" | "two_player"
    std::string started_at;
    std::string completed_at;
    std::string notes;
};

struct Trial {
    int         id                   = 0;
    int         session_id           = 0;
    int         trial_number         = 0;
    std::string stimulus_type;       // "circle" | "square" | "cross"
    std::string stimulus_color;      // "red" | "green" | "blue" | "yellow"
    int         player               = 1;   // 1 or 2
    double      stimulus_onset_epoch = 0.0; // std::time(nullptr) at display
    float       reaction_time_ms     = -1.0f; // -1 = miss/timeout
    bool        false_start          = false;
    double      response_epoch       = 0.0;
};

// Sentinel — use instead of optional to avoid nullable floats in hot path
inline bool is_miss(const Trial& t) { return t.reaction_time_ms < 0.0f; }
```

---

## Deliverable 3 — `src/data/database.hpp` + `database.cpp`

```cpp
// database.hpp
#pragma once
#include <string>
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>
#include "models.hpp"

class Database {
public:
    explicit Database(const std::string& path = "data/reaction.db");

    void create_schema();   // idempotent

    // Players
    int  insert_player(const std::string& name);
    int  get_or_create_player(const std::string& name);
    std::vector<Player> get_all_players();

    // Sessions
    int  insert_session(const Session& s);
    void close_session(int session_id);

    // Trials
    int  insert_trial(const Trial& t);
    std::vector<Trial> get_session_trials(int session_id);
    std::vector<Trial> get_player_trials(int player_id);

    static std::string now_iso();

private:
    SQLite::Database db_;
    void ensure_dir(const std::string& path);
};
```

Implementation notes for `database.cpp`:
- Open with `SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE`
- Execute `PRAGMA journal_mode=WAL;` and `PRAGMA foreign_keys=ON;` in constructor
- Use `SQLite::Statement` with `bind()` for all writes — no string concatenation in SQL
- `get_or_create_player`: SELECT first; INSERT only if not found; return id either way
- `close_session`: UPDATE sessions SET completed_at = ? WHERE id = ?
- `now_iso()`: use `<chrono>` + `std::put_time` to produce `"2025-01-15T14:32:00Z"`

### Schema DDL (inside `create_schema()`):
```sql
CREATE TABLE IF NOT EXISTS players (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    player1_id   INTEGER NOT NULL REFERENCES players(id),
    player2_id   INTEGER          REFERENCES players(id),
    mode         TEXT    NOT NULL CHECK(mode IN ('single','two_player')),
    started_at   TEXT    NOT NULL,
    completed_at TEXT,
    notes        TEXT    NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS trials (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id           INTEGER NOT NULL REFERENCES sessions(id),
    trial_number         INTEGER NOT NULL,
    stimulus_type        TEXT    NOT NULL,
    stimulus_color       TEXT    NOT NULL,
    player               INTEGER NOT NULL CHECK(player IN (1,2)),
    stimulus_onset_epoch REAL    NOT NULL,
    reaction_time_ms     REAL    NOT NULL DEFAULT -1,
    false_start          INTEGER NOT NULL DEFAULT 0,
    response_epoch       REAL    NOT NULL DEFAULT 0
);
```

---

## Deliverable 4 — `src/core/timer.hpp` + `timer.cpp`

```cpp
// timer.hpp
#pragma once
#include <chrono>
#include <stdexcept>

class ReactionTimer {
public:
    void  start();           // throws if already running
    float stop();            // returns ms; throws if not running
    void  reset() noexcept;
    bool  is_running() const noexcept { return running_; }

private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start_time_;
    bool              running_ = false;
};
```

`timer.cpp` — straightforward `duration_cast<microseconds>` divided by 1000.0f.

---

## Deliverable 5 — `src/core/detection.hpp` + `detection.cpp`

```cpp
// detection.hpp
#pragma once
#include <opencv2/opencv.hpp>
#include <deque>
#include <vector>

struct DetectionResult {
    bool  movement_detected = false;
    float movement_area     = 0.0f;
    float threshold         = 0.0f;
    std::vector<std::vector<cv::Point>> contours; // raw; caller draws if desired
};

class DetectionEngine {
public:
    explicit DetectionEngine(float threshold = 1000.0f, int buffer_size = 3);

    DetectionResult update(const cv::Mat& bgr_frame);
    void set_threshold(float value) noexcept;
    void reset() noexcept;

    float threshold() const noexcept { return threshold_; }

private:
    float              threshold_;
    int                buffer_size_;
    std::deque<cv::Mat> prev_frames_;
};
```

`detection.cpp`: grayscale → GaussianBlur → absdiff → threshold → dilate → findContours.
**Zero drawing calls.** Returns contours in result for caller to optionally render.

---

## Deliverable 6 — `src/core/session.hpp` + `session.cpp`

```cpp
// session.hpp
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <SDL2/SDL_keycode.h>
#include "data/models.hpp"
#include "data/database.hpp"

enum class SessionState {
    Idle,
    Countdown,       // 3-second count before first trial
    Waiting,         // inter-stimulus interval (random 2–5 s)
    StimulusActive,  // stimulus visible; awaiting keypress
    Collecting,      // result recorded; 800 ms feedback display
    Complete
};

struct TrialInfo {
    std::string stimulus_type;
    std::string stimulus_color;
    int         player       = 1;
    int         trial_number = 0;
    int         total_trials = 0;
};

struct LastResult {
    float reaction_time_ms = -1.0f;  // -1 = miss
    bool  false_start      = false;
    int   player           = 1;
};

class GameSession {
public:
    static constexpr int   TRIALS_PER_PLAYER = 10;
    static constexpr float ISI_MIN_S         = 2.0f;
    static constexpr float ISI_MAX_S         = 5.0f;
    static constexpr float TIMEOUT_MS        = 2000.0f;
    static constexpr float FEEDBACK_MS       = 800.0f;
    static constexpr float COUNTDOWN_S       = 3.0f;

    GameSession(const std::string& mode,
                const std::vector<std::string>& player_names,
                Database& db);

    void         start();  // registers players, inserts session row, → Countdown
    SessionState tick(float current_time_s, const std::vector<SDL_Keycode>& keys);

    SessionState               get_state()        const noexcept;
    int                        get_active_player() const noexcept;
    std::optional<TrialInfo>   get_current_trial() const;
    std::optional<LastResult>  get_last_result()   const;
    float                      get_countdown_remaining(float now_s) const;
    const std::vector<Trial>&  get_results()       const;

private:
    std::string              mode_;
    std::vector<std::string> player_names_;
    Database&                db_;

    SessionState     state_      = SessionState::Idle;
    int              session_id_ = 0;
    std::vector<int> player_ids_;
    std::vector<Trial> trials_;

    Trial* current_trial_ = nullptr;  // points into trials_ or nullptr
    int    trial_counter_ = 0;
    int    total_trials_  = 0;

    float phase_start_s_  = 0.0f;
    float isi_duration_s_ = 0.0f;

    std::optional<LastResult> last_result_;

    // Helpers
    int  current_player() const noexcept;
    bool key_is_valid(SDL_Keycode key) const noexcept;
    void enter_waiting(float t);
    void enter_stimulus(float t);
    void enter_collecting(float t);
    void record_response(float elapsed_ms);
    void record_timeout();
    void record_false_start(float t, SDL_Keycode key);
    void complete();

    static std::string random_stimulus_type();
    static std::string random_stimulus_color();
};
```

State transitions:
```
Idle → Countdown   : start()
Countdown → Waiting : elapsed >= COUNTDOWN_S * 1000
Waiting → StimulusActive : elapsed >= isi_duration * 1000
  (keypress during Waiting by active player → false_start, restart Waiting)
StimulusActive → Collecting : valid keypress (record rt) OR elapsed >= TIMEOUT_MS
Collecting → Waiting  : elapsed >= FEEDBACK_MS AND trial_counter < total_trials
Collecting → Complete : elapsed >= FEEDBACK_MS AND trial_counter >= total_trials
```

Key validity: `SDLK_SPACE` → player 1, `SDLK_RETURN` → player 2.
Inactive player's key is silently ignored in all states.

---

## Deliverable 7 — `src/main.cpp` (Phase 1 stub)

```cpp
#include <iostream>
#include "data/database.hpp"
#include "core/timer.hpp"

int main() {
    std::cout << "Real-Time Reaction — Phase 1 build check\n";

    Database db;
    db.create_schema();
    std::cout << "Database ready.\n";

    int pid = db.get_or_create_player("Test Player");
    std::cout << "Test player id: " << pid << "\n";

    ReactionTimer timer;
    timer.start();
    // busy-wait 1ms — just a sanity check
    auto t0 = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now() - t0).count() < 1) {}
    float ms = timer.stop();
    std::cout << "Timer check: " << ms << " ms (expect ~1)\n";

    std::cout << "Phase 1 OK.\n";
    return 0;
}
```

---

## Tests

### `tests/test_timer.cpp`
```cpp
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
```

### `tests/test_database.cpp`
Use in-memory DB: `Database db(":memory:")`.
- `create_schema()` called twice — no exception
- `insert_player` returns positive integer
- `get_or_create_player` returns same id on second call
- `insert_session` returns positive integer; `close_session` sets completed_at
- `insert_trial` with all fields; `get_session_trials` returns it with correct values
- `get_player_trials` returns only trials for that player's sessions

### `tests/test_session.cpp`
Use `Database db(":memory:")`.
- Initial state is `Idle`
- `start()` transitions to `Countdown`
- Tick past `COUNTDOWN_S` → `Waiting`
- Active player keypress during `Waiting` → still `Waiting`, last trial has `false_start=true`
- Inactive player keypress during `Waiting` → state unchanged, no false start recorded
- Tick past `ISI_MAX_S + 0.1` during `Waiting` → `StimulusActive`
- Valid keypress during `StimulusActive` → `Collecting`, `reaction_time_ms > 0`
- Tick past `TIMEOUT_MS` with no keypress → `Collecting`, `is_miss()` on last trial
- After `TRIALS_PER_PLAYER` completed trials → `Complete`

---

## Acceptance Criteria
- [ ] `cmake -B build && cmake --build build` succeeds with no warnings on -Wall
- [ ] `./build/rtr` prints Phase 1 lines and exits cleanly
- [ ] `data/reaction.db` created with tables: players, sessions, trials
- [ ] `cd build && ctest --output-on-failure` — all tests pass
- [ ] `DetectionEngine` compiles and `update()` returns `DetectionResult` without drawing

## Out of Scope
- SDL2, ImGui, OpenGL — any UI or window creation
- Two-player UI
- Analytics, CSV export
- Phase 2+ source files (stubs with empty `main()` are fine for compilation)
