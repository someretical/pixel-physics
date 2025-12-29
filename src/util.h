#ifndef PIXELS_UTIL_H
#define PIXELS_UTIL_H

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <array>
#include <glm/ext/vector_int2.hpp>
#include <pcg_extras.hpp>
#include <pcg_random.hpp>
#include <random>


namespace pixels::util
{
constexpr static const char *SPDLOG_FORMAT = "[%Y-%m-%d %H:%M:%S.%e] [%t] [%n] [%^%l%$] %v";

// auto inline check_x_in_lvl_range(const int x)
// {
//     return x >= 0 and x < level_bounds.w;
// }

// auto inline check_y_in_lvl_range(const int y)
// {
//     return y >= 0 and y < level_bounds.h;
// }

// auto inline check_in_lvl_range(const glm::ivec2 p)
// {
//     return check_x_in_lvl_range(p.x) and check_y_in_lvl_range(p.y);
// }

std::pair<glm::ivec2, SDL_MouseButtonFlags> get_mouse_info(SDL_Renderer *renderer);

struct rng
{
    pcg32 rng_s{pcg_extras::seed_seq_from<std::random_device>{}};
    std::array<std::uniform_int_distribution<short>, 8> shorts{
        std::uniform_int_distribution<short>(0, 1), std::uniform_int_distribution<short>(0, 2),
        std::uniform_int_distribution<short>(0, 3), std::uniform_int_distribution<short>(0, 4),
        std::uniform_int_distribution<short>(0, 5), std::uniform_int_distribution<short>(0, 6),
        std::uniform_int_distribution<short>(0, 7), std::uniform_int_distribution<short>(0, 8),
    };

    pcg32 rng_f{pcg_extras::seed_seq_from<std::random_device>{}};
    std::uniform_real_distribution<float> floats{0.0f, 1.0f};
};

// /*
//  * If a is MORE dense than b, then b has no chance of sinking below a.
//  * if a is less dense than b, we take the difference in their densities (b - a) which should be in the range [0, 1]
//  * and compare it to a random float in the range [0, 1]. If the random float is less than the difference in
//  densities,
//  * then b sinks below a.
//  */
// bool density_check(const cell &a, const cell &b, rng &rng);
} // namespace pixels::util

#endif // PIXELS_UTIL_H
