#include "chip8.h"

#include <fstream>
#include <iostream>
#include <random>
#include <type_traits>

namespace {
constexpr std::uint8_t FONTSET[] = {
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
    memory.fill(0);
    v.fill(0);
    display.fill(0);
    key.fill(0);
    stack.fill(0);

    pc = PROGRAM_START;
    opcode = 0;
    index = 0;
    sp = 0;
    delay_timer = 0;
    sound_timer = 0;
    draw_flag = true;

    load_fonts();
}

void Chip8::load_fonts() {
    for (std::size_t i = 0; i < sizeof(FONTSET); ++i) {
        memory[i] = FONTSET[i];
    }
}


namespace {
constexpr std::array<char, 8> STATE_MAGIC = {'C', '8', 'S', 'T', 'A', 'T', 'E', '\0'};
constexpr std::uint32_t STATE_VERSION = 1;

template <typename T>
bool write_binary(std::ofstream& file, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return static_cast<bool>(file);
}

template <typename T>
bool read_binary(std::ifstream& file, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(file);
}

template <typename T, std::size_t N>
bool write_array(std::ofstream& file, const std::array<T, N>& values) {
    static_assert(std::is_trivially_copyable_v<T>);
    file.write(reinterpret_cast<const char*>(values.data()),
               static_cast<std::streamsize>(sizeof(T) * N));
    return static_cast<bool>(file);
}

template <typename T, std::size_t N>
bool read_array(std::ifstream& file, std::array<T, N>& values) {
    static_assert(std::is_trivially_copyable_v<T>);
    file.read(reinterpret_cast<char*>(values.data()),
              static_cast<std::streamsize>(sizeof(T) * N));
    return static_cast<bool>(file);
}
}

bool Chip8::save_state(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to create save state: " << filename << '\n';
        return false;
    }

    const std::uint16_t reserved = 0;
    const std::uint8_t saved_draw_flag = draw_flag ? 1U : 0U;

    const bool ok =
        file.write(STATE_MAGIC.data(),
                   static_cast<std::streamsize>(STATE_MAGIC.size())) &&
        write_binary(file, STATE_VERSION) &&
        write_array(file, memory) &&
        write_array(file, v) &&
        write_binary(file, index) &&
        write_binary(file, pc) &&
        write_array(file, stack) &&
        write_binary(file, sp) &&
        write_binary(file, delay_timer) &&
        write_binary(file, sound_timer) &&
        write_binary(file, opcode) &&
        write_array(file, display) &&
        write_binary(file, saved_draw_flag) &&
        write_binary(file, reserved);

    if (!ok) {
        std::cerr << "Failed to write save state: " << filename << '\n';
        return false;
    }

    std::cout << "Saved state: " << filename << '\n';
    return true;
}

bool Chip8::load_state(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open save state: " << filename << '\n';
        return false;
    }

    std::array<char, STATE_MAGIC.size()> magic{};
    std::uint32_t version = 0;
    std::uint8_t loaded_draw_flag = 0;
    std::uint16_t reserved = 0;

    const bool header_ok =
        file.read(magic.data(), static_cast<std::streamsize>(magic.size())) &&
        read_binary(file, version);

    if (!header_ok || magic != STATE_MAGIC || version != STATE_VERSION) {
        std::cerr << "Invalid or unsupported save state: " << filename << '\n';
        return false;
    }

    const bool ok =
        read_array(file, memory) &&
        read_array(file, v) &&
        read_binary(file, index) &&
        read_binary(file, pc) &&
        read_array(file, stack) &&
        read_binary(file, sp) &&
        read_binary(file, delay_timer) &&
        read_binary(file, sound_timer) &&
        read_binary(file, opcode) &&
        read_array(file, display) &&
        read_binary(file, loaded_draw_flag) &&
        read_binary(file, reserved);

    if (!ok || sp > STACK_SIZE || pc >= MEMORY_SIZE) {
        std::cerr << "Corrupt save state: " << filename << '\n';
        initialise();
        return false;
    }

    draw_flag = loaded_draw_flag != 0;
    key.fill(0);
    draw_flag = true;
    std::cout << "Loaded state: " << filename << '\n';
    return true;
}

