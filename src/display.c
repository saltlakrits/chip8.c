#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>

#define WIDTH 64
#define HEIGHT 32

/* Display pixels are on/off, so only write 1 or 0 */

/* Draws to display. starts at x/y coords. nibble height is 0-15 (nibble).
   the "sprite" is a byte, so 1000 0001 would flip one pixel at start,
   and another 7 pixels to the right.
   return 1 IF ANY PIXELS WERE TURNED OFF (and set that to register 15, VF),
   else 0 */
uint8_t draw(SDL_Surface *surface, uint8_t x_coord, uint8_t y_coord,
             uint8_t nibble_height, uint8_t *sprite_ptr) {

  /* Starting positions should modulo */
  x_coord = x_coord % (WIDTH - 1);
  y_coord = y_coord % (HEIGHT - 1);

  /* sprite_ptr points at first byte of several that make up a sprite,
     where each byte is a new horizontal line, on the next line vertically */
  uint8_t sprite_line = 0;
  uint8_t pixel_arr[8];

  /* Pixels to pass into SDL_ReadSurfacePixel() */
  uint8_t r;
  uint8_t g;
  uint8_t b;

  /* Coordinates + their offset */
  uint8_t offset_x;
  uint8_t offset_y;

  /* Result "boolean" int */
  uint8_t has_flipped = 0;

  for (int y_offset = 0; y_offset < nibble_height; y_offset++) {
    offset_y = y_coord + y_offset;
    if (offset_y >= HEIGHT) {
      continue;
    }

    sprite_line = sprite_ptr[y_offset];

    for (int i = 0; i < 8; i++) {
      pixel_arr[i] = (sprite_line & (1 << (7 - i)));
    }

    for (int x_offset = 0; x_offset < 8; x_offset++) {
      offset_x = x_coord + x_offset;
      if (offset_x >= WIDTH) {
        continue;
      }
      if (pixel_arr[x_offset]) {

        /* has_flipped was checked HERE, which is faulty! */
        SDL_ReadSurfacePixel(surface, offset_x, offset_y, &r, &g, &b, NULL);
        if ((r | g | b) == 0) {
          /* If pixel WAS off, flip to on */
          SDL_WriteSurfacePixel(surface, offset_x, offset_y, 255, 255, 255,
                                255);
        } else {
          /* If pixel WAS on, flip to off, also set return bool */
          has_flipped = 1;
          SDL_WriteSurfacePixel(surface, offset_x, offset_y, 0, 0, 0, 255);
        }
      }
    }
  }

  return has_flipped;
}

void init_font(uint8_t *mem) {
  /* Lazy way to put a font in memory. Puts the font in the memory, starting at
   * the pointer */

  // 0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  // 0x20, 0x60, 0x20, 0x20, 0x70, // 1
  // 0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  // 0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  // 0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  // 0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  // 0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  // 0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  // 0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  // 0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  // 0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  // 0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  // 0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  // 0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  // 0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  // 0xF0, 0x80, 0xF0, 0x80, 0x80  // F

  uint8_t font_arr[] = {
      0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70, 0xF0, 0x10,
      0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0, 0x90, 0x90, 0xF0, 0x10,
      0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0, 0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0,
      0x10, 0x20, 0x40, 0x40, 0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0,
      0x10, 0xF0, 0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0,
      0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0, 0xF0, 0x80,
      0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80};

  memcpy(mem, font_arr, 5 * 16);
}
