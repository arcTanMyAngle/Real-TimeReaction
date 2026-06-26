#pragma once
#include <memory>
#include <string>
#include <vector>
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
