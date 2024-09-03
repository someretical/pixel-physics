#include "simulator.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include <utility>

void process_input(AppContext *app) {
    //    auto kb_state{SDL_GetKeyboardState(nullptr)};
    const auto &[mouse_pos, mouse_state] = get_mouse_info();
    auto radius = app->cursor.brush_radius;
    auto lvl_x = std::clamp(static_cast<int>(mouse_pos.x / 2), 0, level_size.x - 1);
    auto lvl_y = std::clamp(static_cast<int>(mouse_pos.y / 2), 0, level_size.y - 1);
    auto lvl_top_left = glm::ivec2{lvl_x - radius, lvl_y - radius};
    auto lvl_bottom_right = glm::ivec2{lvl_x + radius, lvl_y + radius};
    auto lvl_top_right = glm::ivec2{lvl_x + radius, lvl_y - radius};
    auto lvl_bottom_left = glm::ivec2{lvl_x - radius, lvl_y + radius};

    if (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        for (auto i{lvl_top_left.y}; i < lvl_bottom_right.y; i++) {
            for (auto j{lvl_top_left.x}; j < lvl_bottom_right.x; j++) {
                if (check_in_lvl_range({j, i})) {
                    auto &cell = app->cells[i][j];
                    cell.material = app->cursor.selected_material;
                    cell.has_been_updated = true;
                    cell.displaceable = true;
                    cell.velocity = {0, 0};
                }
            }
        }
    } else if (mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
        for (auto i{lvl_top_left.y}; i < lvl_bottom_right.y; i++) {
            for (auto j{lvl_top_left.x}; j < lvl_bottom_right.x; j++) {
                if (check_in_lvl_range({j, i})) {
                    app->cells[i][j] = air_cell;
                }
            }
        }
    }
}

