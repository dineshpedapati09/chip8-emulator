#include "chip8.h"

#include <SDL2/SDL.h>

#include <atomic>
#include <cstdint>
#include <iostream>

namespace {
constexpr int SCALE = 10;
constexpr int WIDTH = static_cast<int>(Chip8::DISPLAY_WIDTH) * SCALE;
constexpr int HEIGHT = static_cast<int>(Chip8::DISPLAY_HEIGHT) * SCALE;

constexpr int DEFAULT_CPU_CYCLES_PER_FRAME = 10;
constexpr int MIN_CPU_CYCLES_PER_FRAME = 1;
constexpr int MAX_CPU_CYCLES_PER_FRAME = 1000;
constexpr std::uint64_t MICROSECONDS_PER_SECOND = 1000000ULL;
constexpr std::uint64_t FRAME_TIME_US = MICROSECONDS_PER_SECOND / 60ULL;
constexpr const char* STATE_FILE = "chip8_state.bin";

struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

struct Palette {
    const char* name;
    Color background;
    Color foreground;
};

constexpr Palette PALETTES[] = {
    {"Classic Green Screen", {0, 0, 0}, {120, 255, 120}},
    {"Amber CRT", {0, 0, 0}, {255, 190, 60}},
    {"Neon High-Contrast", {10, 0, 20}, {255, 255, 255}}
};

constexpr std::size_t PALETTE_COUNT = sizeof(PALETTES) / sizeof(PALETTES[0]);

const SDL_Keycode KEYMAP[Chip8::KEY_COUNT] = {
    SDLK_x, // 0
    SDLK_1, // 1
    SDLK_2, // 2
    SDLK_3, // 3
    SDLK_q, // 4
    SDLK_w, // 5
    SDLK_e, // 6
    SDLK_a, // 7
    SDLK_s, // 8
    SDLK_d, // 9
    SDLK_z, // A
    SDLK_c, // B
    SDLK_4, // C
    SDLK_r, // D
    SDLK_f, // E
    SDLK_v  // F
};

void print_controls(int cycles_per_frame, std::size_t palette_index) {
    std::cout << "CPU speed: " << cycles_per_frame
              << " cycles/frame (~" << cycles_per_frame * 60
              << " cycles/sec)\n";
    std::cout << "Palette: " << PALETTES[palette_index].name << '\n';
}

void audio_callback(void* userdata, std::uint8_t* stream, int len) {
    static double phase = 0.0;

    constexpr double SAMPLE_RATE = 44100.0;
    constexpr double FREQUENCY = 440.0;
    constexpr double PHASE_STEP = FREQUENCY / SAMPLE_RATE;
    constexpr std::int16_t AMPLITUDE = 3000;

    auto* beeping = static_cast<std::atomic<bool>*>(userdata);
    auto* audio_buffer = reinterpret_cast<std::int16_t*>(stream);
    const int samples = len / static_cast<int>(sizeof(std::int16_t));

    for (int i = 0; i < samples; ++i) {
        if (beeping->load(std::memory_order_relaxed)) {
            audio_buffer[i] = (phase < 0.5) ? AMPLITUDE : -AMPLITUDE;
            phase += PHASE_STEP;
            if (phase >= 1.0) {
                phase -= 1.0;
            }
        } else {
            audio_buffer[i] = 0;
            phase = 0.0;
        }
    }
}

void draw_graphics(SDL_Renderer* renderer,
                   const Chip8& chip8,
                   const Palette& palette) {
    SDL_SetRenderDrawColor(renderer,
                           palette.background.r,
                           palette.background.g,
                           palette.background.b,
                           255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer,
                           palette.foreground.r,
                           palette.foreground.g,
                           palette.foreground.b,
                           255);

    for (std::size_t y = 0; y < Chip8::DISPLAY_HEIGHT; ++y) {
        for (std::size_t x = 0; x < Chip8::DISPLAY_WIDTH; ++x) {
            const std::size_t index = x + y * Chip8::DISPLAY_WIDTH;
            if (chip8.display[index] == 0) {
                continue;
            }

            SDL_Rect rect{
                static_cast<int>(x) * SCALE,
                static_cast<int>(y) * SCALE,
                SCALE,
                SCALE
            };
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    SDL_RenderPresent(renderer);
}

void handle_input(Chip8& chip8,
                  bool& running,
                  int& cycles_per_frame,
                  std::size_t& palette_index,
                  bool& state_message_pending) {
    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            continue;
        }

        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            const SDL_Keycode keycode = event.key.keysym.sym;

            if (keycode == SDLK_ESCAPE) {
                running = false;
                continue;
            }

            if (keycode == SDLK_EQUALS || keycode == SDLK_KP_PLUS) {
                if (cycles_per_frame < MAX_CPU_CYCLES_PER_FRAME) {
                    ++cycles_per_frame;
                }
                print_controls(cycles_per_frame, palette_index);
                continue;
            }

            if (keycode == SDLK_MINUS || keycode == SDLK_KP_MINUS) {
                if (cycles_per_frame > MIN_CPU_CYCLES_PER_FRAME) {
                    --cycles_per_frame;
                }
                print_controls(cycles_per_frame, palette_index);
                continue;
            }

            if (keycode == SDLK_0) {
                cycles_per_frame = DEFAULT_CPU_CYCLES_PER_FRAME;
                print_controls(cycles_per_frame, palette_index);
                continue;
            }

            if (keycode == SDLK_F1 || keycode == SDLK_F2 || keycode == SDLK_F3) {
                palette_index = static_cast<std::size_t>(keycode - SDLK_F1);
                print_controls(cycles_per_frame, palette_index);
                continue;
            }

            if (keycode == SDLK_F5) {
                state_message_pending = chip8.save_state(STATE_FILE);
                continue;
            }

            if (keycode == SDLK_F9) {
                state_message_pending = chip8.load_state(STATE_FILE);
                continue;
            }

            for (std::size_t i = 0; i < Chip8::KEY_COUNT; ++i) {
                if (keycode == KEYMAP[i]) {
                    chip8.key[i] = 1;
                    break;
                }
            }
        }

        if (event.type == SDL_KEYUP) {
            for (std::size_t i = 0; i < Chip8::KEY_COUNT; ++i) {
                if (event.key.keysym.sym == KEYMAP[i]) {
                    chip8.key[i] = 0;
                    break;
                }
            }
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <ROM file>\n";
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return 1;
    }

    std::atomic<bool> beeping{false};

    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = &beeping;

    const SDL_AudioDeviceID audio_device =
        SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);

