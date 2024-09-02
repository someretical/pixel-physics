#ifndef PIXELS_APPCONTEXT_H
#define PIXELS_APPCONTEXT_H

#include "definitions.h"
#include "util.h"

#include <SDL3/SDL.h>
#include <random>

struct AppContext {
    std::array<std::array<cell_t, level_size.x>, level_size.y> cells;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame_buffer;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
    Material selected_material = Material::Sand;
    Random rng;

    AppContext(SDL_Window *window, SDL_Renderer *renderer) : window(window), renderer(renderer), rng() {
        frame_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                         window_size.x,
                                         window_size.y);
        if (not frame_buffer) {
            SDL_Fail();
        }
        for (auto &&row: cells) {
            row.fill(air_cell);
        }
    }

    ~AppContext() {
        SDL_DestroyTexture(frame_buffer);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }
};

#endif //PIXELS_APPCONTEXT_H
