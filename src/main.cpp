#include "AppContext.h"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

using namespace pixels;

constexpr static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    *appstate = nullptr;
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern(util::SPDLOG_FORMAT);

    auto res = pixels::core::AppContext::Create();
    if (not res)
    {
        spdlog::error("AppContext::Create failed");
        return SDL_APP_FAILURE;
    }
    *appstate = res->release();
    core::AppContext *ctx = (core::AppContext *)*appstate;

    // // --- Create SSBO ---
    // glGenBuffers(1, &chunkSSBO);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkSSBO);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_VISIBLE_CHUNKS * sizeof(ChunkDraw), nullptr, GL_DYNAMIC_DRAW);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkSSBO);

    // // --- Create dummy chunks ---
    // worldChunks.resize(MAX_VISIBLE_CHUNKS);
    // createChunkTextureArray(MAX_VISIBLE_CHUNKS);
    // for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++)
    // {
    //     std::vector<uint8_t> data(TEX_SIZE * TEX_SIZE * 4, 0);
    //     for (int y = PAD; y < CHUNK_SIZE + PAD; y++)
    //         for (int x = PAD; x < CHUNK_SIZE + PAD; x++)
    //         {
    //             int idx = (y * TEX_SIZE + x) * 4;
    //             data[idx] = i % 3 + 1; // material
    //             data[idx + 1] = x + y; // temp gradient
    //             data[idx + 2] = 0;
    //             data[idx + 3] = 255;
    //         }
    //     worldChunks[i].layer = i;
    //     uploadChunk(i, data);
    // }

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    core::AppContext *ctx = (core::AppContext *)appstate;
    auto window = ctx->gpu_engine->window;

    ImGui_ImplSDL3_ProcessEvent(event);
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == SDL_GetWindowID(window))
        return SDL_APP_SUCCESS;

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

bool p_open{true};
SDL_AppResult SDL_AppIterate(void *appstate)
{
    core::AppContext *ctx = (core::AppContext *)appstate;
    auto &gpu = ctx->gpu_engine;
    auto window = ctx->gpu_engine->window;

    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    if (width != gpu->wgpu_surface_width || height != gpu->wgpu_surface_height)
        gpu->ResizeSurface(width, height);

    // Check surface status for error. If texture is not optimal, try to reconfigure the surface.
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(gpu->wgpu_surface, &surface_texture);
    if (ImGui_ImplWGPU_IsSurfaceStatusError(surface_texture.status))
    {
        gpu->logger->error("Unrecoverable Surface Texture status={:08x}", (int)surface_texture.status);
        return SDL_APP_FAILURE;
    }
    if (ImGui_ImplWGPU_IsSurfaceStatusSubOptimal(surface_texture.status))
    {
        if (surface_texture.texture)
            wgpuTextureRelease(surface_texture.texture);
        if (width > 0 && height > 0)
            gpu->ResizeSurface(width, height);
        return SDL_APP_CONTINUE;
    }

    // Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);

    // Rendering
    ImGui::Render();

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = gpu->wgpu_surface_configuration.format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
    view_desc.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
    view_desc.aspect = WGPUTextureAspect_All;

    WGPUTextureView texture_view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

    WGPURenderPassColorAttachment color_attachments = {};
    color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachments.loadOp = WGPULoadOp_Clear;
    color_attachments.storeOp = WGPUStoreOp_Store;
    color_attachments.clearValue = {clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                                    clear_color.z * clear_color.w, clear_color.w};
    color_attachments.view = texture_view;

    WGPURenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachments;
    render_pass_desc.depthStencilAttachment = nullptr;

    WGPUCommandEncoderDescriptor enc_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(gpu->wgpu_device, &enc_desc);

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {};
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
    wgpuQueueSubmit(gpu->wgpu_queue, 1, &cmd_buffer);

    wgpuSurfacePresent(gpu->wgpu_surface);
    // Tick needs to be called in Dawn to display validation errors
    wgpuDeviceTick(gpu->wgpu_device);
    wgpuTextureViewRelease(texture_view);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd_buffer);

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    pixels::core::AppContext *ctx = (pixels::core::AppContext *)appstate;
    if (not ctx)
    {
        spdlog::error("SDL_AppQuit called with null appstate and result {}", (int)result);
    }
    else
    {
        ctx->logger->trace("SDL_AppQuit called with result {}", (int)result);
        delete ctx;
    }
}
