/* Fixed-timestep main loop, game flow (title, menus, levels, intermission) and input mapping. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"
#include "font.h"
#include "game.h"
#include "gfx.h"
#include "platform.h"
#include "render.h"
#include "sound.h"

static const char *levels[] = {"assets/levels/e1m1.txt", "assets/levels/e1m2.txt", "assets/levels/e1m3.txt",
                               "assets/levels/e1m4.txt", "assets/levels/e1m5.txt", "assets/levels/e1m6.txt",
                               "assets/levels/e1m7.txt", "assets/levels/e1m8.txt"};
#define NUMLEVELS (int)(sizeof levels / sizeof levels[0])

static uint8_t fb[SCREENW * SCREENH];
static plat_input in;
static int running = 1;

enum { GM_TITLE, GM_LEVEL, GM_INTER, GM_FINALE };
static int mode = GM_TITLE;
static player_t carry;   /* inventory at the start of the current level, for retries */
static int mouse_acc;     /* mouse x motion not yet consumed by a tic */
static int fade;          /* intermission/menus: tics in this screen */
static int automap;

/* demo recording / playback */
#define MAXDEMO (TICRATE * 60 * 60)
static ticcmd_t *democmds;
static int demolen, demopos, demoskill, demolevel;
static const char *record_path;
static int playing_demo;

/* ---------------------------------------------------------------- options and bindings */

enum { A_FWD, A_BACK, A_LEFT, A_RIGHT, A_TURNL, A_TURNR, A_RUN, A_FIRE, A_USE, A_COUNT };
static const char *action_names[A_COUNT] = {"FORWARD", "BACKWARD", "STRAFE LEFT", "STRAFE RIGHT",
                                            "TURN LEFT", "TURN RIGHT", "RUN", "FIRE", "USE"};
static int binds[A_COUNT][2] = {
    {PK_A + ('W' - 'A'), PK_UP}, {PK_A + ('S' - 'A'), PK_DOWN}, {PK_A, 0}, {PK_A + ('D' - 'A'), 0},
    {PK_LEFT, 0}, {PK_RIGHT, 0}, {PK_LSHIFT, PK_RSHIFT}, {PK_MOUSE1, PK_LCTRL}, {PK_SPACE, PK_A + ('E' - 'A')},
};
static int sens = 5, sfxvol = 12, musvol = 9;

static void save_config(void)
{
    char buf[512];
    int n = snprintf(buf, sizeof buf, "sens %d\nsfx %d\nmusic %d\n", sens, sfxvol, musvol);
    for (int a = 0; a < A_COUNT; a++)
        n += snprintf(buf + n, sizeof buf - (size_t)n, "bind %d %d %d\n", a, binds[a][0], binds[a][1]);
    plat_write_file("config.txt", buf, n);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void load_config(void)
{
    char *text;
    if (plat_read_file("config.txt", (void **)&text) < 0) return;
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        int a, k0, k1;
        if (sscanf(line, "bind %d %d %d", &a, &k0, &k1) == 3 && a >= 0 && a < A_COUNT) {
            binds[a][0] = clampi(k0, 0, PK_COUNT - 1);
            binds[a][1] = clampi(k1, 0, PK_COUNT - 1);
        } else {
            sscanf(line, "sens %d", &sens);
            sscanf(line, "sfx %d", &sfxvol);
            sscanf(line, "music %d", &musvol);
        }
    }
    sens = clampi(sens, 1, 20);
    sfxvol = clampi(sfxvol, 0, 15);
    musvol = clampi(musvol, 0, 15);
    free(text);
}

/* presses since the last tic, so a tap shorter than a tic still counts */
static uint8_t tapped[A_COUNT], weapon_tap;

static int held(int a) { return tapped[a] || in.down[binds[a][0]] || (binds[a][1] && in.down[binds[a][1]]); }

static void latch_taps(void)
{
    for (int a = 0; a < A_COUNT; a++)
        if (in.pressed[binds[a][0]] || (binds[a][1] && in.pressed[binds[a][1]])) tapped[a] = 1;
    for (int w = 0; w < WP_COUNT; w++)
        if (in.pressed[PK_1 + w]) weapon_tap = (uint8_t)(w + 1);
}

