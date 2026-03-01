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
    FILE * f = fopen("roms/6-keypad.ch8", "rb");
    unsigned char screen[RESOLUTION_WIDTH][RESOLUTION_HEIGHT]; 
    struct registers regs;

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
    
    while (run) {
        // Handle events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
               run = 0;
            }
        }
        
        // Execute instructions (limit to prevent infinite loop in one frame)
        for (int i = 0; i < 10 && regs.PC < 0x200 + rom_size; i++) {
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
              regs.V[0xF] = (sum > 255) ? 1 : 0;
              regs.V[nibble_2] = sum & 0xFF;
            }
            else if (nibble_4 == 0x5) {
              regs.V[0xF] = (regs.V[nibble_2] >= regs.V[nibble_3]);
              regs.V[nibble_2] = regs.V[nibble_2]- regs.V[nibble_3];
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
              regs.V[0xF] = (regs.V[nibble_3] >= regs.V[nibble_2]);
              regs.V[nibble_2] = regs.V[nibble_3]- regs.V[nibble_2];
            }     
            else if (nibble_4 == 0xE) {
              if (SHIFT_USES_VY) {
                regs.V[nibble_2] = regs.V[nibble_3];
              }
              unsigned char first_bit = regs.V[nibble_2] & 0b10000000;
              regs.V[nibble_2] = regs.V[nibble_2] << 1;
              regs.V[0xF] = first_bit;
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
            //TODO Later CHIP8 models changed this to BXNN, make configurable
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
          else if (nibble_1 == 0xE) { //Jump if key pressed
            unsigned char key_val = regs.V[nibble_1];
            if (nibble_3 == 0x9 && nibble_4 == 0xE) {
              if (is_key_pressed(key_val)) {
                regs.PC += 2;
              }
            }
            if (nibble_3 == 0xA && nibble_4 == 0x1) {
              if (!is_key_pressed(key_val)) {
                regs.PC += 2;
              }
            }            
          }
          else {
            // Skip unknown instr
            printf("Unknown instruction: %04X at PC=%04X\n", full_instruction, regs.PC);
            regs.PC += 2;
          }
        } // End instruction execution loop
        
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
