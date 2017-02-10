cmake_minimum_required (VERSION 3.3)
project (wavy)

set (CMAKE_C_COMPILER clang)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set (CMAKE_C_STANDARD 99)
set (CMAKE_C_EXTENSIONS OFF)
add_definitions (-D_GNU_SOURCE)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic")

include_directories (
    include
    /usr/include/cairo
    /usr/include/pango-1.0
    /usr/include/glib-2.0
    /usr/lib/glib-2.0/include
)

file (GLOB SOURCES src/*.c)
add_executable (wavy ${SOURCES})
add_library (waveform SHARED src/lib/waveform.c)
target_link_libraries (
    wavy
    m
    wlc
    xkbcommon
    pthread
    lua
    gobject-2.0
    cairo
    pango-1.0
    pangocairo-1.0
)
