#include "AppContext.h"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <array>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <glad/gl.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

static SDL_Window *window = NULL;
static SDL_GLContext gl_context = NULL;

constexpr static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

const int WINDOW_W = 640;
const int WINDOW_H = 480;

const int CHUNK_SIZE = 64;
const int PAD = 1;
const int TEX_SIZE = CHUNK_SIZE + PAD * 2;

const int SCALE = 2;
const int MAX_VISIBLE_CHUNKS = 16;

// Struct for SSBO
struct ChunkDraw
{
    int screenX;
    int screenY;
    int layer;
    int padding; // for alignment
};

// Dummy world: each chunk is one texture
GLuint chunkTexArray;
GLuint chunkSSBO;
GLuint shaderProgram;
GLuint quadVAO;

float zoom = SCALE;
int cameraX = 0;
int cameraY = 0;

struct Chunk
{
    GLuint layer;              // Texture array layer
    std::vector<uint8_t> data; // RGBA8UI
};
std::vector<Chunk> worldChunks;

std::array<ChunkDraw, MAX_VISIBLE_CHUNKS> ssboData;

// --- Shader sources ---
const char *vertexShaderSrc = R"(
#version 430 core

layout (location = 0) in vec2 aPos;

struct ChunkDraw { ivec2 screenPos; int layer; int pad; };
layout(std430, binding = 0) buffer ChunkBuffer { ChunkDraw chunks[]; };

uniform float uZoom;

out vec2 vWorldCell;
flat out int vLayer;

void main() {
    ChunkDraw c = chunks[gl_InstanceID];

    vec2 screenPos = aPos * uZoom + vec2(c.screenPos);

    gl_Position = vec4(
        (screenPos.x / 320.0) - 1.0,
        1.0 - (screenPos.y / 240.0),
        0.0, 1.0
    );

    vWorldCell = aPos;
    vLayer = c.layer;
}
)";

const char *fragmentShaderSrc = R"(
#version 430 core
uniform usampler2DArray uChunkArray;

in vec2 vWorldCell;
flat in int vLayer;

out vec4 FragColor;

void main() {
    ivec2 cell = ivec2(floor(vWorldCell)) + ivec2(1); // padded
    uvec4 data = texelFetch(uChunkArray, ivec3(cell, vLayer), 0);
    uint material = data.r;
    uint temp     = data.g;

    vec3 color;
    if (material == 0u) color = vec3(0.0);
    else if (material == 1u) color = vec3(0.8,0.7,0.5);
    else if (material == 2u) color = vec3(0.2,0.5,0.9);
    else color = vec3(1.0,0.0,1.0);

    color += float(temp)/255.0 * vec3(0.4,0.1,0.0);

    FragColor = vec4(color,1.0);
}
)";

// --- Helper to compile shaders ---
GLuint compileShader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint success;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        spdlog::error("Shader compile error: {}", log);
    }
    return s;
}

GLuint createShaderProgram()
{
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        spdlog::error("Shader program error: {}", log);
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

// --- Setup quad VAO ---
void setupQuad()
{
    float quadVerts[8] = {0, 0, CHUNK_SIZE, 0, 0, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE};
    GLuint VBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);
}

// --- Create dummy chunk textures ---
void createChunkTextureArray(int numChunks)
{
    glGenTextures(1, &chunkTexArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, chunkTexArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8UI, TEX_SIZE, TEX_SIZE, numChunks);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// --- Upload chunk data ---
void uploadChunk(int layer, const std::vector<uint8_t> &paddedData)
{
    glBindTexture(GL_TEXTURE_2D_ARRAY, chunkTexArray);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, TEX_SIZE, TEX_SIZE, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                    paddedData.data());
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%H:%M:%S.%e] [thread %t] [%l] %v");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // GL 4.6 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window = SDL_CreateWindow("Dear ImGui SDL3+OpenGL3 example", (int)(1280 * main_scale), (int)(800 * main_scale),
                              window_flags);
    if (window == nullptr)
    {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        spdlog::error("SDL_GL_CreateContext failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!gladLoadGL(SDL_GL_GetProcAddress))
    {
        spdlog::error("gladLoadGL failed");
        return SDL_APP_FAILURE;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling,
                                     // changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale; // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this
                                     // unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will call AddFontDefault() to select an embedded font: either
    // AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small
    //   threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code
    // (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double
    // backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See
    // Makefile.emscripten for details.
    // style.FontSizeBase = 20.0f;
    // io.Fonts->AddFontDefaultVector();
    // io.Fonts->AddFontDefaultBitmap();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    // IM_ASSERT(font != nullptr);

    // --- Create SSBO ---
    glGenBuffers(1, &chunkSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_VISIBLE_CHUNKS * sizeof(ChunkDraw), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkSSBO);

    // --- Create dummy chunks ---
    worldChunks.resize(MAX_VISIBLE_CHUNKS);
    createChunkTextureArray(MAX_VISIBLE_CHUNKS);
    for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++)
    {
        std::vector<uint8_t> data(TEX_SIZE * TEX_SIZE * 4, 0);
        for (int y = PAD; y < CHUNK_SIZE + PAD; y++)
            for (int x = PAD; x < CHUNK_SIZE + PAD; x++)
            {
                int idx = (y * TEX_SIZE + x) * 4;
                data[idx] = i % 3 + 1; // material
                data[idx + 1] = x + y; // temp gradient
                data[idx + 2] = 0;
                data[idx + 3] = 255;
            }
        worldChunks[i].layer = i;
        uploadChunk(i, data);
    }

    auto res = pixels::core::AppContext::Create();
    if (not res)
    {
        spdlog::error("AppContext::Create failed");
        return SDL_APP_FAILURE;
    }

    *appstate = res->release();

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == SDL_GetWindowID(window))
        return SDL_APP_SUCCESS;

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code
    // to learn more about Dear ImGui!).
    bool p_open = true;
    ImGui::ShowDemoWindow(&p_open);

    // Rendering
    ImGui::Render();
    ImGuiIO &io = ImGui::GetIO();

    // other stuff
    for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++)
    {
        ssboData[i].screenX = (i % 4) * CHUNK_SIZE * SCALE - cameraX * SCALE;
        ssboData[i].screenY = (i / 4) * CHUNK_SIZE * SCALE - cameraY * SCALE;
        ssboData[i].layer = worldChunks[i].layer;
        ssboData[i].padding = 0;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, MAX_VISIBLE_CHUNKS * sizeof(ChunkDraw), ssboData.data());

    // --- Render ---
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);
    glBindVertexArray(quadVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, chunkTexArray);
    glUniform1i(glGetUniformLocation(shaderProgram, "uChunkArray"), 0);
    glUniform1f(glGetUniformLocation(shaderProgram, "uZoom"), zoom);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, MAX_VISIBLE_CHUNKS);

    // glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    // glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w,
    //              clear_color.w);
    // glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);

    pixels::core::AppContext *ctx = (pixels::core::AppContext *)appstate;
    delete ctx;

    spdlog::info("Calling SDL_Quit");
    SDL_Quit();
}
