#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

#include <glm/vec2.hpp>

#include <array>
#include <utility>

constexpr static glm::i32vec2 base_window_size{640, 480};

enum class Material : int8_t {
    Air = 0,
    Sand = 1,
};

constexpr static SDL_Color background_colour{93, 88, 90, 255};

constexpr static std::array<SDL_Color, 2> material_colours{
        SDL_Color{0, 0, 0, 0},
        SDL_Color{255, 255, 0, 255},
};

typedef struct cell_t {
    glm::vec2 velocity;     // 8 bytes
    Material material;      // 1 byte
    bool has_been_updated;  // 1 byte
} cell_t;

constexpr static cell_t air_cell{{0, 0}, Material::Air, true};

bool check_x_in_range(int x) {
    return x >= 0 and x < base_window_size.x;
}

bool check_y_in_range(int y) {
    return y >= 0 and y < base_window_size.y;
}

bool check_in_range(glm::ivec2 point) {
    return check_x_in_range(point.x) and check_y_in_range(point.y);
}

SDL_AppResult SDL_Fail() {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
    return SDL_APP_FAILURE;
}

struct AppContext {
    std::array<std::array<cell_t, base_window_size.x>, base_window_size.y> cells;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame_buffer;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;

    AppContext(SDL_Window *window, SDL_Renderer *renderer) : window(window), renderer(renderer) {
        frame_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                         base_window_size.x,
                                         base_window_size.y);
        if (not frame_buffer) {
            SDL_Fail();
        }
        for (auto &&row: cells) {
            row.fill(air_cell);
        }
    }

    ~AppContext() {
        SDL_DestroyTexture(frame_buffer);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (not SDL_Init(SDL_INIT_VIDEO)) {
        return SDL_Fail();
    }

    auto display_id{SDL_GetPrimaryDisplay()};
    if (not display_id) {
        return SDL_Fail();
    }

    auto display_scale{SDL_GetDisplayContentScale(display_id)};
    if (display_scale == 0.f) {
        return SDL_Fail();
    }

    SDL_Window *window = SDL_CreateWindow("Pixel Physics", base_window_size.x, base_window_size.y, 0);
    if (not window) {
        return SDL_Fail();
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (not renderer) {
        return SDL_Fail();
    }
    SDL_SetRenderVSync(renderer, SDL_TRUE);

    if (SDL_ShowWindow(window) == SDL_FALSE) {
        return SDL_Fail();
    }

    {
        int width, height, bbwidth, bbheight;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
        SDL_Log("Display ID:\t%i", display_id);
        SDL_Log("Display scale:\t%f%%", display_scale * 100);
        SDL_Log("Window size:\t%ix%i", width, height);
        SDL_Log("Backbuffer size:\t%ix%i", bbwidth, bbheight);
        if (width != bbwidth) {
            SDL_Log("This is a highdpi environment.");
        }
    }

    *appstate = new AppContext{
            window,
            renderer,
    };

    SDL_Log("Application started successfully!");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, const SDL_Event *event) {
    auto *app = (AppContext *) appstate;

    if (event->type == SDL_EVENT_QUIT) {
        app->app_quit = SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
//    auto start{SDL_GetPerformanceCounter()};

    auto *app = (AppContext *) appstate;
    auto ticks{SDL_GetTicks()};

    for (auto &&row: app->cells) {
        for (auto &&cell: row) {
            cell.has_been_updated = false;
        }
    }

    app->cells[240][base_window_size.x / 2].material = Material::Sand;

    for (auto i{0}; i < base_window_size.x; i++) {
        for (auto j{0}; j < base_window_size.y; j++) {
            auto &cell = app->cells[j][i];

            if (cell.has_been_updated) {
                continue;
            }

            switch (cell.material) {
                case Material::Air: {
                    break;
                }
                case Material::Sand: {
                    glm::ivec2 below{i, j + 1};
                    if (below.y >= base_window_size.y) {
                        cell.has_been_updated = true;
                        continue;
                    }

                    if (app->cells[below.y][below.x].material == Material::Air) {
                        cell.has_been_updated = true;
                        app->cells[below.y][below.x] = cell;
                        cell = air_cell;
                        continue;
                    }

                    glm::ivec2 below_left{i - 1, j + 1};
                    glm::ivec2 below_right{i + 1, j + 1};

                    auto first = (ticks % 2 == 0) ? below_left : below_right;
                    auto &first_cell = app->cells[first.y][first.x];
                    auto second = (ticks % 2 == 0) ? below_right : below_left;
                    auto &second_cell = app->cells[second.y][second.x];

                    if (check_x_in_range(first.x) and not first_cell.has_been_updated and
                        first_cell.material == Material::Air) {
                        cell.has_been_updated = true;
                        app->cells[first.y][first.x] = cell;
                        cell = air_cell;
                        continue;
                    }

                    if (check_x_in_range(second.x) and not second_cell.has_been_updated and
                        second_cell.material == Material::Air) {
                        cell.has_been_updated = true;
                        app->cells[second.y][second.x] = cell;
                        cell = air_cell;
                        continue;
                    }
                }
            }
        }
    }

//    auto end{SDL_GetPerformanceCounter()};
//    auto elapsed{(end - start) / static_cast<double>(SDL_GetPerformanceFrequency())};
//    SDL_Log("Update structures:\t%f",elapsed);
//
//    start = SDL_GetPerformanceCounter();

    SDL_SetRenderDrawColor(app->renderer, background_colour.r, background_colour.g, background_colour.b,
                           background_colour.a);
    SDL_RenderClear(app->renderer);

    SDL_Color *pixels;
    int pitch = sizeof(SDL_Color) * base_window_size.x;
    SDL_LockTexture(app->frame_buffer, nullptr, reinterpret_cast<void **>(&pixels), &pitch);

    for (auto y{0}; y < base_window_size.y; y++) {
        for (auto x{0}; x < base_window_size.x; x++) {
            auto &cell = app->cells[y][x];
            auto &colour = material_colours[std::to_underlying(cell.material)];
            memcpy(reinterpret_cast<void *>(pixels + (y * base_window_size.x + x)), &colour, sizeof(SDL_Color));
        }
    }

    SDL_UnlockTexture(app->frame_buffer);
    SDL_RenderTexture(app->renderer, app->frame_buffer, nullptr, nullptr);
    SDL_RenderPresent(app->renderer);

//    auto end = SDL_GetPerformanceCounter();
//    auto elapsed = (end - start) / static_cast<double>(SDL_GetPerformanceFrequency());
//    SDL_Log("FPS:\t%f", 1.f / elapsed);

    return app->app_quit;
}

void SDL_AppQuit(void *appstate) {
    delete (AppContext *) appstate;

    SDL_Quit();
    SDL_Log("Application quit successfully!");
}