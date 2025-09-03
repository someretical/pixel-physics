#ifndef PIXELS_TEXTURES_H
#define PIXELS_TEXTURES_H

#include <SDL3/SDL_render.h>

#include <array>

namespace textures {
enum Layer : int8_t {
    Background = 0,
    Pixels,
    Cursor,
    LayerCount,
};

using layers_t = std::array<SDL_Texture *, LayerCount>;
} // namespace textures

#endif // PIXELS_TEXTURES_H
