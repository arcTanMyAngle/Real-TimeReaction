#include "renderer.hpp"
#include "colors.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <stdexcept>
#include <string>

Renderer::Renderer(const std::string& title, int w, int h) {
    init_sdl(title, w, h);
    init_imgui();
    last_ticks_ = SDL_GetPerformanceCounter();
}

Renderer::~Renderer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    if (gl_context_) SDL_GL_DeleteContext(gl_context_);
    if (window_)     SDL_DestroyWindow(window_);
    SDL_Quit();
}

void Renderer::init_sdl(const std::string& title, int w, int h) {
    // main() handles its own entry point (SDL_MAIN_HANDLED), so tell SDL.
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window_ = SDL_CreateWindow(
        title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window_)
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_)
        throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());

    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);  // vsync

    // GLEW must be initialised before any ImGui OpenGL calls.
    glewExperimental = GL_TRUE;
    const GLenum glew_status = glewInit();
    if (glew_status != GLEW_OK)
        throw std::runtime_error(
            std::string("glewInit failed: ") +
            reinterpret_cast<const char*>(glewGetErrorString(glew_status)));
    glGetError();  // clear the benign INVALID_ENUM some drivers set on glewInit
}

void Renderer::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // don't litter an imgui.ini next to the exe

    apply_global_style();

    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void Renderer::apply_global_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.FrameRounding  = 6.0f;
    s.GrabRounding   = 6.0f;
    s.WindowPadding  = ImVec2(24.0f, 24.0f);
    s.ItemSpacing    = ImVec2(12.0f, 12.0f);
    s.FramePadding   = ImVec2(12.0f, 8.0f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]      = Colors::BG;
    c[ImGuiCol_ChildBg]       = Colors::PANEL;
    c[ImGuiCol_PopupBg]       = Colors::PANEL;
    c[ImGuiCol_Text]          = Colors::TEXT;
    c[ImGuiCol_TextDisabled]  = Colors::MUTED;
    c[ImGuiCol_FrameBg]       = Colors::PANEL;
    c[ImGuiCol_FrameBgHovered]= ImVec4(0.16f, 0.16f, 0.21f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.26f, 1.0f);
    c[ImGuiCol_Button]        = Colors::PANEL;
    c[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.20f, 0.26f, 1.0f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.24f, 0.24f, 0.30f, 1.0f);
    c[ImGuiCol_Border]        = ImVec4(0.20f, 0.20f, 0.26f, 1.0f);

    ImGui::GetIO().FontGlobalScale = 1.15f;
}

void Renderer::begin_frame() {
    const Uint64 now  = SDL_GetPerformanceCounter();
    const Uint64 freq = SDL_GetPerformanceFrequency();
    dt_ = (last_ticks_ > 0)
              ? static_cast<float>(static_cast<double>(now - last_ticks_) / freq)
              : 0.0f;
    last_ticks_ = now;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Renderer::end_frame() {
    ImGui::Render();
    int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(window_, &dw, &dh);
    glViewport(0, 0, dw, dh);
    glClearColor(Colors::BG.x, Colors::BG.y, Colors::BG.z, Colors::BG.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}
