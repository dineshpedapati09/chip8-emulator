cmake_minimum_required(VERSION 3.16)

project(chip8_emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(chip8
    main.cpp
    chip8.cpp
)

target_include_directories(chip8 PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_directories(chip8 PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(chip8 PRIVATE ${SDL2_LIBRARIES})
target_compile_options(chip8 PRIVATE ${SDL2_CFLAGS_OTHER})
