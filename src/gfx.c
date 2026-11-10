/* All art is generated here at startup: palette, colormaps, wall/flat textures, sprites. */
#include "gfx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"

uint8_t palette[256 * 3];
uint8_t colormap[NLIGHTS][256];
uint8_t walltex[WT_COUNT][64 * 64];
uint8_t flattex[FT_COUNT][64 * 64];
image_t sprites[SPR_COUNT];

/* ---------------------------------------------------------------- palette */

static const uint8_t ramps[16][2][3] = {
    {{0, 0, 0}, {236, 236, 236}},     {{18, 9, 3}, {206, 146, 86}},    {{28, 22, 12}, {240, 220, 176}},
    {{26, 0, 0}, {255, 84, 60}},      {{32, 12, 0}, {255, 176, 64}},   {{28, 24, 0}, {255, 248, 128}},
    {{0, 18, 0}, {128, 240, 96}},     {{12, 15, 4}, {176, 186, 108}},  {{0, 2, 30}, {120, 150, 255}},
    {{0, 16, 20}, {130, 244, 244}},   {{14, 0, 24}, {206, 128, 255}},  {{34, 16, 8}, {255, 204, 166}},
    {{8, 11, 16}, {186, 204, 224}},   {{14, 0, 0}, {176, 24, 20}},     {{30, 6, 16}, {255, 156, 204}},
    {{70, 22, 0}, {255, 255, 236}},
};

uint8_t gfx_nearest(int r, int g, int b)
{
    int best = 0, bestd = 1 << 30;
    for (int i = 0; i < 255; i++) {
        int dr = r - palette[i * 3], dg = g - palette[i * 3 + 1], db = b - palette[i * 3 + 2];
        int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    return (uint8_t)best;
}

static void make_palette(void)
{
    for (int r = 0; r < 16; r++)
        for (int s = 0; s < 16; s++) {
            float t = powf(s / 15.0f, 0.9f);
            for (int c = 0; c < 3; c++)
                palette[(r * 16 + s) * 3 + c] = (uint8_t)(ramps[r][0][c] + (ramps[r][1][c] - ramps[r][0][c]) * t + 0.5f);
        }
    for (int l = 0; l < NLIGHTS; l++) {
        float f = 1.0f - (float)l / NLIGHTS;
        f = f * f * 0.6f + f * 0.4f;
        for (int i = 0; i < 256; i++)
            colormap[l][i] = gfx_nearest((int)(palette[i * 3] * f), (int)(palette[i * 3 + 1] * f), (int)(palette[i * 3 + 2] * f));
        colormap[l][TRANSPARENT] = TRANSPARENT;
    }
}

void gfx_tint_palette(uint8_t out[256 * 3], int r, int g, int b, int amount)
{
    const int tint[3] = {r, g, b};
    for (int i = 0; i < 256 * 3; i++) out[i] = (uint8_t)(palette[i] + (tint[i % 3] - palette[i]) * amount / 256);
}

/* ---------------------------------------------------------------- noise */

static float hashf(int x, int y, int seed)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffff) / 16777216.0f;
}

static float vnoise(float x, float y, int period, int seed)
{
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    int x0 = ((xi % period) + period) % period, y0 = ((yi % period) + period) % period;
    int x1 = (x0 + 1) % period, y1 = (y0 + 1) % period;
    float a = hashf(x0, y0, seed), b = hashf(x1, y0, seed), c = hashf(x0, y1, seed), d = hashf(x1, y1, seed);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

/* tileable on 64x64 when (64 / scale) is an integer */
static float fbm(float x, float y, float scale, int seed, int oct)
{
    float sum = 0, amp = 0.5f, norm = 0;
    int period = (int)(64 / scale);
    for (int o = 0; o < oct; o++) {
        sum += amp * vnoise(x / scale, y / scale, period, seed + o * 17);
        norm += amp;
        amp *= 0.5f;
        scale *= 0.5f;
        period *= 2;
    }
    return sum / norm;
}

static uint8_t sh(int ramp, float s)
{
    int v = (int)lrintf(s);
    int max = ramp == R_FIRE ? 14 : 15;
    if (v < 0) v = 0;
    if (v > max) v = max;
    return RAMP(ramp, v);
}

/* ---------------------------------------------------------------- wall textures (built row-major, stored column-major) */

typedef uint8_t tex64[64][64]; /* [y][x] */

static void store_wall(int id, tex64 t)
{
    for (int x = 0; x < 64; x++)
        for (int y = 0; y < 64; y++) walltex[id][x * 64 + y] = t[y][x];
}

static void tex_blocks(tex64 t, int ramp, int bw, int bh, int seed, float base, int mortar_ramp, float mortar)
{
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            int row = y / bh, off = (row & 1) * (bw / 2);
            int bx = (x + off) % 64, col = bx / bw, lx = bx % bw, ly = y % bh;
            float n = fbm((float)x, (float)y, 8, seed, 3);
            if (lx == 0 || ly == 0) {
                t[y][x] = sh(mortar_ramp, mortar + n * 2);
                continue;
            }
            float s = base + hashf(col, row, seed) * 3 - 1.5f + (n - 0.5f) * 4;
            if (lx == 1 || ly == 1) s += 2;
            if (lx == bw - 1 || ly == bh - 1) s -= 2;
            t[y][x] = sh(ramp, s);
        }
}

