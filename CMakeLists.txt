cmake_minimum_required(VERSION 3.3)
project(wavy C)

# Using clang for now because gcc gives lots of useless warnings.
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
add_definitions(-D_GNU_SOURCE)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
add_subdirectory(protocols)

find_package(Cairo REQUIRED)
find_package(Pango REQUIRED)

include_directories(
    include
    protocols
    ${CAIRO_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
)

add_executable(wavy
    src/bar.c
    src/border.c
    src/callbacks.c
    src/commands.c
    src/config.c
    src/extensions.c
    src/input.c
    src/layout.c
    src/log.c
    src/utils.c
    src/vector.c
    src/wallpaper.c
    src/wavy.c
    src/wayland.c
)

target_link_libraries(wavy
    m
    wlc
    xkbcommon
    pthread
    lua
    input
    gobject-2.0
    cairo
    pango-1.0
    pangocairo-1.0
    wayland-client
    wayland-server
    wavy-protocols
)

add_library(waveform SHARED src/waveform.c)
