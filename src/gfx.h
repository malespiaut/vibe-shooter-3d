/* Procedurally generated art: palette, colormaps, textures, sprites. Render side (float allowed). */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#include "game.h"

#define NLIGHTS 32
#define TRANSPARENT 255
#define RAMP(r, s) ((uint8_t)((r) * 16 + (s)))
enum { R_GRAY, R_BROWN, R_TAN, R_RED, R_ORANGE, R_YELLOW, R_GREEN, R_OLIVE,
       R_BLUE, R_CYAN, R_PURPLE, R_SKIN, R_STEEL, R_BLOOD, R_PINK, R_FIRE };

typedef struct {
    int w, h;
    uint8_t *pix; /* column-major: pix[x * h + y], TRANSPARENT = see-through */
} image_t;

extern uint8_t palette[256 * 3];
extern uint8_t colormap[NLIGHTS][256];
extern uint8_t walltex[WT_COUNT][64 * 64]; /* column-major */
extern uint8_t flattex[FT_COUNT][64 * 64]; /* row-major */
extern image_t sprites[SPR_COUNT];

void gfx_init(void);
/* palette tinted toward (r,g,b) by amount 0..255 */
void gfx_tint_palette(uint8_t out[256 * 3], int r, int g, int b, int amount);
uint8_t gfx_nearest(int r, int g, int b);

#endif
