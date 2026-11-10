#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#define VIEWH 208 /* 3D view height; the status bar fills the rest */

/* Draws the 3D view and weapon. extra_turn: render-only view angle offset (radians) for smooth mouse look. */
void r_render_view(uint8_t *fb, float extra_turn);
void r_draw_hud(uint8_t *fb);
/* palette for this frame, including damage/pickup flashes */
void r_frame_palette(uint8_t out[256 * 3]);
/* darken the whole screen (menus over the game) */
void r_dim(uint8_t *fb, int level);

#endif