static void rivet(tex64 t, int x, int y)
{
    t[y][x] = RAMP(R_STEEL, 13);
    t[y][x + 1] = RAMP(R_STEEL, 9);
    t[y + 1][x] = RAMP(R_STEEL, 8);
    t[y + 1][x + 1] = RAMP(R_STEEL, 4);
}

static void tex_metal(tex64 t, float base, int seed)
{
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            int lx = x % 32, ly = y % 32;
            float s = base + (fbm((float)x, (float)y, 16, seed, 2) - 0.5f) * 2 + (hashf(x, y, seed) - 0.5f) * 0.8f;
            if (lx == 0 || ly == 0) s = base - 4;
            else if (lx == 1 || ly == 1) s += 3;
            else if (lx == 31 || ly == 31) s -= 2;
            t[y][x] = sh(R_STEEL, s);
        }
    for (int py = 0; py < 64; py += 32)
        for (int px = 0; px < 64; px += 32) {
            rivet(t, px + 3, py + 3);
            rivet(t, px + 27, py + 3);
            rivet(t, px + 3, py + 27);
            rivet(t, px + 27, py + 27);
        }
}

static void text_on(tex64 t, int x, int y, const char *s, uint8_t c)
{
    for (; *s; s++, x += 6)
        for (int gy = 0; gy < 7; gy++)
            for (int gx = 0; gx < 5; gx++)
                if (font_pixel(*s, gx, gy) && x + gx < 64 && y + gy < 64) t[y + gy][x + gx] = c;
}

