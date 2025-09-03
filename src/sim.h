#ifndef PIXELS_SIM_H
#define PIXELS_SIM_H

#include "physics.h"

#include <SDL3/SDL_pixels.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>

#include <array>
#include <atomic>

namespace sim {
constexpr static glm::ivec2 level_size{ 320, 240 };
constexpr static auto window_size{ level_size * 2 };

enum mat_t : int8_t {
    Air = 0,
    Sand,
    RedSand,
    Water,
    Oil,
    END_MARKER,
};

struct mat_info_t {
    SDL_Color colour;
    float density;
    int friction;
    bool moveable;
};

constexpr static std::array material_info{
    mat_info_t{ SDL_Color{ 0, 0, 0, 0 }, 1.0f, 0, true },         // Air
    mat_info_t{ SDL_Color{ 236, 196, 131, 255 }, 6.0f, 0, true }, // Sand
    mat_info_t{ SDL_Color{ 160, 82, 89, 255 }, 6.5f, 0, true },   // RedSand
    mat_info_t{ SDL_Color{ 101, 192, 220, 255 }, 3.0f, 1, true }, // Water
    mat_info_t{ SDL_Color{ 161, 136, 127, 255 }, 2.5f, 1, true }, // Oil
};

constexpr static int g = 1;
constexpr static int max_y_velocity = 32;
constexpr static int min_y_velocity = -32;

struct cell_t {
    glm::vec<2, short, glm::defaultp> velocity; // 4 bytes
    mat_t material;                             // 1 byte
    bool updated;                               // 1 byte

    constexpr cell_t(glm::vec2 velocity, mat_t material, bool updated)
        : velocity(velocity), material(material), updated(updated) {}

    constexpr cell_t() : cell_t({ 0, 0 }, mat_t::Air, false) {}

    constexpr ~cell_t() = default;

    inline auto colour() const {
        return material_info[material].colour;
    }

    inline auto density() const {
        return material_info[material].density;
    }

    inline auto friction() const {
        return material_info[material].friction;
    }

    inline auto moveable() const {
        return material_info[material].moveable;
    }
};

constexpr static cell_t air_cell{ { 0, 0 }, mat_t::Air, false };

using cell_row_t = std::array<sim::cell_t, sim::level_size.x>;
using cell_matrix_t = std::array<cell_row_t, sim::level_size.y>;

class TripleBuffer {
private:
    std::array<cell_matrix_t, 3> buffers{};
    std::atomic<int> write_index{ 0 };
    std::atomic<int> read_index{ 1 };
    std::atomic<int> latest_index{ 2 };

public:
    // Writer thread
    inline auto &getWriteBuffer() {
        return buffers[write_index.load()];
    }

    inline void publishFrame() {
        int write = write_index.load();
        int latest = latest_index.load();

        // Swap write and latest
        write_index.store(latest);
        latest_index.store(write);
    }

    // GUI thread - always gets most recent complete frame
    inline const auto &getLatestFrame() {
        int latest = latest_index.load();
        int read = read_index.load();

        // Swap read and latest if there's a newer frame
        if (latest != read) {
            read_index.store(latest);
            latest_index.store(read);
        }

        return buffers[read_index.load()];
    }
};

struct chunk_t {
    TripleBuffer buffers{};
};
} // namespace sim

#endif // PIXELS_SIM_H