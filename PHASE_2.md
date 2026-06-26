# Phase 2: Renderer, Screens, Game Loop

## Goal
Working SDL2 + OpenGL + Dear ImGui window. Camera feed as live OpenGL texture.
IScreen interface with Menu, Game (single-player), and Results screens.
`main.cpp` becomes the real entry point.

## Prerequisites
- Phase 1 Acceptance Criteria all passing
- System deps confirm: SDL2, GLEW, OpenGL present (cmake -B build should find them)

---

## Deliverable 1 — `src/ui/colors.hpp`

Define the full `Colors::` namespace from CLAUDE.md.
Also define display constants here:
```cpp
namespace Display {
    constexpr int   W          = 1280;
    constexpr int   H          = 720;
    constexpr int   CAMERA_W   = 640;
    constexpr int   CAMERA_H   = 480;
    constexpr float TARGET_FPS = 60.0f;
}
```

---

## Deliverable 2 — `src/ui/screen.hpp`

```cpp
#pragma once
#include <memory>
#include <SDL2/SDL.h>

// Forward declare to avoid including session headers here
class Database;

struct ScreenResult {
    bool        transition = false;
    // populated by the leaving screen; main.cpp reads and acts on it
    enum class Next { Stay, Menu, Game, Results, Analytics, Quit } next = Next::Stay;
    // payload for Game screen construction
    std::vector<std::string> player_names;
    std::string mode;  // "single" | "two_player"
};

class IScreen {
public:
    virtual ~IScreen() = default;
    virtual void handle_event(const SDL_Event& e) = 0;
    virtual void update(float dt) = 0;
    virtual void render() = 0;
    virtual ScreenResult get_result() const = 0;
};
```

---

## Deliverable 3 — `src/ui/renderer.hpp` + `renderer.cpp`

```cpp
// renderer.hpp
#pragma once
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <string>

class Renderer {
public:
    Renderer(const std::string& title, int w, int h);
    ~Renderer();

    // No copy
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    void begin_frame();      // ImGui_ImplSDL2_NewFrame + ImGui::NewFrame
    void end_frame();        // ImGui::Render + SDL_GL_SwapWindow

    float delta_time() const noexcept { return dt_; }
    SDL_Window* window() const noexcept { return window_; }

private:
    SDL_Window*   window_     = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    float         dt_         = 0.0f;
    Uint64        last_ticks_ = 0;

    void init_sdl(const std::string& title, int w, int h);
    void init_imgui();
    void apply_global_style();
};
```

`renderer.cpp` implementation notes:
- SDL2 init flags: `SDL_INIT_VIDEO | SDL_INIT_TIMER`
- GL attributes: major=3, minor=3, profile=CORE, double buffer=1, depth=24
- Window flags: `SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN`
- After `SDL_GL_CreateContext`: call `glewInit()`, check `GLEW_OK`
- ImGui: `IMGUI_CHECKVERSION()`, `ImGui::CreateContext()`, `ImPlot::CreateContext()`
- ImGui backends: `ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_)`, `ImGui_ImplOpenGL3_Init("#version 330")`
- `apply_global_style()`: set rounding, colors from `Colors::` namespace, font scale
- `begin_frame()`: compute dt from SDL_GetPerformanceCounter; call ImGui New Frame sequence
- `end_frame()`: `glClearColor` with `Colors::BG`, `glClear`, `ImGui::Render`, `ImGui_ImplOpenGL3_RenderDrawData`, `SDL_GL_SwapWindow`
- Destructor: `ImPlot::DestroyContext()`, `ImGui::DestroyContext()`, destroy SDL window + context, `SDL_Quit()`

---

## Deliverable 4 — `src/ui/camera_texture.hpp` + `camera_texture.cpp`

```cpp
// camera_texture.hpp
#pragma once
#include <GL/glew.h>
#include <opencv2/opencv.hpp>

class CameraTexture {
public:
    CameraTexture(int width, int height, int camera_index = 0);
    ~CameraTexture();

    bool open();
    void update();   // call once per frame; uploads latest frame to GL
    void close();

    GLuint texture_id()  const noexcept { return tex_id_; }
    bool   is_open()     const noexcept { return cap_.isOpened(); }
    int    width()       const noexcept { return width_; }
    int    height()      const noexcept { return height_; }

    // Latest frame for DetectionEngine (BGR)
    const cv::Mat& latest_frame() const noexcept { return latest_bgr_; }

private:
    int           width_, height_, cam_index_;
    cv::VideoCapture cap_;
    cv::Mat          latest_bgr_;
    GLuint           tex_id_ = 0;

    void init_texture();
};
```

