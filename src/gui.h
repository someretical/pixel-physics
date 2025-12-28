#ifndef PIXELS_GUI_H
#define PIXELS_GUI_H

#include "physics.h"

#include <SDL3/SDL_pixels.h>

namespace pixels::gui
{
constexpr static SDL_Color background_colour{93, 88, 90, 255};
constexpr static SDL_Color cursor_colour{255, 255, 255, 64};

struct Cursor
{
    enum class BrushShape
    {
        Square,
        Circle
    };

    enum class BrushStroke
    {
        Fill,
        Dotted,
    };

    physics::material selected_material = physics::Sand;
    int brush_radius = 10;
    constexpr static int min_radius = 1;
    constexpr static int max_radius = 100;
    BrushShape brush_shape = BrushShape::Square;
};
} // namespace pixels::gui

#endif // PIXELS_GUI_H