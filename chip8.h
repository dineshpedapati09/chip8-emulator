#ifndef CHIP8_H
#define CHIP8_H

#include <array>
#include <cstdint>
#include <string>

class Chip8 {
public:
    static constexpr std::uint16_t PROGRAM_START = 0x200;
    static constexpr std::size_t MEMORY_SIZE = 4096;
    static constexpr std::size_t DISPLAY_WIDTH = 64;
    static constexpr std::size_t DISPLAY_HEIGHT = 32;
    static constexpr std::size_t DISPLAY_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    static constexpr std::size_t KEY_COUNT = 16;
    static constexpr std::size_t REGISTER_COUNT = 16;
    static constexpr std::size_t STACK_SIZE = 16;

    Chip8();

    bool load_rom(const std::string& filename);
    bool save_state(const std::string& filename) const;
    bool load_state(const std::string& filename);

    void emulate_cycle();
    void update_timers();

    bool draw_flag = false;
    std::array<std::uint8_t, DISPLAY_SIZE> display{};
    std::array<std::uint8_t, KEY_COUNT> key{};

    std::uint8_t get_sound_timer() const noexcept { return sound_timer; }

private:
    std::array<std::uint8_t, MEMORY_SIZE> memory{};
    std::array<std::uint8_t, REGISTER_COUNT> v{};
    std::uint16_t index = 0;
    std::uint16_t pc = PROGRAM_START;
    std::array<std::uint16_t, STACK_SIZE> stack{};
    std::uint8_t sp = 0;
    std::uint8_t delay_timer = 0;
    std::uint8_t sound_timer = 0;
    std::uint16_t opcode = 0;

    void initialise();
    void load_fonts();
};

#endif
