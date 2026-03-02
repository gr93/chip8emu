#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "stack.h"
//cl main.c stack.c /I include /link lib/x64/SDL2.lib && main

#define RESOLUTION_WIDTH 64
#define RESOLUTION_HEIGHT 32
#define SHIFT_USES_VY 1
#define TARGET_FPS 60
#define FRAME_DELAY (1000 / TARGET_FPS)
#define CYCLES_PER_FRAME 1000

void clear_arr(unsigned char screen[RESOLUTION_WIDTH][RESOLUTION_HEIGHT]) {
    for(int i = 0; i < RESOLUTION_WIDTH; i++) {
        for(int j = 0; j < RESOLUTION_HEIGHT; j++) {
            screen[i][j] = 0;
        }
    }
}

void draw_screen(SDL_Renderer *r, unsigned char screen[RESOLUTION_WIDTH][RESOLUTION_HEIGHT]) {
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    for(int i = 0; i < RESOLUTION_WIDTH; i++) {
        for(int j = 0; j < RESOLUTION_HEIGHT; j++) {
            if (screen[i][j] == 1) {
                SDL_RenderDrawPoint(r, i, j);
            }
        }
    }
    SDL_RenderPresent(r);
}

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

struct registers {
  unsigned short PC; // PC
  unsigned short I; //Index reg
  unsigned char V[16]; //GP 8 bit regs
  unsigned char delay_timer;
  unsigned char sound_timer;
};

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

unsigned short RAM[4096];

