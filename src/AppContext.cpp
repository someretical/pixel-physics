#include "AppContext.h"
#include "simulator.h"

#include <SDL3/SDL_log.h>

std::optional<AppContext *> AppContext::Create() {
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

    auto window =
        SDL_CreateWindow("Pixel Physics", sim::window_size.x, sim::window_size.y, SDL_WINDOW_KEYBOARD_GRABBED);
    if (not window) {
        return std::nullopt;
    }

    auto renderer = SDL_CreateRenderer(window, nullptr);
    if (not renderer) {
        return std::nullopt;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (not SDL_SetRenderLogicalPresentation(
            renderer,
            sim::level_size.x,
            sim::level_size.y,
            SDL_LOGICAL_PRESENTATION_LETTERBOX
        )) {
        return std::nullopt;
    }

    //SDL_SetRenderVSync(renderer, 1);

    if (not SDL_ShowWindow(window)) {
        return std::nullopt;
    }

    SDL_SetRenderVSync(renderer, 1);

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

    auto ctx{ new AppContext{} };
    ctx->window = window;
    ctx->renderer = renderer;
    ctx->pixel_format = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32);

    {
        auto tex = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET,
            sim::level_size.x,
            sim::level_size.y
        );

        if (not tex) {
            SDL_Fail();
        }
        ctx->texture_layers[textures::Background] = tex;
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_PIXELART);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    }

    {
        auto tex = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            sim::level_size.x,
            sim::level_size.y
        );
        if (not tex) {
            SDL_Fail();
        }
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_PIXELART);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        ctx->texture_layers[textures::Pixels] = tex;
    }

    {
        auto tex = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET,
            sim::level_size.x,
            sim::level_size.y
        );
        if (not tex) {
            SDL_Fail();
        }
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_PIXELART);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        ctx->texture_layers[textures::Cursor] = tex;
    }

    auto physics_thread{ SDL_CreateThread(physics_thread_start, "PhysicsThread", ctx) };
    if (not physics_thread) {
        SDL_Log("Failed to create physics thread");
        SDL_Fail();
    }
    ctx->physics_thread = physics_thread;
    SDL_Log("Physics thread started");

    return std::make_optional(ctx);
}

AppContext::AppContext() {}

extern volatile bool physics_thread_stop_token;

AppContext::~AppContext() {
    physics_thread_stop_token = true;
    SDL_WaitThread(physics_thread, nullptr);

    for (auto &t : texture_layers) {
        SDL_DestroyTexture(t);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}