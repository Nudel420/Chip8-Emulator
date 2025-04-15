#define _CRT_SECURE_NO_WARNINGS
#include "/include/raylib.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
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

#define START_ADDRESS 0x200 // 0x200 needs 12 bits to be displayed 512 in base 10
#define FONTSET_START_ADDRESS 0x50 
#define FONTSET_SIZE 80

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32

typedef struct Chip8 {
  u8 V[16]; // general purpose registers rangig from V1 to VE
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
} Chip8;

// Font each character consists of 5 bytes, example of letter "F":
/************
  11110000
  10000000
  11110000
  10000000
  10000000
*************/
// With 16 characters each 5 bytes big we need 16*5 = 80 bytes of storage for all characters
u8 fontset[FONTSET_SIZE] = 
  {
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

void init_chip8(Chip8 *chip8){
  // init seed for random numbers
  srand(time(0));
  chip8->pc = START_ADDRESS;
}

// loads rom
// returns 0 if the loading of the file has failed
// returns 1 if the file was loaded successfully
int load_rom(Chip8 *chip8, const char *file_name) {
  FILE *rom_file = fopen(file_name, "rb");
  if (rom_file == NULL) {
    printf("Error opening the ROM, errno: %d\n", errno);
    return -1;
  }
  // read file size
  fseek(rom_file, 0, SEEK_END);
  u32 file_size = ftell(rom_file);
  rewind(rom_file);
  printf("File size of `%s` is: %d\n", file_name, file_size);
  if (file_size < 0) {
    printf("File size is 0, errno: %d\n", errno);
    fclose(rom_file);
    return -1;
  }

  // write the data from the file into the chip8 memory, starting at 0x200
  fread(chip8->memory + START_ADDRESS, sizeof(u8), file_size, rom_file);
  fclose(rom_file);
  return 0;
}

void load_font(Chip8* chip8, u8 fontset[]){
  for(size_t i = 0; i < FONTSET_SIZE; i++){
    chip8->memory[FONTSET_START_ADDRESS + i] = fontset[i];
  }
}

int random_byte(){
  int num = rand() % 255 + 1;
  return num;
}

/*********************************
all 34 instructions of the Chip8
*********************************/

// 00E0: Clear the display
void op_00E0(Chip8 *chip8){
  for (size_t i = 0; i < SCREEN_HEIGHT; i++) {
    for(size_t j = 0; j < SCREEN_WIDTH; j++){
      chip8->video[j * i] = 0; // chip8 video buffer is 64 pixels wide and 32 tall
    }
  }
}

// 00EE: Return from a subroutine
void op_00EE(Chip8 *chip8){
  chip8->sp--;
  chip8->pc = chip8->stack[chip8->sp];
}

// 1nnn: Jump to location nnn
void op_1nnn(Chip8 *chip8){
  u16 nnn = chip8->opcode & 0x0FFF; // only look at the lowest 3 bytes the first one is the instruction
  chip8->pc = nnn;
}

// 6XNN: Set Register Vx to NN
void op_6XNN(Chip8 *chip8){
  u8 x = (chip8->opcode & 0x0F00) >> 8; //  x = 0x0A00 >> 8 = 0x0A = 10 otherwise 0x0A00 = 2560
  u8 nn = chip8->opcode & 0x00FF;
  chip8->V[x] = nn;
}

// 
void op_(Chip8 *chip8){

}
int main(int argc, char **argv) {

  Chip8 chip8 = {0};
  init_chip8(&chip8);

  load_rom(&chip8, argv[1]);
  for (size_t i = 0; i < 10; i++) {
  printf("random number: %d\n", random_byte());
  }
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