int main(int  argc, char * const  argv[]) {
    // Seed random number generator for CXNN instruction
    srand(time(NULL));
    
    //Set up SDL window and renderer
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *w = SDL_CreateWindow(
        "ASCII", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 320, 0
    );
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(r, 64, 32);  // CHIP-8 resolution
    SDL_RenderSetIntegerScale(r, SDL_TRUE);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 0);
    SDL_StartTextInput();

    // Load ROM
    FILE * f = fopen("roms/mario.ch8", "rb");
    unsigned char screen[RESOLUTION_WIDTH][RESOLUTION_HEIGHT]; 
    struct registers regs = {0}; // Initialize all registers to 0

    //Initialize stack
    struct stack s;
    stack_init(&s);
    
    // Initialize screen to all 0s (black)
    clear_arr(screen);
    
    //Load font into RAM
    for (int i=0; i<80; i++) {
      RAM[i] = chip8_fontset[i];
    }
    
    // Load ROM into RAM starting at 0x200, init PC to 0x200
    unsigned char byte;
    int rom_size = 0;
    while (fread(&byte, 1, 1, f) == 1) {
      RAM[0x200 + rom_size] = byte;
      rom_size++;
    }
    fclose(f);
    regs.PC = 0x200;
    printf("Loaded %d bytes into RAM\n", rom_size);
    
    // Initial render (black screen)
    draw_screen(r, screen);
    
    int run = 1; 
    SDL_Event e;


    //Waiting for key reg
    int waiting_for_key_reg = -1;
    
    while (run) {
      Uint32 frame_start = SDL_GetTicks();
      
      //Decrement delay timer if needed
      if (regs.delay_timer > 0) {
        regs.delay_timer -= 1;
      }

      //Play sound if needed
      // if (regs.sound_timer > 0) {
      //   //SDL play sound
      // }

      // Handle events
      while (SDL_PollEvent(&e)) {
          printf("%X %i\n", e.type, waiting_for_key_reg);
          if (e.type == SDL_QUIT) {
              run = 0;
          }
          else if (e.type == SDL_KEYDOWN && waiting_for_key_reg >= 0) {
            printf("%X", e.key.keysym.sym);
            int chip8_key = key_to_chip8(e.key.keysym.sym);
            if (chip8_key >= 0) {
              regs.V[waiting_for_key_reg] = (unsigned char)chip8_key;
              waiting_for_key_reg = -1;
              regs.PC += 2;
            }
          }
      }
      
      for(int i=0; i<CYCLES_PER_FRAME && waiting_for_key_reg < 0; i++) {
        unsigned char byte1 = RAM[regs.PC];
        unsigned char byte2 = RAM[regs.PC + 1];

        unsigned char nibble_1 = byte1 >> 4;
        unsigned char nibble_2 = byte1 & 0b00001111;
        unsigned char nibble_3 = byte2 >> 4;
        unsigned char nibble_4 = byte2 & 0b00001111;
        unsigned short full_instruction = (byte1 << 8) | byte2;
        if(full_instruction == 0x00E0) {
            printf("Clearing the display\n");
            clear_arr(screen);
            draw_screen(r, screen);  // Redraw after clearing
            regs.PC += 0x2;
        }
        else if (full_instruction == 0x00EE) { //Return from subroutine
          regs.PC = stack_pop(&s);
        }
        else if (nibble_1 == 0x1) { //Jump PC
          regs.PC = (nibble_2 << 8) | (nibble_3 << 4) | nibble_4;
        }
        else if (nibble_1 == 0x2) { //Call subroutine
          stack_push(&s, regs.PC+2);
          regs.PC = (nibble_2 << 8) | (nibble_3 << 4) | nibble_4;
        }
        else if (nibble_1 == 0x3) { //Conditional skip
          regs.PC += 2;
          unsigned char val = (nibble_3 << 4) | nibble_4;
          if (regs.V[nibble_2] == val) {
            regs.PC += 2;
          }
        }
        else if (nibble_1 == 0x4) { //Conditional skip
          regs.PC += 2;
          unsigned char val = (nibble_3 << 4) | nibble_4;
          if (regs.V[nibble_2] != val) {
            regs.PC += 2;
          }
        }
        else if (nibble_1 == 0x5) { //Conditional skip
          regs.PC += 2;
          if (regs.V[nibble_2] == regs.V[nibble_3]) {
            regs.PC += 2;
          }
        }
        else if (nibble_1 == 0x6) { //Set V register
          unsigned char val = (nibble_3 << 4) | nibble_4;
          regs.V[nibble_2] = val;
          regs.PC += 2;
        }
        else if (nibble_1 == 0x7) { //Add to V register
          unsigned char val = (nibble_3 << 4) | nibble_4;
          regs.V[nibble_2] = (regs.V[nibble_2] + val);
          regs.PC += 2;
        }
        else if (nibble_1 == 0x8) { //Arithmetic operators
          if (nibble_4 == 0x0) {
            regs.V[nibble_2] = regs.V[nibble_3];
          }
          else if (nibble_4 == 0x1) {
            regs.V[nibble_2] = regs.V[nibble_2] | regs.V[nibble_3];
          }
          else if (nibble_4 == 0x2) {
            regs.V[nibble_2] = regs.V[nibble_2] & regs.V[nibble_3];
          }
          else if (nibble_4 == 0x3) {
            regs.V[nibble_2] = regs.V[nibble_2] ^ regs.V[nibble_3];
          }
          else if (nibble_4 == 0x4) {
            unsigned short sum = regs.V[nibble_2] + regs.V[nibble_3];
            regs.V[nibble_2] = sum & 0xFF;
            regs.V[0xF] = (sum > 255) ? 1 : 0;
          }
          else if (nibble_4 == 0x5) {
            bool carry = regs.V[nibble_2] >= regs.V[nibble_3];
            regs.V[nibble_2] = regs.V[nibble_2]- regs.V[nibble_3];
            regs.V[0xF] = carry;
          }
          else if (nibble_4 == 0x6) {
            if (SHIFT_USES_VY) {
              regs.V[nibble_2] = regs.V[nibble_3];
            }
            unsigned char last_bit = regs.V[nibble_2] & 0b00000001;
            regs.V[nibble_2] = regs.V[nibble_2] >> 1;
            regs.V[0xF] = last_bit;
          }
          else if (nibble_4 == 0x7) {
            bool carry = regs.V[nibble_3] >= regs.V[nibble_2];
            regs.V[nibble_2] = regs.V[nibble_3]- regs.V[nibble_2];
            regs.V[0xF] = carry;
          }     
          else if (nibble_4 == 0xE) {
            if (SHIFT_USES_VY) {
              regs.V[nibble_2] = regs.V[nibble_3];
            }
            unsigned char first_bit = regs.V[nibble_2] & 0b10000000;
            regs.V[nibble_2] = regs.V[nibble_2] << 1;
            regs.V[0xF] = first_bit >> 7;
          }   
          regs.PC += 2;                
        }
        else if (nibble_1 == 0x9) { //Conditional skip
          regs.PC += 2;
          if (regs.V[nibble_2] != regs.V[nibble_3]) {
            regs.PC += 2;
          }
        }
        else if (nibble_1 == 0xA) { //Set index register
          unsigned short val = (nibble_2 << 8) | (nibble_3 << 4) | nibble_4;
          regs.I = val;
          regs.PC += 2;
        }
        else if (nibble_1 == 0xB) {
          //TODO Later CHIP8 models changed this to BXNN, so make the instruction type configurable
          regs.PC = ((nibble_2 << 8) | (nibble_3 << 4) | nibble_4) + regs.V[0];
        }
        else if (nibble_1 == 0xC) { //Random number AND NN
          unsigned char nn = (nibble_3 << 4) | nibble_4;
          regs.V[nibble_2] = (rand() % 256) & nn;
          regs.PC += 2;
        }
        else if (nibble_1 == 0xD) { //Draw pixels
          short x_coord = regs.V[nibble_2] % 64;
          short y_coord = regs.V[nibble_3] % 32;
          regs.V[0xF] = 0;
          for(int i=0; i < nibble_4; i++) {
            unsigned char sprite_row = RAM[regs.I + i];
            for (int j = 7; j >= 0; j--) {
                if (sprite_row & (1 << j) && x_coord < 64 && y_coord < 32) {
                    if (screen[x_coord][y_coord] == 1) {
                      screen[x_coord][y_coord] = 0;
                      regs.V[0xF] = 1;
                    }
                    else {
                      screen[x_coord][y_coord] = 1;
                    }
                }
                x_coord += 1;
            }
            x_coord = regs.V[nibble_2] % 64;
            y_coord += 1;
          }
          draw_screen(r, screen);
          regs.PC += 2;
        }
        else if (nibble_1 == 0xE) { //Skip if key pressed
          unsigned char key_val = regs.V[nibble_2];
          if (nibble_3 == 0x9 && nibble_4 == 0xE) {
            regs.PC += 2;
            if (is_key_pressed(key_val)) {
              regs.PC += 2;
            }
          }
          if (nibble_3 == 0xA && nibble_4 == 0x1) {
            regs.PC += 2;
            if (!is_key_pressed(key_val)) {
              regs.PC += 2;
            }
          }            
        }
        else if (nibble_1 == 0xF) {
          if (nibble_3 == 0x0 && nibble_4 == 0x7) {
            regs.V[nibble_2] = regs.delay_timer;
            regs.PC += 2;
          }
          else if (nibble_3 == 0x1 && nibble_4 == 0x5) {
            regs.delay_timer = regs.V[nibble_2];
            regs.PC += 2;
          }
          else if (nibble_3 == 0x1 && nibble_4 == 0x8) {
            regs.sound_timer = regs.V[nibble_2];
            regs.PC += 2;
          }
          else if (nibble_3 == 0x1 && nibble_4 == 0xE) {
            regs.I += regs.V[nibble_2];
            regs.PC += 2;
          }
          else if (nibble_3 == 0x0 && nibble_4 == 0xA) {
            waiting_for_key_reg = nibble_2;
          }
          else if (nibble_3 == 0x2 && nibble_4 == 0x9) {
            regs.I = regs.V[nibble_2] * 5;
            regs.PC += 2;
          }
          else if (nibble_3 == 0x3 && nibble_4 == 0x3) {
            unsigned char val = regs.V[nibble_2];
            unsigned char hundreds = (int)val / 100;
            unsigned char tens = ((int)val / 10) % 10;
            unsigned char ones = (int)val % 10;
            RAM[regs.I] = hundreds;
            RAM[regs.I + 1] = tens;
            RAM[regs.I + 2] = ones;
            regs.PC += 2;
          }
          else if (nibble_3 == 0x5 && nibble_4 == 0x5) {
            for(int i = 0; i <= nibble_2; i++) {
              RAM[regs.I + i] = regs.V[i];
            }
            regs.PC += 2;
          }
          else if (nibble_3 == 0x6 && nibble_4 == 0x5) {
            for(int i = 0; i <= nibble_2; i++) {
              regs.V[i] = RAM[regs.I + i];
            }
            regs.PC += 2;
          }
        }
        else {
          // Skip unknown instr
          printf("Unknown instruction: %04X at PC=%04X\n", full_instruction, regs.PC);
          regs.PC += 2;
        }
      }

      //Wait for next frame
      Uint32 frame_time = SDL_GetTicks() - frame_start;
      if (frame_time < FRAME_DELAY) {
        SDL_Delay(FRAME_DELAY - frame_time);
      }
        
    }

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
