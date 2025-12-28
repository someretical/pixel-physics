#ifndef PIXELS_GPUCONTEXT_H
#define PIXELS_GPUCONTEXT_H

#include <SDL3/SDL_gpu.h>

#include <memory>
#include <optional>

// struct GPUContext {
//     SDL_GPUDevice *device{};
//     SDL_GPUTexture *pixels_texture{};
//     SDL_GPUTexture *gui_texture{};
//     SDL_GPUGraphicsPipeline *pixel_pipeline;
//     SDL_GPUGraphicsPipeline *gui_pipeline;
//     SDL_GPUBuffer *vertex_buffer;
//     SDL_GPUSampler *nearest_sampler; // for pixels and gui textures (sharp pixels when upscaled)

//     static std::optional<std::unique_ptr<GPUContext>> Create(SDL_Window *window);

//     GPUContext() = default;
//     ~GPUContext();
// };

#endif // PIXELS_GPUCONTEXT_H