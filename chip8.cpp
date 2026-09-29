#include "chip8.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

namespace {
constexpr uint16_t PROGRAM_START = 0x200;
constexpr uint16_t MEMORY_SIZE = 4096;
constexpr uint8_t DISPLAY_WIDTH = 64;
constexpr uint8_t DISPLAY_HEIGHT = 32;

const uint8_t chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};
}

Chip8::Chip8() {
    initialise();
}

void Chip8::initialise() {
    pc = PROGRAM_START;
    opcode = 0;
    index = 0;
    sp = 0;

    std::memset(display, 0, sizeof(display));
    std::memset(stack, 0, sizeof(stack));
    std::memset(v, 0, sizeof(v));
    std::memset(memory, 0, sizeof(memory));
    std::memset(key, 0, sizeof(key));

    load_fonts();

    delay_timer = 0;
    sound_timer = 0;
    draw_flag = true;
}

void Chip8::load_fonts() {
    for (int i = 0; i < 80; ++i) {
        memory[i] = chip8_fontset[i];
    }
}

bool Chip8::load_rom(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        std::cerr << "Failed to open ROM: " << filename << '\n';
        return false;
    }

    const std::streamsize size = file.tellg();
    if (size < 0) {
        std::cerr << "Failed to determine ROM size: " << filename << '\n';
        return false;
    }

    const std::streamsize max_size = MEMORY_SIZE - PROGRAM_START;
    if (size > max_size) {
        std::cerr << "ROM too large to fit in memory\n";
        return false;
    }

    file.seekg(0, std::ios::beg);

    if (!file.read(reinterpret_cast<char*>(memory + PROGRAM_START), size)) {
        std::cerr << "Failed to read ROM: " << filename << '\n';
        return false;
    }

    std::cout << "Loaded ROM: " << filename << '\n';
    return true;
}

void Chip8::update_timers() {
    if (delay_timer > 0) {
        --delay_timer;
    }

    if (sound_timer > 0) {
        --sound_timer;
    }
}

