cmake_minimum_required(VERSION 3.25)
project(pixels)

set(CMAKE_CXX_STANDARD 23)
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

if(CMAKE_BUILD_TYPE MATCHES Release)
    # Let CMake generate the executable for windows without a console
    set_target_properties(pixels PROPERTIES WIN32_EXECUTABLE TRUE)
endif()