void process_physics(AppContext *app) {
    bool flip = app->rng.gen_int();
    // Turn 0 or 1 into -1 or 1
    auto direction = -(2 * flip - 1);

    for (auto y{level_size.y - 1}; y != 0; y--) {
        /*
         * The reason we need this whole flip and direction thing is because we want to randomise how we process the cells.
         * In the simplest case, we just iterate in increasing x and y. However, this introduces bias into how cells
         * that are less viscous are processed. Since they can spread sideways, if we process in increasing x then we will
         * introduce a bias towards the right. Thus, we want to randomise between increasing/decreasing x.
         */
        for (
                auto x{flip * (level_size.x - 1)};
                x != (!flip * level_size.x);
                x += direction
                ) {
            auto &cell = app->cells[y][x];

            if (cell.has_been_updated) {
                continue;
            }

            switch (cell.material) {
                case Material::END_MARKER:
                case Material::Air: {
                    break;
                }
                case Material::RedSand:
                case Material::Sand: {
                    if (not cell.displaceable) {
                        break;
                    }
                    // We want to track how far down it can fall and if it can fall at all
                    cell.velocity.y = std::clamp(cell.velocity.y + g, min_y_velocity, max_y_velocity);
                    int s_y = 0;
                    // If s_y is equal to cell.velocity.y then we are not obstructed
                    while (s_y < cell.velocity.y) {
                        auto next = glm::ivec2{x, y + s_y + 1};
                        if (next.y >= level_size.y) {
                            // We can examine s_y afterward to see how far we fell
                            // If s_y is 0, then we did not fall at all as we reached the bottom already
                            break;
                        }

                        auto &next_cell = app->cells[next.y][next.x];
                        if (next_cell.displaceable and density_le_chance(next_cell, cell, app->rng)) {
                            s_y++;
                        } else {
                            // The particle hit something that is not displaceable and/or
                            // that something is denser than it and the particle stops falling because of it
                            break;
                        }
                    }

                    if (s_y == 0 and y == level_size.y - 1) {
                        // We are at the bottom, and we cannot fall any further, so we cancel v_y
                        cell.velocity.y = 0;

                        // This line is different for sand and water
                        // In fact this branch doesn't exist for water
                        cell.has_been_updated = true;
                        break;
                    } else if (s_y == 0) {
                        // We could not fall any further straight down,
                        // but we can still fall to the side
                        // So we cancel v_y and continue past this entire if block...
                        cell.velocity.y = 0;
                    } else {
                        // We can fall down by s_y cells
                        // Since we are falling vertically, we cannot use memmove
                        for (auto i{0}; i < s_y; i++) {
                            auto &cur = app->cells[y + i][x];
                            auto &next = app->cells[y + i + 1][x];
                            cur.has_been_updated = true;
                            next.has_been_updated = true;
                            std::swap(cur, next);
                        }
                        break;
                    }

                    // Do not try the strategy of moving to the left and then moving down in one go!
                    // Or rather, you could try it but I already did and my result looked funky
                    // This method looks a lot more natural.
                    glm::ivec2 below_left{x - 1, y + 1};
                    glm::ivec2 below_right{x + 1, y + 1};
                    auto first = app->rng.gen_int() ? below_left : below_right;
                    auto second = app->rng.gen_int() ? below_right : below_left;
                    std::array points{first, second};

                    for (auto &point: points) {
                        if (not check_x_in_lvl_range(point.x)) {
                            continue;
                        }

                        auto &point_cell = app->cells[point.y][point.x];
                        if (point_cell.displaceable and density_le_chance(point_cell, cell, app->rng)) {
                            cell.has_been_updated = true;
                            point_cell.has_been_updated = true;
                            std::swap(cell, point_cell);
                            break;
                        }
                    }

                    break;
                }
                case Material::Water: {
                    if (not cell.displaceable) {
                        break;
                    }

                    // We want to track how far down it can fall and if it can fall at all
                    cell.velocity.y += g;
                    int s_y = 0;
                    // If s_y is equal to cell.velocity.y then we are not obstructed
                    while (s_y < cell.velocity.y) {
                        auto next = glm::ivec2{x, y + s_y + 1};
                        if (next.y >= level_size.y) {
                            // We can examine s_y afterward to see how far we fell
                            // If s_y is 0, then we did not fall at all as we reached the bottom already
                            break;
                        }

                        auto &next_cell = app->cells[next.y][next.x];
                        if (next_cell.displaceable and density_le_chance(next_cell, cell, app->rng)) {
                            s_y++;
                        } else {
                            // The particle hit something that is not displaceable and/or
                            // that something is denser than it and the particle stops falling because of it
                            break;
                        }
                    }

                    if (s_y == 0) {
                        // We could not fall any further straight down,
                        // but we can still fall to the side
                        // So we cancel v_y and continue past this entire if block...
                        cell.velocity.y = 0;
                    } else {
                        // We can fall down by s_y cells
                        // Since we are falling vertically, we cannot use memmove
                        for (auto i{0}; i < s_y; i++) {
                            auto &cur = app->cells[y + i][x];
                            auto &next = app->cells[y + i + 1][x];
                            cur.has_been_updated = true;
                            next.has_been_updated = true;
                            std::swap(cur, next);
                        }

                        // Already processed
                        break;
                    }

                    /*
                     * Procedure for water:
                     * 1. Pick direction (either left or right)
                     * 2. Attempt to advance in that direction OR if we have reached max slipperiness, terminate the algorithm
                     * 3. If we can advance, swap the cells
                     * 4. Try to move down
                     * 5. If we can move down, swap the cells and terminate the algorithm
                     * 6. Go back to 2
                     */
                    int slip_dir;
                    if (cell.velocity.x == 0) {
                        slip_dir = app->rng.gen_int() ? -1 : 1;
                        cell.velocity.x = slip_dir;
                    } else if (cell.velocity.x > 0) {
                        slip_dir = 1;
                    } else {
                        slip_dir = -1;
                    }

                    auto max_slip = slipperiness(cell) * slip_dir;
                    auto s_x = 0;

                    while (s_x != max_slip) {
                        auto &cur_x = app->cells[y][x + s_x];
                        auto next_x = glm::ivec2{x + s_x + slip_dir, y};
                        if (not check_x_in_lvl_range(next_x.x)) {
                            cur_x.has_been_updated = true;
                            cur_x.velocity.x *= -1;
                            break;
                        }

                        auto &next_x_cell = app->cells[next_x.y][next_x.x];
                        if (next_x_cell.displaceable and density_le_chance(next_x_cell, cur_x, app->rng)) {
                            cur_x.has_been_updated = true;
                            next_x_cell.has_been_updated = true;
                            std::swap(cur_x, next_x_cell);

                            // Check if we can fall down
//                            if (y < level_size.y - 1) {
//                                auto below = glm::ivec2{next_x.x, y + 1};
//                                auto &next_y_cell = app->cells[below.y][below.x];
//                                if (next_y_cell.displaceable and
//                                    density_le_chance(next_y_cell, next_x_cell, app->rng)) {
//                                    next_y_cell.has_been_updated = true;
//                                    next_x_cell.has_been_updated = true;
//                                    std::swap(next_y_cell, next_x_cell);
//                                    break;
//                                }
//                            }
                        } else {
                            cur_x.has_been_updated = true;
                            cur_x.velocity.x *= -1;
                            next_x_cell.has_been_updated = true;
                            break;
                        }

                        s_x += slip_dir;
                    }

                    break;
                }
            }
        }
    }
}

