#include "simulator.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include <glm/vec3.hpp>

#include <utility>


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
                    if (!cell.displaceable) {
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
                        if (not check_x_in_range(point.x)) {
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
                    if (!cell.displaceable) {
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

                    while (max_slip != 0 and s_x != max_slip) {
                        auto &cur_x = app->cells[y][x + s_x];
                        auto next_x = glm::ivec2{x + s_x + slip_dir, y};
                        if (not check_x_in_range(next_x.x)) {
                            cur_x.has_been_updated = true;
                            cur_x.velocity.x = 0;
                            break;
                        }

                        auto &next_x_cell = app->cells[next_x.y][next_x.x];
                        if (next_x_cell.displaceable and density_le_chance(next_x_cell, cur_x, app->rng)) {
                            cur_x.has_been_updated = true;
                            next_x_cell.has_been_updated = true;
                            std::swap(cur_x, next_x_cell);

                            // Check if we can fall down
                            if (y < level_size.y - 1) {
                                auto below = glm::ivec2{next_x.x, y + 1};
                                auto &next_y_cell = app->cells[below.y][below.x];
                                if (next_y_cell.displaceable and density_le_chance(next_y_cell, next_x_cell, app->rng)) {
                                    next_y_cell.has_been_updated = true;
                                    next_x_cell.has_been_updated = true;
                                    std::swap(next_y_cell, next_x_cell);
                                    break;
                                }
                            }
                        } else {
                            cur_x.has_been_updated = true;
                            cur_x.velocity.x = 0;
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

void process_rendering(AppContext *app) {
    SDL_SetRenderDrawColor(app->renderer, background_colour.r, background_colour.g, background_colour.b,
                           background_colour.a);
    SDL_RenderClear(app->renderer);

    SDL_Color *pixels;
    int pitch = sizeof(SDL_Color) * window_size.x;
    SDL_LockTexture(app->frame_buffer, nullptr, reinterpret_cast<void **>(&pixels), &pitch);

    for (auto y{0}; y < level_size.y; y++) {
        for (auto x{0}; x < level_size.x; x++) {
            auto &cell = app->cells[y][x];

            if (not cell.has_been_updated) {
                continue;
            }

            auto &colour = material_colour[std::to_underlying(cell.material)];

            auto top_left = glm::ivec2{x * 2, y * 2};
            auto top_right = glm::ivec2{x * 2 + 1, y * 2};
            auto bottom_left = glm::ivec2{x * 2, y * 2 + 1};
            auto bottom_right = glm::ivec2{x * 2 + 1, y * 2 + 1};
            std::array points{top_left, top_right, bottom_left, bottom_right};

            for (auto &&point: points) {
                memcpy(reinterpret_cast<void *>(pixels + (point.y * window_size.x + point.x)), &colour,
                       sizeof(SDL_Color));
            }
        }
    }

    SDL_UnlockTexture(app->frame_buffer);
    SDL_RenderTexture(app->renderer, app->frame_buffer, nullptr, nullptr);
    SDL_RenderPresent(app->renderer);

    for (auto &&row: app->cells) {
        for (auto &&cell: row) {
            cell.has_been_updated = false;
        }
    }
}