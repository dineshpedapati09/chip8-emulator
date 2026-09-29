# CHIP-8 Emulator

A simple CHIP-8 emulator written in C++ using SDL2.

## Files

- `main.cpp` - SDL2 window, keyboard, audio and emulator loop
- `chip8.cpp` - CHIP-8 CPU, memory, display and opcode implementation
- `chip8.h` - CHIP-8 class declaration
- `CMakeLists.txt` - CMake build configuration

## Build

Install a C++ compiler, CMake and SDL2 development libraries.

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Run

```bash
./chip8 path/to/rom.ch8
```

## Keyboard

```text
CHIP-8     Keyboard
1 2 3 C    1 2 3 4
4 5 6 D    Q W E R
7 8 9 E    A S D F
A 0 B F    Z X C V
```

Press `Esc` to quit.
