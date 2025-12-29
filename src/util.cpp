#include "util.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>

#include <cmath>

namespace pixels::util
{
// bool density_check(const cell &a, const cell &b, physics::rng &rng)
// {
//     auto diff = b.density() - a.density();
//     if (diff >= 1.0f)
//     {
//         return true;
//     }
//     return diff != 0.0f && rng.floats(rng.rng_f) < diff;
// }

std::pair<glm::ivec2, SDL_MouseButtonFlags> get_mouse_info(SDL_Renderer *renderer)
{
    glm::vec2 raw_position{};
    auto mouse_state{SDL_GetMouseState(&raw_position.x, &raw_position.y)};

    glm::vec2 logical_position{};
    SDL_RenderCoordinatesFromWindow(renderer, raw_position.x, raw_position.y, &logical_position.x, &logical_position.y);

    glm::ivec2 mouse_pos{std::lround(logical_position.x), std::lround(logical_position.y)};
    return std::make_pair(mouse_pos, mouse_state);
}
} // namespace pixels::util