`camera_texture.cpp` notes:
- `init_texture()`: `glGenTextures`, bind, set `GL_LINEAR` filter, `GL_CLAMP_TO_EDGE` wrap
- `update()`:
  1. `cap_ >> latest_bgr_`; return if empty
  2. `cv::cvtColor(latest_bgr_, rgb, cv::COLOR_BGR2RGB)`
  3. `glBindTexture(GL_TEXTURE_2D, tex_id_)`
  4. `glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data)`
- If camera fails to open: leave `tex_id_` bound to a 1×1 black texture (app must not crash)

---

## Deliverable 5 — `src/ui/screens/menu_screen.hpp` + `menu_screen.cpp`

```cpp
class MenuScreen : public IScreen {
public:
    explicit MenuScreen(Database& db);

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

private:
    Database& db_;
    char  p1_name_[64] = {};
    char  p2_name_[64] = {};
    bool  two_player_  = false;
    ScreenResult result_;

    void render_title();
    void render_name_inputs();
    void render_mode_buttons();
    void render_start_button();
    void render_analytics_button();
};
```

Layout (all ImGui, fullscreen window):
```
Title: "REAL-TIME REACTION"   — large, Colors::ACCENT
Subtitle: "Reaction Time Trainer"  — Colors::MUTED, smaller

[ Single Player ]  [ Two Player ]   — toggle buttons; active = Colors::ACCENT border

"Player 1"
[InputText ─────────────────── ]

(if two_player_)
"Player 2"
[InputText ─────────────────── ]

[ START ]   — disabled (greyed) if p1_name_ is empty
             (also p2_name_ empty if two_player_)

[ Analytics ]  — bottom of panel
```

On START click: populate `result_` with `transition=true`, `next=Next::Game`,
`player_names`, `mode`.
On Analytics click: `result_` with `next=Next::Analytics`.

---

## Deliverable 6 — `src/ui/screens/game_screen.hpp` + `game_screen.cpp`

```cpp
class GameScreen : public IScreen {
public:
    GameScreen(Database& db, const std::vector<std::string>& player_names,
               const std::string& mode);
    ~GameScreen();

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

private:
    Database&      db_;
    GameSession    session_;
    CameraTexture  camera_;
    ScreenResult   result_;

    std::vector<SDL_Keycode> keys_this_frame_;

    void render_camera_panel(ImVec2 pos, ImVec2 size);
    void render_stimulus(ImVec2 camera_pos, ImVec2 camera_size,
                         const TrialInfo& info);
    void render_countdown_overlay(ImVec2 pos, ImVec2 size, float remaining_s);
    void render_hud_panel(ImVec2 pos, ImVec2 size);
    void render_progress_bar(ImVec2 pos, ImVec2 size);
    void draw_stimulus_shape(ImDrawList* dl, const std::string& type,
                              const std::string& color, ImVec2 center, float radius);
};
```

Layout:
```
┌─────────────────── 1280×720 ──────────────────┐
│  Camera panel (640×480, left-aligned, y=120)  │
│  + stimulus overlay when StimulusActive       │
│  + countdown overlay when Countdown            │
│─────────────────────────────────────────────  │
│  HUD panel (right 640px, full height):        │
│    State label, trial counter,                │
│    last reaction time, key hint               │
│─────────────────────────────────────────────  │
│  Progress bar (full width, bottom 20px)       │
└───────────────────────────────────────────────┘
```

Stimulus drawing via `ImDrawList`:
```cpp
// circle:  dl->AddCircleFilled(center, radius, color32)
// square:  dl->AddRectFilled(min, max, color32)
// cross:   two dl->AddRectFilled calls (horizontal + vertical bars, thickness=radius*0.3)

// Stimulus only visible during StimulusActive state
// Countdown: large centered text overlay on camera panel

// Color map (name → ImU32 via ImGui::ColorConvertFloat4ToU32):
// "red"    → ImVec4{0.86f, 0.20f, 0.20f, 1.0f}
// "green"  → ImVec4{0.20f, 0.78f, 0.31f, 1.0f}
// "blue"   → ImVec4{0.24f, 0.51f, 0.86f, 1.0f}
// "yellow" → ImVec4{0.86f, 0.78f, 0.12f, 1.0f}
```

