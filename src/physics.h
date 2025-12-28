#ifndef PIXELS_PHYSICS_H
#define PIXELS_PHYSICS_H

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <array>
#include <barrier>
#include <condition_variable>
#include <glm/detail/type_vec2.hpp>
#include <mutex>
#include <pcg_random.hpp>
#include <random>
#include <spdlog/spdlog.h>
#include <stop_token>
#include <thread>

namespace pixels::core
{
class AppContext; // forward declaration
}

namespace pixels::physics
{
constexpr static SDL_Rect level_bounds{0, 0, 480, 360};
constexpr static uint32_t TPS = 1; // ticks per second
constexpr static uint32_t TICK_DELAY = 1000 / TPS;

constexpr static int sx_min{0};
constexpr static int sx_max{7};
constexpr static int sy_min{0};
constexpr static int sy_max{7};
constexpr static int a{3};
constexpr static int b{7};
constexpr static int k{16};

using svec2 = glm::tvec2<short, glm::packed_highp>;

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

enum material : uint8_t
{
    Air = 0,
    Sand,
    RedSand,
    Water,
    Oil,
    END_MARKER,
};

struct material_props
{
    SDL_Color colour;
    float density;
    int friction;
    bool moveable;
};

constexpr static std::array material_info{
    material_props{SDL_Color{0, 0, 0, 0}, 1.0f, 0, true},         // Air
    material_props{SDL_Color{236, 196, 131, 255}, 6.0f, 0, true}, // Sand
    material_props{SDL_Color{160, 82, 89, 255}, 6.5f, 0, true},   // RedSand
    material_props{SDL_Color{101, 192, 220, 255}, 3.0f, 1, true}, // Water
    material_props{SDL_Color{161, 136, 127, 255}, 2.5f, 1, true}, // Oil
};

struct alignas(4) cell
{
    uint8_t type;
    int8_t temperature;
    uint8_t variation;
    bool active : 1;
    uint8_t unused : 7;

    inline auto colour() const
    {
        return material_info[this->type].colour;
    }

    inline auto density() const
    {
        return material_info[this->type].density;
    }

    inline auto friction() const
    {
        return material_info[this->type].friction;
    }

    inline auto moveable() const
    {
        return material_info[this->type].moveable;
    }
};
static_assert(sizeof(cell) == 4);

constexpr static cell air_cell{Air, 0, 0, 0};

using cell_row_t = std::array<cell, level_bounds.w>;
using cell_matrix_t = std::array<cell_row_t, level_bounds.h>;

class TripleBuffer
{
  private:
    std::array<cell_matrix_t, 3> buffers{};
    std::atomic<int> write_index{0};
    std::atomic<int> read_index{1};
    std::atomic<int> latest_index{2};

  public:
    // Writer thread
    inline auto &getWriteBuffer()
    {
        return buffers[write_index.load()];
    }

    inline void publishFrame()
    {
        int write = write_index.load();
        int latest = latest_index.load();

        // Swap write and latest
        write_index.store(latest);
        latest_index.store(write);
    }

    // GUI thread - always gets most recent complete frame
    inline const auto &getLatestFrame()
    {
        int latest = latest_index.load();
        int read = read_index.load();

        // Swap read and latest if there's a newer frame
        if (latest != read)
        {
            read_index.store(latest);
            latest_index.store(read);
        }

        return buffers[read_index.load()];
    }
};

void physics_worker_main(std::stop_token st, int id, core::AppContext *ctx);
struct Engine
{

    std::atomic<bool> paused{false};
    std::mutex pause_mutex;
    std::condition_variable_any pause_cv;

    constexpr static int NUM_WORKERS = 1;
    std::array<std::jthread, NUM_WORKERS> workers;
    std::barrier<> step_barrier{NUM_WORKERS};

    Engine(core::AppContext *ctx)
    {
        spdlog::trace("Spawning {} physics worker threads", NUM_WORKERS);
        for (int i = 0; i < NUM_WORKERS; ++i)
        {
            workers[i] = std::jthread(physics_worker_main, i, ctx);
            spdlog::trace("  Spawned thread {}", i);
        }
    }

    ~Engine()
    {
        for (int i = 0; i < NUM_WORKERS; ++i)
        {
            workers[i].request_stop();
        }
    }
};
} // namespace pixels::physics

#endif // PIXELS_PHYSICS_H