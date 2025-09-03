#include "simulator.h"
#include "cursor.h"
#include "gui.h"
#include "static_vector.h"
#include "util.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>
#include <glm/ext/vector_int2.hpp>

#include <cstring>
#include <utility>

extern volatile bool physics_thread_stop_token;

void process_input(AppContext *app, sim::cell_matrix_t &write_buf) {
    const auto &[mouse_pos, mouse_state] = get_mouse_info(app->renderer);
    auto radius = app->cursor.brush_radius;
    auto brush_top_left = physics::svec2{ mouse_pos.x - radius, mouse_pos.y - radius };
    auto brush_bottom_right = physics::svec2{ mouse_pos.x + radius, mouse_pos.y + radius };
    auto brush_top_right = physics::svec2{ mouse_pos.x + radius, mouse_pos.y - radius };
    auto brush_bottom_left = physics::svec2{ mouse_pos.x - radius, mouse_pos.y + radius };

    if (mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) {
        for (auto i{ brush_top_left.y }; i < brush_bottom_right.y; i++) {
            for (auto j{ brush_top_left.x }; j < brush_bottom_right.x; j++) {
                if (check_in_lvl_range({ j, i })) {
                    auto &cell = write_buf[i][j];
                    cell.material = app->cursor.selected_material;
                    cell.velocity.x = 0;
                    cell.velocity.y = 0;
                }
            }
        }
    } else if (mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) {
        for (auto i{ brush_top_left.y }; i < brush_bottom_right.y; i++) {
            for (auto j{ brush_top_left.x }; j < brush_bottom_right.x; j++) {
                if (check_in_lvl_range({ j, i })) {
                    write_buf[i][j] = sim::air_cell;
                }
            }
        }
    }
}

int physics_thread_start(void *data) {
    auto ctx{ (AppContext *)data };

    // We use 16 different colours to partition the grid
    // Choose a = 11 and b = 19 for coprimes to scatter the distribution

    // Every 64 iterations, we reshuffle the order of the colours
    // Within each process_physics call, we randomise the dx and dy offset for each colour to minimize the number of rng
    // calls The shift is between 0 and 15 inclusive

    physics::rng rngs{};

    pcg_extras::seed_seq_from<std::random_device> seed_source_shuffle{};
    pcg32 rng_shuffle{ seed_source_shuffle };

    uint8_t shuffle_counter = 0;
    colour_update_order_t colours{};
    for (size_t i{ 0 }; i < colours.size(); ++i) {
        colours[i] = static_cast<short>(i);
    }

    while (not physics_thread_stop_token) {

        for (const auto colour : colours) {
            auto begin{ SDL_GetTicks() };

            if (shuffle_counter == 0) {
                std::shuffle(colours.begin(), colours.end(), rng_shuffle);
            }

            auto &write_buf{ ctx->chunk.buffers.getWriteBuffer() };
            auto &read_buf{ ctx->chunk.buffers.getLatestFrame() };
            write_buf = read_buf;

            process_input(ctx, write_buf);

            process_physics(ctx, colour, rngs, write_buf);

            ctx->chunk.buffers.publishFrame();

            shuffle_counter++;
            shuffle_counter &= 0x3F; // clamp counter from 0 to 63

            auto elapsed_ticks = SDL_GetTicks() - begin;
            if (elapsed_ticks < 2) {
                SDL_Delay(static_cast<uint32_t>(1 - elapsed_ticks));
            }
            SDL_Log("Frame took %llu ms", elapsed_ticks);
        }
    }

    return 0;
}

