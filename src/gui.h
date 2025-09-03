#ifndef PIXELS_GUI_H
#define PIXELS_GUI_H

#include <SDL3/SDL_pixels.h>

namespace gui {
constexpr static SDL_Color background_colour{ 93, 88, 90, 255 };
constexpr static SDL_Color cursor_colour{ 255, 255, 255, 64 };

constexpr static int min_radius = 1;
constexpr static int max_radius = 100;
} // namespace gui

#endif // PIXELS_GUI_H