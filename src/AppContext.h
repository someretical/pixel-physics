#ifndef PIXELS_APPCONTEXT_H
#define PIXELS_APPCONTEXT_H

#include "definitions.h"
#include "util.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <array>

struct Cursor {
    enum class BrushShape {
        Square,
        Circle
    };

    enum class BrushStroke {
        Fill,
        Dotted,
    };

    Material selected_material = Material::Sand;
    int brush_radius = 10;
    BrushShape brush_shape = BrushShape::Square;
};

struct AppContext {
    std::array<std::array<cell_t, level_size.x>, level_size.y> cells;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame_buffer;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
    Random rng;
    Cursor cursor;

    AppContext(SDL_Window *window, SDL_Renderer *renderer) : window(window), renderer(renderer), rng() {
        frame_buffer = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            level_size.x,
            level_size.y
        );
        if (not frame_buffer) {
            SDL_Fail();
        }
        for (auto &row : cells) {
            //            row.fill(cell_t({0, 0}, Material::Sand, true, true));
            row.fill(air_cell);
        }
    }

    ~AppContext() {
        SDL_DestroyTexture(frame_buffer);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }
};

#endif // PIXELS_APPCONTEXT_H