bool Chip8::load_rom(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file) {
        std::cerr << "Failed to open ROM: " << filename << '\n';
        return false;
    }

    const std::streamoff size = file.tellg();
    if (size < 0) {
        std::cerr << "Failed to determine ROM size: " << filename << '\n';
        return false;
    }

    const std::streamoff max_size =
        static_cast<std::streamoff>(MEMORY_SIZE - PROGRAM_START);

    if (size > max_size) {
        std::cerr << "ROM is too large: " << size
                  << " bytes; maximum is " << max_size << " bytes.\n";
        return false;
    }

    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(memory.data() + PROGRAM_START),
                   static_cast<std::streamsize>(size))) {
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
    // A CHIP-8 instruction is two bytes. Guard the fetch so a corrupt
    // program cannot read beyond the 4 KiB address space.
    if (pc >= MEMORY_SIZE - 1) {
        std::cerr << "Program counter out of bounds: 0x"
                  << std::hex << pc << std::dec << '\n';
        return;
    }

    opcode = static_cast<std::uint16_t>(memory[pc] << 8) | memory[pc + 1];

    const std::uint8_t x = static_cast<std::uint8_t>((opcode & 0x0F00) >> 8);
    const std::uint8_t y = static_cast<std::uint8_t>((opcode & 0x00F0) >> 4);
    const std::uint8_t kk = static_cast<std::uint8_t>(opcode & 0x00FF);
    const std::uint16_t nnn = opcode & 0x0FFF;

    switch (opcode & 0xF000) {
    case 0x0000:
        switch (opcode & 0x00FF) {
        case 0x00E0:
            display.fill(0);
            draw_flag = true;
            pc += 2;
            break;

        case 0x00EE:
            if (sp == 0) {
                std::cerr << "Stack underflow on RET\n";
                pc += 2;
            } else {
                --sp;
                pc = static_cast<std::uint16_t>(stack[sp] + 2);
            }
            break;

        default:
            // 0NNN is an RCA 1802/SCHIP-era SYS call. It is commonly
            // ignored by modern CHIP-8 interpreters.
            pc += 2;
            break;
        }
        break;

    case 0x1000:
        pc = nnn;
        break;

    case 0x2000:
        if (sp >= STACK_SIZE) {
            std::cerr << "Stack overflow on CALL\n";
            pc += 2;
        } else {
            stack[sp] = pc;
            ++sp;
            pc = nnn;
        }
        break;

    case 0x3000:
        pc = static_cast<std::uint16_t>(pc + ((v[x] == kk) ? 4u : 2u));
        break;

    case 0x4000:
        pc = static_cast<std::uint16_t>(pc + ((v[x] != kk) ? 4u : 2u));
        break;

    case 0x5000:
        if ((opcode & 0x000F) != 0) {
            std::cerr << "Unknown opcode: 0x"
                      << std::hex << opcode << std::dec << '\n';
            pc += 2;
        } else {
            pc = static_cast<std::uint16_t>(pc + ((v[x] == v[y]) ? 4u : 2u));
        }
        break;

    case 0x6000:
        v[x] = kk;
        pc += 2;
        break;

    case 0x7000:
        v[x] = static_cast<std::uint8_t>(v[x] + kk);
        pc += 2;
        break;

    case 0x8000:
        switch (opcode & 0x000F) {
        case 0x0:
            v[x] = v[y];
            pc += 2;
            break;

        case 0x1:
            v[x] |= v[y];
            pc += 2;
            break;

        case 0x2:
            v[x] &= v[y];
            pc += 2;
            break;

        case 0x3:
            v[x] ^= v[y];
            pc += 2;
            break;

        case 0x4: {
            const std::uint16_t sum =
                static_cast<std::uint16_t>(v[x]) + v[y];
            v[0xF] = (sum > 0xFF) ? 1 : 0;
            v[x] = static_cast<std::uint8_t>(sum);
            pc += 2;
            break;
        }

        case 0x5:
            v[0xF] = (v[x] >= v[y]) ? 1 : 0;
            v[x] = static_cast<std::uint8_t>(v[x] - v[y]);
            pc += 2;
            break;

        case 0x6:
            v[0xF] = static_cast<std::uint8_t>(v[x] & 0x01);
            v[x] >>= 1;
            pc += 2;
            break;

        case 0x7:
            v[0xF] = (v[y] >= v[x]) ? 1 : 0;
            v[x] = static_cast<std::uint8_t>(v[y] - v[x]);
            pc += 2;
            break;

        case 0xE:
            v[0xF] = static_cast<std::uint8_t>((v[x] >> 7) & 0x01);
            v[x] = static_cast<std::uint8_t>(v[x] << 1);
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
            pc = static_cast<std::uint16_t>(pc + ((v[x] != v[y]) ? 4u : 2u));
        }
        break;

    case 0xA000:
        index = nnn;
        pc += 2;
        break;

    case 0xB000:
        pc = static_cast<std::uint16_t>(nnn + v[0]);
        break;

    case 0xC000: {
        static std::mt19937 generator{std::random_device{}()};
        static std::uniform_int_distribution<int> distribution(0, 255);
        const auto random_byte =
            static_cast<std::uint8_t>(distribution(generator));
        v[x] = static_cast<std::uint8_t>(random_byte & kk);
        pc += 2;
        break;
    }

    case 0xD000: {
        const std::uint8_t start_x = v[x];
        const std::uint8_t start_y = v[y];
        const std::uint8_t height =
            static_cast<std::uint8_t>(opcode & 0x000F);

        v[0xF] = 0;

        for (std::uint8_t row = 0; row < height; ++row) {
            const std::uint16_t sprite_address =
                static_cast<std::uint16_t>(index + row);

            if (sprite_address >= MEMORY_SIZE) {
                break;
            }

            const std::uint8_t sprite_byte = memory[sprite_address];

            for (int bit = 0; bit < 8; ++bit) {
                if ((sprite_byte & (0x80 >> bit)) == 0) {
                    continue;
                }

                const std::size_t screen_x =
                    (static_cast<std::size_t>(start_x) + static_cast<std::size_t>(bit)) % DISPLAY_WIDTH;
                const std::size_t screen_y =
                    (start_y + row) % DISPLAY_HEIGHT;
                const std::size_t screen_index =
                    screen_x + screen_y * DISPLAY_WIDTH;

                if (display[screen_index] != 0) {
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
        case 0x9E:
            pc = static_cast<std::uint16_t>(pc + ((key[v[x] & 0x0F] != 0) ? 4u : 2u));
            break;

        case 0xA1:
            pc = static_cast<std::uint16_t>(pc + ((key[v[x] & 0x0F] == 0) ? 4u : 2u));
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
        case 0x07:
            v[x] = delay_timer;
            pc += 2;
            break;

        case 0x0A: {
            bool key_pressed = false;

            for (std::size_t i = 0; i < KEY_COUNT; ++i) {
                if (key[i] != 0) {
                    v[x] = static_cast<std::uint8_t>(i);
                    key_pressed = true;
                    break;
                }
            }

            if (key_pressed) {
                pc += 2;
            }
            break;
        }

        case 0x15:
            delay_timer = v[x];
            pc += 2;
            break;

        case 0x18:
            sound_timer = v[x];
            pc += 2;
            break;

        case 0x1E:
            index = static_cast<std::uint16_t>(index + v[x]);
            pc += 2;
            break;

        case 0x29:
            index = static_cast<std::uint16_t>((v[x] & 0x0F) * 5);
            pc += 2;
            break;

        case 0x33: {
            const std::uint8_t value = v[x];

            if (index <= MEMORY_SIZE - 3) {
                memory[index] = static_cast<std::uint8_t>(value / 100);
                memory[index + 1] =
                    static_cast<std::uint8_t>((value / 10) % 10);
                memory[index + 2] =
                    static_cast<std::uint8_t>(value % 10);
            } else {
                std::cerr << "FX33 memory write out of bounds\n";
            }

            pc += 2;
            break;
        }

        case 0x55:
            for (std::size_t i = 0; i <= x; ++i) {
                if (index + i >= MEMORY_SIZE) {
                    break;
                }
                memory[index + i] = v[i];
            }
            pc += 2;
            break;

        case 0x65:
            for (std::size_t i = 0; i <= x; ++i) {
                if (index + i >= MEMORY_SIZE) {
                    break;
                }
                v[i] = memory[index + i];
            }
            pc += 2;
            break;

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