static void make_walls(void)
{
    static tex64 t;
    tex_blocks(t, R_GRAY, 32, 16, 1, 7, R_GRAY, 3);
    store_wall(WT_STONE, t);

    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            int plank = x / 8, lx = x % 8;
            float grain = fbm((float)x * 4, (float)y * 0.5f + plank * 13, 16, 3 + plank, 3);
            float s = 6 + hashf(plank, 0, 3) * 2 + (grain - 0.5f) * 6;
            if (lx == 0) s = 2;
            if (lx == 1) s += 1.5f;
            if ((y == 6 || y == 57) && (lx == 3 || lx == 4)) s = 12; /* nail heads */
            t[y][x] = sh(R_BROWN, s);
        }
    store_wall(WT_WOOD, t);

    tex_metal(t, 7, 5);
    store_wall(WT_METAL, t);

    tex_blocks(t, R_RED, 16, 8, 7, 5, R_TAN, 5);
    for (int y = 0; y < 64; y++) /* tone the brick down toward brown */
        for (int x = 0; x < 64; x++)
            if (t[y][x] / 16 == R_RED && hashf(x / 4, y / 4, 9) < 0.3f) t[y][x] = sh(R_BROWN, t[y][x] % 16 + 1);
    store_wall(WT_BRICK, t);

    tex_blocks(t, R_GRAY, 32, 16, 11, 6, R_GRAY, 2);
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            float m = fbm((float)x, (float)y + 0.5f * y, 16, 21, 4) + (y / 64.0f) * 0.25f;
            if (m > 0.58f) t[y][x] = sh(m > 0.7f ? R_GREEN : R_OLIVE, 2.5f + (m - 0.58f) * 22 + hashf(x, y, 4) * 2);
        }
    store_wall(WT_MOSS, t);

    tex_metal(t, 5, 13);
    for (int y = 10; y < 32; y++)
        for (int x = 8; x < 56; x++) {
            bool edge = y == 10 || y == 31 || x == 8 || x == 55;
            float s = edge ? 2 : 3 + (y % 2) * 0.6f;
            t[y][x] = edge ? RAMP(R_STEEL, 2) : sh(R_CYAN, s);
        }
    for (int line = 0; line < 6; line++) /* lines of "text" on the screen */
        for (int x = 11; x < 11 + 6 + (int)(hashf(line, 1, 2) * 36); x++)
            if (hashf(x, line, 7) < 0.7f) t[13 + line * 3][x] = RAMP(R_CYAN, 13);
    for (int i = 0; i < 6; i++) {
        t[42][12 + i * 8] = RAMP(i & 1 ? R_GREEN : R_RED, 14);
        t[42][13 + i * 8] = RAMP(i & 1 ? R_GREEN : R_RED, 11);
    }
    for (int y = 48; y < 60; y += 3)
        for (int x = 10; x < 54; x++) t[y][x] = RAMP(R_STEEL, 1);
    store_wall(WT_TECH, t);

    /* doors: plain, then three locked variants with a colored band */
    static const int lockramp[4] = {-1, R_RED, R_BLUE, R_YELLOW};
    static const int lockid[4] = {WT_DOOR, WT_DOOR_RED, WT_DOOR_BLUE, WT_DOOR_YELLOW};
    for (int k = 0; k < 4; k++) {
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 64; x++) {
                float s = 8 + (fbm((float)x, (float)y, 16, 31, 2) - 0.5f) * 2;
                if (y % 8 == 0) s -= 3;
                if (y % 8 == 1) s += 2;
                if (x < 2 || x > 61) s -= 3;
                t[y][x] = sh(R_STEEL, s);
                if (lockramp[k] >= 0 && y >= 26 && y < 38)
                    t[y][x] = sh(lockramp[k], (y == 26 || y == 37) ? 5 : 10 + (fbm((float)x, (float)y, 8, 2, 2) - 0.5f) * 3);
            }
        for (int y = 28; y < 36; y++) /* handle */
            for (int x = 50; x < 54; x++) t[y][x] = sh(R_GRAY, x == 50 ? 12 : 5 + (y & 1));
        store_wall(lockid[k], t);
    }

    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            int lx = x % 16;
            float s = 5 + (lx < 2 ? 3 : lx > 13 ? -2 : 0) + (hashf(x, y, 41) - 0.5f);
            t[y][x] = sh(R_STEEL, s);
        }
    store_wall(WT_DOORSIDE, t);

    for (int on = 0; on < 2; on++) {
        tex_metal(t, 6, 17);
        for (int y = 6; y < 18; y++)
            for (int x = 18; x < 46; x++) t[y][x] = RAMP(R_GRAY, 1);
        text_on(t, 21, 8, "EXIT", on ? RAMP(R_GREEN, 14) : RAMP(R_RED, 12));
        for (int y = 26; y < 54; y++) /* switch housing */
            for (int x = 24; x < 40; x++) t[y][x] = sh(R_GRAY, (x == 24 || y == 26) ? 9 : (x == 39 || y == 53) ? 2 : 4);
        int ly0 = on ? 29 : 40, ly1 = on ? 40 : 51; /* lever */
        for (int y = ly0; y < ly1; y++)
            for (int x = 30; x < 34; x++) t[y][x] = sh(R_STEEL, x == 30 ? 13 : 9 - (x - 30));
        for (int x = 28; x < 36; x++)
            for (int y = (on ? ly0 : ly1 - 3); y < (on ? ly0 + 3 : ly1); y++) t[y][x] = sh(R_RED, x < 30 ? 12 : 8);
        store_wall(on ? WT_EXIT_ON : WT_EXIT, t);
    }
}

