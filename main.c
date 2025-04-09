#include "include/raylib.h"
#include <stdint.h>

// Following the Tutorial https://austinmorlan.com/posts/chip8_emulator/
#define CELL_SIZE 10

unsigned short opcode;      // opcodes each 2 bytes long
unsigned char memory[4096]; // 4K RAM
unsigned char V[16];        // general purpose registers ranging from V1 to VE
unsigned short I;           // Index register
unsigned short pc;          // program counter from 0x000 to 0xFFF

/***********************************************************************
                    Chip8 general memory layout
    0x000-0x1FF -> Chip8 interpreter (contains font set in emu)
    0x050-0x0A0 -> Used for the built in 4x5 pixel font set (0-F)
    0x200-0xFFF -> Program ROM and free RAM
 ***********************************************************************/

int main(int argc, char *argv[]) {
    const int window_width = 64 * CELL_SIZE;
    const int window_height = 32 * CELL_SIZE;

    Color col = GRAY;
    short col_switch = 0;
    InitWindow(window_width, window_height, "CHIP8 Emulator");
    while (!WindowShouldClose()) {
        BeginDrawing();
        for (int i = 0; i < window_height; i++) {
            for (int j = 0; j < window_width; j++) {
                ClearBackground(RAYWHITE);
                if (col_switch % 2 == 0) {
                    col = BLACK;
                } else {
                    col = GRAY;
                }
                DrawRectangle(j * CELL_SIZE, i * CELL_SIZE, CELL_SIZE, CELL_SIZE, col);
            }
        }
        EndDrawing();
    }
    return 0;
}
