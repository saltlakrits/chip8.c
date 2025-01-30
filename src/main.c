#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "display.h"

#define START_ADDR 0x200
#define FIRST_NIBBLE 0xF000
#define SECOND_NIBBLE 0x0F00
#define THIRD_NIBBLE 0x00F0
#define FOURTH_NIBBLE 0x000F
#define SECOND_BYTE 0x00FF
#define ADDR_NIBBLES 0x0FFF

/* Instructions per second */
#define CYCLES 700

/* NOTE: CHIP-8 IS BIG ENDIAN */

/* TODO There are several instructions that are ambiguous, meaning
   they differ between modern and original behaviour.
   One way to maintain (some sort of) compatability is to give the user
   the option to change how they want it to behave.
   This could for example be set with a CLI switch.

   Perhaps I could assign relevant functions to function pointers? */

uint8_t int_to_key(uint8_t key) {
  switch (key) {
  case 0x0:
    return SDL_SCANCODE_X;
  case 0x1:
    return SDL_SCANCODE_1;
  case 0x2:
    return SDL_SCANCODE_2;
  case 0x3:
    return SDL_SCANCODE_3;
  case 0x4:
    return SDL_SCANCODE_Q;
  case 0x5:
    return SDL_SCANCODE_W;
  case 0x6:
    return SDL_SCANCODE_E;
  case 0x7:
    return SDL_SCANCODE_A;
  case 0x8:
    return SDL_SCANCODE_S;
  case 0x9:
    return SDL_SCANCODE_D;
  case 0xA:
    return SDL_SCANCODE_Z;
  case 0xB:
    return SDL_SCANCODE_C;
  case 0xC:
    return SDL_SCANCODE_4;
  case 0xD:
    return SDL_SCANCODE_R;
  case 0xE:
    return SDL_SCANCODE_F;
  case 0xF:
    return SDL_SCANCODE_V;
  default:
    return key;
  }
}

int compare_ts(struct timespec a, struct timespec b) {
  if (a.tv_sec > b.tv_sec) {
    return -1;
  }
  if (b.tv_sec > a.tv_sec) {
    return 1;
  }
  if (a.tv_nsec > b.tv_nsec) {
    return -1;
  }
  if (b.tv_nsec > a.tv_nsec) {
    return 1;
  }
  return 0; // they are equal
}

typedef struct {
  uint16_t *stack;
  uint8_t len;
} Stack;

