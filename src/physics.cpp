#include "physics.h"

#include <SDL3/SDL_timer.h>
#include <spdlog/spdlog.h>
#include <stop_token>

using namespace pixels;

namespace pixels::physics
{
void physics_worker_main(std::stop_token st, int id, physics::Engine *engine)
{
    auto &logger = engine->logger;
    logger->trace("Started...");
    auto &barrier = engine->step_barrier;

    while (true)
    {
        barrier.arrive_and_wait();
        if (st.stop_requested())
        {
            barrier.arrive_and_drop();
            logger->trace("Exiting...");
            break;
        }

        auto begin{SDL_GetTicks()};

        auto elapsed_ticks{SDL_GetTicks() - begin};
        if (elapsed_ticks < TICK_DELAY)
        {
            SDL_Delay(TICK_DELAY - elapsed_ticks);
        }

        logger->trace("Performing physics step (leftover time: {} ms)", TICK_DELAY - elapsed_ticks);

        {
            // have to use unique lock with cv_any.wait
            std::unique_lock lock(engine->pause_mutex);
            engine->pause_cv.wait(lock, st, [&] { return !engine->paused.load(); });
        }
    }
}
} // namespace pixels::physics

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
