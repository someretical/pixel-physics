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

using namespace pixels;

constexpr static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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
    *appstate = nullptr;
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern(util::SPDLOG_FORMAT);

    auto res = pixels::core::AppContext::Create();
    if (not res)
    {
        spdlog::error("AppContext::Create failed");
        return SDL_APP_FAILURE;
    }
    *appstate = res->release();
    core::AppContext *ctx = (core::AppContext *)*appstate;

    // // --- Create SSBO ---
    // glGenBuffers(1, &chunkSSBO);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkSSBO);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_VISIBLE_CHUNKS * sizeof(ChunkDraw), nullptr, GL_DYNAMIC_DRAW);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkSSBO);

    // // --- Create dummy chunks ---
    // worldChunks.resize(MAX_VISIBLE_CHUNKS);
    // createChunkTextureArray(MAX_VISIBLE_CHUNKS);
    // for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++)
    // {
    //     std::vector<uint8_t> data(TEX_SIZE * TEX_SIZE * 4, 0);
    //     for (int y = PAD; y < CHUNK_SIZE + PAD; y++)
    //         for (int x = PAD; x < CHUNK_SIZE + PAD; x++)
    //         {
    //             int idx = (y * TEX_SIZE + x) * 4;
    //             data[idx] = i % 3 + 1; // material
    //             data[idx + 1] = x + y; // temp gradient
    //             data[idx + 2] = 0;
    //             data[idx + 3] = 255;
    //         }
    //     worldChunks[i].layer = i;
    //     uploadChunk(i, data);
    // }

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    core::AppContext *ctx = (core::AppContext *)appstate;
    auto window = ctx->gpu_engine->window;

    ImGui_ImplSDL3_ProcessEvent(event);
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == SDL_GetWindowID(window))
        return SDL_APP_SUCCESS;

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

bool p_open{true};
SDL_AppResult SDL_AppIterate(void *appstate)
{
    core::AppContext *ctx = (core::AppContext *)appstate;
    auto window = ctx->gpu_engine->window;

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
    ImGui::ShowDemoWindow(&p_open);

    // Rendering
    ImGui::Render();
    ImGuiIO &io = ImGui::GetIO();

    // other stuff
    // for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++)
    // {
    //     ssboData[i].screenX = (i % 4) * CHUNK_SIZE * SCALE - cameraX * SCALE;
    //     ssboData[i].screenY = (i / 4) * CHUNK_SIZE * SCALE - cameraY * SCALE;
    //     ssboData[i].layer = worldChunks[i].layer;
    //     ssboData[i].padding = 0;
    // }
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkSSBO);
    // glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, MAX_VISIBLE_CHUNKS * sizeof(ChunkDraw), ssboData.data());

    // // --- Render ---
    // glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);

    // glUseProgram(shaderProgram);
    // glBindVertexArray(quadVAO);
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D_ARRAY, chunkTexArray);
    // glUniform1i(glGetUniformLocation(shaderProgram, "uChunkArray"), 0);
    // glUniform1f(glGetUniformLocation(shaderProgram, "uZoom"), zoom);

    // glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, MAX_VISIBLE_CHUNKS);

    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w,
                 clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    pixels::core::AppContext *ctx = (pixels::core::AppContext *)appstate;
    if (not ctx)
    {
        spdlog::error("SDL_AppQuit called with null appstate and result {}", (int)result);
    }
    else
    {
        ctx->logger->trace("SDL_AppQuit called with result {}", (int)result);
        delete ctx;
    }
}
