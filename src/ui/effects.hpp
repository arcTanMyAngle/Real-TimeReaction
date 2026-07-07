#pragma once
#include <string>
#include <imgui.h>
#include "colors.hpp"
#include "../core/session.hpp"

// ── Screen flash (single frame color flood) ──────────────────────────────────
struct ScreenFlash {
    float  timer     = 0.0f;
    ImVec4 color     = {};
    float  duration  = 0.10f;
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
    ImVec2      origin   = {};  // start position (center of camera feed)

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
    float opacity = 0.035f;  // subtle — heavier values haze the whole screen
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
    ScreenFlash       flash;
    PerformanceLabel  label;
    NewRecordFlash    record;
    ScanlineOverlay   scanlines;
    CameraBorderPulse border;

    void update(float dt);
    void render_foreground(int w, int h);  // flash + scanlines + record
    // label.render() called separately (needs camera-relative position)
};
