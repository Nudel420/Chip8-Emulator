#define _CRT_SECURE_NO_WARNINGS
#include "include/raylib.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// Following the Tutorial https://austinmorlan.com/posts/chip8_emulator/

/*******************************
    Chip8 general memory layout

    0x000-0x1FF -> Chip8 interpreter (contains font set in emu)
    0x050-0x0A0 -> Used for the built in 4x5 pixel font set (0-F)
    0x200-0xFFF -> Program ROM and free RAM
 ********************************/

#define CELL_SIZE 10

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t

#define START_ADDRESS 0x200 // 0x200 needs 12 bits to be displayed 512 in base 10
#define FONTSET_START_ADDRESS 0x50
#define FONTSET_SIZE 80

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define KEYPAD_MAX 16

typedef struct Chip8 {
  u8 V[16];        // general purpose registers rangig from V1 to VE
  u8 memory[4096]; // 4K RAM
  u16 I;           // index register
  u16 pc;          // program counter
  u16 stack[16];   // stack for storing instructions
  u8 sp;           // stack pointer
  u8 delay_timer;
  u8 sound_timer;
  u8 keypad[KEYPAD_MAX];
  u32 video[SCREEN_HEIGHT][SCREEN_WIDTH]; // video display array 32 high and 64 wide
  u16 opcode;                             // opcodes each 2 bytes long
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

void load_font(Chip8 *chip8, u8 fontset[]) {
  for (size_t i = 0; i < FONTSET_SIZE; i++) {
    chip8->memory[FONTSET_START_ADDRESS + i] = fontset[i];
  }
}

int random_byte() {
  int num = rand() % 255 + 1;
  return num;
}

void init_chip8(Chip8 *chip8) {
  // init seed for random numbers
  srand(time(0));
  chip8->pc = START_ADDRESS;
  load_font(chip8, fontset);
}
/*********************************
all 34 instructions of the Chip8
*********************************/

// 00E0: Clear the display
void op_00E0(Chip8 *chip8) {
  memset(chip8->video, 0, sizeof(chip8->video));
}

// 00EE: Return from a subroutine
void op_00EE(Chip8 *chip8) {
  chip8->sp--;
  chip8->pc = chip8->stack[chip8->sp];
}

// 1nnn: Jump to location nnn
void op_1nnn(Chip8 *chip8) {
  u16 nnn = chip8->opcode & 0x0FFF; // only look at the lowest 3 bytes the first one is the instruction
  chip8->pc = nnn;
}

// 2nnn: Call subroutine at nnn
void op_2nnn(Chip8 *chip8) {
  u16 nnn = chip8->opcode & 0x0FFF;
  chip8->stack[chip8->sp] = chip8->pc;
  chip8->sp++;
  chip8->pc = nnn;
}

// 3xkk: Skip next instruction if Vx = kk
void op_3xkk(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 kk = chip8->opcode & 0x00FF;
  if (chip8->V[x] == kk) {
    chip8->pc += 2;
  }
}

// 4xkk: Skip next instruction if Vx != kk
void op_4xkk(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 kk = chip8->opcode & 0x00FF;
  if (chip8->V[x] != kk) {
    chip8->pc += 2;
  }
}

// 5xy0: Skip next instruction if Vx = Vy
void op_5xy0(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;
  if (chip8->V[x] == chip8->V[y]) {
    chip8->pc += 2;
  }
}

// 6xkk: Set Register Vx = kk
void op_6xkk(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8; //  x = 0x0A00 >> 8 = 0x0A = 10 otherwise 0x0A00 = 2560
  u8 kk = chip8->opcode & 0x00FF;
  chip8->V[x] = kk;
}

// 7xkk: Set Vx = Vx + kk
void op_7xkk(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8; //  x = 0x0A00 >> 8 = 0x0A = 10 otherwise 0x0A00 = 2560
  u8 kk = chip8->opcode & 0x00FF;
  chip8->V[x] = chip8->V[x] + kk;
}

// 8xy0: Set Vx = Vy
void op_8xy0(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;
  chip8->V[x] = chip8->V[y];
}

// 8xy1: Set Vx = Vx OR Vy
void op_8xy1(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;
  chip8->V[x] |= chip8->V[y];
  chip8->V[0xF] = 0;
}

// 8xy2: Set Vx = Vx AND Vy
void op_8xy2(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;
  chip8->V[x] &= chip8->V[y];
  chip8->V[0xF] = 0;
}

// 8xy3: Set Vx = Vx XOR Vy
void op_8xy3(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;
  chip8->V[x] ^= chip8->V[y];
  chip8->V[0xF] = 0;
}

// The values of Vx and Vy are added together. If the result is greater
// than 8 bits (i.e., > 255,) VF is set to 1, otherwise 0. Only the lowest
// 8 bits of the result are kept, and stored in Vx.
// This is an ADD with an overflow flag. If the sum is greater than what can
// fit into a byte (255), register VF will be set to 1 as a flag.
// 8xy4: Set Vx = Vx + Vy, set VF = carry
void op_8xy4(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;

  u16 sum = (chip8->V[x] + chip8->V[y]);

  chip8->V[x] = sum & 0xFF;

  if (sum > 255) {
    chip8->V[0xF] = 1;
  } else {
    chip8->V[0xF] = 0;
  }
}

// If Vx > Vy, then VF is set to 1, otherwise 0. Then Vy is subtracted from Vx,
// and the results stored in Vx.
// 8xy5: Set Vx = Vx - Vy, set VF = NOT borrow
void op_8xy5(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;

  u8 temp = chip8->V[x];
  chip8->V[x] -= chip8->V[y];
  if (temp >= chip8->V[y]) {
    chip8->V[0xF] = 1;
  } else {
    chip8->V[0xF] = 0;
  }
}

// If the least-significant bit of Vx is 1, then VF is set to 1, otherwise 0.
// Then Vx is divided by 2.
// A right shift is performed (division by 2), and the least significant bit is saved in Register VF.
// 8xy6: Set Vx = Vx SHR 1
void op_8xy6(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;

  chip8->V[x] = chip8->V[y];
  u8 lsb = chip8->V[x] & 0x0001;
  chip8->V[x] >>= 1;
  chip8->V[0xF] = lsb;
}

// If Vy > Vx, then VF is set to 1, otherwise 0.
// Then Vx is subtracted from Vy, and the results stored in Vx.
// 8xy7 Set Vx = Vy - Vx, set VF = NOT borrow
void op_8xy7(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;

  u8 temp = chip8->V[x];
  chip8->V[x] = (chip8->V[y] - chip8->V[x]);
  if (temp <= chip8->V[y]) {
    chip8->V[0xF] = 1;
  } else {
    chip8->V[0xF] = 0;
  }
}

// If the most-significant bit of Vx is 1, then VF is set to 1, otherwise to 0.
// Then Vx is multiplied by 2.
// A left shift is performed (multiplication by 2), and the most significant bit
// is saved in Register VF.
// 8xyE: Set Vx = Vx SHL 1.
void op_8xyE(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;

  chip8->V[x] = chip8->V[y];

  // msb: 1111 0000 >> 7 = 0000 00011
  u8 msb = (chip8->V[x] & 0xF0) >> 7;
  chip8->V[x] <<= 1;
  chip8->V[0xF] = msb;
}

// 9xy0: Skip next instruction if Vx != Vy
void op_9xy0(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;
  if (chip8->V[x] != chip8->V[y]) {
    chip8->pc += 2;
  }
}

// Annn: Set I = nnn
void op_Annn(Chip8 *chip8) {
  u16 nnn = chip8->opcode & 0x0FFF;
  chip8->I = nnn;
}

// Bnnn: Jump to location nnn + V0
void op_Bnnn(Chip8 *chip8) {
  u16 nnn = chip8->opcode & 0x0FFF;
  chip8->pc = (nnn + chip8->V[0]);
}

// Cxkk: Set Vx = random byte AND kk
void op_Cxkk(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 kk = (chip8->opcode & 0x00FF);
  chip8->V[x] = (random_byte() & kk);
}

// We iterate over the sprite, row by row and column by column. We know there
// are eight columns because a sprite is guaranteed to be eight pixels wide.

// If a sprite pixel is on then there may be a collision with what’s already
// being displayed, so we check if our screen pixel in the same location is set.
// If so we must set the VF register to express collision.

// Then we can just XOR the screen pixel with 0xFFFFFFFF to essentially XOR it
// with the sprite pixel (which we now know is on). We can’t XOR directly because
// the sprite pixel is either 1 or 0 while our video pixel is either 0x00000000 or 0xFFFFFFFF.
// Dxyn: Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision
void op_Dxyn(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 y = (chip8->opcode & 0x00F0) >> 4;
  u8 n = (chip8->opcode & 0x000F);

  u8 x_coord = chip8->V[x] % SCREEN_WIDTH;  // SCREEN_WIDTH is 64
  u8 y_coord = chip8->V[y] % SCREEN_HEIGHT; // SCREEN_HEIGHT is 32

  chip8->V[0xF] = 0;

  for (u8 row = 0; row < n; row++) {
    u8 sprite_byte = chip8->memory[chip8->I + row];

    for (u8 col = 0; col < 8; col++) {
      u8 sprite_pixel = sprite_byte & (0x80 >> col);
      // u32 *screen_pixel = &chip8->video[((y_coord + row) * SCREEN_WIDTH) + (x_coord + col)];
      u32 *screen_pixel = &chip8->video[y_coord + row][x_coord + col];

      if (sprite_pixel) {
        // check if screen_pixel is already on (set to 1)
        if (*screen_pixel == 0xFFFFFFFF) {
          chip8->V[0xF] = 1;
        }

        // XOR with sceen_pixel with sprite_pixel
        // (NOTE): it is not directly XORed because of
        // the difference in magnitude of both values
        *screen_pixel ^= 0xFFFFFFFF;
      }
    }
  }
}

// Ex9E: Skip next instruction if key with value of Vx is pressed
void op_Ex9E(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 value = chip8->V[x];

  if (chip8->keypad[value]) {
    chip8->pc += 2;
  }
}

// ExA1: Skip next instruction if key with the value of Vx is not pressed
void op_ExA1(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 value = chip8->V[x];

  if (!(chip8->keypad[value])) {
    chip8->pc += 2;
  }
}

// Fx07: Set Vx = delay timer value
void op_Fx07(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  chip8->V[x] = chip8->delay_timer;
}

// Fx0A: Wait for a key press, store the value of the key in Vx
void op_Fx0A(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  // TODO: add all remaining keypad
  if (chip8->keypad[0]) {
    chip8->V[x] = 0;
  } else if (chip8->keypad[1]) {
    chip8->V[x] = 1;
  } else if (chip8->keypad[2]) {
    chip8->V[x] = 2;
  } else if (chip8->keypad[3]) {
    chip8->V[x] = 3;
  } else if (chip8->keypad[4]) {
    chip8->V[x] = 4;
  } else if (chip8->keypad[5]) {
    chip8->V[x] = 5;
  } else if (chip8->keypad[6]) {
    chip8->V[x] = 6;
  } else if (chip8->keypad[7]) {
    chip8->V[x] = 7;
  } else if (chip8->keypad[8]) {
    chip8->V[x] = 8;
  } else if (chip8->keypad[9]) {
    chip8->V[x] = 9;
  } else if (chip8->keypad[10]) {
    chip8->V[x] = 10;
  } else if (chip8->keypad[11]) {
    chip8->V[x] = 11;
  } else if (chip8->keypad[12]) {
    chip8->V[x] = 12;
  } else if (chip8->keypad[13]) {
    chip8->V[x] = 13;
  } else if (chip8->keypad[14]) {
    chip8->V[x] = 14;
  } else if (chip8->keypad[15]) {
    chip8->V[x] = 15;
  } else {
    chip8->pc -= 2;
  }
}

// Fx15: Set delay timer = Vx
void op_Fx15(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  chip8->delay_timer = chip8->V[x];
}

// Fx18: Set sound timer = Vx
void op_Fx18(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  chip8->sound_timer = chip8->V[x];
}

// Fx1E: Set I = I + Vx
void op_Fx1E(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  chip8->I = chip8->I + chip8->V[x];
}

// Fx29: Set I = location of sprite for digit Vx
// Font characters are located at 0x50, each being five bytes long
void op_Fx29(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  chip8->I = FONTSET_START_ADDRESS + (chip8->V[x] * 5);
}

// The interpreter takes the decimal value of Vx,
// and places the hundreds digit in memory at location in I,
// the tens digit at location I+1
// and the ones digit at location I+2
// Fx33: Store BCD representation of Vx in memory locations I, I+1, I+2
void op_Fx33(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  u8 digit = chip8->V[x];

  u8 digit_one = digit % 10;
  digit /= 10;
  u8 digit_ten = digit % 10;
  digit /= 10;
  u8 digit_hundred = digit % 10;

  chip8->memory[chip8->I] = digit_hundred;
  chip8->memory[chip8->I + 1] = digit_ten;
  chip8->memory[chip8->I + 2] = digit_one;
}

// Fx55: Store registers V0 through Vx in memory starting at location I
void op_Fx55(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  for (u8 i = 0; i <= x; ++i) {
    chip8->memory[chip8->I + i] = chip8->V[i];
  }
}

// Fx65: Read registers V0 through Vx from meory starting at location I
void op_Fx65(Chip8 *chip8) {
  u8 x = (chip8->opcode & 0x0F00) >> 8;
  for (u8 i = 0; i <= x; ++i) {
    chip8->V[i] = chip8->memory[chip8->I + i];
  }
}

// Fetch, Decode and Exectue instruction
// inspired by: https://github.com/jborza/emuchip8/blob/master/cpu.c
void process_instruction(Chip8 *chip8) {

  /* Fetch */

  // Catch first byte and second byte separatly because memory is u8
  // but an instruction consists of 2 bytes
  chip8->opcode = (chip8->memory[chip8->pc]);
  chip8->opcode <<= 8;
  chip8->opcode |= (chip8->memory[chip8->pc + 1]);
  chip8->pc += 2;

  /* Decode & Execute */

  // an opcode consists of 4 bytes with the following encoding
  // examples: [0xVxy0], [0xVnnn], [0xV00n], etc.

  // u8 x = (chip8->opcode & 0x0F00) >> 8;
  // u8 y = (chip8->opcode & 0x00F0) >> 4;
  // u16 nnn = (chip8->opcode & 0x0FFF);
  // u8 nn = (chip8->opcode & 0x00FF);
  // u8 n = (chip8->opcode & 0x000F);

  // switch to distinguish between the first bit ranging
  // from 0 - F (16)
  switch (chip8->opcode & 0xF000) {
  case (0x0000): {
    // lookup the last two bytes
    switch (chip8->opcode & 0x00FF) {
    case 0x00E0:
      op_00E0(chip8);
      break;
    case 0x00EE:
      op_00EE(chip8);
      break;
    }
    break;
  }
  case (0x1000): {
    op_1nnn(chip8);
    break;
  }
  case (0x2000): {
    op_2nnn(chip8);
    break;
  }
  case (0x3000): {
    op_3xkk(chip8);
    break;
  }
  case (0x4000): {
    op_4xkk(chip8);
    break;
  }
  case (0x5000): {
    op_5xy0(chip8);
    break;
  }
  case (0x6000): {
    op_6xkk(chip8);
    break;
  }
  case (0x7000): {
    op_7xkk(chip8);
    break;
  }
  case (0x8000): {
    switch (chip8->opcode & 0x000F) {
    case (0x0000): {
      op_8xy0(chip8);
      break;
    }
    case (0x0001): {
      op_8xy1(chip8);
      break;
    }
    case (0x0002): {
      op_8xy2(chip8);
      break;
    }
    case (0x0003): {
      op_8xy3(chip8);
      break;
    }
    case (0x0004): {
      op_8xy4(chip8);
      break;
    }
    case (0x0005): {
      op_8xy5(chip8);
      break;
    }
    case (0x0006): {
      op_8xy6(chip8);
      break;
    }
    case (0x0007): {
      op_8xy7(chip8);
      break;
    }
    case (0x000E): {
      op_8xyE(chip8);
      break;
    }
    }
    break;
  }
  case (0x9000): {
    op_9xy0(chip8);
    break;
  }
  case (0xA000): {
    op_Annn(chip8);
    break;
  }
  case (0xB000): {
    op_Bnnn(chip8);
    break;
  }
  case (0xC000): {
    op_Cxkk(chip8);
    break;
  }
  case (0xD000): {
    op_Dxyn(chip8);
    break;
  }
  case (0xE000): {
    switch (chip8->opcode & 0x00FF) {
    case (0x009E): {
      op_Ex9E(chip8);
      break;
    }
    case (0x00A1): {
      op_ExA1(chip8);
      break;
    }
    }
    break;
  }
  case (0xF000): {
    switch (chip8->opcode & 0x00FF) {
    case (0x0007): {
      op_Fx07(chip8);
      break;
    }
    case (0x000A): {
      op_Fx0A(chip8);
      break;
    }
    case (0x0015): {
      op_Fx15(chip8);
      break;
    }
    case (0x0018): {
      op_Fx18(chip8);
      break;
    }
    case (0x001E): {
      op_Fx1E(chip8);
      break;
    }
    case (0x0029): {
      op_Fx29(chip8);
      break;
    }
    case (0x0033): {
      op_Fx33(chip8);
      break;
    }
    case (0x0055): {
      op_Fx55(chip8);
      break;
    }
    case (0x0065): {
      op_Fx65(chip8);
      break;
    }
    }
    break;
  }
  default: {
    printf("unknown opcode [0x0000]: 0x%X\n", chip8->opcode);
  }
  }
}

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

void handleInput(Chip8 *chip8) {
  chip8->keypad[0x0] = IsKeyDown(KEY_X);
  chip8->keypad[0x1] = IsKeyDown(KEY_ONE);
  chip8->keypad[0x2] = IsKeyDown(KEY_TWO);
  chip8->keypad[0x3] = IsKeyDown(KEY_THREE);
  chip8->keypad[0x4] = IsKeyDown(KEY_Q);
  chip8->keypad[0x5] = IsKeyDown(KEY_W);
  chip8->keypad[0x6] = IsKeyDown(KEY_E);
  chip8->keypad[0x7] = IsKeyDown(KEY_A);
  chip8->keypad[0x8] = IsKeyDown(KEY_S);
  chip8->keypad[0x9] = IsKeyDown(KEY_D);
  chip8->keypad[0xA] = IsKeyDown(KEY_Y);
  chip8->keypad[0xB] = IsKeyDown(KEY_C);
  chip8->keypad[0xC] = IsKeyDown(KEY_FOUR);
  chip8->keypad[0xD] = IsKeyDown(KEY_R);
  chip8->keypad[0xE] = IsKeyDown(KEY_F);
  chip8->keypad[0xF] = IsKeyDown(KEY_V);
}
int main(int argc, char **argv) {

  Chip8 chip8 = {0};
  init_chip8(&chip8);

        /* ROMS */
  // load_rom(&chip8, "IBM.ch8");
  // load_rom(&chip8, "br8kout.ch8");
  // load_rom(&chip8, "fulltest.ch8");
  // load_rom(&chip8, "4-flags.ch8");
  // load_rom(&chip8, "5-quirks.ch8");
  // load_rom(&chip8, "6-keypad.ch8");
  // load_rom(&chip8, "Space.ch8");
  load_rom(&chip8, "binding.ch8");


  double last_instruction_time = GetTime();
  InitWindow(SCREEN_WIDTH * CELL_SIZE, SCREEN_HEIGHT * CELL_SIZE, "CHIP8 Emulator");
  SetTargetFPS(60);
  while (!WindowShouldClose()) {

    double now = GetTime();
    handleInput(&chip8);

    // update instructions with 700Hz
    double instruction_interval = 1.0 / 700;
    int test = 0;
    while((now - last_instruction_time) >= instruction_interval){
      process_instruction(&chip8);
      last_instruction_time += instruction_interval;
      test++;
    }

    // update timers with 60Hz
    if(chip8.delay_timer > 0)chip8.delay_timer --;
    if(chip8.sound_timer > 0)chip8.sound_timer --;

    BeginDrawing();
    ClearBackground(BLACK);
    for (int i = 0; i < SCREEN_HEIGHT; i++) {
      for (int j = 0; j < SCREEN_WIDTH; j++) {
        if (chip8.video[i][j]) {
          DrawRectangle(j * CELL_SIZE, i * CELL_SIZE, CELL_SIZE, CELL_SIZE, WHITE);
        }
      }
    }
    EndDrawing();
  }
  return 0;
}
