#ifndef PIXELS_APPCONTEXT_H
#define PIXELS_APPCONTEXT_H

#include "gpu.h"
#include "physics.h"
#include "util.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <memory>
#include <optional>
#include <spdlog/logger.h>

namespace pixels::core
{
struct AppContext
{
  private:
    struct Token
    {
    };

  public:
    std::unique_ptr<physics::Engine> physics_engine{nullptr};
    std::unique_ptr<gpu::Engine> gpu_engine{nullptr};
    SDL_AppResult app_result{SDL_APP_CONTINUE};

    std::shared_ptr<spdlog::logger> logger;

    static std::optional<std::unique_ptr<AppContext>> Create()
    {
        auto ctx = std::make_unique<AppContext>(Token{});

        ctx->logger = spdlog::stdout_color_mt("AppContext");
        ctx->logger->set_level(spdlog::level::trace);
        ctx->logger->set_pattern(util::SPDLOG_FORMAT);

        ctx->logger->trace("Creating AppContext...");

        auto gpu = gpu::Engine::Create();
        if (not gpu.has_value())
        {
            ctx->logger->error("gpu::Engine::Create failed");
            return std::nullopt;
        }
        ctx->gpu_engine = std::move(*gpu);

        auto physics = physics::Engine::Create();
        if (not physics.has_value())
        {
            ctx->logger->error("physics::Engine::Create failed");
            return std::nullopt;
        }
        ctx->physics_engine = std::move(*physics);

        return ctx;
    }

    // Token is a private member so only the static factory method can construct a new instance
    AppContext(Token) {};
    ~AppContext()
    {
        logger->trace("AppContext destructor called");
    }

    // Delete the Copy Constructor
    AppContext(const AppContext &) = delete;

    // Delete the Copy Assignment Operator
    AppContext &operator=(const AppContext &) = delete;
};

} // namespace pixels::core

#endif // PIXELS_APPCONTEXT_H