void Chip8::emulate_cycle() {
    opcode = static_cast<uint16_t>(memory[pc] << 8) | memory[pc + 1];

    const uint8_t x = (opcode & 0x0F00) >> 8;
    const uint8_t y = (opcode & 0x00F0) >> 4;

    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode & 0x00FF) {
                case 0x00E0:
                    std::memset(display, 0, sizeof(display));
                    draw_flag = true;
                    pc += 2;
                    break;

                case 0x00EE:
                    if (sp == 0) {
                        std::cerr << "Stack underflow on RET\n";
                        pc += 2;
                    } else {
                        --sp;
                        pc = stack[sp] + 2;
                    }
                    break;

                default:
                    std::cerr << "Unknown opcode: 0x"
                              << std::hex << opcode << std::dec << '\n';
                    pc += 2;
                    break;
            }
            break;

        case 0x1000:
            pc = opcode & 0x0FFF;
            break;

        case 0x2000:
            if (sp >= 16) {
                std::cerr << "Stack overflow on CALL\n";
                pc += 2;
            } else {
                stack[sp] = pc;
                ++sp;
                pc = opcode & 0x0FFF;
            }
            break;

        case 0x3000:
            pc += (v[x] == (opcode & 0x00FF)) ? 4 : 2;
            break;

        case 0x4000:
            pc += (v[x] != (opcode & 0x00FF)) ? 4 : 2;
            break;

        case 0x5000:
            if ((opcode & 0x000F) != 0) {
                std::cerr << "Unknown opcode: 0x"
                          << std::hex << opcode << std::dec << '\n';
                pc += 2;
            } else {
                pc += (v[x] == v[y]) ? 4 : 2;
            }
            break;

        case 0x6000:
            v[x] = opcode & 0x00FF;
            pc += 2;
            break;

        case 0x7000:
            v[x] += opcode & 0x00FF;
            pc += 2;
            break;

        case 0x8000:
            switch (opcode & 0x000F) {
                case 0x0000:
                    v[x] = v[y];
                    pc += 2;
                    break;

                case 0x0001:
                    v[x] |= v[y];
                    pc += 2;
                    break;

                case 0x0002:
                    v[x] &= v[y];
                    pc += 2;
                    break;

                case 0x0003:
                    v[x] ^= v[y];
                    pc += 2;
                    break;

                case 0x0004: {
                    const uint16_t sum = static_cast<uint16_t>(v[x]) + v[y];
                    v[0xF] = (sum > 0xFF) ? 1 : 0;
                    v[x] = static_cast<uint8_t>(sum & 0xFF);
                    pc += 2;
                    break;
                }

                case 0x0005:
                    v[0xF] = (v[x] >= v[y]) ? 1 : 0;
                    v[x] = static_cast<uint8_t>(v[x] - v[y]);
                    pc += 2;
                    break;

                case 0x0006:
                    v[0xF] = v[x] & 0x01;
                    v[x] >>= 1;
                    pc += 2;
                    break;

                case 0x0007:
                    v[0xF] = (v[y] >= v[x]) ? 1 : 0;
                    v[x] = static_cast<uint8_t>(v[y] - v[x]);
                    pc += 2;
                    break;

                case 0x000E:
                    v[0xF] = (v[x] >> 7) & 0x01;
                    v[x] <<= 1;
                    pc += 2;
                    break;

                default:
                    std::cerr << "Unknown opcode: 0x"
                              << std::hex << opcode << std::dec << '\n';
                    pc += 2;
                    break;
            }
            break;

        case 0x9000:
            if ((opcode & 0x000F) != 0) {
                std::cerr << "Unknown opcode: 0x"
                          << std::hex << opcode << std::dec << '\n';
                pc += 2;
            } else {
                pc += (v[x] != v[y]) ? 4 : 2;
            }
            break;

        case 0xA000:
            index = opcode & 0x0FFF;
            pc += 2;
            break;

        case 0xB000:
            pc = static_cast<uint16_t>((opcode & 0x0FFF) + v[0]);
            break;

        case 0xC000: {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<int> dist(0, 255);

            v[x] = static_cast<uint8_t>(dist(gen)) & (opcode & 0x00FF);
            pc += 2;
            break;
        }

        case 0xD000: {
            const uint8_t start_x = v[x];
            const uint8_t start_y = v[y];
            const uint8_t height = opcode & 0x000F;

            v[0xF] = 0;

            for (int y_line = 0; y_line < height; ++y_line) {
                const uint16_t sprite_address = index + y_line;
                if (sprite_address >= MEMORY_SIZE) {
                    break;
                }

                const uint8_t pixel = memory[sprite_address];

                for (int x_line = 0; x_line < 8; ++x_line) {
                    if ((pixel & (0x80 >> x_line)) == 0) {
                        continue;
                    }

                    const int screen_x = (start_x + x_line) % DISPLAY_WIDTH;
                    const int screen_y = (start_y + y_line) % DISPLAY_HEIGHT;
                    const int screen_index =
                        screen_x + screen_y * DISPLAY_WIDTH;

                    if (display[screen_index] == 1) {
                        v[0xF] = 1;
                    }

                    display[screen_index] ^= 1;
                }
            }

            draw_flag = true;
            pc += 2;
            break;
        }

        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x009E:
                    pc += (key[v[x] & 0x0F] != 0) ? 4 : 2;
                    break;

                case 0x00A1:
                    pc += (key[v[x] & 0x0F] == 0) ? 4 : 2;
                    break;

                default:
                    std::cerr << "Unknown opcode: 0x"
                              << std::hex << opcode << std::dec << '\n';
                    pc += 2;
                    break;
            }
            break;

        case 0xF000:
            switch (opcode & 0x00FF) {
                case 0x0007:
                    v[x] = delay_timer;
                    pc += 2;
                    break;

                case 0x000A: {
                    bool key_pressed = false;

                    for (int i = 0; i < 16; ++i) {
                        if (key[i] != 0) {
                            v[x] = static_cast<uint8_t>(i);
                            key_pressed = true;
                            break;
                        }
                    }

                    // Do not advance PC until a key is pressed.
                    if (key_pressed) {
                        pc += 2;
                    }
                    break;
                }

                case 0x0015:
                    delay_timer = v[x];
                    pc += 2;
                    break;

                case 0x0018:
                    sound_timer = v[x];
                    pc += 2;
                    break;

                case 0x001E:
                    index += v[x];
                    pc += 2;
                    break;

                case 0x0029:
                    index = static_cast<uint16_t>((v[x] & 0x0F) * 5);
                    pc += 2;
                    break;

                case 0x0033: {
                    const uint8_t value = v[x];

                    if (index + 2 < MEMORY_SIZE) {
                        memory[index] = value / 100;
                        memory[index + 1] = (value / 10) % 10;
                        memory[index + 2] = value % 10;
                    }

                    pc += 2;
                    break;
                }

                case 0x0055: {
                    for (int i = 0; i <= x && index + i < MEMORY_SIZE; ++i) {
                        memory[index + i] = v[i];
                    }
                    pc += 2;
                    break;
                }

                case 0x0065: {
                    for (int i = 0; i <= x && index + i < MEMORY_SIZE; ++i) {
                        v[i] = memory[index + i];
                    }
                    pc += 2;
                    break;
                }

                default:
                    std::cerr << "Unknown opcode: 0x"
                              << std::hex << opcode << std::dec << '\n';
                    pc += 2;
                    break;
            }
            break;

        default:
            std::cerr << "Unknown opcode: 0x"
                      << std::hex << opcode << std::dec << '\n';
            pc += 2;
            break;
    }
}
