cmake_minimum_required(VERSION 3.16)

project(chip8_emulator
    VERSION 1.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(chip8
    main.cpp
    chip8.cpp
    chip8.h
)

target_link_libraries(chip8 PRIVATE PkgConfig::SDL2)

if (MSVC)
    target_compile_options(chip8 PRIVATE /W4)
else()
    target_compile_options(chip8 PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )
endif()
