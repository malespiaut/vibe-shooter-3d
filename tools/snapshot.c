/* Headless renderer for checking visuals without a window.
 *   snapshot LEVEL OUT.ppm [x y angle_deg [tics]]   render the view at a tile position
 *   snapshot --sprites OUT.ppm                      contact sheet of all sprites and textures */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "game.h"
#include "gfx.h"
#include "platform.h"
#include "render.h"

static uint8_t fb[SCREENW * SCREENH];

static void write_ppm(const char *path, const uint8_t *pal)
{
    FILE *f = fopen(path, "wb");
    if (!f) exit(1);
    fprintf(f, "P6\n%d %d\n255\n", SCREENW, SCREENH);
    for (int i = 0; i < SCREENW * SCREENH; i++) fwrite(&pal[fb[i] * 3], 1, 3, f);
    fclose(f);
}

static void blit(const image_t *im, int ox, int oy)
{
    for (int x = 0; x < im->w; x++)
        for (int y = 0; y < im->h; y++) {
            uint8_t c = im->pix[x * im->h + y];
            int sx = ox + x, sy = oy + y;
            if (c != TRANSPARENT && sx < SCREENW && sy < SCREENH) fb[sy * SCREENW + sx] = c;
        }
}

int main(int argc, char **argv)
{
    if (argc < 3) return 1;
    fixed_init();
    gfx_init();
    if (!strcmp(argv[1], "--sprites")) {
        memset(fb, RAMP(R_PURPLE, 3), sizeof fb);
        int first = argc > 3 ? atoi(argv[3]) : 0, x = 0, y = 0, rowh = 0; /* flow layout from sprite #first */
        for (int i = first; i < SPR_COUNT; i++) {
            const image_t *im = &sprites[i];
            if (x + im->w > SCREENW) {
                x = 0;
                y += rowh;
                rowh = 0;
            }
            blit(im, x, y);
            x += im->w;
            if (im->h > rowh) rowh = im->h;
        }
        write_ppm(argv[2], palette);
        return 0;
    }
    if (!strcmp(argv[1], "--textures")) {
        for (int t = 0; t < WT_COUNT + FT_COUNT; t++) {
            int ox = (t % 5) * 64, oy = (t / 5) * 64;
            for (int x = 0; x < 64; x++)
                for (int y = 0; y < 64 && oy + y < SCREENH; y++)
                    fb[(oy + y) * SCREENW + ox + x] = t < WT_COUNT ? walltex[t][x * 64 + y] : flattex[t - WT_COUNT][y * 64 + x];
        }
        write_ppm(argv[2], palette);
        return 0;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    static char text[65536];
    text[fread(text, 1, sizeof text - 1, f)] = 0;
    fclose(f);
    if (!g_load_level(text, SK_MEDIUM, 0, NULL)) return 1;
    if (argc >= 6) {
        g.player.x = (fixed_t)(atof(argv[3]) * FRACUNIT);
        g.player.y = (fixed_t)(atof(argv[4]) * FRACUNIT);
        g.player.angle = (angle_t)(atof(argv[5]) / 360.0 * 4294967296.0);
    }
    int tics = argc >= 7 ? atoi(argv[6]) : 0;
    ticcmd_t cmd = {0};
    for (int i = 0; i < tics; i++) g_ticker(&cmd);
    r_render_view(fb, 0);
    r_draw_hud(fb);
    uint8_t pal[768];
    r_frame_palette(pal);
    write_ppm(argv[2], pal);
    return 0;
}
