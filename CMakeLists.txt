cmake_minimum_required(VERSION 3.25)
project(pixels)
include(CMakePrintHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# SDL
set(SDL_STATIC ON)
add_subdirectory(dependencies/SDL)


# glm
add_subdirectory(dependencies/glm)


# pcg-cpp
add_library(pcg-cpp INTERFACE)
# we use system interface because then the targer compile options for warnings do not apply to this header only library
target_include_directories(pcg-cpp SYSTEM INTERFACE
    dependencies/pcg-cpp/include
)

# imgui
file(GLOB IMGUI_SOURCES 
    dependencies/imgui/*.cpp
    dependencies/imgui/backends/imgui_impl_sdlgpu3.cpp
    dependencies/imgui/backends/imgui_impl_sdl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC 
    dependencies/imgui
)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)


# warnings
add_library(pixels-warnings INTERFACE)
if(MSVC)
    target_compile_options(pixels-warnings INTERFACE /W4 /WX)
else()
    target_compile_options(pixels-warnings INTERFACE -Wall -Wextra -Werror)
endif()


# pixels
add_executable(pixels
    src/main.cpp
    src/AppContext.cpp
    src/GPUContext.cpp
    src/simulator.cpp
    src/util.cpp
)
target_link_libraries(pixels PRIVATE 
    pixels-warnings
    SDL3::SDL3-static 
    glm::glm
    pcg-cpp
    imgui
)


set(TARGET_METADATA "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_BUILD_TYPE}")
set_target_properties(pixels PROPERTIES
        OUTPUT_NAME "${CMAKE_PROJECT_NAME}-${TARGET_METADATA}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)


get_target_property(CMAKE_OUTPUT_NAME pixels OUTPUT_NAME)
file(WRITE CMAKE_OUTPUT "${CMAKE_OUTPUT_NAME}")

if (MSVC)
    target_link_options(pixels PRIVATE "/MANIFEST:EMBED")
endif ()

if (CMAKE_BUILD_TYPE MATCHES Release)
    # Let CMake generate the executable for windows without a console
    set_target_properties(pixels PROPERTIES WIN32_EXECUTABLE TRUE)
endif ()