static void make_flats(void)
{
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            int lx = x % 16, ly = y % 16;
            float n = fbm((float)x, (float)y, 8, 51, 3);
            float s = 6 + hashf(x / 16, y / 16, 52) * 2 + (n - 0.5f) * 3;
            if (lx == 0 || ly == 0) s = 2.5f;
            else if (lx == 1 || ly == 1) s += 1;
            flattex[FT_STONE][y * 64 + x] = sh(R_GRAY, s);

            float d = fbm((float)x, (float)y, 16, 53, 4);
            flattex[FT_DIRT][y * 64 + x] = sh(R_BROWN, 3 + d * 6 + (hashf(x, y, 54) - 0.5f) * 1.5f);

            int dx = (x + (y / 8 % 2) * 4) % 8, dy = y % 8;
            bool bump = (dx == dy / 2 + 1 || dx == dy / 2 + 2) && dy < 6;
            float m = 6 + (fbm((float)x, (float)y, 16, 55, 2) - 0.5f) * 2;
            flattex[FT_METAL][y * 64 + x] = sh(R_STEEL, bump ? m + 3 : (dx == dy / 2 + 3 && dy < 6) ? m - 2 : m);

            int cx = x % 32, cy = y % 32;
            float c = 4 + (fbm((float)x, (float)y, 16, 56, 2) - 0.5f) * 1.5f;
            if (cx == 0 || cy == 0) c = 1.5f;
            else if (cx >= 11 && cx < 21 && cy >= 11 && cy < 21)
                c = (cx == 11 || cy == 11 || cx == 20 || cy == 20) ? 3 : 13; /* light fixture */
            flattex[FT_CEIL_PANEL][y * 64 + x] = (cx >= 12 && cx < 20 && cy >= 12 && cy < 20) ? sh(R_TAN, c) : sh(R_GRAY, c);

            float r = fbm((float)x, (float)y, 16, 57, 4);
            flattex[FT_CEIL_ROCK][y * 64 + x] = sh(r > 0.5f ? R_BROWN : R_GRAY, 1.5f + r * 5);
        }
}

/* ---------------------------------------------------------------- sprite drawing */

static image_t *cur;

static void begin(int id, int w, int h)
{
    cur = &sprites[id];
    cur->w = w;
    cur->h = h;
    cur->pix = malloc((size_t)(w * h));
    memset(cur->pix, TRANSPARENT, (size_t)(w * h));
}

static void put(int x, int y, uint8_t c)
{
    if (x >= 0 && y >= 0 && x < cur->w && y < cur->h) cur->pix[x * cur->h + y] = c;
}

/* Shaded solid capsule (limbs, barrels). Light comes from the upper left. */
static void capsule(float x0, float y0, float x1, float y1, float r, int ramp, float base)
{
    int minx = (int)floorf(fminf(x0, x1) - r), maxx = (int)ceilf(fmaxf(x0, x1) + r);
    int miny = (int)floorf(fminf(y0, y1) - r), maxy = (int)ceilf(fmaxf(y0, y1) + r);
    float vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float t = len2 > 0 ? ((px - x0) * vx + (py - y0) * vy) / len2 : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            float nx = (px - (x0 + vx * t)) / r, ny = (py - (y0 + vy * t)) / r;
            float d2 = nx * nx + ny * ny;
            if (d2 > 1) continue;
            float light = -(nx * 0.6f + ny * 0.8f);
            put(x, y, sh(ramp, base + light * 3.5f + (1 - d2) * 1.0f));
        }
}

static void ellipse(float cx, float cy, float rx, float ry, int ramp, float base)
{
    for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++)
        for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
            float nx = (x + 0.5f - cx) / rx, ny = (y + 0.5f - cy) / ry;
            float d2 = nx * nx + ny * ny;
            if (d2 > 1) continue;
            float light = -(nx * 0.6f + ny * 0.8f);
            put(x, y, sh(ramp, base + light * 3.5f + (1 - d2) * 1.5f));
        }
}

static void rectf(int x0, int y0, int x1, int y1, int ramp, float base)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float s = base;
            if (x == x0 || y == y0) s += 2;
            if (x == x1 || y == y1) s -= 2;
            put(x, y, sh(ramp, s));
        }
}

static void splat(float cx, float cy, float r, int n, int seed, int ramp, float base)
{
    for (int i = 0; i < n; i++) {
        float a = hashf(i, 0, seed) * 6.2832f, d = hashf(i, 1, seed) * r;
        float s = 0.6f + hashf(i, 2, seed) * 1.4f;
        ellipse(cx + cosf(a) * d, cy + sinf(a) * d, s, s, ramp, base);
    }
}

/* dark 1-pixel outline around the shape for readability */
static void outline(void)
{
    uint8_t *copy = malloc((size_t)(cur->w * cur->h));
    memcpy(copy, cur->pix, (size_t)(cur->w * cur->h));
    for (int x = 0; x < cur->w; x++)
        for (int y = 0; y < cur->h; y++) {
            if (copy[x * cur->h + y] != TRANSPARENT) continue;
            bool near = (x > 0 && copy[(x - 1) * cur->h + y] != TRANSPARENT) ||
                        (x < cur->w - 1 && copy[(x + 1) * cur->h + y] != TRANSPARENT) ||
                        (y > 0 && copy[x * cur->h + y - 1] != TRANSPARENT) ||
                        (y < cur->h - 1 && copy[x * cur->h + y + 1] != TRANSPARENT);
            if (near) cur->pix[x * cur->h + y] = RAMP(R_GRAY, 0);
        }
    free(copy);
}

