cmake_minimum_required(VERSION 3.25)
project(pixels)
include(CMakePrintHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

set(SDL_STATIC ON)
add_subdirectory(dependencies/SDL EXCLUDE_FROM_ALL)

add_subdirectory(dependencies/glm EXCLUDE_FROM_ALL)

add_executable(pixels src/main.cpp
        src/util.cpp
        src/util.h
        src/definitions.h
        src/simulator.cpp
        src/simulator.h
        src/AppContext.h)

target_link_libraries(pixels PRIVATE SDL3::SDL3-static)

target_link_libraries(pixels PRIVATE glm::glm)

set(TARGET_METADATA "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_BUILD_TYPE}")
set_target_properties(pixels PROPERTIES
        OUTPUT_NAME "${CMAKE_PROJECT_NAME}-${TARGET_METADATA}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if (MSVC)
    target_link_options(pixels PRIVATE "/MANIFEST:EMBED")
endif ()

if (CMAKE_BUILD_TYPE MATCHES Release)
    # Let CMake generate the executable for windows without a console
    set_target_properties(pixels PROPERTIES WIN32_EXECUTABLE TRUE)
endif ()


