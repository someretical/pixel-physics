cmake_minimum_required(VERSION 3.25)

# Enforce Ninja Multi-Config
if(NOT CMAKE_GENERATOR MATCHES "Ninja Multi-Config")
  message(FATAL_ERROR "This project requires the Ninja generator.\n"
                      "Please use: -G \"Ninja Multi-Config\"")
endif()
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel")

project(pixels)
include(CMakePrintHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Boost
set(BOOST_INCLUDE_LIBRARIES system)
set(BUILD_SHARED_LIBS
    OFF
    CACHE BOOL "Build static libs" FORCE)
add_subdirectory(dependencies/boost EXCLUDE_FROM_ALL)

# spdlog
add_subdirectory(dependencies/spdlog EXCLUDE_FROM_ALL)

# SDL
set(SDL_STATIC ON)
add_subdirectory(dependencies/SDL EXCLUDE_FROM_ALL)

# glm
add_subdirectory(dependencies/glm EXCLUDE_FROM_ALL)

# dawn
option(DAWN_USE_GLFW OFF) # disable builtin GLFW in DAWN when we use SDL2 / SDL3

option(DAWN_FETCH_DEPENDENCIES
       "Use fetch_dawn_dependencies.py as an alternative to using depot_tools"
       ON)
set(DAWN_BUILD_MONOLITHIC_LIBRARY
    "STATIC"
    CACHE STRING "Build monolithic library: SHARED, STATIC, or OFF.")

# Dawn builds many things by default - disable things we don't need
option(DAWN_BUILD_SAMPLES "Enables building Dawn's samples" OFF)
option(TINT_BUILD_CMD_TOOLS "Build the Tint command line tools" OFF)
option(TINT_BUILD_DOCS "Build documentation" OFF)
option(TINT_BUILD_TESTS "Build tests" OFF)
if(WIN32)
  option(DAWN_FORCE_SYSTEM_COMPONENT_LOAD "Allow system component fallback" ON)
  option(TINT_BUILD_SPV_READER "Build the SPIR-V input reader" OFF)
  option(TINT_BUILD_WGSL_READER "Build the WGSL input reader" ON)
  option(TINT_BUILD_GLSL_WRITER "Build the GLSL output writer" OFF)
  option(TINT_BUILD_GLSL_VALIDATOR "Build the GLSL output validator" OFF)
  option(TINT_BUILD_SPV_WRITER "Build the SPIR-V output writer" ON)
  option(TINT_BUILD_WGSL_WRITER "Build the WGSL output writer" ON)
endif()
# check if WAYLAND is the current Session Type and enable DAWN_USE_WAYLAND
# Wayland option @compile time You can override this using: cmake
# -DDAWN_USE_WAYLAND=X (X = ON | OFF)
if(LINUX)
  if($ENV{XDG_SESSION_TYPE} MATCHES wayland)
    option(DAWN_USE_WAYLAND "Enable support for Wayland surface" ON)
  endif()
endif()

add_subdirectory(dependencies/dawn EXCLUDE_FROM_ALL)

# imgui
set(IMGUI_DAWN_DIR dependencies/dawn)
file(GLOB IMGUI_SOURCES dependencies/imgui/*.cpp
     dependencies/imgui/backends/imgui_impl_wgpu.cpp
     dependencies/imgui/backends/imgui_impl_sdl3.cpp)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC dependencies/imgui)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
target_link_libraries(imgui PRIVATE SDL3::SDL3-static webgpu_dawn)

# warnings
add_library(pixels-warnings INTERFACE)
if(MSVC)
  target_compile_options(pixels-warnings INTERFACE /W4 /WX)
else()
  target_compile_options(pixels-warnings INTERFACE -Wall -Wextra -Werror)
endif()

# pixels
add_executable(pixels src/main.cpp src/physics.cpp src/util.cpp)

# 0 = Trace, 1 = Debug, 2 = Info, 3 = Warn, 4 = Error, 5 = Critical, 6 = Off
target_compile_definitions(
  pixels
  PRIVATE $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
          $<$<CONFIG:Release>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
          $<$<CONFIG:MinSizeRel>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_ERROR>
  PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN)

target_include_directories(pixels PRIVATE dependencies/pcg-cpp/include)
target_link_libraries(
  pixels
  INTERFACE webgpu_cpp
  PRIVATE SDL3::SDL3-static imgui spdlog::spdlog glm::glm webgpu_dawn)

set(TARGET_TRIPLE
    "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}")
set_target_properties(
  pixels
  PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}-${TARGET_TRIPLE}-$<CONFIG>"
             RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
             WIN32_EXECUTABLE $<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>)

if(MSVC)
  target_link_options(pixels PRIVATE "/MANIFEST:EMBED")
endif()
