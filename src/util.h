#ifndef PIXELS_UTIL_H
#define PIXELS_UTIL_H

#include "definitions.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <glm/ext/vector_int2.hpp>
#include <pcg_extras.hpp>
#include <pcg_random.hpp>
#include <random>
#include <utility>

struct Random {
private:
    pcg32 rng;
    std::uniform_int_distribution<int> uni_int;
    std::uniform_real_distribution<float> uni_real;

public:
    Random() {
        pcg_extras::seed_seq_from<std::random_device> seed_source;
        rng.seed(seed_source);

        uni_int = std::uniform_int_distribution<int>(0, 1);
        uni_real = std::uniform_real_distribution<float>(0.f, 1.f);
    }
    ~Random() = default;

    Random(Random const &) = delete;
    void operator=(Random const &x) = delete;

    auto gen_int() {
        return uni_int(rng);
    }

    auto gen_real() {
        return uni_real(rng);
    }
};

auto inline check_x_in_lvl_range(const int x) {
    return x >= 0 and x < level_size.x;
}

auto inline check_y_in_lvl_range(const int y) {
    return y >= 0 and y < level_size.y;
}

auto inline check_in_lvl_range(const glm::ivec2 point) {
    return check_x_in_lvl_range(point.x) and check_y_in_lvl_range(point.y);
}

auto inline colour(const cell_t &cell) {
    return material_colour[std::to_underlying(cell.material)];
}

float inline density(const cell_t &cell);

auto inline slipperiness(const cell_t &cell) {
    return material_slipperiness[std::to_underlying(cell.material)];
}

std::pair<glm::ivec2, SDL_MouseButtonFlags> get_mouse_info(SDL_Renderer *renderer);

/*
 * If a is MORE dense than b, then b has no chance of sinking below a.
 * if a is less dense than b, we take the difference in their densities (b - a) which should be in the range [0, 1]
 * and compare it to a random float in the range [0, 1]. If the random float is less than the difference in densities,
 * then b sinks below a.
 */
bool density_le_chance(const cell_t &a, const cell_t &b, Random &rng);

SDL_AppResult SDL_Fail();

#endif // PIXELS_UTIL_H