static void process_cursor(const AppContext *app, SDL_Color *pixels) {
    const auto &[mouse_pos, mouse_state] = get_mouse_info();
    auto radius = app->cursor.brush_radius;

    // This version aligns the cursor to the cell grid
    auto lvl_x = std::clamp(static_cast<int>(mouse_pos.x / 2), 0, level_size.x - 1);
    auto lvl_y = std::clamp(static_cast<int>(mouse_pos.y / 2), 0, level_size.y - 1);
    auto lvl_top_left = glm::ivec2{lvl_x - radius, lvl_y - radius};
    auto lvl_bottom_right = glm::ivec2{lvl_x + radius, lvl_y + radius};
    auto lvl_top_right = glm::ivec2{lvl_x + radius, lvl_y - radius};
    auto lvl_bottom_left = glm::ivec2{lvl_x - radius, lvl_y + radius};

    for (auto y{lvl_top_left.y}; y <= lvl_bottom_right.y; y++) {
        if (check_in_lvl_range({lvl_top_left.x, y})) {
            std::array points{
                    glm::ivec2{lvl_top_left.x * 2, y * 2},
                    glm::ivec2{lvl_top_left.x * 2 + 1, y * 2},
                    glm::ivec2{lvl_top_left.x * 2, y * 2 + 1},
                    glm::ivec2{lvl_top_left.x * 2 + 1, y * 2 + 1},
            };

            for (auto point: points) {
                memcpy(reinterpret_cast<void *>(pixels + (point.y * window_size.x + point.x)), &cursor_colour,
                       sizeof(SDL_Color));
            }
        }
    }

    for (auto y{lvl_top_right.y}; y <= lvl_bottom_right.y; y++) {
        if (check_in_lvl_range({lvl_top_right.x, y})) {
            std::array points{
                    glm::ivec2{lvl_top_right.x * 2, y * 2},
                    glm::ivec2{lvl_top_right.x * 2 + 1, y * 2},
                    glm::ivec2{lvl_top_right.x * 2, y * 2 + 1},
                    glm::ivec2{lvl_top_right.x * 2 + 1, y * 2 + 1},
            };

            for (auto point: points) {
                memcpy(reinterpret_cast<void *>(pixels + (point.y * window_size.x + point.x)), &cursor_colour,
                       sizeof(SDL_Color));
            }
        }
    }

    for (auto x{lvl_top_left.x}; x <= lvl_top_right.x; x++) {
        if (check_in_lvl_range({x, lvl_top_left.y})) {
            std::array points{
                    glm::ivec2{x * 2, lvl_top_left.y * 2},
                    glm::ivec2{x * 2 + 1, lvl_top_left.y * 2},
                    glm::ivec2{x * 2, lvl_top_left.y * 2 + 1},
                    glm::ivec2{x * 2 + 1, lvl_top_left.y * 2 + 1},
            };

            for (auto point: points) {
                memcpy(reinterpret_cast<void *>(pixels + (point.y * window_size.x + point.x)), &cursor_colour,
                       sizeof(SDL_Color));
            }
        }
    }

    for (auto x{lvl_bottom_left.x}; x <= lvl_bottom_right.x; x++) {
        if (check_in_lvl_range({x, lvl_bottom_left.y})) {
            std::array points{
                    glm::ivec2{x * 2, lvl_bottom_left.y * 2},
                    glm::ivec2{x * 2 + 1, lvl_bottom_left.y * 2},
                    glm::ivec2{x * 2, lvl_bottom_left.y * 2 + 1},
                    glm::ivec2{x * 2 + 1, lvl_bottom_left.y * 2 + 1},
            };

            for (auto point: points) {
                memcpy(reinterpret_cast<void *>(pixels + (point.y * window_size.x + point.x)), &cursor_colour,
                       sizeof(SDL_Color));
            }
        }
    }

    // This version aligns the cursor to the pixel grid. Each cell takes up 2x2 pixels
    //    auto top_left = glm::ivec2{mouse_pos.x - radius, mouse_pos.y - radius};
    //    auto top_right = glm::ivec2{mouse_pos.x + radius, mouse_pos.y - radius};
    //    auto bottom_left = glm::ivec2{mouse_pos.x - radius, mouse_pos.y + radius};
    //    auto bottom_right = glm::ivec2{mouse_pos.x + radius, mouse_pos.y + radius};
    //
    //    for (auto y{top_left.y}; y <= bottom_right.y; y++) {
    //        if (check_in_win_range({top_left.x, y})) {
    //            memcpy(reinterpret_cast<void *>(pixels + (y * window_size.x + top_left.x)), &cursor_colour, sizeof(SDL_Color));
    //        }
    //    }
    //
    //    for (auto y{top_right.y}; y <= bottom_right.y; y++) {
    //        if (check_in_win_range({top_right.x, y})) {
    //            memcpy(reinterpret_cast<void *>(pixels + (y * window_size.x + top_right.x)), &cursor_colour, sizeof(SDL_Color));
    //        }
    //    }
    //
    //    for (auto x{top_left.x}; x <= top_right.x; x++) {
    //        if (check_in_win_range({x, top_left.y})) {
    //            memcpy(reinterpret_cast<void *>(pixels + (top_left.y * window_size.x + x)), &cursor_colour, sizeof(SDL_Color));
    //        }
    //    }
    //
    //    for (auto x{bottom_left.x}; x <= bottom_right.x; x++) {
    //        if (check_in_win_range({x, bottom_left.y})) {
    //            memcpy(reinterpret_cast<void *>(pixels + (bottom_left.y * window_size.x + x)), &cursor_colour, sizeof(SDL_Color));
    //        }
    //    }
}

