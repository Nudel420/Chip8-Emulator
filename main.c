#include <stdio.h>

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

int main(void) {
    printf("Hello World!\n");
    pc = 1;
    printf("%lu\n", sizeof(memory));
    return 0;
}
