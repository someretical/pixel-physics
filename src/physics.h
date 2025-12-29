#ifndef PIXELS_PHYSICS_H
#define PIXELS_PHYSICS_H

#include "util.h"

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <array>
#include <barrier>
#include <condition_variable>
#include <glm/detail/type_vec2.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <pcg_random.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
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

class Engine; // forward declaration
void physics_worker_main(std::stop_token st, int id, physics::Engine *engine);
struct Engine
{
  private:
    struct Token
    {
    };

  public:
    std::shared_ptr<spdlog::logger> logger;

    std::atomic<bool> paused{false};
    std::mutex pause_mutex;
    std::condition_variable_any pause_cv;

    constexpr static int NUM_WORKERS = 4;
    std::barrier<> step_barrier{NUM_WORKERS};

    // this must come last so it is the first to be destroyed
    // this ensures all above shared members remain intact
    std::array<std::jthread, NUM_WORKERS> workers;

    static std::optional<std::unique_ptr<Engine>> Create()
    {
        auto engine = std::make_unique<Engine>(Token{});

        engine->logger = spdlog::stdout_color_mt("PhysicsEngine");
        engine->logger->set_level(spdlog::level::trace);
        engine->logger->set_pattern(util::SPDLOG_FORMAT);
        engine->logger->trace("Creating PhysicsEngine...");

        for (int i = 0; i < NUM_WORKERS; ++i)
        {
            engine->logger->trace("Spawning physics thread {}/{}", i + 1, NUM_WORKERS);
            engine->workers[i] = std::jthread(physics_worker_main, i, engine.get());
        }

        return engine;
    }

    Engine(Token)
    {
    }

    ~Engine()
    {
        for (int i = 0; i < NUM_WORKERS; ++i)
        {
            workers[i].request_stop();
            logger->trace("Requested stop for physics thread {}/{}", i + 1, NUM_WORKERS);
        }

        paused.store(false);
        pause_cv.notify_all(); // this actually acquires the lock internally (at least on MSVC)
        logger->trace("Notified all physics threads");

        for (int i = 0; i < NUM_WORKERS; ++i)
        {
            workers[i].join();
            logger->trace("Joined physics thread {}/{}", i + 1, NUM_WORKERS);
        }
    }
};
} // namespace pixels::physics

#endif // PIXELS_PHYSICS_H