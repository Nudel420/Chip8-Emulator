#include "include/raylib.h"
#include <stdint.h>
#include <stdio.h>

// Following the Tutorial https://austinmorlan.com/posts/chip8_emulator/

/*******************************
    Chip8 general memory layout

    0x000-0x1FF -> Chip8 interpreter (contains font set in emu)
    0x050-0x0A0 -> Used for the built in 4x5 pixel font set (0-F)
    0x200-0xFFF -> Program ROM and free RAM
 ********************************/

/*********************************
    16 key-layout on normal keyboard

    Keypad       Keyboard
    +-+-+-+-+    +-+-+-+-+
    |1|2|3|C|    |1|2|3|4|
    +-+-+-+-+    +-+-+-+-+
    |4|5|6|D|    |Q|W|E|R|
    +-+-+-+-+ => +-+-+-+-+
    |7|8|9|E|    |A|S|D|F|
    +-+-+-+-+    +-+-+-+-+
    |A|0|B|F|    |Y|X|C|V|
    +-+-+-+-+    +-+-+-+-+
 *********************************/

#define CELL_SIZE 10

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t

u8 registers[16]; // general purpose registers rangig from V1 to VE
u8 memory[4096];  // 4K RAM
u16 index_reg;    // index register
u16 pc;           // program counter
u16 stack[16];    // stack for storing instructions
u8 sp;            // stack pointer
u8 delay_timer;
u8 sound_timer;
u8 keypad[16];
u32 video[64 * 32]; // video display array
u16 opcode;         // opcodes each 2 bytes long

#define START_ADRESS  0x200 // 0x200 needs 12 bits to be displayed 512 in base 10

// loads rom
// returns 0 if the loading of the file has failed
// returns 1 if the file was loaded successfully
u8 load_rom(const char *file_name) {
  FILE *rom_file = fopen(file_name, "rb");
  if (rom_file == NULL) {
    perror("Error opening the ROM!");
    return 0;
  }
  // go to end of the file and read the size
  fseek(rom_file, 0, SEEK_END);
  u32 file_size = ftell(rom_file);
  // DEBUG
  // printf("File size of `%s` is: %d\n", file_name, file_size);

  // move file pointer to the beginning of the file
  // so that you can copy from the start
  fseek(rom_file, 0, SEEK_SET);
  fread(memory + START_ADRESS, sizeof(u32),file_size, rom_file);
  fclose(rom_file);
  return 1;
}

int main(int argc, char *argv[]) {

  load_rom(argv[1]);
  return 0;

  const int window_width = 64 * CELL_SIZE;
  const int window_height = 32 * CELL_SIZE;

  Color col = WHITE;
  short col_switch = 0;
  InitWindow(window_width, window_height, "CHIP8 Emulator");
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    for (int i = 0; i < window_height; i++) {
      col_switch += 1;
      for (int j = 0; j < window_width; j++) {
        if (col_switch % 2 == 0) {
          col = GRAY;
        } else {
          col = WHITE;
        }
        DrawRectangle(j * CELL_SIZE, i * CELL_SIZE, CELL_SIZE, CELL_SIZE, col);
        col_switch++;
      }
    }
    EndDrawing();
  }
  return 0;
}
