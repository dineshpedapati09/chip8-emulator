#include "chip8.h"

#include <SDL2/SDL.h>
#include <cstdint>
#include <iostream>
#include <atomic>
#include <cmath>

constexpr int SCALE = 10;
constexpr int WIDTH = 64 * SCALE;
constexpr int HEIGHT = 32 * SCALE;
constexpr int CPU_CYCLES_PER_FRAME = 10;
constexpr uint32_t TIMER_INTERVAL_MS = 1000 / 60;

// Keyboard mapping
const SDL_Keycode keymap[16] = {
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

void audio_callback(void* userdata, uint8_t* stream, int len) {
    static double phase = 0.0;
    constexpr double sample_rate = 44100.0;
    constexpr double frequency = 440.0;
    constexpr double phase_step = frequency / sample_rate;

    auto* beeping = static_cast<std::atomic<bool>*>(userdata);
    auto* audio_buffer = reinterpret_cast<int16_t*>(stream);
    const int samples = len / static_cast<int>(sizeof(int16_t));

    for (int i = 0; i < samples; ++i) {
        if (beeping->load(std::memory_order_relaxed)) {
            audio_buffer[i] = (phase < 0.5) ? 3000 : -3000;
            phase += phase_step;

            if (phase >= 1.0) {
                phase -= 1.0;
            }
        } else {
            audio_buffer[i] = 0;
            phase = 0.0;
        }
    }
}

void draw_graphics(SDL_Renderer* renderer, const Chip8& chip8) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (chip8.display[x + y * 64] == 1) {
                SDL_Rect rect = {
                    x * SCALE,
                    (31 - y) * SCALE,
                    SCALE,
                    SCALE
                };
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

void handle_input(Chip8& chip8, bool& running) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }

        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }

            for (int i = 0; i < 16; ++i) {
                if (event.key.keysym.sym == keymap[i]) {
                    chip8.key[i] = 1;
                }
            }
        }

        if (event.type == SDL_KEYUP) {
            for (int i = 0; i < 16; ++i) {
                if (event.key.keysym.sym == keymap[i]) {
                    chip8.key[i] = 0;
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <ROM file>\n";
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL Error: " << SDL_GetError() << '\n';
        return 1;
    }

    std::atomic<bool> beeping{false};

    SDL_AudioSpec want{};
    SDL_AudioSpec have{};

    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audio_callback;
    want.userdata = &beeping;

    SDL_AudioDeviceID audio_device =
        SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

    if (audio_device == 0) {
        std::cerr << "Audio disabled: " << SDL_GetError() << '\n';
    } else {
        SDL_PauseAudioDevice(audio_device, 0);
    }

    SDL_Window* window = SDL_CreateWindow(
        "Chip-8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH,
        HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "Window error: " << SDL_GetError() << '\n';

        if (audio_device != 0) {
            SDL_CloseAudioDevice(audio_device);
        }

        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!renderer) {
        std::cerr << "Renderer error: " << SDL_GetError() << '\n';
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
    uint32_t last_timer_update = SDL_GetTicks();

    while (running) {
        const uint32_t frame_start = SDL_GetTicks();

        handle_input(chip8, running);

        // Run several CPU instructions each frame.
        for (int i = 0; i < CPU_CYCLES_PER_FRAME; ++i) {
            chip8.emulate_cycle();
        }

        // CHIP-8 timers run at 60 Hz, independently of CPU cycles.
        const uint32_t now = SDL_GetTicks();

        if (now - last_timer_update >= TIMER_INTERVAL_MS) {
            chip8.update_timers();
            last_timer_update = now;
        }

        beeping.store(chip8.get_sound_timer() > 0,
                      std::memory_order_relaxed);

        if (chip8.draw_flag) {
            draw_graphics(renderer, chip8);
            chip8.draw_flag = false;
        }

        const uint32_t frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < TIMER_INTERVAL_MS) {
            SDL_Delay(TIMER_INTERVAL_MS - frame_time);
        }
    }

    beeping.store(false, std::memory_order_relaxed);

    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
