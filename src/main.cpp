#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_sdl2.h"

#include "data/database.hpp"
#include "data/models.hpp"
#include "ui/colors.hpp"
#include "ui/renderer.hpp"
#include "ui/screen.hpp"
#include "ui/screens/menu_screen.hpp"
#include "ui/screens/game_screen.hpp"
#include "ui/screens/results_screen.hpp"
#include "ui/screens/analytics_screen.hpp"

int main() {
    Database db;
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
            // The Game screen owns the completed trials; grab them before it is
            // replaced so the Results screen can be constructed with them.
            if (auto* gs = dynamic_cast<GameScreen*>(screen.get()))
                last_trials = gs->get_results();

            switch (sr.next) {
                case ScreenResult::Next::Quit:
                    running = false;
                    break;
                case ScreenResult::Next::Game:
                    last_names = sr.player_names;
                    screen = std::make_unique<GameScreen>(db, sr.player_names,
                                                          sr.mode);
                    break;
                case ScreenResult::Next::Results:
                    screen = std::make_unique<ResultsScreen>(last_trials,
                                                             last_names);
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
