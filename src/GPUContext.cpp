#include "GPUContext.h"
#include "sim.h"

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlgpu3.h>
#include <glm/vec4.hpp>
#include <imgui.h>

std::optional<std::unique_ptr<GPUContext>> GPUContext::Create(SDL_Window *window) {
    // initialize SDL_gpu
    auto gpu_ctx{ std::make_unique<GPUContext>() };
    gpu_ctx->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "opengl");
    auto device{ gpu_ctx->device };
    if (not device) {
        return std::nullopt;
    }

    if (not SDL_ClaimWindowForGPUDevice(device, window)) {
        return std::nullopt;
    }

    // textures
    constexpr SDL_GPUTextureCreateInfo pixels_tex_info{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .width = sim::level_size.x,
        .height = sim::level_size.y,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };

    if (not(gpu_ctx->pixels_texture = SDL_CreateGPUTexture(device, &pixels_tex_info))) {
        return std::nullopt;
    }

    if (not(gpu_ctx->gui_texture = SDL_CreateGPUTexture(device, &pixels_tex_info))) {
        return std::nullopt;
    }

    // samplers
    constexpr SDL_GPUSamplerCreateInfo nearest_sampler_info{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };

    if (not(gpu_ctx->nearest_sampler = SDL_CreateGPUSampler(device, &nearest_sampler_info))) {
        return std::nullopt;
    }

    // vertex buffers
    constexpr glm::vec4 quad_vertices[]{
        { -1.0f, -1.0f, 0.0f, 1.0f }, // Bottom-left
        { 1.0f, -1.0f, 1.0f, 1.0f },  // Bottom-right
        { -1.0f, 1.0f, 0.0f, 0.0f },  // Top-left
        { 1.0f, 1.0f, 1.0f, 0.0f }    // Top-right
    };
    constexpr SDL_GPUBufferCreateInfo buffer_info{
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(quad_vertices),
    };

    if (not(gpu_ctx->vertex_buffer = SDL_CreateGPUBuffer(device, &buffer_info))) {
        return std::nullopt;
    }

    constexpr SDL_GPUTransferBufferCreateInfo transfer_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = sizeof(quad_vertices),
    };

    // the conchequences of using a C library in C++...
    auto transfer_deleter = [device](SDL_GPUTransferBuffer *ptr) { SDL_ReleaseGPUTransferBuffer(device, ptr); };
    std::unique_ptr<SDL_GPUTransferBuffer, decltype(transfer_deleter)> transfer_buffer{
        SDL_CreateGPUTransferBuffer(device, &transfer_info),
        transfer_deleter,
    };
    if (not transfer_buffer) {
        return std::nullopt;
    }

    auto data{ SDL_MapGPUTransferBuffer(device, transfer_buffer.get(), false) };
    if (not data) {
        return std::nullopt;
    }

    memcpy(data, quad_vertices, sizeof(quad_vertices));
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer.get());

    auto cmd_buf{ SDL_AcquireGPUCommandBuffer(device) };
    if (not cmd_buf) {
        return std::nullopt;
    }

    auto copy_pass{ SDL_BeginGPUCopyPass(cmd_buf) };

    SDL_GPUTransferBufferLocation src{
        .transfer_buffer = transfer_buffer.get(),
        .offset = 0,
    };
    SDL_GPUBufferRegion dst{
        .buffer = gpu_ctx->vertex_buffer,
        .offset = 0,
        .size = sizeof(quad_vertices),
    };

    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
    SDL_EndGPUCopyPass(copy_pass);
    if (not SDL_SubmitGPUCommandBuffer(cmd_buf)) {
        return std::nullopt;
    }

    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer.get());

    // imgui
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // IF using Docking Branch

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;                     // Only used in multi-viewports mode.
    init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR; // Only used in multi-viewports mode.
    init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
    ImGui_ImplSDLGPU3_Init(&init_info);

    return std::make_optional(std::move(gpu_ctx));
}

GPUContext::~GPUContext() {
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

    if (pixels_texture)
        SDL_ReleaseGPUTexture(device, pixels_texture);
    if (gui_texture)
        SDL_ReleaseGPUTexture(device, gui_texture);
    if (vertex_buffer)
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
    if (nearest_sampler)
        SDL_ReleaseGPUSampler(device, nearest_sampler);
    if (pixel_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, pixel_pipeline);
    if (gui_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, gui_pipeline);
    if (device)
        SDL_DestroyGPUDevice(device);
}