static const char *key_name(int k)
{
    static char buf[8];
    if (k >= PK_A && k <= PK_Z) {
        buf[0] = (char)('A' + k - PK_A);
        buf[1] = 0;
        return buf;
    }
    if (k >= PK_1 && k <= PK_0) {
        buf[0] = k == PK_0 ? '0' : (char)('1' + k - PK_1);
        buf[1] = 0;
        return buf;
    }
    switch (k) {
    case 0: return "---";
    case PK_UP: return "UP";
    case PK_DOWN: return "DOWN";
    case PK_LEFT: return "LEFT";
    case PK_RIGHT: return "RIGHT";
    case PK_SPACE: return "SPACE";
    case PK_RETURN: return "ENTER";
    case PK_TAB: return "TAB";
    case PK_LSHIFT: return "LSHIFT";
    case PK_RSHIFT: return "RSHIFT";
    case PK_LCTRL: return "LCTRL";
    case PK_RCTRL: return "RCTRL";
    case PK_LALT: return "LALT";
    case PK_RALT: return "RALT";
    case PK_MOUSE1: return "MOUSE1";
    case PK_MOUSE2: return "MOUSE2";
    case PK_MOUSE3: return "MOUSE3";
    case PK_COMMA: return ",";
    case PK_PERIOD: return ".";
    case PK_SLASH: return "/";
    case PK_SEMICOLON: return ";";
    default: snprintf(buf, sizeof buf, "#%d", k); return buf;
    }
}

/* ---------------------------------------------------------------- level flow */

static char *music_text;

static void play_music(const char *path)
{
    free(music_text);
    music_text = NULL;
    if (plat_read_file(path, (void **)&music_text) < 0) music_text = NULL;
    snd_music(music_text);
}

static void play_level_music(void)
{
    char path[64];
    snprintf(path, sizeof path, "assets/music/%s", g.music[0] ? g.music : "e1.txt");
    play_music(path);
}

static bool start_level(int num, int skill, const player_t *inv)
{
    char *text;
    if (plat_read_file(levels[num], (void **)&text) < 0) {
        fprintf(stderr, "cannot read %s\n", levels[num]);
        return false;
    }
    bool ok = g_load_level(text, skill, num, inv);
    free(text);
    if (!ok) {
        fprintf(stderr, "%s: %s\n", levels[num], g.msg);
        return false;
    }
    carry = g.player;
    mode = GM_LEVEL;
    mouse_acc = 0;
    play_level_music();
    return true;
}

/* ---------------------------------------------------------------- menus */

enum { M_NONE, M_MAIN, M_SKILL, M_OPTIONS, M_CONTROLS, M_QUIT, M_SAVE, M_LOAD };
enum { MI_RESUME, MI_NEW, MI_SAVE, MI_LOAD, MI_OPTIONS, MI_QUIT };
static const char *main_names[] = {"RESUME", "NEW GAME", "SAVE GAME", "LOAD GAME", "OPTIONS", "QUIT GAME"};

/* main menu entries depend on whether a game is running */
static int main_items(int *ids)
{
    int n = 0;
    if (mode == GM_LEVEL) ids[n++] = MI_RESUME;
    ids[n++] = MI_NEW;
    if (mode == GM_LEVEL && !g.player.dead) ids[n++] = MI_SAVE;
    ids[n++] = MI_LOAD;
    ids[n++] = MI_OPTIONS;
    ids[n++] = MI_QUIT;
    return n;
}

/* ---------------------------------------------------------------- save games: a header plus the raw game state */

#define NSLOTS 6
#define SAVE_MAGIC "IRONHOLD-SAVE-1"
typedef struct {
    char magic[16];
    char name[40];
    int32_t size; /* sizeof(game_t): rejects saves from a different build layout */
    player_t carry;
} savehdr_t;

static void slot_file(int slot, char *buf, size_t n) { snprintf(buf, n, "save%d.sav", slot); }

static const char *slot_file_path(int slot)
{
    static char path[32];
    slot_file(slot, path, sizeof path);
    return path;
}

