#include <stdint.h>
#include <stdio.h>

#define WIDTH 64
#define HEIGHT 32

/* Display pixels are on/off, so only write 1 or 0 */

void clear_display(void *v_display) {
  // cast for indexing
  uint8_t(*display)[HEIGHT][WIDTH] = v_display;

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      (*display)[y][x] = 0;
    }
  }
}

/* Draws to display. starts at x/y coords. nibble height is 0-15 (nibble).
   the "sprite" is a byte, so 1000 0001 would flip one pixel at start,
   and another 7 pixels to the right.
   return 1 IF ANY PIXELS WERE TURNED OFF (and set that to register 15, VF),
   else 0 */
uint8_t draw(void *v_display, uint8_t x_coord, uint8_t y_coord,
             uint8_t nibble_height, uint8_t *sprite_ptr) {
  uint8_t(*display)[HEIGHT][WIDTH] = v_display;

  /* Starting positions should modulo */
  x_coord = x_coord % (WIDTH - 1);
  y_coord = y_coord % (HEIGHT - 1);

  /* sprite_ptr points at first byte of several that make up a sprite,
     where each byte is a new horizontal line, on the next line vertically */
  uint8_t sprite_line = 0;
  uint8_t pixel_arr[8];

  uint8_t has_flipped = 0;
  for (int y_offset = 0; y_offset < nibble_height; y_offset++) {
    if (y_coord + y_offset > HEIGHT) {
      continue;
    }

    sprite_line = sprite_ptr[y_offset];

    for (int i = 0; i < 8; i++) {
      pixel_arr[i] = (sprite_line & (1 << (7 - i)));
    }

    for (int x_offset = 0; x_offset < 8; x_offset++) {
      if (x_coord + x_offset > WIDTH) {
        continue;
      }
      if (pixel_arr[x_offset]) {
        has_flipped = 1;
        (*display)[y_coord + y_offset][x_coord + x_offset] ^= 1;
      }
    }
  }

  return has_flipped;
}

void print_display(void *v_display) {
  uint8_t(*display)[HEIGHT][WIDTH] = v_display;

  puts("DEBUG: Printing display after drawing to it");
  /* Print out the display here for debug */
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 64; x++) {
      if ((*display)[y][x] == 1) {
        printf("#");
      } else {
        printf(" ");
      }
    }
    printf("\n");
  }
}
