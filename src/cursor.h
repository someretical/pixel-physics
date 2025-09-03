#ifndef PIXELS_CURSOR_H
#define PIXELS_CURSOR_H

#include "sim.h"

struct Cursor {
    enum class BrushShape {
        Square,
        Circle
    };

    enum class BrushStroke {
        Fill,
        Dotted,
    };

    sim::mat_t selected_material = sim::mat_t::Sand;
    int brush_radius = 10;
    BrushShape brush_shape = BrushShape::Square;
};

#endif // PIXELS_CURSOR_H