static void process_brush(AppContext *app, SDL_Color *pixels) {
    for (auto n_y{0}; n_y < level_size.y; n_y++) {
        for (auto n_x{0}; n_x < level_size.x; n_x++) {
            auto &cell = app->cells[n_y][n_x];

            auto &colour = material_colour[std::to_underlying(cell.material)];

            auto top_left = glm::ivec2{n_x * 2, n_y * 2};
            auto top_right = glm::ivec2{n_x * 2 + 1, n_y * 2};
            auto bottom_left = glm::ivec2{n_x * 2, n_y * 2 + 1};
            auto bottom_right = glm::ivec2{n_x * 2 + 1, n_y * 2 + 1};
            std::array points{top_left, top_right, bottom_left, bottom_right};

            for (auto &point: points) {
                memcpy(reinterpret_cast<void *>(pixels + (point.y * window_size.x + point.x)), &colour,
                       sizeof(SDL_Color));
            }
        }
    }
}

void process_rendering(AppContext *app) {
    SDL_SetRenderDrawColor(app->renderer, background_colour.r, background_colour.g, background_colour.b,
                           background_colour.a);
    SDL_RenderClear(app->renderer);

    SDL_Color *pixels;
    int pitch = sizeof(SDL_Color) * window_size.x;
    SDL_LockTexture(app->frame_buffer, nullptr, reinterpret_cast<void **>(&pixels), &pitch);

    process_brush(app, pixels);
    process_cursor(app, pixels);

    SDL_UnlockTexture(app->frame_buffer);
    SDL_RenderTexture(app->renderer, app->frame_buffer, nullptr, nullptr);
    SDL_RenderPresent(app->renderer);

    for (auto &&row: app->cells) {
        for (auto &&cell: row) {
            cell.has_been_updated = false;
        }
    }
}