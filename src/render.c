/* Wolfenstein-style raycaster with textured floors/ceilings, doors, pushwalls and billboard sprites.
 * Float is fine here: the renderer only reads game state. */
#include "render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "game.h"
#include "gfx.h"
#include "platform.h"

#define HALF_FOV_TAN 0.7673f /* ~75 degree horizontal FOV */
static const float proj = (SCREENW / 2) / HALF_FOV_TAN;
static float zbuf[SCREENW];

static float fx(fixed_t v) { return v / 65536.0f; }

static int light_for(float dist)
{
    int l = g.ambient + (int)(dist * 1.6f) - (g.player.flash ? 6 : 0);
    return l < 0 ? 0 : l >= NLIGHTS ? NLIGHTS - 1 : l;
}

static void draw_wall_column(uint8_t *fb, int x, float dist, const uint8_t *texcol, int light)
{
    float h = proj / dist;
    float top = VIEWH / 2.0f - h / 2;
    int y0 = (int)ceilf(top), y1 = (int)ceilf(top + h);
    if (y0 < 0) y0 = 0;
    if (y1 > VIEWH) y1 = VIEWH;
    float step = 64 / h, v = (y0 - top) * step;
    const uint8_t *cm = colormap[light];
    for (int y = y0; y < y1; y++, v += step) fb[y * SCREENW + x] = cm[texcol[(int)v & 63]];
}

static void draw_floors(uint8_t *fb, float px, float py, float dx, float dy, float plx, float ply)
{
    const uint8_t *ft = flattex[g.floortex], *ct = flattex[g.ceiltex];
    for (int y = VIEWH / 2; y < VIEWH; y++) {
        float rowdist = 0.5f * proj / (y + 0.5f - VIEWH / 2.0f);
        float sx = (2 * plx) * rowdist / SCREENW, sy = (2 * ply) * rowdist / SCREENW;
        float wx = px + (dx - plx) * rowdist + sx * 0.5f, wy = py + (dy - ply) * rowdist + sy * 0.5f;
        const uint8_t *cm = colormap[light_for(rowdist)];
        uint8_t *frow = fb + y * SCREENW, *crow = fb + (VIEWH - 1 - y) * SCREENW;
        for (int x = 0; x < SCREENW; x++, wx += sx, wy += sy) {
            int tx = (int)(wx * 64) & 63, ty = (int)(wy * 64) & 63;
            frow[x] = cm[ft[ty * 64 + tx]];
            crow[x] = cm[ct[ty * 64 + tx]];
        }
    }
}

static void cast_walls(uint8_t *fb, float px, float py, float dx, float dy, float plx, float ply)
{
    for (int x = 0; x < SCREENW; x++) {
        float cam = 2 * (x + 0.5f) / SCREENW - 1;
        float rdx = dx + plx * cam, rdy = dy + ply * cam;
        if (fabsf(rdx) < 1e-6f) rdx = 1e-6f;
        if (fabsf(rdy) < 1e-6f) rdy = 1e-6f;
        int mx = (int)px, my = (int)py;
        float ddx = fabsf(1 / rdx), ddy = fabsf(1 / rdy);
        int stx = rdx < 0 ? -1 : 1, sty = rdy < 0 ? -1 : 1;
        float sdx = rdx < 0 ? (px - mx) * ddx : (mx + 1 - px) * ddx;
        float sdy = rdy < 0 ? (py - my) * ddy : (my + 1 - py) * ddy;
        float dist = 64;
        int tex = WT_STONE, side = 0;
        float u = 0;
        for (int i = 0; i < 256; i++) {
            if (sdx < sdy) {
                sdx += ddx;
                mx += stx;
                side = 0;
            } else {
                sdy += ddy;
                my += sty;
                side = 1;
            }
            if (mx < 0 || my < 0 || mx >= MAPW || my >= MAPH) break;
            int tl = g.tile[my][mx];
            if (tl == TL_EMPTY) continue;
            if (tl == TL_DOOR) {
                const door_t *d = &g.doors[g.doorat[my][mx]];
                float t, along;
                if (d->vertical) {
                    t = (mx + 0.5f - px) / rdx;
                    along = py + t * rdy - my;
                } else {
                    t = (my + 0.5f - py) / rdy;
                    along = px + t * rdx - mx;
                }
                if (t <= 0 || along < 0 || along >= 1 || along < fx(d->open)) continue;
                dist = t;
                u = along - fx(d->open);
                side = !d->vertical;
                static const int doortex[4] = {WT_DOOR, WT_DOOR_RED, WT_DOOR_BLUE, WT_DOOR_YELLOW};
                tex = doortex[d->lock];
                break;
            }
            if (tl == TL_PUSHMOVING) {
                const pushwall_t *pw = &g.push;
                float bx = pw->tx + pw->dx * fx(pw->pos), by = pw->ty + pw->dy * fx(pw->pos);
                float tx1 = (bx - px) / rdx, tx2 = (bx + 1 - px) / rdx;
                float ty1 = (by - py) / rdy, ty2 = (by + 1 - py) / rdy;
                float tminx = fminf(tx1, tx2), tminy = fminf(ty1, ty2);
                float tnear = fmaxf(tminx, tminy), tfar = fminf(fmaxf(tx1, tx2), fmaxf(ty1, ty2));
                if (tnear >= tfar || tnear <= 0) continue;
                dist = tnear;
                side = tminx > tminy ? 0 : 1;
                u = side == 0 ? py + tnear * rdy - by : px + tnear * rdx - bx;
                tex = pw->tex;
                break;
            }
            dist = side == 0 ? sdx - ddx : sdy - ddy;
            u = side == 0 ? py + dist * rdy : px + dist * rdx;
            u -= floorf(u);
            if ((side == 0 && rdx < 0) || (side == 1 && rdy > 0)) u = 1 - u;
            tex = g.tex[my][mx];
            int prevx = side == 0 ? mx - stx : mx, prevy = side == 1 ? my - sty : my;
            if (g.tile[prevy][prevx] == TL_DOOR) tex = WT_DOORSIDE;
            break;
        }
        if (dist < 0.01f) dist = 0.01f;
        zbuf[x] = dist;
        int tu = (int)(u * 64) & 63;
        int light = light_for(dist) + (side ? 2 : 0); /* darker y-sides give corners definition */
        draw_wall_column(fb, x, dist, &walltex[tex][tu * 64], light > NLIGHTS - 1 ? NLIGHTS - 1 : light);
    }
}

