#include "physics.h"
#include "AppContext.h"

#include <SDL3/SDL_timer.h>
#include <spdlog/spdlog.h>
#include <stop_token>

using namespace pixels;

namespace pixels::physics
{
void physics_worker_main(std::stop_token st, int id, pixels::core::AppContext *ctx)
{
    spdlog::trace("Starting...", id);

    auto &step_barrier = ctx->physics_engine.step_barrier;
    auto &pause_mutex = ctx->physics_engine.pause_mutex;
    auto &pause_cv = ctx->physics_engine.pause_cv;
    auto &paused = ctx->physics_engine.paused;

    // make all threads start at the same time
    step_barrier.arrive_and_wait();

    while (!st.stop_requested())
    {
        auto begin{SDL_GetTicks()};

        {
            std::unique_lock lock(pause_mutex);
            pause_cv.wait(lock, [&] { return !paused.load() || st.stop_requested(); });
        }

        if (st.stop_requested())
            break;

        spdlog::trace("Doing fake work...", id);

        auto elapsed_ticks{SDL_GetTicks() - begin};
        if (elapsed_ticks < TICK_DELAY)
        {
            SDL_Delay(static_cast<uint32_t>(TICK_DELAY - elapsed_ticks));
        }
        spdlog::trace("Tick took {}ms", elapsed_ticks);

        step_barrier.arrive_and_wait();
    }

    spdlog::trace("Exiting...", id);
}

// void physics_process_input(cell_matrix_t &write_buf)
// {
//     const auto &[mouse_pos, mouse_state] = util::get_mouse_info(app->renderer);
//     auto radius = app->cursor.brush_radius;
//     auto brush_top_left = physics::svec2{mouse_pos.x - radius, mouse_pos.y - radius};
//     auto brush_bottom_right = physics::svec2{mouse_pos.x + radius, mouse_pos.y + radius};

//     if (mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
//     {
//         for (auto i{brush_top_left.y}; i < brush_bottom_right.y; i++)
//         {
//             for (auto j{brush_top_left.x}; j < brush_bottom_right.x; j++)
//             {
//                 if (check_in_lvl_range({j, i}))
//                 {
//                     auto &cell = write_buf[i][j];
//                     cell.material = app->cursor.selected_material;
//                     cell.velocity.x = 0;
//                     cell.velocity.y = 0;
//                 }
//             }
//         }
//     }
//     else if (mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))
//     {
//         for (auto i{brush_top_left.y}; i < brush_bottom_right.y; i++)
//         {
//             for (auto j{brush_top_left.x}; j < brush_bottom_right.x; j++)
//             {
//                 if (check_in_lvl_range({j, i}))
//                 {
//                     write_buf[i][j] = air_cell;
//                 }
//             }
//         }
//     }
// }
} // namespace pixels::physics
