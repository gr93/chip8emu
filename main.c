#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "stack.h"
//cl main.c stack.c /I include /link lib/x64/SDL2.lib && main

// ==========================
// Configuration Constants
// ==========================
#define RESOLUTION_WIDTH 64
#define RESOLUTION_HEIGHT 32
#define SHIFT_USES_VY 1
#define TARGET_FPS 60
#define FRAME_DELAY (1000 / TARGET_FPS)
#define CYCLES_PER_FRAME 10
#define MEMORY_SIZE 4096
#define ROM_START_ADDRESS 0x200

// ==========================
// Keyboard Functions
// ==========================
SDL_Keycode chip8_to_key(int chip8_key) {
    switch(chip8_key) {
        case 0x0: return SDLK_x;
        case 0x1: return SDLK_1;
        case 0x2: return SDLK_2;
        case 0x3: return SDLK_3;
        case 0x4: return SDLK_q;
        case 0x5: return SDLK_w;
        case 0x6: return SDLK_e;
        case 0x7: return SDLK_a;
        case 0x8: return SDLK_s;
        case 0x9: return SDLK_d;
        case 0xA: return SDLK_z;
        case 0xB: return SDLK_c;
        case 0xC: return SDLK_4;
        case 0xD: return SDLK_r;
        case 0xE: return SDLK_f;
        case 0xF: return SDLK_v;
        default: return SDLK_UNKNOWN;
    }
}

int key_to_chip8(SDL_Keycode key) {
    switch(key) {
        case SDLK_x: return 0x0;
        case SDLK_1: return 0x1;
        case SDLK_2: return 0x2;
        case SDLK_3: return 0x3;
        case SDLK_q: return 0x4;
        case SDLK_w: return 0x5;
        case SDLK_e: return 0x6;
        case SDLK_a: return 0x7;
        case SDLK_s: return 0x8;
        case SDLK_d: return 0x9;
        case SDLK_z: return 0xA;
        case SDLK_c: return 0xB;
        case SDLK_4: return 0xC;
        case SDLK_r: return 0xD;
        case SDLK_f: return 0xE;
        case SDLK_v: return 0xF;
        default: return -1;  // Invalid key
    }
}

int is_key_pressed(int chip8_key) {
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    SDL_Keycode key = chip8_to_key(chip8_key);
    if (key == SDLK_UNKNOWN) return 0;
    SDL_Scancode scancode = SDL_GetScancodeFromKey(key);
    return state[scancode];
}

// ==========================
// Type Definitions
// ==========================
typedef struct {
    unsigned short PC;          // Program Counter
    unsigned short I;           // Index register
    unsigned char V[16];        // General purpose 8-bit registers (V0-VF)
    unsigned char delay_timer;  // Delay timer
    unsigned char sound_timer;  // Sound timer
} Registers;

typedef struct {
    unsigned char byte1;
    unsigned char byte2;
    unsigned char nibble1;
    unsigned char nibble2;
    unsigned char nibble3;
    unsigned char nibble4;
    unsigned short full;
    unsigned short nnn;  // 12-bit address
    unsigned char nn;    // 8-bit constant
    unsigned char x;     // 4-bit register index
    unsigned char y;     // 4-bit register index
    unsigned char n;     // 4-bit constant
} Instruction;

// ==========================
// Global Variables
// ==========================
unsigned short RAM[MEMORY_SIZE];
unsigned char screen[RESOLUTION_WIDTH][RESOLUTION_HEIGHT];
Registers regs;
struct stack call_stack;
int waiting_for_key_reg = -1;
SDL_AudioDeviceID audio_device = 0;

// ==========================
// Audio Callback
// ==========================
void audio_callback(void* userdata, Uint8* stream, int len) {
    static float phase = 0;
    Sint16* buffer = (Sint16*)stream;
    int samples = len / 2;
    
    for (int i = 0; i < samples; i++) {
        buffer[i] = (Sint16)(sin(phase) * 3000);  // 440Hz square wave beep
        phase += 2.0f * 3.14159265f * 440.0f / 44100.0f;
        if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;
    }
}

