#ifndef PIXELS_APPCONTEXT_H
#define PIXELS_APPCONTEXT_H

#include "cursor.h"
#include "partition_table.h"
#include "sim.h"
#include "textures.h"
#include "util.h"
#include "GPUContext.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include <array>
#include <memory>
#include <optional>

struct AppContext {
    SDL_Window *window{};
    std::unique_ptr<GPUContext> gpu_ctx{};
    SDL_Thread *physics_thread{};
    SDL_AppResult app_quit{ SDL_APP_CONTINUE };
    Cursor cursor{};

    sim::chunk_t chunk{};

    static std::optional<std::unique_ptr<AppContext>> Create();

    AppContext();
    ~AppContext();
};

#endif // PIXELS_APPCONTEXT_H