/* ---------------------------------------------------------------- the grunt */

enum { POSE_STAND, POSE_WALK1, POSE_WALK2, POSE_AIM, POSE_FIRE, POSE_PAIN };

static void soldier(int pose)
{
    float lean = pose == POSE_PAIN ? -3 : 0;
    float lf = pose == POSE_WALK1 ? -4 : pose == POSE_WALK2 ? 3 : 0;
    float rf = pose == POSE_WALK1 ? 3 : pose == POSE_WALK2 ? -4 : 0;
    /* legs and boots */
    capsule(28, 40, 27 + lf, 57, 3.2f, R_OLIVE, 5);
    capsule(36, 40, 37 + rf, 57, 3.2f, R_OLIVE, 6);
    capsule(26 + lf, 59, 29 + lf, 59, 2.6f, R_BROWN, 3);
    capsule(36 + rf, 59, 39 + rf, 59, 2.6f, R_BROWN, 3);
    /* torso, belt, webbing */
    ellipse(32 + lean * 0.5f, 31, 9, 11, R_OLIVE, 8);
    rectf(24 + (int)lean / 2, 38, 40 + (int)lean / 2, 40, R_BROWN, 5);
    capsule(26 + lean * 0.5f, 23, 37 + lean * 0.5f, 37, 1.0f, R_BROWN, 4);
    /* head and helmet */
    float hx = 32 + lean, hy = 15;
    ellipse(hx, hy + 1, 5, 6, R_SKIN, 8);
    ellipse(hx, hy - 2, 6.5f, 4.5f, R_STEEL, 7);
    capsule(hx - 6.5f, hy + 0.5f, hx + 6.5f, hy + 0.5f, 0.8f, R_STEEL, 4);
    put((int)hx - 2, (int)hy + 2, RAMP(R_GRAY, 1));
    put((int)hx + 2, (int)hy + 2, RAMP(R_GRAY, 1));
    if (pose == POSE_PAIN) {
        ellipse(hx, hy + 5, 1.5f, 1.2f, R_BLOOD, 3);
        splat(32, 30, 6, 10, 77, R_BLOOD, 9);
    } else
        capsule(hx - 1.5f, hy + 5, hx + 1.5f, hy + 5, 0.5f, R_SKIN, 4);
    /* arms and rifle */
    if (pose == POSE_AIM || pose == POSE_FIRE) {
        capsule(23, 24, 30, 31, 2.6f, R_OLIVE, 7);
        capsule(41, 24, 34, 31, 2.6f, R_OLIVE, 7);
        ellipse(32, 30, 4.5f, 4.5f, R_GRAY, 4);
        ellipse(32, 30, 2.2f, 2.2f, R_GRAY, 0);
        ellipse(29, 32, 2, 2, R_SKIN, 8);
        ellipse(35, 32, 2, 2, R_SKIN, 8);
        if (pose == POSE_FIRE) {
            for (int i = 0; i < 8; i++) {
                float a = i * 0.785f + 0.3f;
                capsule(32, 29, 32 + cosf(a) * 10, 29 + sinf(a) * 9, 1.2f, R_FIRE, 11);
            }
            ellipse(32, 29, 6, 6, R_FIRE, 12);
        }
    } else {
        float sw = pose == POSE_WALK1 ? 2 : pose == POSE_WALK2 ? -2 : 0;
        capsule(23 + lean, 24, 25 + lean + sw, 35, 2.6f, R_OLIVE, 7);
        capsule(41 + lean, 24, 38 + lean - sw, 34, 2.6f, R_OLIVE, 6);
        capsule(20 + lean, 37 + sw, 44 + lean, 29 - sw, 1.6f, R_GRAY, 4); /* rifle across the chest */
        capsule(40 + lean, 30 - sw, 46 + lean, 27 - sw, 1.0f, R_GRAY, 2);
        ellipse(25 + lean + sw, 35, 2, 2, R_SKIN, 8);
        ellipse(38 + lean - sw, 33, 2, 2, R_SKIN, 8);
    }
    outline();
}