// ==========================
// CHIP-8 Font Set
// ==========================
unsigned char chip8_fontset[80] = {
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

// ==========================
// Display Functions
// ==========================
void clear_screen() {
    for(int i =0; i < RESOLUTION_WIDTH; i++) {
        for(int j = 0; j < RESOLUTION_HEIGHT; j++) {
            screen[i][j] = 0;
        }
    }
}

void draw_screen(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for(int i = 0; i < RESOLUTION_WIDTH; i++) {
        for(int j = 0; j < RESOLUTION_HEIGHT; j++) {
            if (screen[i][j] == 1) {
                SDL_RenderDrawPoint(renderer, i, j);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

// ==========================
// Instruction Decoding
// ==========================
Instruction decode_instruction(unsigned short pc) {
    Instruction instr;
    instr.byte1 = RAM[pc];
    instr.byte2 = RAM[pc + 1];
    instr.nibble1 = instr.byte1 >> 4;
    instr.nibble2 = instr.byte1 & 0x0F;
    instr.nibble3 = instr.byte2 >> 4;
    instr.nibble4 = instr.byte2 & 0x0F;
    instr.full = (instr.byte1 << 8) | instr.byte2;
    instr.nnn = instr.full & 0x0FFF;
    instr.nn = instr.byte2;
    instr.x = instr.nibble2;
    instr.y = instr.nibble3;
    instr.n = instr.nibble4;
    return instr;
}

// ==========================
// Instruction Execution
// ==========================
void execute_arithmetic(Instruction instr) {
    unsigned char x = instr.x;
    unsigned char y = instr.y;
    
    switch(instr.n) {
        case 0x0:  // VX = VY
            regs.V[x] = regs.V[y];
            break;
        case 0x1:  // VX |= VY
            regs.V[x] = regs.V[x] | regs.V[y];
            break;
        case 0x2:  // VX &= VY
            regs.V[x] = regs.V[x] & regs.V[y];
            break;
        case 0x3:  // VX ^= VY
            regs.V[x] = regs.V[x] ^ regs.V[y];
            break;
        case 0x4: { // VX += VY (with carry)
            unsigned short sum = regs.V[x] + regs.V[y];
            regs.V[x] = sum & 0xFF;
            regs.V[0xF] = (sum > 255) ? 1 : 0;
            break;
        }
        case 0x5: { // VX -= VY (with borrow)
            bool carry = regs.V[x] >= regs.V[y];
            regs.V[x] = regs.V[x] - regs.V[y];
            regs.V[0xF] = carry;
            break;
        }
        case 0x6: { // VX >>= 1
            if (SHIFT_USES_VY) {
                regs.V[x] = regs.V[y];
            }
            unsigned char last_bit = regs.V[x] & 0x01;
            regs.V[x] = regs.V[x] >> 1;
            regs.V[0xF] = last_bit;
            break;
        }
        case 0x7: { // VX = VY - VX (with borrow)
            bool carry = regs.V[y] >= regs.V[x];
            regs.V[x] = regs.V[y] - regs.V[x];
            regs.V[0xF] = carry;
            break;
        }
        case 0xE: { // VX <<= 1
            if (SHIFT_USES_VY) {
                regs.V[x] = regs.V[y];
            }
            unsigned char first_bit = regs.V[x] & 0x80;
            regs.V[x] = regs.V[x] << 1;
            regs.V[0xF] = first_bit >> 7;
            break;
        }
    }
    regs.PC += 2;
}

void execute_misc(Instruction instr) {
    unsigned char x = instr.x;
    unsigned char nibble3 = instr.nibble3;
    unsigned char nibble4 = instr.nibble4;
    
    if (nibble3 == 0x0 && nibble4 == 0x7) {
        regs.V[x] = regs.delay_timer;
        regs.PC += 2;
    }
    else if (nibble3 == 0x1 && nibble4 == 0x5) {
        regs.delay_timer = regs.V[x];
        regs.PC += 2;
    }
    else if (nibble3 == 0x1 && nibble4 == 0x8) {
        regs.sound_timer = regs.V[x];
        regs.PC += 2;
    }
    else if (nibble3 == 0x1 && nibble4 == 0xE) {
        regs.I += regs.V[x];
        regs.PC += 2;
    }
    else if (nibble3 == 0x0 && nibble4 == 0xA) {
        waiting_for_key_reg = x;
    }
    else if (nibble3 == 0x2 && nibble4 == 0x9) {
        regs.I = regs.V[x] * 5;
        regs.PC += 2;
    }
    else if (nibble3 == 0x3 && nibble4 == 0x3) {
        unsigned char val = regs.V[x];
        RAM[regs.I] = val / 100;
        RAM[regs.I + 1] = (val / 10) % 10;
        RAM[regs.I + 2] = val % 10;
        regs.PC += 2;
    }
    else if (nibble3 == 0x5 && nibble4 == 0x5) {
        for(int i = 0; i <= x; i++) {
            RAM[regs.I + i] = regs.V[i];
        }
        regs.PC += 2;
    }
    else if (nibble3 == 0x6 && nibble4 == 0x5) {
        for(int i = 0; i <= x; i++) {
            regs.V[i] = RAM[regs.I + i];
        }
        regs.PC += 2;
    }
}

void execute_draw(Instruction instr, SDL_Renderer *renderer) {
    short x_coord = regs.V[instr.x] % RESOLUTION_WIDTH;
    short y_coord = regs.V[instr.y] % RESOLUTION_HEIGHT;
    regs.V[0xF] = 0;
    
    for(int i = 0; i < instr.n; i++) {
        unsigned char sprite_row = RAM[regs.I + i];
        for (int j = 7; j >= 0; j--) {
            if ((sprite_row & (1 << j)) && x_coord < RESOLUTION_WIDTH && y_coord < RESOLUTION_HEIGHT) {
                if (screen[x_coord][y_coord] == 1) {
                    screen[x_coord][y_coord] = 0;
                    regs.V[0xF] = 1;
                } else {
                    screen[x_coord][y_coord] = 1;
                }
            }
            x_coord++;
        }
        x_coord = regs.V[instr.x] % RESOLUTION_WIDTH;
        y_coord++;
    }
    draw_screen(renderer);
    regs.PC += 2;
}

void execute_instruction(Instruction instr, SDL_Renderer *renderer) {
    if (instr.full == 0x00E0) {
        // Clear screen
        clear_screen();
        draw_screen(renderer);
        regs.PC += 2;
    }
    else if (instr.full == 0x00EE) {
        // Return from subroutine
        regs.PC = stack_pop(&call_stack);
    }
    else if (instr.nibble1 == 0x1) {
        // Jump to address NNN
        regs.PC = instr.nnn;
    }
    else if (instr.nibble1 == 0x2) {
        // Call subroutine at NNN
        stack_push(&call_stack, regs.PC + 2);
        regs.PC = instr.nnn;
    }
    else if (instr.nibble1 == 0x3) {
        // Skip if VX == NN
        regs.PC += 2;
        if (regs.V[instr.x] == instr.nn) {
            regs.PC += 2;
        }
    }
    else if (instr.nibble1 == 0x4) {
        // Skip if VX != NN
        regs.PC += 2;
        if (regs.V[instr.x] != instr.nn) {
            regs.PC += 2;
        }
    }
    else if (instr.nibble1 == 0x5) {
        // Skip if VX == VY
        regs.PC += 2;
        if (regs.V[instr.x] == regs.V[instr.y]) {
            regs.PC += 2;
        }
    }
    else if (instr.nibble1 == 0x6) {
        // VX = NN
        regs.V[instr.x] = instr.nn;
        regs.PC += 2;
    }
    else if (instr.nibble1 == 0x7) {
        // VX += NN
        regs.V[instr.x] += instr.nn;
        regs.PC += 2;
    }
    else if (instr.nibble1 == 0x8) {
        // Arithmetic operations
        execute_arithmetic(instr);
    }
    else if (instr.nibble1 == 0x9) {
        // Skip if VX != VY
        regs.PC += 2;
        if (regs.V[instr.x] != regs.V[instr.y]) {
            regs.PC += 2;
        }
    }
    else if (instr.nibble1 == 0xA) {
        // I = NNN
        regs.I = instr.nnn;
        regs.PC += 2;
    }
    else if (instr.nibble1 == 0xB) {
        // Jump to NNN + V0
        regs.PC = instr.nnn + regs.V[0];
    }
    else if (instr.nibble1 == 0xC) {
        // VX = random & NN
        regs.V[instr.x] = (rand() % 256) & instr.nn;
        regs.PC += 2;
    }
    else if (instr.nibble1 == 0xD) {
        // Draw sprite
        execute_draw(instr, renderer);
    }
    else if (instr.nibble1 == 0xE && instr.nn == 0x9E) {
        // Skip if key VX is pressed
        regs.PC += 2;
        if (is_key_pressed(regs.V[instr.x])) {
            regs.PC += 2;
        }
    }
    else if (instr.nibble1 == 0xE && instr.nn == 0xA1) {
        // Skip if key VX is not pressed
        regs.PC += 2;
        if (!is_key_pressed(regs.V[instr.x])) {
            regs.PC += 2;
        }
    }
    else if (instr.nibble1 == 0xF) {
        // Miscellaneous operations
        execute_misc(instr);
    }
    else {
        // Unknown instruction
        regs.PC += 2;
    }
}

// ==========================
// Initialization
// ==========================
void init_chip8() {
    clear_screen();
    
    regs.PC = ROM_START_ADDRESS;
    regs.I = 0;
    regs.delay_timer = 0;
    regs.sound_timer = 0;
    for (int i = 0; i < 16; i++) {
        regs.V[i] = 0;
    }
    
    stack_init(&call_stack);
    
    for (int i = 0; i < 80; i++) {
        RAM[i] = chip8_fontset[i];
    }
    
    waiting_for_key_reg = -1;
}

bool load_rom(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    
    unsigned char byte;
    int rom_size = 0;
    while (fread(&byte, 1, 1, f) == 1) {
        RAM[ROM_START_ADDRESS + rom_size] = byte;
        rom_size++;
    }
    fclose(f);
    return true;
}

// ==========================
// Main Program
// ==========================

int main(int argc, char * const argv[]) {
    srand(time(NULL));
    
    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window *w = SDL_CreateWindow(
        "CHIP-8 Emulator", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        640, 320, 0
    );
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(r, RESOLUTION_WIDTH, RESOLUTION_HEIGHT);
    SDL_RenderSetIntegerScale(r, SDL_TRUE);
    SDL_StartTextInput();

    // Initialize audio
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_callback;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);

    // Initialize CHIP-8
    init_chip8();
    
    // Load ROM
    if (!load_rom("roms/snake.ch8")) {
        printf("Failed to load ROM\n");
        return 1;
    }
    
    // Initial render
    draw_screen(r);
    
    // Main loop
    bool running = true;
    SDL_Event e;
    
    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        
        // Update timers
        if (regs.delay_timer > 0) {
            regs.delay_timer--;
        }
        if (regs.sound_timer > 0) {
            SDL_PauseAudioDevice(audio_device, 0);  // Play beep
            regs.sound_timer--;
        } else {
            SDL_PauseAudioDevice(audio_device, 1);  // Stop beep
        }

        // Handle events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            else if (e.type == SDL_KEYDOWN && waiting_for_key_reg >= 0) {
                int chip8_key = key_to_chip8(e.key.keysym.sym);
                if (chip8_key >= 0) {
                    regs.V[waiting_for_key_reg] = (unsigned char)chip8_key;
                    waiting_for_key_reg = -1;
                    regs.PC += 2;
                }
            }
        }
        
        // Execute instructions
        for (int i = 0; i < CYCLES_PER_FRAME && waiting_for_key_reg < 0; i++) {
            Instruction instr = decode_instruction(regs.PC);
            execute_instruction(instr, r);
        }

        // Frame rate limiting
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    // Cleanup
    if (audio_device) {
        SDL_CloseAudioDevice(audio_device);
    }
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