static void slot_name(int slot, char *out, size_t n)
{
    char path[32];
    void *data;
    slot_file(slot, path, sizeof path);
    long len = plat_read_file(path, &data);
    snprintf(out, n, "- EMPTY -");
    if (len < 0) return;
    const savehdr_t *h = data;
    if (len == (long)(sizeof *h + sizeof g) && !memcmp(h->magic, SAVE_MAGIC, sizeof SAVE_MAGIC) && h->size == (int32_t)sizeof g)
        snprintf(out, n, "%.39s", h->name);
    else
        snprintf(out, n, "- INCOMPATIBLE -");
    free(data);
}

static bool load_game(int slot)
{
    void *data;
    long len = plat_read_file(slot_file_path(slot), &data);
    if (len < 0) return false;
    const savehdr_t *h = data;
    bool ok = len == (long)(sizeof *h + sizeof g) && !memcmp(h->magic, SAVE_MAGIC, sizeof SAVE_MAGIC) &&
              h->size == (int32_t)sizeof g;
    if (ok) {
        static game_t tmp; /* validate before touching the live game */
        memcpy(&tmp, (const uint8_t *)data + sizeof *h, sizeof tmp);
        game_t live = g;
        g = tmp;
        ok = g_validate();
        if (ok) {
            carry = h->carry;
            mode = GM_LEVEL;
            mouse_acc = 0;
            play_level_music();
        } else
            g = live;
    }
    free(data);
    return ok;
}

static bool save_game(int slot)
{
    static uint8_t buf[sizeof(savehdr_t) + sizeof(game_t)];
    savehdr_t h = {0};
    memcpy(h.magic, SAVE_MAGIC, sizeof SAVE_MAGIC);
    int secs = g.leveltime / TICRATE;
    snprintf(h.name, sizeof h.name, "E1M%d %.16s %d:%02d", (g.levelnum + 1) % 10, g.levelname, secs / 60 % 1000, secs % 60);
    h.size = (int32_t)sizeof g;
    h.carry = carry;
    memcpy(buf, &h, sizeof h);
    memcpy(buf + sizeof h, &g, sizeof g);
    return plat_write_file(slot_file_path(slot), buf, sizeof buf);
}
static int menu = M_NONE, sel, waiting_bind = -1;
static int menu_stack[8], menu_depth;

static void open_menu(int m)
{
    if (menu != M_NONE && menu_depth < 8) menu_stack[menu_depth++] = menu;
    menu = m;
    sel = 0;
}

static void close_menu(void)
{
    menu = menu_depth ? menu_stack[--menu_depth] : M_NONE;
    sel = 0;
}

static int menu_count(void)
{
    switch (menu) {
    case M_MAIN: {
        int ids[8];
        return main_items(ids);
    }
    case M_SAVE: case M_LOAD: return NSLOTS;
    case M_SKILL: return SK_COUNT;
    case M_OPTIONS: return 5;
    case M_CONTROLS: return A_COUNT + 1;
    default: return 0;
    }
}

static void menu_activate(int dir)
{
    switch (menu) {
    case M_MAIN: {
        int ids[8];
        main_items(ids);
        if (dir) return;
        switch (ids[sel]) {
        case MI_RESUME: close_menu(); break;
        case MI_NEW: open_menu(M_SKILL); break;
        case MI_SAVE: open_menu(M_SAVE); break;
        case MI_LOAD: open_menu(M_LOAD); break;
        case MI_OPTIONS: open_menu(M_OPTIONS); break;
        default: open_menu(M_QUIT); break;
        }
        break;
    }
    case M_SAVE:
        if (dir) return;
        if (save_game(sel)) {
            snprintf(g.msg, sizeof g.msg, "GAME SAVED");
            g.msgtics = TICRATE * 2;
        }
        menu = M_NONE;
        menu_depth = 0;
        break;
    case M_LOAD:
        if (dir) return;
        if (load_game(sel)) {
            menu = M_NONE;
            menu_depth = 0;
        } else
            snd_play(SFX_NOWAY, 200, 128);
        break;
    case M_SKILL:
        if (dir) return;
        menu = M_NONE;
        menu_depth = 0;
        start_level(0, sel, NULL);
        demolen = 0; /* a new game starts a new recording */
        demoskill = sel;
        demolevel = 0;
        break;
    case M_OPTIONS:
        if (sel == 0) sens = clampi(sens + (dir ? dir : 1), 1, 20);
        else if (sel == 1) sfxvol = clampi(sfxvol + (dir ? dir : 1), 0, 15);
        else if (sel == 2) musvol = clampi(musvol + (dir ? dir : 1), 0, 15);
        else if (sel == 3 && !dir) open_menu(M_CONTROLS);
        else if (sel == 4 && !dir) close_menu();
        snd_set_volume(sfxvol, musvol);
        save_config();
        if (dir || sel < 3) snd_play(SFX_ITEM, 160, 128);
        break;
    case M_CONTROLS:
        if (dir) return;
        if (sel == A_COUNT) close_menu();
        else waiting_bind = sel;
        break;
    default: break;
    }
}

