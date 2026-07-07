#pragma once
#include <memory>
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include "imgui.h"

// Begin a chrome-less window that fills the live OS window (so the UI tracks
// resizes instead of a hard-coded 1280x720). Pass scrollable=true for content
// screens that can exceed the window height, so a scrollbar appears and every
// control stays reachable.
inline void begin_fullscreen(const char* id, bool scrollable = false) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!scrollable)
        flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin(id, nullptr, flags);
}

// Forward declare to avoid including session headers here
class Database;

struct ScreenResult {
    bool        transition = false;
    // populated by the leaving screen; main.cpp reads and acts on it
    enum class Next { Stay, Menu, Game, Results, Analytics, Quit } next = Next::Stay;
    // payload for Game screen construction
    std::vector<std::string> player_names;
    std::string mode;        // "single" | "two_player"
    std::string game_mode = "classic";  // "classic"|"race"|"blitz"|"survival"
};

class IScreen {
public:
    virtual ~IScreen() = default;
    virtual void handle_event(const SDL_Event& e) = 0;
    virtual void update(float dt) = 0;
    virtual void render() = 0;
    virtual ScreenResult get_result() const = 0;
};
