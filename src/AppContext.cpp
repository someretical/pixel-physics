#include "AppContext.h"
#include "simulator.h"

#include <SDL3/SDL_log.h>

std::optional<std::unique_ptr<AppContext>> AppContext::Create() {
    auto ctx{ std::make_unique<AppContext>() };

    if (not SDL_Init(SDL_INIT_VIDEO)) {
        return std::nullopt;
    }

    const auto display_id{ SDL_GetPrimaryDisplay() };
    if (not display_id) {
        return std::nullopt;
    }

    const auto display_scale{ SDL_GetDisplayContentScale(display_id) };
    if (display_scale == 0.f) {
        return std::nullopt;
    }

    if (not(ctx->window =
                SDL_CreateWindow("Pixel Physics", sim::window_size.x, sim::window_size.y, SDL_WINDOW_KEYBOARD_GRABBED)
        )) {
        return std::nullopt;
    }

    auto gpu_device{ GPUContext::Create(ctx->window) };
    if (not gpu_device) {
        return std::nullopt;
    }
    ctx->gpu_device = std::move(gpu_device.value());

    if (not SDL_ShowWindow(ctx->window)) {
        return std::nullopt;
    }

    int width, height, bbwidth, bbheight;
    SDL_GetWindowSize(ctx->window, &width, &height);
    SDL_GetWindowSizeInPixels(ctx->window, &bbwidth, &bbheight);
    SDL_Log("Display ID:\t%i", display_id);
    SDL_Log("Display scale:\t%f%%", display_scale * 100);
    SDL_Log("Window size:\t%ix%i", width, height);
    SDL_Log("Backbuffer size:\t%ix%i", bbwidth, bbheight);
    if (width != bbwidth) {
        SDL_Log("This is a highdpi environment.");
    }
    auto physics_thread{ SDL_CreateThread(physics_thread_start, "PhysicsThread", ctx.get()) };
    if (not physics_thread) {
        SDL_Log("Failed to create physics thread");
        SDL_Fail();
    }
    ctx->physics_thread = physics_thread;
    SDL_Log("Physics thread started");

    return std::make_optional(std::move(ctx));
}

AppContext::AppContext() {}

extern volatile bool physics_thread_stop_token;

AppContext::~AppContext() {
    physics_thread_stop_token = true;
    SDL_WaitThread(physics_thread, nullptr);

    if (window)
        SDL_DestroyWindow(window);
}
