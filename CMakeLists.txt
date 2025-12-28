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

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Boost
set(BOOST_INCLUDE_LIBRARIES system)
set(BUILD_SHARED_LIBS
    OFF
    CACHE BOOL "Build static libs" FORCE)
add_subdirectory(dependencies/boost EXCLUDE_FROM_ALL)

# SDL
set(SDL_STATIC ON)
add_subdirectory(dependencies/SDL EXCLUDE_FROM_ALL)

# glad
add_library(glad STATIC dependencies/glad/src/gl.c)
target_include_directories(glad PUBLIC dependencies/glad/include)

# imgui
file(GLOB IMGUI_SOURCES dependencies/imgui/*.cpp
     dependencies/imgui/backends/imgui_impl_opengl3.cpp
     dependencies/imgui/backends/imgui_impl_sdl3.cpp)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC dependencies/imgui)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

# warnings
add_library(pixels-warnings INTERFACE)
if(MSVC)
  target_compile_options(pixels-warnings INTERFACE /W4 /WX)
else()
  target_compile_options(pixels-warnings INTERFACE -Wall -Wextra -Werror)
endif()

# pixels
add_executable(pixels src/main.cpp)
target_link_libraries(pixels PRIVATE SDL3::SDL3-static glad imgui)

set(TARGET_QUAD
    "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}-$<CONFIG>"
)
set_target_properties(
  pixels PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}-${TARGET_QUAD}"
                    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
  target_link_options(pixels PRIVATE "/MANIFEST:EMBED")
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
  # Let CMake generate the executable for windows without a console
  set_target_properties(pixels PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