typedef struct {
    float depth, sx;
    int sprite, bright;
} vis_t;

static int vis_cmp(const void *a, const void *b)
{
    float da = ((const vis_t *)a)->depth, db = ((const vis_t *)b)->depth;
    return da < db ? 1 : da > db ? -1 : 0;
}

static void draw_sprite(uint8_t *fb, const vis_t *v)
{
    const image_t *im = &sprites[v->sprite];
    float scale = proj / (v->depth * 64);
    float w = im->w * scale, h = im->h * scale;
    float left = v->sx - w / 2, bottom = VIEWH / 2.0f + 0.5f * proj / v->depth, top = bottom - h;
    int x0 = (int)ceilf(left), x1 = (int)ceilf(left + w);
    int y0 = (int)ceilf(top), y1 = (int)ceilf(bottom);
    if (x0 < 0) x0 = 0;
    if (x1 > SCREENW) x1 = SCREENW;
    if (y0 < 0) y0 = 0;
    if (y1 > VIEWH) y1 = VIEWH;
    const uint8_t *cm = colormap[v->bright ? 0 : light_for(v->depth)];
    for (int x = x0; x < x1; x++) {
        if (v->depth >= zbuf[x]) continue;
        int tx = (int)((x - left) / scale);
        if (tx < 0 || tx >= im->w) continue;
        const uint8_t *col = &im->pix[tx * im->h];
        for (int y = y0; y < y1; y++) {
            int ty = (int)((y - top) / scale);
            if (ty < 0 || ty >= im->h) continue;
            uint8_t c = col[ty];
            if (c != TRANSPARENT) fb[y * SCREENW + x] = cm[c];
        }
    }
}

static void draw_sprites(uint8_t *fb, float px, float py, float dx, float dy, float plx, float ply)
{
    static vis_t vis[MAXACTORS];
    int n = 0;
    float inv = 1.0f / (plx * dy - dx * ply);
    for (int i = 0; i < MAXACTORS; i++) {
        const actor_t *a = &g.actors[i];
        if (!a->inuse) continue;
        int spr = g_state_sprite(a->state);
        if (spr < 0) continue;
        float rx = fx(a->x) - px, ry = fx(a->y) - py;
        float tx = inv * (dy * rx - dx * ry), ty = inv * (-ply * rx + plx * ry);
        if (ty < 0.1f) continue;
        vis[n++] = (vis_t){ty, SCREENW / 2.0f * (1 + tx / ty), spr, g_state_bright(a->state)};
    }
    qsort(vis, (size_t)n, sizeof vis[0], vis_cmp);
    for (int i = 0; i < n; i++) draw_sprite(fb, &vis[i]);
}

static void draw_psprite(uint8_t *fb)
{
    const player_t *p = &g.player;
    int spr = g_state_sprite(p->wstate);
    if (spr < 0) return;
    const image_t *im = &sprites[spr];
    float bob = fx(p->bob) * 60;
    if (bob > 8) bob = 8;
    float t = g.leveltime * 0.18f;
    int ox = SCREENW / 2 - im->w / 2 + (int)(sinf(t) * bob);
    int oy = VIEWH - im->h + (int)(fabsf(cosf(t)) * bob * 0.6f) + 4 + p->wy / FRACUNIT;
    const uint8_t *cm = colormap[g_state_bright(p->wstate) ? 0 : (g.ambient / 2)];
    for (int x = 0; x < im->w; x++) {
        int sx = ox + x;
        if (sx < 0 || sx >= SCREENW) continue;
        for (int y = 0; y < im->h; y++) {
            int sy = oy + y;
            uint8_t c = im->pix[x * im->h + y];
            if (sy >= 0 && sy < VIEWH && c != TRANSPARENT) fb[sy * SCREENW + sx] = cm[c];
        }
    }
}

