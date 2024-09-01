#ifndef PIXELS_UTIL_H
#define PIXELS_UTIL_H

#include "definitions.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <random>

struct Random {
private:
    std::mt19937 rng;
    std::uniform_int_distribution<int> uni;
    std::uniform_real_distribution<float> uni_real;

public:
    Random() : rng(std::random_device{}()), uni(0, 1), uni_real(0.0f, 1.0f) {}
    ~Random() = default;

    Random(Random const &) = delete;
    void operator=(Random const &x) = delete;

    auto gen_int() {
        return uni(rng);
    }

    auto gen_real() {
        return uni_real(rng);
    }
};

auto inline check_x_in_range(const int x) {
    return x >= 0 and x < level_size.x;
}

auto inline check_y_in_range(const int y) {
    return y >= 0 and y < level_size.y;
}

auto inline check_in_range(const glm::ivec2 point) {
    return check_x_in_range(point.x) and check_y_in_range(point.y);
}

auto inline colour(const cell_t &cell) {
    return material_colour[std::to_underlying(cell.material)];
}

auto inline density(const cell_t &cell) {
    return material_density[std::to_underlying(cell.material)];
}

auto inline viscosity(const cell_t &cell) {
    return material_viscosity[std::to_underlying(cell.material)];
}

/*
 * If a is MORE dense than b, then b has no chance of sinking below a.
 * if a is less dense than b, we take the difference in their densities (b - a) which should be in the range [0, 1]
 * and compare it to a random float in the range [0, 1]. If the random float is less than the difference in densities,
 * then b sinks below a.
 */
auto inline density_le_chance(const cell_t &a, const cell_t &b, Random &rng) {
    auto diff = density(b) - density(a);
    return diff && rng.gen_real() < diff;
}

SDL_AppResult SDL_Fail();

#endif //PIXELS_UTIL_H