inline void switch_statement(
    AppContext *app [[maybe_unused]], physics::rng &rngs, sim::cell_matrix_t &write_buf, const int x, const int y
) {
    auto &cell{ write_buf[y][x] };

    switch (cell.material) {
        case sim::mat_t::END_MARKER:
        case sim::mat_t::Air: {
            break;
        }
        case sim::mat_t::RedSand:
        case sim::mat_t::Sand: {
            if (y == sim::level_size.y - 1) {
                cell.velocity.y = 0;
                break;
            }
            auto &cell下 = write_buf[y + 1][x];
            if (cell下.moveable() && cell下.density() < cell.density() && density_check(cell下, cell, rngs)) {
                std::swap(cell, cell下);
                break;
            }

            static StaticVector<physics::svec2, 2> round1{};
            round1.clear();
            const physics::svec2 左{ x - 1, y };
            const physics::svec2 下左{ x - 1, y + 1 };
            const physics::svec2 右{ x + 1, y };
            const physics::svec2 下右{ x + 1, y + 1 };

            if (左.x >= 0 && 下左.y < sim::level_size.y) {
                auto &cell_左{ write_buf[左.y][左.x] };
                auto &cell_下左{ write_buf[下左.y][下左.x] };
                if (cell_左.moveable() && cell_左.density() < cell.density() && cell_下左.moveable()
                    && cell_下左.density() < cell.density()) {
                    round1.push_back(下左);
                }
            }

            if (右.x < sim::level_size.x && 右.y < sim::level_size.y) {
                auto &cell_右{ write_buf[右.y][右.x] };
                auto &cell_下右{ write_buf[下右.y][下右.x] };
                if (cell_右.moveable() && cell_右.density() < cell.density() && cell_下右.moveable()
                    && cell_下右.density() < cell.density()) {
                    round1.push_back(下右);
                }
            }

            if (round1.size() == 1) {
                auto &dest_c{ write_buf[round1[0].y][round1[0].x] };
                if (density_check(dest_c, cell, rngs)) {
                    std::swap(cell, dest_c);
                }
            } else if (round1.size() == 2) {
                const auto dest{ round1[rngs.shorts[0](rngs.rng_s)] };
                auto &dest_c{ write_buf[dest.y][dest.x] };
                if (density_check(dest_c, cell, rngs)) {
                    std::swap(cell, dest_c);
                }
            }
            break;
        }

        case sim::mat_t::Oil:
        case sim::mat_t::Water: {
            if (y == sim::level_size.y - 1) {
                cell.velocity.y = 0;
                break;
            }
            auto &cell下 = write_buf[y + 1][x];
            if (cell下.moveable() && cell下.density() < cell.density() && density_check(cell下, cell, rngs)) {
                std::swap(cell, cell下);
                break;
            }

            static StaticVector<physics::svec2, 2> round1{};
            round1.clear();
            static StaticVector<physics::svec2, 4> round2{};
            round2.clear();

            const physics::svec2 左{ x - 1, y };
            const physics::svec2 下左{ x - 1, y + 1 };
            const physics::svec2 右{ x + 1, y };
            const physics::svec2 下右{ x + 1, y + 1 };
            bool b_左 = false;
            bool b_右 = false;

            if (左.x >= 0 && 下左.y < sim::level_size.y) {
                auto &cell_左{ write_buf[左.y][左.x] };
                auto &cell_下左{ write_buf[下左.y][下左.x] };
                if (cell_左.moveable() && cell_左.density() < cell.density()) {
                    round2.push_back(左);
                    b_左 = true;
                    if (cell_下左.moveable() && cell_下左.density() < cell.density()) {
                        round1.push_back(下左);
                    }
                }
            }

            if (右.x < sim::level_size.x && 右.y < sim::level_size.y) {
                auto &cell_右{ write_buf[右.y][右.x] };
                auto &cell_下右{ write_buf[下右.y][下右.x] };
                if (cell_右.moveable() && cell_右.density() < cell.density()) {
                    round2.push_back(右);
                    b_右 = true;
                    if (cell_下右.moveable() && cell_下右.density() < cell.density()) {
                        round1.push_back(下右);
                    }
                }
            }

            if (round1.size() == 1) {
                auto &dest_c{ write_buf[round1[0].y][round1[0].x] };
                if (density_check(dest_c, cell, rngs)) {
                    std::swap(cell, dest_c);
                    break;
                }
            } else if (round1.size() == 2) {
                const auto dest{ round1[rngs.shorts[0](rngs.rng_s)] };
                auto &dest_c{ write_buf[dest.y][dest.x] };
                if (density_check(dest_c, cell, rngs)) {
                    std::swap(cell, dest_c);
                    break;
                }
            }

            const physics::svec2 左左{ x - 2, y };
            const physics::svec2 右右{ x + 2, y };

            if (b_左 && 左左.x >= 0) {
                auto &cell_左左{ write_buf[左左.y][左左.x] };
                if (cell_左左.moveable() && cell_左左.density() < cell.density()) {
                    round2.push_back(左左);
                }
            }

            if (b_右 && 右右.x < sim::level_size.x) {
                auto &cell_右右{ write_buf[右右.y][右右.x] };
                if (cell_右右.moveable() && cell_右右.density() < cell.density()) {
                    round2.push_back(右右);
                }
            }

            if (round2.size() == 1) {
                auto dest{ round2[0] };
                auto &dest_c{ write_buf[dest.y][dest.x] };
                if (density_check(dest_c, cell, rngs)) {
                    std::swap(cell, dest_c);
                }
            } else if (round2.size() > 1) {
                const auto dest{ round2[rngs.shorts[round2.size() - 2](rngs.rng_s)] };
                auto &dest_c{ write_buf[dest.y][dest.x] };
                if (density_check(dest_c, cell, rngs)) {
                    std::swap(cell, dest_c);
                }
            }

            break;
        }
    }
}

