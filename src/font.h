#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/* Hand-drawn 5x7 font in a 6x8 cell. Lowercase draws as uppercase. */
int font_pixel(char c, int x, int y);
int font_width(const char *s, int scale);
/* Draws with a 1-pixel drop shadow when shadow is nonzero. */
void font_draw(uint8_t *fb, int x, int y, const char *s, uint8_t color, int scale, int shadow);
void font_draw_center(uint8_t *fb, int y, const char *s, uint8_t color, int scale);

#endif
