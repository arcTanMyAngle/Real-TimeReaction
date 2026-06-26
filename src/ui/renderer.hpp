#pragma once
#include <GL/glew.h>
#include <SDL2/SDL.h>
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