`handle_event`: collect `SDL_KEYDOWN` symbols into `keys_this_frame_`.
`update`: call `session_.tick(SDL_GetTicks64() / 1000.0f, keys_this_frame_)`;
clear `keys_this_frame_`; if state == Complete set `result_.transition = true`.
On ESC: set result to Menu.

---

## Deliverable 7 — `src/ui/screens/results_screen.hpp` + `results_screen.cpp`

```cpp
class ResultsScreen : public IScreen {
public:
    ResultsScreen(const std::vector<Trial>& trials,
                  const std::vector<std::string>& player_names);

    void handle_event(const SDL_Event& e) override;
    void update(float dt) override;
    void render() override;
    ScreenResult get_result() const override;

private:
    std::vector<Trial>       trials_;
    std::vector<std::string> player_names_;
    ScreenResult             result_;
    int                      scroll_offset_ = 0;

    struct Stats {
        int   count       = 0;
        int   misses      = 0;
        int   false_starts= 0;
        float avg_ms      = -1.0f;
        float best_ms     = -1.0f;
        float worst_ms    = -1.0f;
    };

    Stats compute_stats(int player) const;
    ImVec4 rt_color(float ms) const;  // success/warning/danger thresholds

    void render_stats_row(const Stats& s, int player);
    void render_trial_log();
    void render_action_buttons();
};
```

RT color thresholds: `< 250ms` → SUCCESS, `250–400ms` → WARNING, `> 400ms` → DANGER.
`render_action_buttons`: [ PLAY AGAIN ] → result Next::Menu; [ MAIN MENU ] → Next::Menu.

---

## Deliverable 8 — Updated `src/main.cpp`

```cpp
#include <iostream>
#include <memory>
#include <SDL2/SDL.h>
#include "data/database.hpp"
#include "ui/renderer.hpp"
#include "ui/screen.hpp"
#include "ui/screens/menu_screen.hpp"
#include "ui/screens/game_screen.hpp"
#include "ui/screens/results_screen.hpp"
#include "ui/screens/analytics_screen.hpp"

int main() {
    Database  db;
    db.create_schema();

    Renderer renderer("Real-Time Reaction", Display::W, Display::H);
    std::unique_ptr<IScreen> screen = std::make_unique<MenuScreen>(db);

    std::vector<Trial>       last_trials;
    std::vector<std::string> last_names;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;
            screen->handle_event(e);
        }

        renderer.begin_frame();
        screen->update(renderer.delta_time());
        screen->render();

        ScreenResult sr = screen->get_result();
        if (sr.transition) {
            switch (sr.next) {
                case ScreenResult::Next::Quit:
                    running = false; break;
                case ScreenResult::Next::Game:
                    last_names = sr.player_names;
                    screen = std::make_unique<GameScreen>(db, sr.player_names, sr.mode);
                    break;
                case ScreenResult::Next::Results:
                    // GameScreen sets last_trials before transitioning
                    screen = std::make_unique<ResultsScreen>(last_trials, last_names);
                    break;
                case ScreenResult::Next::Analytics:
                    screen = std::make_unique<AnalyticsScreen>(db);
                    break;
                case ScreenResult::Next::Menu:
                default:
                    screen = std::make_unique<MenuScreen>(db);
                    break;
            }
        }

        renderer.end_frame();
    }
    return 0;
}
```

Note: `GameScreen` must store completed trials and expose them before signalling `Next::Results`.
Add `const std::vector<Trial>& get_results() const;` to `GameScreen` and retrieve in main before
switching screens.

---

## Acceptance Criteria
- [ ] `cmake -B build && cmake --build build` succeeds
- [ ] `./build/rtr` opens a 1280×720 window titled "Real-Time Reaction"
- [ ] Menu screen: name input fields work; START disabled when empty
- [ ] Game screen: camera feed displayed (or black panel if no camera — no crash)
- [ ] 3-2-1 countdown overlay visible before first trial
- [ ] Stimulus appears over camera panel during StimulusActive
- [ ] SPACE registers a reaction; RT displayed in HUD immediately after
- [ ] False start shows a red full-panel flash indicator
- [ ] Timeout (2s) records as MISS; shown in results
- [ ] Results screen shows avg, best, worst, miss count
- [ ] PLAY AGAIN and MAIN MENU buttons navigate correctly
- [ ] Window closes cleanly (no segfault, no leaked GL resources)
- [ ] `cd build && ctest --output-on-failure` still passes

## Out of Scope
- Two-player layout (Phase 3)
- Analytics screen implementation (Phase 4)
- Sound / audio
- Fullscreen toggle