static void soldier_dying(int frame)
{
    if (frame == 0) {
        soldier(POSE_PAIN);
        splat(30, 24, 9, 14, 91, R_BLOOD, 8);
        return;
    }
    if (frame == 1) { /* buckling */
        capsule(28, 46, 25, 58, 3.2f, R_OLIVE, 5);
        capsule(36, 46, 40, 58, 3.2f, R_OLIVE, 6);
        ellipse(30, 40, 9, 9, R_OLIVE, 7);
        ellipse(24, 30, 5, 6, R_SKIN, 7);
        ellipse(23, 27, 6.5f, 4.5f, R_STEEL, 6);
        capsule(22, 36, 14, 44, 2.6f, R_OLIVE, 6);
        capsule(38, 36, 46, 42, 2.6f, R_OLIVE, 6);
        splat(30, 38, 8, 16, 92, R_BLOOD, 8);
    } else if (frame == 2) { /* on knees */
        capsule(22, 56, 40, 58, 3.2f, R_OLIVE, 5);
        ellipse(32, 50, 10, 7, R_OLIVE, 6);
        ellipse(20, 47, 5, 5, R_SKIN, 6);
        ellipse(19, 45, 6, 4, R_STEEL, 5);
        splat(32, 52, 9, 16, 93, R_BLOOD, 7);
    } else { /* lying */
        ellipse(32, 60, 22, 3, R_BLOOD, 4);
        capsule(14, 58, 44, 58, 4, R_OLIVE, 5);
        capsule(40, 59, 54, 60, 2.8f, R_OLIVE, 4);
        ellipse(12, 57, 4.5f, 4, R_SKIN, 6);
        ellipse(10, 56, 5, 3.5f, R_STEEL, 5);
        capsule(48, 61, 53, 61, 2, R_BROWN, 3);
        capsule(24, 62, 38, 55, 1.4f, R_GRAY, 3);
        splat(30, 58, 6, 12, 94, R_BLOOD, 6);
    }
    outline();
}

/* ---------------------------------------------------------------- items, decor, effects */

