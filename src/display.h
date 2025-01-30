#pragma once
#include <stdint.h>
void clear_display(void *v_display);
uint8_t draw(void *v_display, uint8_t x, uint8_t y, uint8_t nibble_height,
             uint8_t *sprite);
void print_display(void *v_display);

void init_font(uint8_t *mem);