static void menu_input(void)
{
    if (waiting_bind >= 0) {
        for (int k = 1; k < PK_COUNT; k++)
            if (in.pressed[k]) {
                if (k != PK_ESCAPE) {
                    binds[waiting_bind][1] = binds[waiting_bind][0];
                    binds[waiting_bind][0] = k;
                    save_config();
                }
                waiting_bind = -1;
                break;
            }
        return;
    }
    if (menu == M_QUIT) {
        if (in.pressed[PK_A + ('Y' - 'A')] || in.pressed[PK_RETURN]) running = 0;
        if (in.pressed[PK_A + ('N' - 'A')] || in.pressed[PK_ESCAPE]) close_menu();
        return;
    }
    int n = menu_count();
    if (in.pressed[PK_ESCAPE] || in.pressed[PK_BACKSPACE]) {
        close_menu();
        snd_play(SFX_SWITCH, 120, 128);
        return;
    }
    if (in.pressed[PK_UP] || in.pressed[PK_WHEELUP]) sel = (sel + n - 1) % n, snd_play(SFX_NOWAY, 100, 128);
    if (in.pressed[PK_DOWN] || in.pressed[PK_WHEELDOWN]) sel = (sel + 1) % n, snd_play(SFX_NOWAY, 100, 128);
    if (in.pressed[PK_LEFT]) menu_activate(-1);
    if (in.pressed[PK_RIGHT]) menu_activate(1);
    if (in.pressed[PK_RETURN] || in.pressed[PK_SPACE]) menu_activate(0);
}

static void slider(int x, int y, int v, int max)
{
    for (int i = 0; i <= max; i++)
        for (int yy = 0; yy < 7; yy++) fb[(y + yy) * SCREENW + x + i * 4] = i <= v ? RAMP(R_RED, 12) : RAMP(R_GRAY, 5);
}

static void draw_menu(void)
{
    if (menu == M_NONE) return;
    r_dim(fb, 26);
    static const char *skills[] = {"RECRUIT", "SOLDIER", "VETERAN"};
    static const char *opts[] = {"MOUSE SPEED", "SFX VOLUME", "MUSIC VOLUME", "CONTROLS", "BACK"};
    const char *title = "", *items[A_COUNT + 1];
    int n = menu_count();
    switch (menu) {
    case M_MAIN: {
        int ids[8];
        title = "IRONHOLD";
        main_items(ids);
        for (int i = 0; i < n; i++) items[i] = main_names[ids[i]];
        break;
    }
    case M_SAVE: case M_LOAD: {
        static char names[NSLOTS][48];
        title = menu == M_SAVE ? "SAVE GAME" : "LOAD GAME";
        for (int i = 0; i < n; i++) {
            slot_name(i, names[i], sizeof names[i]);
            items[i] = names[i];
        }
        break;
    }
    case M_SKILL: title = "CHOOSE YOUR FATE";
        for (int i = 0; i < n; i++) items[i] = skills[i];
        break;
    case M_OPTIONS: title = "OPTIONS";
        for (int i = 0; i < n; i++) items[i] = opts[i];
        break;
    case M_CONTROLS: title = "CONTROLS";
        for (int i = 0; i < A_COUNT; i++) items[i] = action_names[i];
        items[A_COUNT] = "BACK";
        break;
    case M_QUIT:
        font_draw_center(fb, 100, "QUIT TO DESKTOP?", RAMP(R_GRAY, 14), 2);
        font_draw_center(fb, 124, "PRESS Y OR N", RAMP(R_GRAY, 10), 1);
        return;
    }
    font_draw_center(fb, 30, title, RAMP(R_RED, 13), 3);
    int small = menu == M_CONTROLS || menu == M_SAVE || menu == M_LOAD, lh = small ? 14 : 18, y0 = small ? 64 : 70;
    for (int i = 0; i < n; i++) {
        int y = y0 + i * lh, x = menu == M_OPTIONS || small ? 50 : (SCREENW - font_width(items[i], 2)) / 2;
        uint8_t c = i == sel ? RAMP(R_YELLOW, 14) : RAMP(R_GRAY, 11);
        if (small) font_draw(fb, x, y, items[i], c, 1, 1);
        else font_draw(fb, x, y, items[i], c, 2, 1);
        if (i == sel) font_draw(fb, x - 14, y, ">", RAMP(R_RED, 13), small ? 1 : 2, 1);
        if (menu == M_OPTIONS && i < 3) slider(200, y + 3, i == 0 ? sens : i == 1 ? sfxvol : musvol, i == 0 ? 20 : 15);
        if (menu == M_CONTROLS && i < A_COUNT) {
            char s[32];
            snprintf(s, sizeof s, "%s  %s", key_name(binds[i][0]), key_name(binds[i][1]));
            font_draw(fb, 180, y, waiting_bind == i ? "PRESS A KEY..." : s, waiting_bind == i ? RAMP(R_RED, 13) : c, 1, 1);
        }
    }
    if (menu == M_CONTROLS) font_draw_center(fb, 200, "ENTER: BIND  ESC: BACK", RAMP(R_GRAY, 9), 1);
}

