/* Simulation: fixed point only, no platform, no floats. */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "fixed.h"

#define TICRATE 35
#define MAPW 64
#define MAPH 64
#define MAXACTORS 512
#define MAXDOORS 96

/* ---- shared ids (renderer and sound generate the data behind them) ---- */
enum { /* wall textures */
    WT_STONE, WT_WOOD, WT_METAL, WT_BRICK, WT_MOSS, WT_TECH,
    WT_DOOR, WT_DOORSIDE, WT_DOOR_RED, WT_DOOR_BLUE, WT_DOOR_YELLOW,
    WT_EXIT, WT_EXIT_ON, WT_COUNT
};
enum { FT_STONE, FT_DIRT, FT_METAL, FT_CEIL_PANEL, FT_CEIL_ROCK, FT_COUNT };

enum { /* sprite images */
    SPR_GRUNT_S, SPR_GRUNT_W1, SPR_GRUNT_W2, SPR_GRUNT_A, SPR_GRUNT_F, SPR_GRUNT_P,
    SPR_GRUNT_D1, SPR_GRUNT_D2, SPR_GRUNT_D3, SPR_GRUNT_D4,
    SPR_PUFF1, SPR_PUFF2, SPR_PUFF3, SPR_BLOOD1, SPR_BLOOD2, SPR_BLOOD3,
    SPR_STIM, SPR_MEDKIT, SPR_ARMOR, SPR_CLIP, SPR_SHELLS,
    SPR_KEYR, SPR_KEYB, SPR_KEYY, SPR_SHOTGUN,
    SPR_LAMP, SPR_PILLAR, SPR_BARREL,
    /* first-person weapon frames */
    SPR_PISTOL, SPR_PISTOL_F1, SPR_PISTOL_F2,
    SPR_SGUN, SPR_SGUN_F, SPR_SGUN_R1, SPR_SGUN_R2,
    SPR_COUNT
};

enum { /* sound effects */
    SFX_NONE, SFX_PISTOL, SFX_SHOTGUN, SFX_SGCOCK, SFX_ENEMYFIRE,
    SFX_GRUNT_SEE, SFX_GRUNT_PAIN, SFX_GRUNT_DIE, SFX_PLAYER_PAIN, SFX_PLAYER_DIE,
    SFX_ITEM, SFX_WPNUP, SFX_DOOR_OPEN, SFX_DOOR_CLOSE, SFX_PUSHWALL, SFX_SWITCH, SFX_NOWAY,
    SFX_COUNT
};

enum { TL_EMPTY, TL_WALL, TL_DOOR, TL_PUSH, TL_PUSHMOVING, TL_EXIT };

/* ---- per-tic player input: a demo is a seed plus a list of these ---- */
enum { BT_ATTACK = 1, BT_USE = 2 };
typedef struct {
    int8_t forward, side; /* -50..50 */
    int16_t turn;         /* added to angle as turn << 16 */
    uint8_t buttons;
    uint8_t weapon; /* 0 = no change, else weapon slot 1..N */
} ticcmd_t;

/* ---- actors ---- */
enum {
    MT_GRUNT, MT_PUFF, MT_BLOOD,
    MT_STIM, MT_MEDKIT, MT_ARMOR, MT_CLIP, MT_SHELLS, MT_KEYR, MT_KEYB, MT_KEYY, MT_SHOTGUN,
    MT_LAMP, MT_PILLAR, MT_BARREL,
    MT_COUNT
};
enum {
    MF_SOLID = 1, MF_SHOOTABLE = 2, MF_PICKUP = 4, MF_COUNTKILL = 8, MF_COUNTITEM = 16,
    MF_AMBUSH = 32, MF_CORPSE = 64, MF_DROPPED = 128
};

typedef struct {
    uint8_t inuse, type;
    int16_t state, tics;
    int32_t flags;
    fixed_t x, y, momx, momy;
    angle_t angle;
    int32_t health;
    int32_t movedir, movecount, reactiontime, threshold;
    int32_t awake;
} actor_t;

typedef struct {
    int16_t spawnstate, seestate, painstate, attackstate, deathstate;
    int32_t health, painchance, flags;
    fixed_t speed, radius;
    int16_t seesound, painsound, deathsound;
} mobjinfo_t;

/* ---- player ---- */
enum { WP_PISTOL, WP_SHOTGUN, WP_COUNT };
enum { AM_BULLET, AM_SHELL, AM_COUNT };
enum { KEY_RED = 1, KEY_BLUE = 2, KEY_YELLOW = 4 };

typedef struct {
    fixed_t x, y, momx, momy;
    angle_t angle;
    int32_t health, armor;
    int32_t ammo[AM_COUNT], maxammo[AM_COUNT];
    int32_t weaponowned; /* bitmask */
    int32_t readyweapon, pendingweapon;
    int32_t keys;
    int16_t wstate, wtics; /* first-person weapon state */
    fixed_t wy;            /* weapon raise/lower offset, 0 = up */
    int32_t refire;
    int32_t damagecount, bonuscount;
    int32_t attackdown, usedown;
    int32_t dead, deadtics;
    fixed_t bob;
    int32_t flash; /* tics of muzzle flash light */
} player_t;

typedef struct {
    uint8_t tx, ty, vertical, lock; /* vertical: plane runs north-south (walk east-west through it) */
    uint8_t state;                  /* 0 closed, 1 opening, 2 open, 3 closing */
    fixed_t open;                   /* 0..FRACUNIT */
    int32_t timer;
} door_t;

typedef struct {
    int32_t active, tx, ty, dx, dy, tex;
    fixed_t pos; /* 0..dist tiles */
    int32_t dist;
} pushwall_t;

typedef struct { int16_t sfx; fixed_t x, y; uint8_t local; } sndev_t;

enum { SK_EASY, SK_MEDIUM, SK_HARD, SK_COUNT };

typedef struct {
    /* map */
    int32_t w, h;
    uint8_t tile[MAPH][MAPW];
    uint8_t tex[MAPH][MAPW];
    int8_t doorat[MAPH][MAPW];
    door_t doors[MAXDOORS];
    int32_t ndoors;
    pushwall_t push;
    int32_t floortex, ceiltex, ambient; /* ambient: 0 bright .. 31 dark */
    char levelname[32];
    int32_t par;

    actor_t actors[MAXACTORS];
    player_t player;
    uint32_t rng;
    int32_t leveltime, skill, levelnum;
    int32_t kills, totalkills, items, totalitems, secrets, totalsecrets;
    int32_t exiting; /* >0: level done, counts tics */

    char msg[64];
    int32_t msgtics;

    /* transient output for the platform side; not part of the checksum */
    sndev_t snd[32];
    int32_t nsnd;
} game_t;

extern game_t g;
extern const mobjinfo_t mobjinfo[MT_COUNT];

/* Parse a level from text. Returns false with an error message in g.msg. */
bool g_load_level(const char *text, int skill, int levelnum, const player_t *carry);
void g_ticker(const ticcmd_t *cmd);
uint32_t g_checksum(void);
void g_new_player(player_t *p);
int16_t g_state_sprite(int state); /* image for an actor/weapon state, -1 = none */
int g_state_bright(int state);

#endif
