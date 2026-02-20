#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
//cl main.c /I include /link SDL2.lib && main

#define RESOLUTION_WIDTH 64
#define RESOLUTION_HEIGHT 32

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
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *w = SDL_CreateWindow(
        "ASCII", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 320, 0
    );
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(r, 64, 32);  // CHIP-8 resolution
    SDL_RenderSetIntegerScale(r, SDL_TRUE);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 0);

    FILE * f = fopen("roms/logo.ch8", "rb");
    unsigned char screen[RESOLUTION_WIDTH][RESOLUTION_HEIGHT]; 
    struct registers regs;
    
    // Initialize screen to all 0s (black)
    clear_arr(screen);
    
    //Load font into RAM
    for (int i=0; i<80; i++) {
      RAM[i] = chip8_fontset[i];
    }
    
    // Load ROM into RAM starting at 0x200
    unsigned char byte;
    int rom_size = 0;
    while (fread(&byte, 1, 1, f) == 1) {
      RAM[0x200 + rom_size] = byte;
      rom_size++;
    }
    fclose(f);
    printf("Loaded %d bytes into RAM\n", rom_size);
    
    regs.PC = 0x200;
    
    // Initial render (black screen)
    draw_screen(r, screen);
    
    while (regs.PC < 0x200 + rom_size) {
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
        else if (nibble_1 == 0x1) { //Jump PC
          regs.PC = (nibble_2 << 8) | (nibble_3 << 4) | nibble_4;
        }
        else if (nibble_1 == 0x6) { //Set V register
          unsigned char val = (nibble_3 << 4) | nibble_4;
          regs.V[nibble_2] = val;
          regs.PC += 2;
        }
        else if (nibble_1 == 0x7) { //Add to V register
          unsigned char val = (nibble_3 << 4) | nibble_4;
          regs.V[nibble_2] = (regs.V[nibble_2] + val) % 0xFF; //wrap around if it exceeds 8 bits
          regs.PC += 2;
        }
        else if (nibble_1 == 0xA) { //Set index register
          unsigned short val = (nibble_2 << 8) | (nibble_3 << 4) | nibble_4;
          regs.I = val;
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
        else {
          // Skip unknown instr
          printf("Unknown instruction: %04X at PC=%04X\n", full_instruction, regs.PC);
          regs.PC += 2;
        }
    }
    
    int run = 1; SDL_Event e;
    while (run) {
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT) run = 0;
    }

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
