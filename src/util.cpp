#include "util.h"
#include "definitions.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <cmath>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>
#include <utility>

float density(const cell_t &cell) {
    return material_density[std::to_underlying(cell.material)];
}

bool density_le_chance(const cell_t &a, const cell_t &b, Random &rng) {
    auto diff = density(b) - density(a);
    return diff != 0.f && rng.gen_real() < diff;
}

std::pair<glm::ivec2, SDL_MouseButtonFlags> get_mouse_info(SDL_Renderer *renderer) {
    glm::vec2 raw_position{};
    auto mouse_state{ SDL_GetMouseState(&raw_position.x, &raw_position.y) };

    glm::vec2 logical_position{};
    SDL_RenderCoordinatesFromWindow(renderer, raw_position.x, raw_position.y, &logical_position.x, &logical_position.y);

    glm::ivec2 mouse_pos{ std::lround(logical_position.x), std::lround(logical_position.y) };
    return std::make_pair(mouse_pos, mouse_state);
}

SDL_AppResult SDL_Fail() {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
    return SDL_APP_FAILURE;
}