/* ---------------------------------------------------------------- screens */

static void draw_title(void)
{
    for (int y = 0; y < SCREENH; y++) {
        uint8_t c = RAMP(R_BLOOD, clampi(y / 30, 0, 6));
        memset(fb + y * SCREENW, c, SCREENW);
    }
    const char *t = "IRONHOLD";
    int scale = 6, w = font_width(t, scale), x0 = (SCREENW - w + scale) / 2, y0 = 50;
    for (const char *s = t; *s; s++, x0 += 6 * scale)
        for (int gy = 0; gy < 7 * scale; gy++)
            for (int gx = 0; gx < 5 * scale; gx++)
                if (font_pixel(*s, gx / scale, gy / scale)) {
                    int edge = gx % scale == 0 || gy % scale == 0;
                    fb[(y0 + gy + 3) * SCREENW + x0 + gx + 3] = 0;
                    fb[(y0 + gy) * SCREENW + x0 + gx] = RAMP(R_STEEL, clampi(15 - gy / 4 + edge * 2, 3, 15));
                }
    font_draw_center(fb, 110, "EPISODE ONE: THE BREACH", RAMP(R_ORANGE, 12), 1);
    if ((fade / 20) & 1) font_draw_center(fb, 180, "PRESS ENTER", RAMP(R_GRAY, 13), 1);
    font_draw_center(fb, 226, "A VIBE-SHOOTER GAME", RAMP(R_GRAY, 6), 1);
}

static void draw_intermission(void)
{
    for (int y = 0; y < SCREENH; y++)
        for (int x = 0; x < SCREENW; x++) fb[y * SCREENW + x] = colormap[16][walltex[WT_STONE][(x & 63) * 64 + (y & 63)]];
    font_draw_center(fb, 24, g.levelname, RAMP(R_RED, 13), 2);
    font_draw_center(fb, 46, "COMPLETED", RAMP(R_GRAY, 13), 2);
    char s[64];
    int t = fade / 3; /* count up for a bit of drama */
    int pk = g.totalkills ? g.kills * 100 / g.totalkills : 100, pi = g.totalitems ? g.items * 100 / g.totalitems : 100,
        ps = g.totalsecrets ? g.secrets * 100 / g.totalsecrets : 100;
    snprintf(s, sizeof s, "KILLS    %3d%%", pk < t ? pk : t);
    font_draw(fb, 90, 90, s, RAMP(R_GRAY, 14), 2, 1);
    snprintf(s, sizeof s, "ITEMS    %3d%%", pi < t ? pi : t);
    font_draw(fb, 90, 112, s, RAMP(R_GRAY, 14), 2, 1);
    snprintf(s, sizeof s, "SECRETS  %3d%%", ps < t ? ps : t);
    font_draw(fb, 90, 134, s, RAMP(R_GRAY, 14), 2, 1);
    int secs = g.leveltime / TICRATE;
    snprintf(s, sizeof s, "TIME  %2d:%02d", secs / 60, secs % 60);
    font_draw(fb, 90, 164, s, RAMP(R_YELLOW, 13), 2, 1);
    snprintf(s, sizeof s, "PAR   %2d:%02d", g.par / 60, g.par % 60);
    font_draw(fb, 90, 184, s, RAMP(R_YELLOW, 10), 2, 1);
    if (fade > 60) font_draw_center(fb, 220, "PRESS ENTER TO CONTINUE", RAMP(R_GRAY, 10), 1);
}

