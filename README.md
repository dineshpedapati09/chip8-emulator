# CHIP-8 Emulator

A CHIP-8 emulator written in C++17 using SDL2, with runtime emulation-speed control, savestates, and selectable display palettes.

## Project files

- `main.cpp` - SDL2 window, input, audio, rendering, runtime controls, and main loop
- `chip8.cpp` - CHIP-8 CPU, memory, timers, display, opcode implementation, and savestate serialization
- `chip8.h` - CHIP-8 emulator interface and constants
- `CMakeLists.txt` - CMake build configuration
- `README.md` - Build, run, controls, and feature documentation

## Requirements

- C++17-compatible compiler
- CMake 3.16 or newer
- SDL2 development libraries
- `pkg-config`

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libsdl2-dev
```

## Build

From the repository root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Run

From the `build` directory:

```bash
./chip8 path/to/rom.ch8
```

Example:

```bash
./chip8 ../roms/IBM.ch8
```

## CHIP-8 keyboard

```text
CHIP-8       Keyboard

1 2 3 C      1 2 3 4
4 5 6 D      Q W E R
7 8 9 E      A S D F
A 0 B F      Z X C V
```

Press `Esc` or close the window to quit.

## Runtime features

### Configurable emulation speed

The default speed is **10 CHIP-8 instructions per frame**, approximately 600 instructions per second.

- `+` or keypad `+` - increase CPU cycles per frame by 1
- `-` or keypad `-` - decrease CPU cycles per frame by 1
- `0` - reset CPU speed to 10 cycles per frame
- Allowed range: 1 to 1000 cycles per frame

The CHIP-8 delay and sound timers remain tied to 60 Hz rather than the CPU speed.

### Savestate management

- `F5` - save the complete emulator state to `chip8_state.bin`
- `F9` - load `chip8_state.bin`

The savestate stores the emulator memory, V registers, index register, program counter, stack, stack pointer, delay timer, sound timer, opcode, display, and display-dirty flag. Key states are cleared when a state is loaded so a held host key cannot unexpectedly affect the restored program.

The state file is a local binary file in the current working directory. It is not a ROM and does not need to be committed to GitHub.

### Custom display color schemes

- `F1` - Classic Green Screen
- `F2` - Amber CRT
- `F3` - Neon High-Contrast

The palette changes immediately without modifying the CHIP-8 display memory.

## Timing

The emulator updates the CHIP-8 delay and sound timers at 60 Hz using a high-resolution SDL performance counter. CPU speed is controlled independently by the cycles-per-frame setting.

## Notes

This implementation uses the traditional 64x32 CHIP-8 display, wrap-around sprite drawing, and the original CHIP-8-style `8xy6`/`8xyE` shift behavior where `Vx` is shifted directly.

`Fx55` and `Fx65` leave `I` unchanged, which is a common modern CHIP-8 compatibility choice.

## Troubleshooting

If CMake cannot find SDL2, verify that the SDL2 development package and `pkg-config` are installed:

```bash
pkg-config --modversion sdl2
```

That command should print the installed SDL2 version.