static void make_items(void)
{
    begin(SPR_STIM, 64, 64);
    rectf(26, 52, 38, 61, R_GRAY, 12);
    rectf(31, 54, 33, 59, R_RED, 10);
    rectf(29, 56, 35, 57, R_RED, 10);
    outline();

    begin(SPR_MEDKIT, 64, 64);
    rectf(20, 46, 44, 61, R_GRAY, 12);
    rectf(30, 48, 34, 59, R_RED, 10);
    rectf(26, 52, 38, 55, R_RED, 10);
    rectf(28, 43, 36, 45, R_GRAY, 7);
    outline();

    begin(SPR_ARMOR, 64, 64);
    for (int y = 38; y < 62; y++)
        for (int x = 19; x <= 45; x++) {
            float dx = fabsf(x - 32.0f);
            if (y < 46 && dx < 5 - (y - 38) * 0.4f) continue;                 /* V neck */
            if (y < 48 && dx > 10 + (y - 38) * 0.3f) continue;               /* arm holes */
            if (y > 57 && dx > 13 - (y - 57) * 1.5f) continue;                 /* rounded hem */
            float sd = 8 - (x - 32) / 13.0f * 2.5f + (y % 6 == 0 ? -2 : 0);
            put(x, y, sh(R_GREEN, sd));
        }
    rectf(27, 48, 37, 55, R_GREEN, 11); /* chest plate */
    outline();

    begin(SPR_CLIP, 64, 64);
    rectf(29, 50, 35, 61, R_GRAY, 5);
    rectf(30, 47, 34, 49, R_YELLOW, 10);
    outline();

    begin(SPR_SHELLS, 64, 64);
    for (int i = 0; i < 4; i++) {
        capsule(25 + i * 5, 51, 25 + i * 5, 58, 2, R_RED, 8);
        rectf(23 + i * 5, 58, 27 + i * 5, 61, R_YELLOW, 9);
    }
    outline();

    static const int keyramp[3] = {R_RED, R_BLUE, R_YELLOW};
    for (int k = 0; k < 3; k++) {
        begin(SPR_KEYR + k, 64, 64);
        rectf(26, 46, 38, 61, keyramp[k], 10);
        rectf(28, 49, 36, 52, R_GRAY, 14);
        rectf(29, 55, 31, 58, R_YELLOW, 12);
        outline();
    }

    begin(SPR_SHOTGUN, 64, 64);
    capsule(10, 56, 44, 56, 1.6f, R_STEEL, 7);
    capsule(10, 59, 40, 59, 1.4f, R_STEEL, 5);
    capsule(28, 58, 36, 58, 2.2f, R_BROWN, 7);
    capsule(42, 58, 56, 61, 3, R_BROWN, 6);
    outline();

    begin(SPR_LAMP, 64, 64);
    capsule(32, 20, 32, 60, 1.5f, R_STEEL, 6);
    capsule(26, 61, 38, 61, 1.5f, R_STEEL, 4);
    ellipse(32, 14, 7, 7, R_FIRE, 11);
    ellipse(32, 14, 4, 4, R_FIRE, 14);
    outline();

    begin(SPR_PILLAR, 64, 64);
    for (int y = 4; y < 64; y++)
        for (int x = 22; x < 42; x++) {
            float nx = (x + 0.5f - 32) / 10;
            float s = 7 + (-nx * 4) + (1 - nx * nx) * 2 + (hashf(x, y, 61) - 0.5f) * 1.5f;
            if (y < 9 || y > 57) s += (y < 9 ? 2 : -1);
            put(x, y, sh(R_GRAY, s));
        }
    rectf(19, 4, 44, 8, R_GRAY, 8);
    rectf(19, 58, 44, 63, R_GRAY, 6);
    outline();

    begin(SPR_BARREL, 64, 64);
    for (int y = 36; y < 63; y++)
        for (int x = 22; x < 42; x++) {
            float nx = (x + 0.5f - 32) / 10;
            float s = 6 + (-nx * 4) + (1 - nx * nx) * 2;
            if (y == 40 || y == 58) s -= 3;
            put(x, y, sh(R_GREEN, s));
        }
    ellipse(32, 37, 10, 2.5f, R_GREEN, 12);
    outline();

    for (int i = 0; i < 3; i++) {
        begin(SPR_PUFF1 + i, 64, 64);
        float r = 2.5f + i * 1.8f;
        ellipse(32, 31 - i * 2, r, r, R_GRAY, 10 - i * 3);
        if (i == 0) ellipse(32, 31, 1.6f, 1.6f, R_FIRE, 13);
    }
    for (int i = 0; i < 3; i++) {
        begin(SPR_BLOOD1 + i, 64, 64);
        splat(32, 30 + i * 3, 2 + i * 2.5f, 6 + i * 3, 100 + i, R_BLOOD, 9 - i * 2);
    }
}

/* ---------------------------------------------------------------- first-person weapons (128x96, bottom-centered) */

static void flash(float x, float y, float r, int rays)
{
    for (int i = 0; i < rays; i++) {
        float a = i * 6.2832f / rays + 0.2f, l = r * (i & 1 ? 1.5f : 2.0f);
        capsule(x, y, x + cosf(a) * l, y + sinf(a) * l * 0.8f, r * 0.18f, R_FIRE, 11);
    }
    ellipse(x, y, r, r * 0.85f, R_FIRE, 13);
    ellipse(x, y, r * 0.5f, r * 0.45f, R_FIRE, 15);
}

/* pistol seen from behind: slide, sights, grip, right hand */
static void pistol(int frame)
{
    int k = frame == 2 ? -5 : 0, top = 50 + k;
    if (frame == 1) flash(64, 36, 9, 10);
    if (frame == 2) ellipse(64, 38 + k, 4, 3, R_GRAY, 8);
    for (int y = top + 20; y < 96; y++) /* grip */
        for (int x = 56; x <= 72; x++) put(x, y, sh(R_GRAY, 3 - (x - 64) / 8.0f * 1.5f + ((x + y) % 4 == 0 ? -1 : 0)));
    for (int y = top; y < top + 4; y++) /* slide top face */
        for (int x = 52; x <= 76; x++) put(x, y, sh(R_STEEL, 11 - (y - top) * 0.7f));
    for (int y = top + 4; y < top + 22; y++) /* slide rear face with serrations */
        for (int x = 52; x <= 76; x++) {
            float sd = 6.5f - (x - 64) / 12.0f * 2.5f;
            if (y > top + 9 && (x - 52) % 3 == 0) sd -= 2.5f;
            if (x == 52) sd += 2;
            put(x, y, sh(R_STEEL, sd));
        }
    rectf(53, top - 4, 58, top - 1, R_GRAY, 3); /* rear sight */
    rectf(70, top - 4, 75, top - 1, R_GRAY, 3);
    rectf(63, top - 3, 65, top - 1, R_STEEL, 9); /* front sight in the notch */
    ellipse(66, 88 - k * 0.3f, 17, 12, R_SKIN, 8); /* back of the hand */
    capsule(50, 84, 56, top + 26, 3.6f, R_SKIN, 9);  /* thumb */
    for (int i = 0; i < 3; i++) capsule(74, 80 + i * 5, 82, 83 + i * 5, 2.4f, R_SKIN, 6 - i * 0.5f);
    capsule(80, 102, 100, 120, 15, R_OLIVE, 5); /* sleeve */
    outline();
}

