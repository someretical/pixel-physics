#ifndef PIXELS_UTIL_H
#define PIXELS_UTIL_H

#include "sim.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <glm/ext/vector_int2.hpp>
#include <pcg_extras.hpp>
#include <pcg_random.hpp>

#include <random>
#include <utility>

auto inline check_x_in_lvl_range(const int x) {
    return x >= 0 and x < sim::level_size.x;
}

auto inline check_y_in_lvl_range(const int y) {
    return y >= 0 and y < sim::level_size.y;
}

auto inline check_in_lvl_range(const glm::ivec2 point) {
    return check_x_in_lvl_range(point.x) and check_y_in_lvl_range(point.y);
}

auto inline colour(const sim::cell_t &cell) {
    return sim::material_info[cell.material].colour;
}

auto inline density(const sim::cell_t &cell) {
    return sim::material_info[cell.material].density;
}

auto inline slipperiness(const sim::cell_t &cell) {
    return sim::material_info[cell.material].friction;
}

std::pair<glm::ivec2, SDL_MouseButtonFlags> get_mouse_info(SDL_Renderer *renderer);

/*
 * If a is MORE dense than b, then b has no chance of sinking below a.
 * if a is less dense than b, we take the difference in their densities (b - a) which should be in the range [0, 1]
 * and compare it to a random float in the range [0, 1]. If the random float is less than the difference in densities,
 * then b sinks below a.
 */
bool density_check(const sim::cell_t &a, const sim::cell_t &b, physics::rng &rng);

SDL_AppResult SDL_Fail();

#endif // PIXELS_UTIL_H
