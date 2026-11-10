/* Headless renderer for checking visuals without a window.
 *   snapshot LEVEL OUT.ppm [x y angle_deg [tics]]   render the view at a tile position
 *   snapshot --sprites OUT.ppm [first] | --textures OUT.ppm   art contact sheets
 *   snapshot --wav DIR [musicfile]                  each sfx as DIR/sfxN.wav, plus 30 s of music */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "game.h"
#include "gfx.h"
#include "platform.h"
#include "render.h"
#include "sound.h"

static uint8_t fb[SCREENW * SCREENH];

/* the mixer locks around the audio thread; there is none here */
void plat_audio_lock(void) {}
void plat_audio_unlock(void) {}

static void write_wav(const char *path, const int16_t *s, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) exit(1);
    uint32_t data = (uint32_t)frames * 4, riff = 36 + data, rate = MIXRATE, bps = MIXRATE * 4, fmt = 16;
    uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f);
    fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&bps, 4, 1, f);
    fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(s, 4, (size_t)frames, f);
    fclose(f);
}

static int dump_audio(const char *dir, const char *music)
{
    static int16_t buf[MIXRATE * 2 * 30];
    char path[512];
    snd_init();
    snd_set_volume(15, 0);
    for (int i = 1; i < SFX_COUNT; i++) {
        snd_play(i, 255, 128);
        snd_mix(buf, MIXRATE * 2);
        snprintf(path, sizeof path, "%s/sfx%02d.wav", dir, i);
        write_wav(path, buf, MIXRATE * 2);
    }
    if (music) {
        FILE *f = fopen(music, "rb");
        static char text[65536];
        if (!f) return 1;
        text[fread(text, 1, sizeof text - 1, f)] = 0;
        fclose(f);
        snd_set_volume(0, 15);
        snd_music(text);
        snd_mix(buf, MIXRATE * 30);
        snprintf(path, sizeof path, "%s/music.wav", dir);
        write_wav(path, buf, MIXRATE * 30);
    }
    return 0;
}

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
    if (!strcmp(argv[1], "--wav")) return dump_audio(argv[2], argc > 3 ? argv[3] : NULL);
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
