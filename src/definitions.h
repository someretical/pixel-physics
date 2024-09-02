#ifndef PIXELS_DEFINITIONS_H
#define PIXELS_DEFINITIONS_H

#include <SDL3/SDL_video.h>
#include <glm/vec2.hpp>
#include <array>
#include <utility>

constexpr static glm::i32vec2 level_size{640, 480};
constexpr static auto window_size{level_size * 2};

enum class Material : int8_t {
    Air = 0,
    Sand = 1,
    Water = 2,
    RedSand = 3,
    END_MARKER,
};

constexpr static std::array material_colour{
        SDL_Color{0, 0, 0, 0},
        SDL_Color{236, 196, 131, 255},
        SDL_Color{101, 192, 220, 255},
        SDL_Color{255, 0, 0, 255},
};

constexpr static std::array material_density{
        0.0f,
        1.8f,
        1.0f,
        1.5f,
};

constexpr static std::array material_slipperiness{
        0,
        1,
        10,
        1,
};

static_assert(material_colour.size() == std::to_underlying(Material::END_MARKER));

constexpr static int g = 1;
constexpr static int max_y_velocity = 8;
constexpr static int min_y_velocity = -8;

constexpr static SDL_Color background_colour{93, 88, 90, 255};

typedef struct cell_t {
    glm::ivec2 velocity;     // 8 bytes
    Material material;      // 1 byte
    bool has_been_updated;  // 1 byte
    bool displaceable;       // 1 byte

    constexpr cell_t(glm::vec2 velocity, Material material, bool has_been_updated, bool displacable)
            : velocity(
            velocity), material(material), has_been_updated(has_been_updated), displaceable(displacable) {}

    constexpr cell_t() : cell_t({0, 0}, Material::Air, true, true) {}

    constexpr ~cell_t() = default;
} cell_t;

constexpr static cell_t air_cell{{0, 0}, Material::Air, true, true};

#endif //PIXELS_DEFINITIONS_H
