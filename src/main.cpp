#define SDL_MAIN_USE_CALLBACKS

#include "AppContext.h"
#include "definitions.h"
#include "simulator.h"
#include "util.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

#include <utility>

SDL_AppResult SDL_AppInit(void **appstate, [[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
    if (not SDL_Init(SDL_INIT_VIDEO)) {
        return SDL_Fail();
    }

    auto display_id{SDL_GetPrimaryDisplay()};
    if (not display_id) {
        return SDL_Fail();
    }

    auto display_scale{SDL_GetDisplayContentScale(display_id)};
    if (display_scale == 0.f) {
        return SDL_Fail();
    }

    SDL_Window *window = SDL_CreateWindow("Pixel Physics", window_size.x, window_size.y, SDL_WINDOW_KEYBOARD_GRABBED);
    if (not window) {
        return SDL_Fail();
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (not renderer) {
        return SDL_Fail();
    }
    SDL_SetRenderVSync(renderer, SDL_TRUE);

    if (SDL_ShowWindow(window) == SDL_FALSE) {
        return SDL_Fail();
    }

    {
        int width, height, bbwidth, bbheight;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
        SDL_Log("Display ID:\t%i", display_id);
        SDL_Log("Display scale:\t%f%%", display_scale * 100);
        SDL_Log("Window size:\t%ix%i", width, height);
        SDL_Log("Backbuffer size:\t%ix%i", bbwidth, bbheight);
        if (width != bbwidth) {
            SDL_Log("This is a highdpi environment.");
        }
    }

    *appstate = new AppContext{
            window,
            renderer,
    };

    SDL_Log("Application started successfully!");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, const SDL_Event *event) {
    auto *app = (AppContext *) appstate;

    switch (event->type) {
        case SDL_EVENT_MOUSE_WHEEL: {
            if (event->wheel.y > 0) {
                app->cursor.brush_radius = std::clamp(app->cursor.brush_radius + 1, min_radius, max_radius);
            } else if (event->wheel.y < 0) {
                app->cursor.brush_radius = std::clamp(app->cursor.brush_radius - 1, min_radius, max_radius);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            switch (event->button.button) {
                case SDL_BUTTON_MIDDLE: {
                    auto [mouse_pos, mouse_state] = get_mouse_info();
                    auto lvl_x = std::clamp(static_cast<int>(mouse_pos.x / 2), 0, level_size.x - 1);
                    auto lvl_y = std::clamp(static_cast<int>(mouse_pos.y / 2), 0, level_size.y - 1);

                    if (check_in_lvl_range({lvl_x, lvl_y})) {
                        app->cursor.selected_material = app->cells[lvl_y][lvl_x].material;
                    }
                }
            }

            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            switch (event->key.key) {
                case SDLK_1: {
                    app->cursor.selected_material = Material::Sand;
                    SDL_Log("Selected material: Sand");
                    break;
                }
                case SDLK_2: {
                    app->cursor.selected_material = Material::Water;
                    SDL_Log("Selected material: Water");
                    break;
                }
                case SDLK_3: {
                    app->cursor.selected_material = Material::RedSand;
                    SDL_Log("Selected material: Red Sand");
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
    auto begin{SDL_GetTicks()};
    auto *app = (AppContext *) appstate;

    process_input(app);
    process_physics(app);
    process_rendering(app);

    auto elapsed_ticks = SDL_GetTicks() - begin;
    if (elapsed_ticks < 16) {
        SDL_Delay(16 - elapsed_ticks);
    }

//    SDL_Log("Frame took %i ms", elapsed_ticks);
    return app->app_quit;
}

void SDL_AppQuit(void *appstate) {
    delete (AppContext *) appstate;

    SDL_Quit();
    SDL_Log("Application quit successfully!");
}