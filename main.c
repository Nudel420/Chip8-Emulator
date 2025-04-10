#include "include/raylib.h"
#include <stdint.h>
// Following the Tutorial https://austinmorlan.com/posts/chip8_emulator/

#define SCREEN_CELL 10

unsigned short opcode;      // opcodes each 2 bytes long
unsigned char memory[4096]; // 4K RAM
unsigned char V[16];        // general purpose registers ranging from V1 to VE
unsigned short I;           // Index register
unsigned short pc;          // program counter from 0x000 to 0xFFF

/*
    0x000-0x1FF -> Chip8 interpreter (contains font set in emu)
    0x050-0x0A0 -> Used for the built in 4x5 pixel font set (0-F)
    0x200-0xFFF -> Program ROM and free RAM
  */

int main(int argc, char *argv[]) {
    const int window_width = 64 * SCREEN_CELL;
    const int window_height = 32 * SCREEN_CELL;

    InitWindow(window_width, window_height, "CHIP8 Emulator");
    short test = 0;
    Color col = GRAY;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        for (int i = 0; i < window_height; i++) {
            test += 1;
            for (int j = 0; j < window_width; j++) {
               if(test % 2 == 0) {
                    col = BLACK;
                }else {
                    col = GRAY;
                }
                DrawRectangle(j*SCREEN_CELL, i*SCREEN_CELL, SCREEN_CELL, SCREEN_CELL, col);
                test++;
            }
        }

        EndDrawing();
    }
    return 0;
}