void r_render_view(uint8_t *fb, float extra_turn)
{
    const player_t *p = &g.player;
    float px = fx(p->x), py = fx(p->y);
    float ang = (float)p->angle * (6.2831853f / 4294967296.0f) + extra_turn;
    float dx = cosf(ang), dy = sinf(ang);
    float plx = -dy * HALF_FOV_TAN, ply = dx * HALF_FOV_TAN;
    draw_floors(fb, px, py, dx, dy, plx, ply);
    cast_walls(fb, px, py, dx, dy, plx, ply);
    draw_sprites(fb, px, py, dx, dy, plx, ply);
    if (!p->dead) {
        draw_psprite(fb);
        fb[(VIEWH / 2) * SCREENW + SCREENW / 2] = RAMP(R_GRAY, 12);
        fb[(VIEWH / 2 - 2) * SCREENW + SCREENW / 2] = RAMP(R_GRAY, 9);
        fb[(VIEWH / 2 + 2) * SCREENW + SCREENW / 2] = RAMP(R_GRAY, 9);
        fb[(VIEWH / 2) * SCREENW + SCREENW / 2 - 2] = RAMP(R_GRAY, 9);
        fb[(VIEWH / 2) * SCREENW + SCREENW / 2 + 2] = RAMP(R_GRAY, 9);
    }
    if (g.msgtics) font_draw(fb, 4, 4, g.msg, RAMP(R_YELLOW, 13), 1, 1);
}

/* ---------------------------------------------------------------- status bar */

static void hud_number(uint8_t *fb, int right, int y, int v, const char *suffix)
{
    char s[16];
    snprintf(s, sizeof s, "%d%s", v, suffix);
    font_draw(fb, right - font_width(s, 2), y, s, RAMP(R_RED, 13), 2, 1);
}

void r_draw_hud(uint8_t *fb)
{
    const player_t *p = &g.player;
    for (int y = VIEWH; y < SCREENH; y++)
        for (int x = 0; x < SCREENW; x++) {
            uint8_t c = walltex[WT_METAL][(x & 63) * 64 + ((y - VIEWH) & 63)];
            fb[y * SCREENW + x] = colormap[10][c];
        }
    for (int x = 0; x < SCREENW; x++) fb[VIEWH * SCREENW + x] = RAMP(R_STEEL, 11);
    static const int seps[] = {72, 152, 232, 276};
    for (int i = 0; i < 4; i++)
        for (int y = VIEWH + 3; y < SCREENH - 3; y++) fb[y * SCREENW + seps[i]] = RAMP(R_STEEL, 2);

    int am = g_weapon_ammo(p->readyweapon);
    if (am == AM_NONE) font_draw(fb, 42, VIEWH + 6, "--", RAMP(R_RED, 13), 2, 1);
    else hud_number(fb, 66, VIEWH + 6, p->ammo[am], "");
    font_draw(fb, 8, VIEWH + 23, "AMMO", RAMP(R_GRAY, 10), 1, 1);
    hud_number(fb, 146, VIEWH + 6, p->health, "%");
    font_draw(fb, 80, VIEWH + 23, "HEALTH", RAMP(R_GRAY, 10), 1, 1);
    hud_number(fb, 226, VIEWH + 6, p->armor, "%");
    font_draw(fb, 160, VIEWH + 23, "ARMOR", RAMP(R_GRAY, 10), 1, 1);
    for (int w = 0; w < WP_COUNT; w++) {
        char s[2] = {(char)('1' + w), 0};
        uint8_t c = !(p->weaponowned & (1 << w)) ? RAMP(R_GRAY, 4) : w == p->readyweapon ? RAMP(R_YELLOW, 14) : RAMP(R_GRAY, 12);
        font_draw(fb, 236 + w * 8, VIEWH + 9, s, c, 1, 1);
    }
    font_draw(fb, 238, VIEWH + 23, "ARMS", RAMP(R_GRAY, 10), 1, 1);
    static const int keyramp[3] = {R_RED, R_BLUE, R_YELLOW};
    for (int k = 0; k < 3; k++) {
        int x0 = 286 + k * 10, y0 = VIEWH + 8;
        for (int y = y0; y < y0 + 14; y++)
            for (int x = x0; x < x0 + 7; x++)
                fb[y * SCREENW + x] = (p->keys & (1 << k)) ? RAMP(keyramp[k], (x == x0 || y == y0) ? 14 : 10) : RAMP(R_GRAY, 2);
    }
}

void r_frame_palette(uint8_t out[256 * 3])
{
    const player_t *p = &g.player;
    int red = p->damagecount * 7, gold = p->bonuscount * 14;
    if (p->dead) red = 140;
    if (red > 150) red = 150;
    if (red) gfx_tint_palette(out, 255, 0, 0, red);
    else if (gold) gfx_tint_palette(out, 255, 220, 80, gold);
    else memcpy(out, palette, 256 * 3);
}

void r_dim(uint8_t *fb, int level)
{
    for (int i = 0; i < SCREENW * SCREENH; i++) fb[i] = colormap[level][fb[i]];
}