    if (audio_device == 0) {
        std::cerr << "Audio disabled: " << SDL_GetError() << '\n';
    } else {
        SDL_PauseAudioDevice(audio_device, 0);
    }

    SDL_Window* window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH,
        HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
        if (audio_device != 0) {
            SDL_CloseAudioDevice(audio_device);
        }
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        if (audio_device != 0) {
            SDL_CloseAudioDevice(audio_device);
        }
        SDL_Quit();
        return 1;
    }

    Chip8 chip8;
    if (!chip8.load_rom(argv[1])) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (audio_device != 0) {
            SDL_CloseAudioDevice(audio_device);
        }
        SDL_Quit();
        return 1;
    }

    bool running = true;
    int cycles_per_frame = DEFAULT_CPU_CYCLES_PER_FRAME;
    std::size_t palette_index = 0;
    bool state_message_pending = false;

    std::uint64_t timer_accumulator_us = 0;
    std::uint64_t previous_counter = SDL_GetPerformanceCounter();
    const std::uint64_t performance_frequency = SDL_GetPerformanceFrequency();

    print_controls(cycles_per_frame, palette_index);
    std::cout << "F1/F2/F3: display palette | +/-: CPU speed | 0: reset speed\n"
              << "F5: save state | F9: load state | Esc: quit\n";

    while (running) {
        const std::uint64_t frame_start = SDL_GetPerformanceCounter();

        handle_input(chip8, running, cycles_per_frame, palette_index,
                     state_message_pending);

        for (int i = 0; i < cycles_per_frame && running; ++i) {
            chip8.emulate_cycle();
        }

        const std::uint64_t now = SDL_GetPerformanceCounter();
        const std::uint64_t elapsed_ticks = now - previous_counter;
        previous_counter = now;

        const std::uint64_t elapsed_us =
            (elapsed_ticks * MICROSECONDS_PER_SECOND) /
            performance_frequency;
        timer_accumulator_us += elapsed_us;

        while (timer_accumulator_us >= FRAME_TIME_US) {
            chip8.update_timers();
            timer_accumulator_us -= FRAME_TIME_US;
        }

        beeping.store(chip8.get_sound_timer() > 0,
                      std::memory_order_relaxed);

        if (chip8.draw_flag) {
            draw_graphics(renderer, chip8, PALETTES[palette_index]);
            chip8.draw_flag = false;
        }

        const std::uint64_t frame_elapsed_ticks =
            SDL_GetPerformanceCounter() - frame_start;
        const std::uint64_t frame_elapsed_us =
            (frame_elapsed_ticks * MICROSECONDS_PER_SECOND) /
            performance_frequency;

        if (frame_elapsed_us < FRAME_TIME_US) {
            const std::uint32_t delay_ms = static_cast<std::uint32_t>(
                (FRAME_TIME_US - frame_elapsed_us) / 1000ULL);
            if (delay_ms > 0) {
                SDL_Delay(delay_ms);
            }
        }

        if (state_message_pending) {
            state_message_pending = false;
        }
    }

    beeping.store(false, std::memory_order_relaxed);

    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 1);
        SDL_CloseAudioDevice(audio_device);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
