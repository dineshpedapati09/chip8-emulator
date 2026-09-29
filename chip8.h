#ifndef CHIP8_H
#define CHIP8_H

#include <cstdint>
#include <string>

class Chip8 {
public:
    Chip8();

    bool load_rom(const std::string& filename);
    void emulate_cycle();
    void update_timers();

    bool draw_flag;
    uint8_t display[64 * 32];
    uint8_t key[16];

    uint8_t get_sound_timer() const { return sound_timer; }

private:
    uint8_t memory[4096];
    uint8_t v[16];
    uint16_t index;
    uint16_t pc;
    uint16_t stack[16];
    uint8_t sp;
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint16_t opcode;

    void initialise();
    void load_fonts();
};

#endif
