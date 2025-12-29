#ifndef PIXELS_GPU
#define PIXELS_GPU

#include "physics.h"
#include "util.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <glad/gl.h>
#include <imgui.h>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>

namespace pixels::gpu
{
class Engine
{
  private:
    struct Token
    {
    };

  public:
    SDL_Window *window{nullptr};
    SDL_GLContext gl_ctx{nullptr};

    std::shared_ptr<spdlog::logger> logger;

    static std::optional<std::unique_ptr<Engine>> Create()
    {
        auto gpu = std::make_unique<Engine>(Token{});

        gpu->logger = spdlog::stdout_color_mt("GPUEngine");
        gpu->logger->set_level(spdlog::level::trace);
        gpu->logger->set_pattern(util::SPDLOG_FORMAT);
        gpu->logger->trace("Creating GPUEngine...");

        if (not SDL_Init(SDL_INIT_VIDEO))
        {
            gpu->logger->error("SDL_Init failed: {}", SDL_GetError());
            return std::nullopt;
        }

        const auto display_id{SDL_GetPrimaryDisplay()};
        if (not display_id)
        {
            gpu->logger->error("SDL_GetPrimaryDisplay failed");
            return std::nullopt;
        }

        const auto display_scale{SDL_GetDisplayContentScale(display_id)};
        if (display_scale == 0.f)
        {
            gpu->logger->error("SDL_GetDisplayContentScale failed");
            return std::nullopt;
        }

        // GL 4.6 + GLSL 130
        const char *glsl_version = "#version 130";
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

        // Create window with graphics context
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_WindowFlags window_flags =
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        gpu->window = SDL_CreateWindow("Pixel Physics", (int)(physics::level_bounds.w * display_scale),
                                       (int)(physics::level_bounds.h * display_scale), window_flags);
        if (not gpu->window)
        {
            gpu->logger->error("SDL_CreateWindow failed: {}", SDL_GetError());
            return std::nullopt;
        }
        gpu->gl_ctx = SDL_GL_CreateContext(gpu->window);
        if (gpu->gl_ctx == nullptr)
        {
            gpu->logger->error("SDL_GL_CreateContext failed: {}", SDL_GetError());
            return std::nullopt;
        }
        if (not gladLoadGL(SDL_GL_GetProcAddress))
        {
            gpu->logger->error("gladLoadGL failed");
            return std::nullopt;
        }

        SDL_GL_MakeCurrent(gpu->window, gpu->gl_ctx);
        SDL_GL_SetSwapInterval(1); // Enable vsync
        SDL_SetWindowPosition(gpu->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(gpu->window);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // ImGui::StyleColorsLight();

        // Setup scaling
        ImGuiStyle &style = ImGui::GetStyle();
        style.ScaleAllSizes(display_scale); // Bake a fixed style scale. (until we have a solution for dynamic style
                                            // scaling, changing this requires resetting Style + calling this again)
        style.FontScaleDpi = display_scale; // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this
                                            // unnecessary. We leave both here for documentation purpose)

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForOpenGL(gpu->window, gpu->gl_ctx);
        ImGui_ImplOpenGL3_Init(glsl_version);

        int width, height, bb_width, bb_height;
        SDL_GetWindowSize(gpu->window, &width, &height);
        SDL_GetWindowSizeInPixels(gpu->window, &bb_width, &bb_height);
        gpu->logger->debug("Display information:");
        gpu->logger->debug("  ID: \t{}", display_id);
        gpu->logger->debug("  Name: \t{}", SDL_GetDisplayName(display_id));
        gpu->logger->debug("  Scale: \t{}%", display_scale * 100);

        gpu->logger->debug("Window information:");
        gpu->logger->debug("  Size: \t{}x{}", width, height);
        gpu->logger->debug("  Backbuffer size: \t{}x{}", bb_width, bb_height);
        return gpu;
    }

    // Token is a private member so only the static factory method can construct a new instance
    Engine(Token) {};
    ~Engine()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_GL_DestroyContext(gl_ctx);
        SDL_DestroyWindow(window);
    }
};
} // namespace pixels::gpu

#endif
