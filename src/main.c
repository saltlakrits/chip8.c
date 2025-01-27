#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "display.h"

#define START_ADR 0x200
#define FIRST_NIBBLE 0xF000
#define SECOND_NIBBLE 0x0F00
#define THIRD_NIBBLE 0x00F0
#define FOURTH_NIBBLE 0x000F
#define SECOND_BYTE 0x00FF
#define ADDR_NIBBLES 0x0FFF

/* NOTE: CHIP-8 IS BIG ENDIAN */

/* TODO There are several instructions that are ambiguous, meaning
   they differ between modern and original behaviour.
   One way to maintain compatability (sort of) is to give the user
   the option to change how they want it to behave.
   This could for example be set with a CLI switch, and I could
   assign relevant functions to function pointers! */

typedef struct {
  uint16_t *stack;
  uint8_t len;
} Stack;

Stack *new_stack() {
  Stack *st = malloc(sizeof *st);
  st->len = 0;
  st->stack = calloc(128, sizeof st->stack);

  return st;
}

/* Push program counter to stack */
void push_pc(uint16_t pc, Stack *st) {
  st->stack[st->len] = pc;
  st->len++;
}

/* Pop program counter from stack */
uint16_t pop_pc(Stack *st) {
  st->len--;
  return st->stack[st->len];
}

