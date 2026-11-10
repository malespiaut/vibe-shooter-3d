/* Soak test: a random-input bot plays every level on every skill while the renderer draws.
 * Meant to be run under -fsanitize=address,undefined; also a plain ctest for crashes/hangs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "gfx.h"
#include "platform.h"
#include "render.h"

static uint8_t fb[SCREENW * SCREENH];

static char *read_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    static char text[65536];
    text[fread(text, 1, sizeof text - 1, f)] = 0;
    fclose(f);
    return text;
}

int main(int argc, char **argv)
{
    int tics = argc > 1 ? atoi(argv[1]) : 2000;
    fixed_init();
    gfx_init();
    uint32_t r = 12345;
    for (int lv = 1; lv <= 8; lv++)
        for (int sk = 0; sk < SK_COUNT; sk++) {
            char path[64];
            snprintf(path, sizeof path, "assets/levels/e1m%d.txt", lv);
            char *text = read_text(path);
            if (!text || !g_load_level(text, sk, lv - 1, NULL)) {
                printf("cannot load %s\n", path);
                return 1;
            }
            g.player.weaponowned = (1 << WP_COUNT) - 1; /* exercise every weapon and door */
            g.player.keys = 7;
            for (int a = 0; a < AM_COUNT; a++) g.player.ammo[a] = 50;
            int deaths = 0;
            ticcmd_t cmd = {0};
            for (int t = 0; t < tics; t++) {
                if (t % 20 == 0) { /* hold each random input for a while so the bot actually travels */
                    r = r * 1664525u + 1013904223u;
                    cmd.forward = (int8_t)((r >> 8) % 3 ? 50 : -25);
                    cmd.side = (int8_t)(((r >> 12) % 3 - 1) * 30);
                    cmd.turn = (int16_t)(((int)((r >> 16) % 9) - 4) * 300);
                    cmd.buttons = (uint8_t)((r >> 20) & 3);
                    cmd.weapon = (uint8_t)((r >> 24) % 8 == 0 ? 1 + (r >> 27) % WP_COUNT : 0);
                }
                g_ticker(&cmd);
                if (g.exiting < 0 || g.player.dead) {
                    deaths++;
                    g_load_level(text, sk, lv - 1, NULL);
                    g.player.weaponowned = (1 << WP_COUNT) - 1;
                    g.player.keys = 7;
                    for (int a = 0; a < AM_COUNT; a++) g.player.ammo[a] = 50;
                }
                if (g.exiting > 0) g.exiting = 0; /* keep playing past the exit */
                if (t % 3 == 0) {
                    r_render_view(fb, 0);
                    r_draw_hud(fb);
                    if (t % 30 == 0) r_draw_automap(fb);
                }
                if (!g_validate()) {
                    printf("%s skill %d tic %d: game state failed validation\n", path, sk, t);
                    return 1;
                }
            }
            printf("%s skill %d: kills %d/%d deaths %d\n", path, sk, g.kills, g.totalkills, deaths);
        }
    return 0;
}