static void draw_finale(void)
{
    memset(fb, RAMP(R_GRAY, 0), sizeof fb);
    static const char *lines[] = {
        "THE WARDEN LIES IN THE RUBBLE", "OF HIS OWN KEEP.", "", "IRONHOLD'S OUTER WALLS ARE BREACHED.",
        "BUT BENEATH THE KEEP, STAIRS", "DESCEND INTO THE DARK...", "", "THANK YOU FOR PLAYING.",
    };
    for (int i = 0; i < 8; i++)
        if (fade > i * 30) font_draw_center(fb, 70 + i * 14, lines[i], RAMP(R_GRAY, 13), 1);
}

/* ---------------------------------------------------------------- game tic */

static void build_cmd(ticcmd_t *cmd)
{
    memset(cmd, 0, sizeof *cmd);
    int speed = held(A_RUN) ? 50 : 25;
    if (held(A_FWD)) cmd->forward = (int8_t)(cmd->forward + speed);
    if (held(A_BACK)) cmd->forward = (int8_t)(cmd->forward - speed);
    if (held(A_RIGHT)) cmd->side = (int8_t)(cmd->side + speed);
    if (held(A_LEFT)) cmd->side = (int8_t)(cmd->side - speed);
    int turn = 0;
    if (held(A_TURNR)) turn += held(A_RUN) ? 1100 : 640;
    if (held(A_TURNL)) turn -= held(A_RUN) ? 1100 : 640;
    turn += mouse_acc * sens * 6;
    mouse_acc = 0;
    cmd->turn = (int16_t)clampi(turn, -32000, 32000);
    if (held(A_FIRE)) cmd->buttons |= BT_ATTACK;
    if (held(A_USE)) cmd->buttons |= BT_USE;
    cmd->weapon = weapon_tap;
    weapon_tap = 0;
    memset(tapped, 0, sizeof tapped);
}

static void play_events(void)
{
    const player_t *p = &g.player;
    for (int i = 0; i < g.nsnd; i++) {
        const sndev_t *e = &g.snd[i];
        if (e->local) {
            snd_play(e->sfx, 255, 128);
            continue;
        }
        float dx = (e->x - p->x) / 65536.0f, dy = (e->y - p->y) / 65536.0f;
        float dist = sqrtf(dx * dx + dy * dy);
        int vol = (int)(255 * (1 - dist / 28));
        float rel = atan2f(dy, dx) - p->angle * (6.2831853f / 4294967296.0f);
        int sep = dist < 0.5f ? 128 : (int)(128 + sinf(rel) * 110);
        snd_play(e->sfx, vol, sep);
    }
}

static void finish_recording(void)
{
    if (!record_path || !demolen) return;
    long len;
    char *text = demo_format(demoskill, demolevel, democmds, demolen, &len);
    plat_write_file(record_path, text, len);
    free(text);
    demolen = 0;
}

static void game_tic(void)
{
    ticcmd_t cmd;
    if (playing_demo) {
        if (demopos >= demolen) {
            running = 0;
            return;
        }
        cmd = democmds[demopos++];
    } else
        build_cmd(&cmd);
    if (record_path && demolen < MAXDEMO) democmds[demolen++] = cmd;
    g_ticker(&cmd);
    play_events();
    if (g.exiting < 0) start_level(g.levelnum, g.skill, &carry);
    else if (g.exiting > TICRATE) {
        mode = GM_INTER;
        fade = 0;
        play_music("assets/music/inter.txt");
        finish_recording();
    }
}

