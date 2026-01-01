#ifndef PIXELS_GPU
#define PIXELS_GPU

#include "physics.h"
#include "util.h"
#include "webgpu/webgpu.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <webgpu/webgpu_cpp.h>

#if defined(SDL_PLATFORM_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif

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

    WGPUInstance wgpu_instance{nullptr};
    WGPUDevice wgpu_device{nullptr};
    WGPUSurface wgpu_surface{nullptr};
    WGPUQueue wgpu_queue{nullptr};
    WGPUSurfaceConfiguration wgpu_surface_configuration{};
    int wgpu_surface_width{1280};
    int wgpu_surface_height{800};

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

        SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        gpu->window = SDL_CreateWindow("Pixel Physics", (int)(physics::level_bounds.w * display_scale),
                                       (int)(physics::level_bounds.h * display_scale), window_flags);
        if (not gpu->window)
        {
            gpu->logger->error("SDL_CreateWindow failed: {}", SDL_GetError());
            return std::nullopt;
        }

        WGPUTextureFormat preferred_fmt = WGPUTextureFormat_Undefined; // acquired from SurfaceCapabilities

        // Google DAWN backend: Adapter and Device acquisition, Surface creation
        wgpu::InstanceDescriptor instance_desc = {};
        static constexpr wgpu::InstanceFeatureName timedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
        instance_desc.requiredFeatureCount = 1;
        instance_desc.requiredFeatures = &timedWaitAny;
        wgpu::Instance instance = wgpu::CreateInstance(&instance_desc);

        wgpu::Adapter adapter = RequestAdapter(gpu->logger, instance);
        ImGui_ImplWGPU_DebugPrintAdapterInfo(adapter.Get());

        gpu->wgpu_device = RequestDevice(gpu->logger, instance, adapter);

        // Create the surface.

        wgpu::Surface surface = CreateWGPUSurface(instance.Get(), gpu->window);
        if (!surface)
        {
            gpu->logger->error("CreateWGPUSurface failed");
            return std::nullopt;
        }

        // Moving Dawn objects into WGPU handles
        gpu->wgpu_instance = instance.MoveToCHandle();
        gpu->wgpu_surface = surface.MoveToCHandle();

        WGPUSurfaceCapabilities surface_capabilities = {};
        wgpuSurfaceGetCapabilities(gpu->wgpu_surface, adapter.Get(), &surface_capabilities);

        preferred_fmt = surface_capabilities.formats[0];

        // WGPU backend: Adapter and Device acquisition, Surface creation
        gpu->wgpu_surface_configuration.presentMode = WGPUPresentMode_Mailbox;
        gpu->wgpu_surface_configuration.alphaMode = WGPUCompositeAlphaMode_Auto;
        gpu->wgpu_surface_configuration.usage = WGPUTextureUsage_RenderAttachment;
        gpu->wgpu_surface_configuration.width = gpu->wgpu_surface_width;
        gpu->wgpu_surface_configuration.height = gpu->wgpu_surface_height;
        gpu->wgpu_surface_configuration.device = gpu->wgpu_device;
        gpu->wgpu_surface_configuration.format = preferred_fmt;

        wgpuSurfaceConfigure(gpu->wgpu_surface, &gpu->wgpu_surface_configuration);
        gpu->wgpu_queue = wgpuDeviceGetQueue(gpu->wgpu_device);

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
        ImGui_ImplSDL3_InitForOther(gpu->window);

        ImGui_ImplWGPU_InitInfo init_info;
        init_info.Device = gpu->wgpu_device;
        init_info.NumFramesInFlight = 3;
        init_info.RenderTargetFormat = gpu->wgpu_surface_configuration.format;
        init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
        ImGui_ImplWGPU_Init(&init_info);

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
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        wgpuSurfaceUnconfigure(wgpu_surface);
        wgpuSurfaceRelease(wgpu_surface);
        wgpuQueueRelease(wgpu_queue);
        wgpuDeviceRelease(wgpu_device);
        wgpuInstanceRelease(wgpu_instance);

        SDL_DestroyWindow(window);
    }

    void ResizeSurface(int width, int height)
    {
        wgpu_surface_configuration.width = wgpu_surface_width = width;
        wgpu_surface_configuration.height = wgpu_surface_height = height;
        wgpuSurfaceConfigure(wgpu_surface, (WGPUSurfaceConfiguration *)&wgpu_surface_configuration);
    }

    static WGPUAdapter RequestAdapter(std::shared_ptr<spdlog::logger> logger, wgpu::Instance &instance)
    {
        wgpu::Adapter acquired_adapter;
        wgpu::RequestAdapterOptions adapter_options;
        auto onRequestAdapter = [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
                                    wgpu::StringView message) {
            if (status != wgpu::RequestAdapterStatus::Success)
            {
                logger->error("Failed to get an adapter: {}", message.data);
                return;
            }
            acquired_adapter = std::move(adapter);
        };

        // Synchronously (wait until) acquire Adapter
        wgpu::Future waitAdapterFunc{
            instance.RequestAdapter(&adapter_options, wgpu::CallbackMode::WaitAnyOnly, onRequestAdapter)};
        wgpu::WaitStatus waitStatusAdapter = instance.WaitAny(waitAdapterFunc, UINT64_MAX);
        IM_ASSERT(acquired_adapter != nullptr && waitStatusAdapter == wgpu::WaitStatus::Success &&
                  "Error on Adapter request");
        return acquired_adapter.MoveToCHandle();
    }

    static WGPUDevice RequestDevice(std::shared_ptr<spdlog::logger> logger, wgpu::Instance &instance,
                                    wgpu::Adapter &adapter)
    {
        // Set device callback functions
        wgpu::DeviceDescriptor device_desc;
        device_desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous, [](const wgpu::Device &,
                                                                                   wgpu::DeviceLostReason type,
                                                                                   wgpu::StringView msg) {
            spdlog::error("{} error: {}", ImGui_ImplWGPU_GetDeviceLostReasonName((WGPUDeviceLostReason)type), msg.data);
        });
        device_desc.SetUncapturedErrorCallback([](const wgpu::Device &, wgpu::ErrorType type, wgpu::StringView msg) {
            spdlog::error("{} error: {}", ImGui_ImplWGPU_GetErrorTypeName((WGPUErrorType)type), msg.data);
        });

        wgpu::Device acquired_device;
        auto onRequestDevice = [&](wgpu::RequestDeviceStatus status, wgpu::Device local_device,
                                   wgpu::StringView message) {
            if (status != wgpu::RequestDeviceStatus::Success)
            {
                logger->error("Failed to get an device: {}", message.data);
                return;
            }
            acquired_device = std::move(local_device);
        };

        // Synchronously (wait until) get Device
        wgpu::Future waitDeviceFunc{
            adapter.RequestDevice(&device_desc, wgpu::CallbackMode::WaitAnyOnly, onRequestDevice)};
        wgpu::WaitStatus waitStatusDevice = instance.WaitAny(waitDeviceFunc, UINT64_MAX);
        IM_ASSERT(acquired_device != nullptr && waitStatusDevice == wgpu::WaitStatus::Success &&
                  "Error on Device request");
        return acquired_device.MoveToCHandle();
    }

    static WGPUSurface CreateWGPUSurface(const WGPUInstance &instance, SDL_Window *window)
    {
        SDL_PropertiesID propertiesID = SDL_GetWindowProperties(window);

        ImGui_ImplWGPU_CreateSurfaceInfo create_info = {};
        create_info.Instance = instance;
#if defined(SDL_PLATFORM_MACOS)
        {
            create_info.System = "cocoa";
            create_info.RawWindow =
                (void *)SDL_GetPointerProperty(propertiesID, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
            return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
        }
#elif defined(SDL_PLATFORM_LINUX)
        if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0)
        {
            create_info.System = "wayland";
            create_info.RawDisplay =
                (void *)SDL_GetPointerProperty(propertiesID, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
            create_info.RawSurface =
                (void *)SDL_GetPointerProperty(propertiesID, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
            return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
        }
        else if (!SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11"))
        {
            create_info.System = "x11";
            create_info.RawWindow = (void *)SDL_GetNumberProperty(propertiesID, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
            create_info.RawDisplay =
                (void *)SDL_GetPointerProperty(propertiesID, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
            return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
        }
#elif defined(SDL_PLATFORM_WIN32)
        {
            create_info.System = "win32";
            create_info.RawWindow =
                (void *)SDL_GetPointerProperty(propertiesID, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
            create_info.RawInstance = (void *)::GetModuleHandle(NULL);
            return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
        }
#else
#error "Unsupported WebGPU native platform!"
#endif
        return nullptr;
    }
};
} // namespace pixels::gpu

#endif
