#ifndef PIXELS_APPCONTEXT_H
#define PIXELS_APPCONTEXT_H

#include "GPUContext.h"
#include "SDL3/SDL_error.h"
// #include "gpu.h"
#include "gui.h"
#include "physics.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>

namespace pixels::core
{
struct AppContext
{
  private:
    struct Token
    {
    };

  public:
    SDL_Window *window{};
    // std::unique_ptr<GPUContext> gpu_ctx{};
    physics::Engine physics_engine;
    SDL_AppResult app_result{SDL_APP_CONTINUE};
    gui::Cursor cursor{};

    physics::TripleBuffer pixels{};

    static std::optional<std::unique_ptr<AppContext>> Create()
    {
        if (not SDL_Init(SDL_INIT_VIDEO))
        {
            spdlog::error("SDL_Init failed: {}", SDL_GetError());
            return std::nullopt;
        }

        const auto display_id{SDL_GetPrimaryDisplay()};
        if (not display_id)
        {
            spdlog::error("SDL_GetPrimaryDisplay failed");
            return std::nullopt;
        }

        const auto display_scale{SDL_GetDisplayContentScale(display_id)};
        if (display_scale == 0.f)
        {
            spdlog::error("SDL_GetDisplayContentScale failed");
            return std::nullopt;
        }

        auto ctx{std::make_unique<AppContext>(Token{})};
        if (not(ctx->window = SDL_CreateWindow("Pixel Physics", physics::level_bounds.w, physics::level_bounds.h,
                                               SDL_WINDOW_KEYBOARD_GRABBED)))
        {
            spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
            return std::nullopt;
        }

        int width, height, bb_width, bb_height;
        SDL_GetWindowSize(ctx->window, &width, &height);
        SDL_GetWindowSizeInPixels(ctx->window, &bb_width, &bb_height);
        spdlog::debug("Display information:");
        spdlog::debug("  ID: \t{}", display_id);
        spdlog::debug("  Name: \t{}", SDL_GetDisplayName(display_id));
        spdlog::debug("  Scale: \t{}%", display_scale * 100);

        spdlog::debug("Window information:");
        spdlog::debug("  Size: \t{}x{}", width, height);
        spdlog::debug("  Backbuffer size: \t{}x{}", bb_width, bb_height);

        return ctx;
    }

    // Token is a private member so only the static factory method can construct a new instance
    AppContext(Token) : physics_engine(this) {};
    ~AppContext()
    {
    }

    // Delete the Copy Constructor
    AppContext(const AppContext &) = delete;

    // Delete the Copy Assignment Operator
    AppContext &operator=(const AppContext &) = delete;
};

} // namespace pixels::core

#endif // PIXELS_APPCONTEXT_H