Stack *new_stack() {
  Stack *st = malloc(sizeof *st);
  st->len = 0;
  st->stack = calloc(128, sizeof *st->stack);

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
  long ins = 0;

  uint8_t is_running = 1;

  /* SDL3 for graphics, sound and controls */
  SDL_Init(SDL_INIT_VIDEO);
  SDL_SetAppMetadata("Chip-8 Emulator", "0.1", NULL);

  /* All drawing happens on a 64x32 surface, but should be rendered
     to 4:3 -- like a tv screen like was used back in the day */
  SDL_Window *window;
  SDL_Renderer *renderer;

  /* create window and renderer (opengl by default) at once, maybe got syntax
  wrong, weird to pass address of pointers */
  SDL_CreateWindowAndRenderer("C8.c", 1920, 1080, SDL_WINDOW_FULLSCREEN,
                              &window, &renderer);
  SDL_Surface *surface = SDL_CreateSurface(64, 32, SDL_PIXELFORMAT_RGBA32);
  SDL_ClearSurface(surface, 0., 0., 0., 1.);

  /* NOTE: Drawing pixels to a surface, converting it to a texture, and then
     scaling that to fit the window (with nearest neighbor scaling) is likely
     to be the most efficient & accurate.

     Another option is to make a texture, and make an array of SDL_FPoint,
     and drawing them as many as possible at a time with SDL_RenderPoints() */

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

  /* Memory; 'actual' memory starts at 0x200 = 512 = START_ADDR,
                 but all memory should be RW */
  uint8_t *mem = calloc(4096, sizeof *mem);
  init_font(&mem[0x50]);

  /* Saves addresses, this is way bigger than it was back then,
     if I understand it correctly -- doesn't matter. Maybe it makes more
     sense to have this as an array so it's, y'know, on the stack... */
  Stack *stack = new_stack();

  /* Registers, 16 of them, and they are 8-bit */
  uint8_t *reg = calloc(16, sizeof *reg);

  /* Program counter */
  uint16_t pc = START_ADDR;

  /* 16-bit index register, points at locations in mem */
  uint16_t ind = 0;

  /* Timers, 8 bit in size, should dec by 1 every Hz (60 times per second) */
  uint8_t delay_timer = 0;
  /* TODO Sound timer should make computer beep while above 0, decs the same way
   */
  uint8_t sound_timer = 0;

  /* Read binary into memory
     0 - 1FF was originally where the interpreter lived, so we
     load the program past that */
  uint16_t counter = START_ADDR;
  uint8_t read_byte = 0;
  while (counter < 4096) {
    read_byte = fgetc(f);
    // printf("byte: 0x%X -> mem[%d]\n", read_byte, counter);
    mem[counter] = read_byte;
    counter++;
  }
  fclose(f);
  // printf("\nDEBUG: Binary successfully loaded into memory! -- File
  // closed.\n");

  /* Main emulator loop:
           - Fetch instruction from memory at current pc
           - Decode instruction to figure out what to do
           - Execute instruction */

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

  struct timespec cycle_start, sleep_until, delay_time, now;
  long cycle_time_ns = 1e9 / CYCLES; // nanoseconds per cycle

  long delay_time_ns = 1e9 / 60; // a 60th of a second in ns

  clock_gettime(CLOCK_MONOTONIC, &now);
  delay_time.tv_sec = now.tv_sec;
  delay_time.tv_nsec = now.tv_nsec + delay_time_ns;
  if (delay_time.tv_nsec >= 1e9) {
    delay_time.tv_sec += 1;
    delay_time.tv_nsec -= 1e9;
  }

  while (is_running) {

    clock_gettime(CLOCK_MONOTONIC, &cycle_start);
    sleep_until.tv_sec = cycle_start.tv_sec;
    sleep_until.tv_nsec = cycle_start.tv_nsec + cycle_time_ns;

    while (sleep_until.tv_nsec >= 1e9) {
      sleep_until.tv_sec += 1;
      sleep_until.tv_nsec -= 1e9;
    }

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
    x_reg = &reg[second_nibble];            // x register value
    y_reg = &reg[third_nibble];             // y register value
    number = instruction & FOURTH_NIBBLE;   // a 4-bit number
    imm_number = instruction & SECOND_BYTE; // 8-bit immediate number
    imm_addr = instruction & ADDR_NIBBLES;  // 12-bit immediate address

    switch (first_nibble) {
    case 0x0:
      switch (fourth_nibble) {
      case 0x0:
        /* Clear display */
        SDL_ClearSurface(surface, 0., 0., 0., 1.);
        // SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer,
        // surface);
        // /* I think I need to set the scale mode every single time */
        // SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        // SDL_RenderTexture(renderer, texture, NULL, NULL);
        // SDL_RenderPresent(renderer);
        // /* Free it! */
        // SDL_DestroyTexture(texture);
        break;

      case 0xE:
        /* Return from subroutine */
        pc = pop_pc(stack);
        break;
      default:
        printf("Unknown instruction 0x%X!\n", instruction);
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
        reg[0xF] = 0; // Original behaviour
        break;
      case 0x2:
        // set VX to (VX & VY)
        *x_reg = (*x_reg & *y_reg);
        reg[0xF] = 0; // Original behaviour
        break;
      case 0x3:
        // set VX to (VX ^ VY)
        *x_reg = (*x_reg ^ *y_reg);
        reg[0xF] = 0; // Original behaviour
        break;
      case 0x4:
        /* add VY to VX, VY unaffected
         should set VF if VX overflows */
        if (*x_reg + *y_reg > 255) {
          *x_reg += *y_reg;
          reg[0xF] = 1;
        } else {
          *x_reg += *y_reg;
          reg[0xF] = 0;
        }
        break;
      case 0x5:
        // VX = VX - VY
        if (*x_reg >= *y_reg) {
          *x_reg = *x_reg - *y_reg;
          reg[0xF] = 1;
        } else {
          *x_reg = *x_reg - *y_reg;
          reg[0xF] = 0;
        }
        break;
      case 0x6:
        // FIXME AMBIGUOUS, original for now
        // VY into VX, then shift VX right
        *x_reg = *y_reg;
        if (*x_reg & 0x1) {
          *x_reg >>= 1;
          reg[0xF] = 1;
        } else {
          *x_reg >>= 1;
          reg[0xF] = 0;
        }
        break;
      case 0x7:
        // VX = VY - VX
        if (*y_reg >= *x_reg) {
          *x_reg = *y_reg - *x_reg;
          reg[0xF] = 1;
        } else {
          *x_reg = *y_reg - *x_reg;
          reg[0xF] = 0;
        }
        break;
      case 0xE:
        // FIXME AMBIGUOUS, original for now
        // VY into VX, then shift VX left
        *x_reg = *y_reg;
        if (*x_reg & 0x80) {
          *x_reg <<= 1;
          reg[0xF] = 1;
        } else {
          *x_reg <<= 1;
          reg[0xF] = 0;
        }
        break;

      default:
        printf("Unknown instruction 0x%X!\n", instruction);
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
      reg[0xF] = draw(surface, *x_reg, *y_reg, number, &mem[ind]);
      // SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
      // /* I think I need to set the scale mode every single time */
      // SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
      // SDL_RenderTexture(renderer, texture, NULL, NULL);
      // SDL_RenderPresent(renderer);
      // /* Free it! */
      // SDL_DestroyTexture(texture);
      break;

    case 0xE:

      SDL_PumpEvents();
      const bool key_state =
          SDL_GetKeyboardState(NULL)[int_to_key((*x_reg & 0xF))];

      switch (fourth_nibble) {
        /* Skips following instruction (i.e. increases PC) depending on
           whether a key is being held -- can't implement until I have input */

      case 0xE:
        // skip if key in VX is held
        if (key_state) {
          pc += 2;
        }
        break;

      case 0x1:
        // skip if key in VX is NOT held
        if (!key_state) {
          pc += 2;
        }
        break;

      default:
        printf("Unknown instruction 0x%X!\n", instruction);
        break;
      }
      break;

    case 0xF:
      /* This could also switch on imm_number, but that felt less readable
       */
      switch ((third_nibble << 4) | fourth_nibble) {
      // switch (imm_number) {
      case 0x07:
        // Set VX to delay timer
        *x_reg = delay_timer;
        break;

      case 0x15:
        // Set delay timer to VX
        delay_timer = *x_reg;
        break;

      case 0x18:
        // Set sound timer to VX
        sound_timer = *x_reg;
        break;

      case 0x1E:
        /* add VX to index, sets non-standard "overflow" flag but should be
           safe, see "Add to index" in the guide */
        if ((ind + *x_reg) > 0xFFF) {
          ind = (ind + *x_reg) & 0xFFF;
          reg[0xF] = 1;
        } else {
          ind = (ind + *x_reg) & 0xFFF;
          reg[0xF] = 0;
        }
        break;

      case 0x0A:
        /* wait (block) until key, put key in VX. timers should still move.
           on the original cosmac vip, it was PRESS AND RELEASE. but just
           press is likely fine. */

        pc -= 2;
        for (int i = 0; i < 16; i++) {
          /* This might be faulty logic! This gets any key, even if it was
           * already held (i.e., isn't a new key)*/
          if (SDL_GetKeyboardState(NULL)[int_to_key(i)]) {
            *x_reg = i;
            /* We got a key, so we inc the program counter again so
               we break out of the instruction loop and continue to the
               next */
            pc += 2;
            break;
          }
        }
        break;

      case 0x29:
        /* set ind to point at hexadecimal char in VX. Font starts at 0x50,
           and each character is a 5-byte sequence. I may not need to mask
           for the last nibble here, but I doubt it's a performance hit
           even if it's unnecessary... */
        ind = 0x50 + (*x_reg & 0xF) * 5;
        break;

      case 0x33:
        /* Take number from VX (0-255 because 8 bits) and get 3 decimal
           numbers, e.g. 159 would be 1, 5, 9 (division and modulo for
           this). Store result in mem[ind], mem[ind+1], mem[ind+2] */

        mem[ind] = *x_reg / 100;
        mem[(ind + 1) & 0xFFF] = (*x_reg % 100) / 10;
        mem[(ind + 2) & 0xFFF] = (*x_reg) % 10;
        break;

      case 0x55:
        /* Store registers V0 through VX (inclusive) to memory, starting at
        ind */
        // printf("0xFX55 instruction, *x_reg is: %d\nind is 0x%X\n", *x_reg,
        // ind);
        for (int i = 0; i <= second_nibble; i++) {
          // puts("inside loop!");
          // if (i > 15) {
          //   printf("x_reg = %p, *x_reg = %d\n", x_reg, *x_reg);
          // }
          mem[ind] = reg[i];
          ind = (ind + 1) & 0xFFF;
        }
        break;

      case 0x65:
        // Opposite of last, loads registers from memory

        // printf("0xFX65 instruction, *x_reg is: %d\n", *x_reg);
        for (int i = 0; i <= second_nibble; i++) {
          // printf("inside 0xFX65; ind = %X, i = %d, saving reg[%d] ->
          // mem[%X]\n",
          // ind, i, i, ind);
          reg[i] = mem[ind];
          ind = (ind + 1) & 0xFFF;
        }
        break;

      default:
        printf("Unknown instruction 0x%X!\n", instruction);
        break;
      }
      break;

    default:
      printf("Unknown instruction 0x%X!\n", instruction);
      break;
    }

    /* Past the big switch, we handle SDL events? */

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
        is_running = 0;
        break;
      case SDL_EVENT_KEY_UP:
        switch (event.key.scancode) {
        case SDL_SCANCODE_ESCAPE:
          is_running = 0;
          break;
        default:
          break;
        }
        break;
      }
    }

    /* DEC TIMERS BY 1 EVERY 60th OF A SECOND */
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (compare_ts(now, delay_time) == -1) {
      if (sound_timer > 0) {
        sound_timer -= 1;
      }
      if (delay_timer > 0) {
        delay_timer -= 1;
      }

      clock_gettime(CLOCK_MONOTONIC, &now);
      delay_time.tv_sec = now.tv_sec;
      delay_time.tv_nsec = now.tv_nsec + delay_time_ns;
      if (delay_time.tv_nsec >= 1e9) {
        delay_time.tv_sec += 1;
        delay_time.tv_nsec -= 1e9;
      }

      SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
      /* I think I need to set the scale mode every single time */
      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
      SDL_RenderTexture(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);
      /* Free it! */
      SDL_DestroyTexture(texture);
    }

    // ins++;
    // if (ins == 317) {
    //   puts("0xF165");
    // }
    // printf("DEBUG: Instruction #%lo, 0x%X\n", ins, instruction);

    /* LIMIT SPEED: roughly 700 instructions per second -> 1/700 seconds per
     * instruction */
    clock_nanosleep(CLOCK_MONOTONIC, 1, &sleep_until, NULL);

    /* TODO PLAY BEEP WHILE SOUND TIMER ISN'T 0 */
  }

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();
  return 0;
}