int main(int argc, char **args) {

  /* Open binary file passed as arg */
  FILE *f = NULL;
  if (argc <= 1) {
    printf("no binary specified, exiting\n");
    return -1;
  } else {
    f = fopen(args[1], "rb");
  }

  /* Seed rng */
  srand(time(NULL));

  /* Memory; 'actual' memory starts at 0x200 = 512 = START_ADR,
                 but all memory should be RW */
  uint8_t *mem = calloc(4096, sizeof *mem);

  /* Saves addresses, this is way bigger than it was back then,
     if I understand it correctly -- doesn't matter. Maybe it makes more
     sense to have this as an array so it's, y'know, on the stack... */
  Stack *stack = new_stack();

  /* Registers, 16 of them, and they are 8-bit */
  uint8_t *reg = calloc(16, sizeof *reg);

  /* Program counter */
  uint16_t pc = START_ADR;

  /* 16-bit index register, points at locations in mem */
  uint16_t ind = 0;

  /* Timers, 8 bit in size, should dec by 1 every Hz (60 times per second) */
  uint8_t delay_timer = 0;
  /* Sound timer should make computer beep while above 0, decs the same way */
  uint8_t sound_timer = 0;

  /* TODO Font, hazy about this, should be in 0x000 - 0x1FF space though */

  /* TODO Key input */

  /* TODO Display kinda NYI, just a 2d array we write to for now */
  // 64 pixels wide, 32 tall. vm type
  uint8_t(*display)[32][64] = malloc(sizeof *display);

  /* Read binary into memory
     0 - 1FF was originally where the interpreter lived, so we
     load the program past that */
  uint16_t counter = START_ADR;
  uint8_t read_byte = 0;
  while (counter < 4096) {
    read_byte = fgetc(f);
    printf("byte: 0x%X -> mem[%d]\n", read_byte, counter);
    mem[counter] = read_byte;
    counter++;
  }
  fclose(f);
  printf("\nDEBUG: Binary successfully loaded into memory! -- File closed.\n");

  /* Main emulator loop:
           - Fetch instruction from memory at current pc
           - Decode instruction to figure out what to do
           - Execute instruction
           TODO need to delay this appropriately, to match the hardware of the
     time -- runs at full speed (way, way too fast) currently */

  uint16_t instruction = 0;
  uint8_t first_nibble = 0;
  uint8_t second_nibble = 0;
  uint8_t third_nibble = 0;
  uint8_t fourth_nibble = 0;

  uint8_t *x_reg = NULL;
  uint8_t *y_reg = NULL;
  uint8_t number = 0;
  uint8_t imm_number = 0;
  uint16_t imm_addr = 0;

  while (1) {
    /* Read two bytes as one instruction, big endian */
    instruction = (mem[pc] << 8) | (mem[pc + 1]);

    /* Increase pc by two */
    pc += 2;
    if (pc >= 4096) {
      /* Not entirely sure what should happen here, as the program
       shouldn't overflow the program counter to begin with */
      // exit(1);
    }

    /* Pick apart and carry out the instruction */

    /* We decode by each nibble */
    first_nibble = ((instruction & FIRST_NIBBLE) >> 12); // top of instruction
    second_nibble = ((instruction & SECOND_NIBBLE) >> 8);
    third_nibble = ((instruction & THIRD_NIBBLE) >> 4);
    fourth_nibble = instruction & FOURTH_NIBBLE;

    /* Depending on the instruction, any nibble or combination of nibbles
                 past the first will carry some meaning. We pick apart all
       possible meanings here, so we can use them easily */
    x_reg = &reg[((instruction & SECOND_NIBBLE) >> 8)]; // x register value
    y_reg = &reg[((instruction & THIRD_NIBBLE) >> 4)];  // y register value
    number = instruction & FOURTH_NIBBLE;               // a 4-bit number
    imm_number = instruction & SECOND_BYTE; // 8-bit immediate number
    imm_addr = instruction & ADDR_NIBBLES;  // 12-bit immediate address

    switch (first_nibble) {
    case 0x0:

      switch (second_nibble) {
      case 0x0:

        switch (third_nibble) {
        case 0xe:

          switch (fourth_nibble) {
          case 0x0:
            /* clear display */
            clear_display(display);
            break;

          case 0xe:
            /* Return from subroutine, i.e. pop pc from stack */
            pc = pop_pc(stack);
            break;
          }
          break;
        }
        break;
      }
      break;

    case 0x1:
      /* jump 1NNN (to imm_addr) */
      pc = imm_addr;
      break;

    case 0x2:
      /* push pc to stack and jump to subroutine */
      push_pc(pc, stack);
      pc = imm_addr;
      break;

    case 0x3:
      // puts("if x ==...");
      if (*x_reg == imm_number) {
        pc += 2;
      }
      break;

    case 0x4:
      // puts("if x !=...");
      if (*x_reg != imm_number) {
        pc += 2;
      }
      break;

    case 0x5:
      // puts("if x == y...");
      if (*x_reg == *y_reg) {
        pc += 2;
      }
      break;

    case 0x6:
      // puts("x = imm");
      *x_reg = imm_number;
      break;

    case 0x7:
      // puts("add x,#imm");
      *x_reg += imm_number;
      break;

    case 0x8:
      switch (fourth_nibble) {
      case 0x0:
        // set VX to value of VY
        *x_reg = *y_reg;
        break;
      case 0x1:
        // set VX to (VX | VY)
        *x_reg = (*x_reg | *y_reg);
        break;
      case 0x2:
        // set VX to (VX & VY)
        *x_reg = (*x_reg & *y_reg);
        break;
      case 0x3:
        // set VX to (VX ^ VY)
        *x_reg = (*x_reg ^ *y_reg);
        break;
      case 0x4:
        /* add VY to VX, VY unaffected
         should set VF if VX overflows */
        reg[0xF] = (*x_reg + *y_reg > 255 ? 1 : 0);
        *x_reg += *y_reg;
        break;
      case 0x5:
        // VX = VX - VY
        reg[0xF] = (*x_reg > *y_reg ? 1 : 0);
        *x_reg = *x_reg - *y_reg;
        break;
      case 0x6:
        // FIXME AMBIGUOUS, modern for now
        // shift VX right
        *x_reg >>= 1;
        break;
      case 0x7:
        // VX = VY - VX
        reg[0xF] = (*y_reg > *x_reg ? 1 : 0);
        *x_reg = *y_reg - *x_reg;
        break;
      case 0xE:
        // FIXME AMBIGUOUS, modern for now
        // shift VX left
        *x_reg <<= 1;
        break;
      }
      break;

    case 0x9:
      // puts("if x != y...");
      if (*x_reg != *y_reg) {
        pc += 2;
      }
      break;

    case 0xA:
      // puts("mov ind,#addr");
      ind = imm_addr;
      break;

    case 0xB:
      // FIXME AMBIGUOUS, original for now, apparently more compatible??
      // Jump to imm_addr + V0 (jump with offset)
      pc = imm_addr + reg[0x0];
      break;

    case 0xC:
      // Generate random number and & it with NN, assign to VX
      *x_reg = rand() & imm_number;
      break;

    case 0xD:
      /* Drawing, see function for info */
      reg[0xF] = draw(display, *x_reg, *y_reg, fourth_nibble, &mem[ind]);
      print_display(display);
      break;

    case 0xE:
      switch (fourth_nibble) {
        /* TODO Skips following instruction (i.e. increases PC) depending on
           whether a key is being held -- can't implement until I have input */

      case 0xE:
        break;

      case 0x1:
        break;
      }
      break;

    case 0xF:
      /* This could also switch on imm_number, but that felt less readable */
      switch ((third_nibble << 4) & fourth_nibble) {
      case 0x07:
        break;

      case 0x15:
        break;

      case 0x18:
        break;

      case 0x1E:
        /* add VX to index, sets flag non-standard but should be safe,
           see "Add to index" in the guide */
        ind += *x_reg;
        reg[0xF] = (((ind + *x_reg) > 0xFFF) ? 1 : 0);
        break;

      case 0x0A:
        /* wait (block) until key, put key in VX. timers should still move.
           on the original cosmac vip, it was PRESS AND RELEASE. but just press
           is likely fine. */
        // FIXME replace this line with checking for key
        pc -= 2; // ONLY IF NOT KEY IS PRESSED, WILL RESULT IN SAME INSTRUCTION
                 // AGAIN
        // FIXME ELSE, PUT KEY IN VX
        break;

      case 0x29:
        // TODO set ind to point at hexadecimal char in VX. Font NYI so can't do
        // it yet.
        break;

      case 0x33:
        /* TODO Take number from VX (0-255 because 8 bits) and get 3 decimal
           numbers, e.g. 159 would be 1, 5, 9 (division and modulo for this).
           Store result in mem[ind], mem[ind+1], mem[ind+2] */
        break;

      case 0x55:
        break;

      case 0x65:
        break;
      }
      break;
    }
  }
  return 0;
}