/* pump shotgun from behind: barrel over magazine tube, sliding forend, two hands */
static void shotgun(int frame)
{
    float pump = frame == 2 ? 9 : frame == 3 ? 16 : 0, k = frame == 1 ? -5 : 0;
    if (frame == 1) flash(64, 18, 13, 12);
    for (int y = (int)(26 + k); y < 96; y++) {
        float t = (y - 26 - k) / 70.0f;
        float bh = 3.5f + t * 6, mh = 3 + t * 5.5f, mx = 64 + 1 + t * 3; /* barrel and tube half-widths */
        for (int x = (int)(mx - mh); x <= (int)(mx + mh); x++) { /* magazine tube, slightly right */
            float nx = (x - mx) / mh;
            put(x, y + 3, sh(R_STEEL, 3.5f - nx * 2 + (1 - nx * nx)));
        }
        for (int x = (int)(64 - bh); x <= (int)(64 + bh); x++) {
            float nx = (x - 64) / bh;
            float sd = 5 - nx * 2.5f + (1 - nx * nx) * 1.5f;
            if (nx > -0.55f && nx < -0.3f) sd += 4; /* specular stripe */
            put(x, y, sh(R_STEEL, sd));
        }
    }
    ellipse(64, 27 + k, 3, 2, R_GRAY, 0); /* muzzle */
    for (int y = 80; y < 96; y++) /* receiver */
        for (int x = 46; x <= 82; x++) put(x, y + (int)k, sh(R_GRAY, 4 - (x - 64) / 18.0f * 2 + (y == 80 ? 3 : 0)));
    int py0 = (int)(52 + pump + k), py1 = (int)(70 + pump + k);
    for (int y = py0; y < py1; y++) { /* wooden forend */
        float t = (y - 26 - k) / 70.0f, hw = 9 + t * 9;
        for (int x = (int)(65 - hw); x <= (int)(65 + hw); x++) {
            float nx = (x - 65) / hw;
            put(x, y, sh(R_BROWN, 7 - nx * 2.5f + (1 - nx * nx) + ((y - py0) % 4 == 0 ? -2.5f : 0)));
        }
    }
    float hy = py0 + 10;
    capsule(20, 110, 44, hy + 6, 13, R_OLIVE, 5); /* left sleeve and hand under the forend */
    ellipse(48, hy + 2, 10, 8, R_SKIN, 8);
    for (int i = 0; i < 3; i++) capsule(54, hy - 4 + i * 4, 62, hy - 6 + i * 4, 2.2f, R_SKIN, 8 - i);
    capsule(108, 110, 90, 92, 14, R_OLIVE, 5); /* right sleeve and hand on the grip */
    ellipse(82, 90 + k, 11, 8, R_SKIN, 8);
    outline();
}

static void make_weapons(void)
{
    for (int f = 0; f < 3; f++) {
        begin(SPR_PISTOL + f, 128, 96);
        pistol(f);
    }
    static const int sg[4] = {SPR_SGUN, SPR_SGUN_F, SPR_SGUN_R1, SPR_SGUN_R2};
    for (int f = 0; f < 4; f++) {
        begin(sg[f], 128, 96);
        shotgun(f);
    }
}

void gfx_init(void)
{
    make_palette();
    make_walls();
    make_flats();
    static const int poses[6] = {POSE_STAND, POSE_WALK1, POSE_WALK2, POSE_AIM, POSE_FIRE, POSE_PAIN};
    for (int i = 0; i < 6; i++) {
        begin(SPR_GRUNT_S + i, 64, 64);
        soldier(poses[i]);
    }
    for (int i = 0; i < 4; i++) {
        begin(SPR_GRUNT_D1 + i, 64, 64);
        soldier_dying(i);
    }
    make_items();
    make_weapons();
}