int main(int argc, char **argv)
{
    const char *play_path = NULL;
    int warp = 0;
    for (int i = 1; i + 1 < argc; i++) {
        if (!strcmp(argv[i], "--record")) record_path = argv[++i];
        else if (!strcmp(argv[i], "--playdemo")) play_path = argv[++i];
        else if (!strcmp(argv[i], "--warp")) warp = clampi(atoi(argv[++i]), 1, NUMLEVELS);
    }
    if (!plat_init("Ironhold")) return 1;
    fixed_init();
    gfx_init();
    snd_init();
    load_config();
    snd_set_volume(sfxvol, musvol);
    plat_set_audio_callback(snd_mix, MIXRATE);
    play_music("assets/music/title.txt");
    democmds = malloc(MAXDEMO * sizeof *democmds);
    if (play_path) {
        char *text;
        if (plat_read_file(play_path, (void **)&text) < 0 ||
            (demolen = demo_parse(text, &demoskill, &demolevel, democmds, MAXDEMO)) < 0) {
            fprintf(stderr, "bad demo %s\n", play_path);
            return 1;
        }
        free(text);
        playing_demo = 1;
        start_level(demolevel, demoskill, NULL);
    } else if (warp) {
        demoskill = SK_MEDIUM;
        demolevel = warp - 1;
        start_level(demolevel, demoskill, NULL);
    }

    uint64_t start = plat_ticks_us(), tics_done = 0;
    while (running) {
        if (!plat_poll(&in)) {
            running = 0;
            break;
        }
        if (menu == M_NONE) {
            mouse_acc += in.mouse_dx;
            latch_taps();
        }

        uint64_t now_tics = (plat_ticks_us() - start) * TICRATE / 1000000;
        int todo = (int)(now_tics - tics_done);
        if (todo > 10) todo = 10; /* don't spiral after a stall */
        tics_done = now_tics;

        if (menu != M_NONE) menu_input();
        else
            switch (mode) {
            case GM_TITLE:
                if (in.pressed[PK_RETURN] || in.pressed[PK_SPACE] || in.pressed[PK_ESCAPE] || in.pressed[PK_MOUSE1]) {
                    open_menu(M_MAIN);
                    snd_play(SFX_SWITCH, 160, 128);
                }
                break;
            case GM_LEVEL:
                if (in.pressed[PK_ESCAPE]) {
                    open_menu(M_MAIN);
                    break;
                }
                if (in.pressed[PK_TAB]) automap = !automap;
                for (int i = 0; i < todo && mode == GM_LEVEL; i++) game_tic();
                break;
            case GM_INTER:
                if (fade > 30 && (in.pressed[PK_RETURN] || in.pressed[PK_SPACE] || in.pressed[PK_MOUSE1])) {
                    player_t inv = g.player;
                    if (g.levelnum + 1 < NUMLEVELS) start_level(g.levelnum + 1, g.skill, &inv);
                    else {
                        mode = GM_FINALE;
                        fade = 0;
                    }
                }
                break;
            case GM_FINALE:
                if (fade > 250 && (in.pressed[PK_RETURN] || in.pressed[PK_ESCAPE])) {
                    mode = GM_TITLE;
                    play_music("assets/music/title.txt");
                }
                break;
            }
        if (mode != GM_LEVEL || menu != M_NONE) fade += todo;

        uint8_t pal[256 * 3];
        if (mode == GM_LEVEL) {
            float extra = mouse_acc * sens * 6 * 65536.0f * (6.2831853f / 4294967296.0f);
            r_render_view(fb, menu == M_NONE ? extra : 0);
            if (automap) r_draw_automap(fb);
            r_draw_hud(fb);
            if (g.player.dead && g.player.deadtics > TICRATE) font_draw_center(fb, 90, "PRESS USE TO TRY AGAIN", RAMP(R_GRAY, 14), 1);
            r_frame_palette(pal);
        } else {
            if (mode == GM_TITLE) draw_title();
            else if (mode == GM_INTER) draw_intermission();
            else draw_finale();
            memcpy(pal, palette, sizeof pal);
        }
        draw_menu();
        if (menu != M_NONE) memcpy(pal, palette, sizeof pal);
        plat_set_palette(pal);
        plat_present(fb);
    }
    finish_recording();
    save_config();
    plat_shutdown();
    return 0;
}
