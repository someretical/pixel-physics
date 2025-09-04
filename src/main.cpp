#define SDL_MAIN_USE_CALLBACKS

#include "AppContext.h"
#include "gui.h"
#include "sim.h"
#include "simulator.h"
#include "util.h"

// Can't remove this include
#include <SDL3/SDL_main.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <glm/ext/vector_float2.hpp>

#include <utility>

volatile bool physics_thread_stop_token = false;

SDL_AppResult SDL_AppInit(void **appstate, [[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
    auto res{ AppContext::Create() };
    if (not res.has_value()) {
        return SDL_Fail();
    }
    *appstate = res.value().release();

    SDL_Log("Application started successfully!");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    auto *app = (AppContext *)appstate;
    SDL_ConvertEventToRenderCoordinates(app->renderer, event);

    switch (event->type) {
        case SDL_EVENT_MOUSE_WHEEL: {
            const auto mod{ SDL_GetModState() };
            const int d_radius{ (mod & SDL_KMOD_LCTRL) ? 5 : 1 };
            if (event->wheel.y > 0) {
                app->cursor.brush_radius = std::min(app->cursor.brush_radius + d_radius, gui::max_radius);
            } else if (event->wheel.y < 0) {
                app->cursor.brush_radius = std::max(app->cursor.brush_radius - d_radius, gui::min_radius);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            switch (event->button.button) {
                case SDL_BUTTON_MIDDLE: {
                    if (check_in_lvl_range({ static_cast<int>(event->button.x), static_cast<int>(event->button.y) })) {
                        const auto &buf{ app->chunk.buffers.getLatestFrame() };
                        app->cursor.selected_material =
                            buf[static_cast<int>(event->button.y)][static_cast<int>(event->button.x)].material;
                    }
                    break;
                }
            }

            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            switch (event->key.key) {
                case SDLK_1: {
                    app->cursor.selected_material = sim::mat_t::Sand;
                    SDL_Log("Selected material: Sand");
                    break;
                }
                case SDLK_2: {
                    app->cursor.selected_material = sim::mat_t::RedSand;
                    SDL_Log("Selected material: Red Sand");
                    break;
                }
                case SDLK_3: {
                    app->cursor.selected_material = sim::mat_t::Water;
                    SDL_Log("Selected material: Water");
                    break;
                }
                case SDLK_4: {
                    app->cursor.selected_material = sim::mat_t::Oil;
                    SDL_Log("Selected material: Oil");
                    break;
                }
                case SDLK_F11: {
                    SDL_SetWindowFullscreen(app->window, !(SDL_GetWindowFlags(app->window) & SDL_WINDOW_FULLSCREEN));
                    break;
                }
                case SDLK_ESCAPE: {
                    app->app_quit = SDL_APP_SUCCESS;
                    break;
                }
                default: {
                    break;
                }
            }
            break;
        }
        case SDL_EVENT_QUIT: {
            app->app_quit = SDL_APP_SUCCESS;
            break;
        }
        default: {
            break;
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    auto begin{ SDL_GetTicks() };
    auto *app{ (AppContext *)appstate };

    process_rendering(app);

    auto elapsed_ticks = SDL_GetTicks() - begin;
    if (elapsed_ticks < 16) {
        SDL_Delay(static_cast<uint32_t>(16 - elapsed_ticks));
    }

    return app->app_quit;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    delete (AppContext *)appstate;

    SDL_Quit();
    SDL_Log("Application quit %s!", result == SDL_APP_SUCCESS ? "successfully" : "with error");
}