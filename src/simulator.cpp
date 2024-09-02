#include "simulator.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

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

                    if (s_y == 0 and y == level_size.y - 1) {
                        // We are at the bottom, and we cannot fall any further, so we cancel v_y
                        cell.velocity.y = 0;
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
                            goto sand_end;
                        }
                    }
                }
                    sand_end:
                    break;
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

                    if (s_y == 0 and y == level_size.y - 1) {
                        // We are at the bottom, and we cannot fall any further, so we cancel v_y
                        cell.velocity.y = 0;
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
                            goto water_end;
                        }
                    }

                    auto left = glm::ivec2{x - 1, y};
                    auto right = glm::ivec2{x + 1, y};
                    first = app->rng.gen_int() ? left : right;
                    second = app->rng.gen_int() ? right : left;

                    for (auto &point: std::array{first, second}) {
                        if (not check_x_in_range(point.x)) {
                            continue;
                        }

                        auto &point_cell = app->cells[point.y][point.x];
                        if (point_cell.displaceable and density_le_chance(point_cell, cell, app->rng)) {
                            cell.has_been_updated = true;
                            point_cell.has_been_updated = true;
                            std::swap(cell, point_cell);
                            goto water_end;
                        }
                    }
                }
                    water_end:
                    break;
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