inline void process_physics(AppContext *app, const int colour, physics::rng &rngs, sim::cell_matrix_t &write_buf) {
    auto sx{ rngs.shorts[7](rngs.rng_s) };
    auto sy{ rngs.shorts[7](rngs.rng_s) };
    const auto cmask{ physics::k - 1 };
    const auto a{ (colour & 1) ? physics::a : physics::b };
    const auto b{ (colour & 1) ? physics::b : physics::a };

    for (auto y{ sy }; y < sim::level_size.y + sy; ++y) {
        auto yval = b * y;
        for (auto x{ sx }; x < sim::level_size.x + sx; ++x) {
            if (((a * x + yval) & cmask) == colour) {
                switch_statement(app, rngs, write_buf, x % sim::level_size.x, y % sim::level_size.y);
            }
        }
    }
}

void render_pixels(AppContext *app);
void render_background(AppContext *app);
void render_cursor(const AppContext *app);

void process_rendering(AppContext *app) {
    render_background(app);
    render_pixels(app);
    render_cursor(app);
    SDL_RenderPresent(app->renderer);
}

void render_pixels(AppContext *app) {
    uint32_t *pixels;
    int pitch;
    SDL_LockTexture(app->texture_layers[textures::Pixels], nullptr, reinterpret_cast<void **>(&pixels), &pitch);

    const auto &buf{ app->chunk.buffers.getLatestFrame() };
    for (auto y{ 0 }; y < sim::level_size.y; y++) {
        for (auto x{ 0 }; x < sim::level_size.x; x++) {
            const auto &cell = buf[y][x];
            const auto &colour = sim::material_info[cell.material].colour;
            const auto data{ SDL_MapRGBA(app->pixel_format, nullptr, colour.r, colour.g, colour.b, colour.a) };

            *(pixels + (y * sim::level_size.x) + x) = data;
        }
    }

    SDL_UnlockTexture(app->texture_layers[textures::Pixels]);
    SDL_RenderTexture(app->renderer, app->texture_layers[textures::Pixels], nullptr, nullptr);
}

void render_background(AppContext *app) {
    SDL_SetRenderTarget(app->renderer, app->texture_layers[textures::Background]);
    SDL_SetRenderDrawColor(
        app->renderer,
        gui::background_colour.r,
        gui::background_colour.g,
        gui::background_colour.b,
        gui::background_colour.a
    );
    SDL_RenderClear(app->renderer);
    SDL_SetRenderTarget(app->renderer, nullptr);
    SDL_RenderTexture(app->renderer, app->texture_layers[textures::Background], nullptr, nullptr);
}

void render_cursor(const AppContext *app) {
    const auto &[mouse_pos, mouse_state] = get_mouse_info(app->renderer);
    auto radius = app->cursor.brush_radius;

    SDL_FRect cursor{
        static_cast<float>(mouse_pos.x - radius),
        static_cast<float>(mouse_pos.y - radius),
        static_cast<float>(radius * 2),
        static_cast<float>(radius * 2),
    };

    // draw new cursor
    SDL_SetRenderDrawColor(
        app->renderer,
        gui::cursor_colour.r,
        gui::cursor_colour.g,
        gui::cursor_colour.b,
        gui::cursor_colour.a
    );
    SDL_RenderFillRect(app->renderer, &cursor);

    SDL_RenderTexture(app->renderer, app->texture_layers[textures::Cursor], nullptr, nullptr);

    SDL_SetRenderTarget(app->renderer, nullptr);
}