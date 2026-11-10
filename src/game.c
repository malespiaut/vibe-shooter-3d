/* Game simulation. Fixed point only: no float/double in this file. */
#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

game_t g;

/* ---------------------------------------------------------------- tables */

enum {
    S_NULL,
    S_GRUNT_STND, S_GRUNT_RUN1, S_GRUNT_RUN2, S_GRUNT_RUN3, S_GRUNT_RUN4,
    S_GRUNT_ATK1, S_GRUNT_ATK2, S_GRUNT_ATK3,
    S_GRUNT_PAIN, S_GRUNT_PAIN2,
    S_GRUNT_DIE1, S_GRUNT_DIE2, S_GRUNT_DIE3, S_GRUNT_DEAD,
    S_HOUND_STND, S_HOUND_RUN1, S_HOUND_RUN2, S_HOUND_RUN3, S_HOUND_RUN4,
    S_HOUND_ATK1, S_HOUND_ATK2, S_HOUND_ATK3, S_HOUND_PAIN,
    S_HOUND_DIE1, S_HOUND_DIE2, S_HOUND_DEAD,
    S_SPIT_STND, S_SPIT_RUN1, S_SPIT_RUN2, S_SPIT_RUN3, S_SPIT_RUN4,
    S_SPIT_ATK1, S_SPIT_ATK2, S_SPIT_ATK3, S_SPIT_PAIN,
    S_SPIT_DIE1, S_SPIT_DIE2, S_SPIT_DIE3, S_SPIT_DEAD,
    S_BRUTE_STND, S_BRUTE_RUN1, S_BRUTE_RUN2, S_BRUTE_RUN3, S_BRUTE_RUN4,
    S_BRUTE_ATK1, S_BRUTE_ATK2, S_BRUTE_ATK3, S_BRUTE_ATK4, S_BRUTE_ATK5, S_BRUTE_ATK6, S_BRUTE_ATK7,
    S_BRUTE_PAIN, S_BRUTE_DIE1, S_BRUTE_DIE2, S_BRUTE_DIE3, S_BRUTE_DEAD,
    S_WARDEN_STND, S_WARDEN_RUN1, S_WARDEN_RUN2, S_WARDEN_RUN3, S_WARDEN_RUN4,
    S_WARDEN_ATK1, S_WARDEN_ATK2, S_WARDEN_ATK3, S_WARDEN_ATK4, S_WARDEN_ATK5, S_WARDEN_PAIN,
    S_WARDEN_DIE1, S_WARDEN_DIE2, S_WARDEN_DIE3, S_WARDEN_DEAD,
    S_FIREBALL1, S_FIREBALL2, S_ROCKET1, S_EXPL1, S_EXPL2, S_EXPL3,
    S_BARREL, S_BEXPL1, S_BEXPL2, S_BEXPL3, S_BEXPL4,
    S_PUFF1, S_PUFF2, S_PUFF3, S_BLOOD1, S_BLOOD2, S_BLOOD3,
    S_STIM, S_MEDKIT, S_ARMOR, S_CLIP, S_SHELLS, S_KEYR, S_KEYB, S_KEYY, S_SHOTGUN,
    S_CHAINGUN, S_LAUNCHER, S_ROCKETAMMO, S_ROCKETBOX, S_BULLETBOX,
    S_LAMP, S_PILLAR,
    S_KNIFE, S_KNIFE1, S_KNIFE2, S_KNIFE3, S_KNIFE4,
    S_PISTOL, S_PISTOL1, S_PISTOL2, S_PISTOL3, S_PISTOL4,
    S_SGUN, S_SGUN1, S_SGUN2, S_SGUN3, S_SGUN4, S_SGUN5, S_SGUN6, S_SGUN7,
    S_CGUN, S_CGUN1, S_CGUN2, S_CGUN3,
    S_RL, S_RL1, S_RL2, S_RL3,
    NUMSTATES
};

static void A_Look(actor_t *a);
static void T_Chase(actor_t *a);
static void T_Missile(actor_t *a);
static void A_FaceTarget(actor_t *a);
static void A_GruntShoot(actor_t *a);
static void A_BruteShoot(actor_t *a);
static void A_Bite(actor_t *a);
static void A_Spit(actor_t *a);
static void A_WardenRocket(actor_t *a);
static void A_BossDeath(actor_t *a);
static void A_Explode(actor_t *a);
static void A_Pain(actor_t *a);
static void A_Scream(actor_t *a);
static void A_Fall(actor_t *a);
static void W_Ready(player_t *p);
static void W_ReFire(player_t *p);
static void W_Knife(player_t *p);
static void W_FirePistol(player_t *p);
static void W_FireShotgun(player_t *p);
static void W_FireChaingun(player_t *p);
static void W_FireRocket(player_t *p);
static void W_Cock(player_t *p);

typedef struct {
    int16_t sprite, tics;
    void (*action)(actor_t *); /* on entry */
    void (*think)(actor_t *);  /* every tic */
    void (*waction)(player_t *);
    int16_t next;
    uint8_t bright;
} state_def;

#define A(spr, t, act, thk, nxt) {spr, t, act, thk, NULL, nxt, 0}
#define AB(spr, t, act, thk, nxt) {spr, t, act, thk, NULL, nxt, 1}
#define W(spr, t, act, nxt) {spr, t, NULL, NULL, act, nxt, 0}
#define WB(spr, t, act, nxt) {spr, t, NULL, NULL, act, nxt, 1}

static const state_def sdef[NUMSTATES] = {
    [S_NULL] = A(-1, -1, NULL, NULL, S_NULL),
    [S_GRUNT_STND] = A(SPR_GRUNT_S, 10, A_Look, NULL, S_GRUNT_STND),
    [S_GRUNT_RUN1] = A(SPR_GRUNT_W1, 7, NULL, T_Chase, S_GRUNT_RUN2),
    [S_GRUNT_RUN2] = A(SPR_GRUNT_S, 7, NULL, T_Chase, S_GRUNT_RUN3),
    [S_GRUNT_RUN3] = A(SPR_GRUNT_W2, 7, NULL, T_Chase, S_GRUNT_RUN4),
    [S_GRUNT_RUN4] = A(SPR_GRUNT_S, 7, NULL, T_Chase, S_GRUNT_RUN1),
    [S_GRUNT_ATK1] = A(SPR_GRUNT_A, 14, A_FaceTarget, NULL, S_GRUNT_ATK2),
    [S_GRUNT_ATK2] = AB(SPR_GRUNT_F, 5, A_GruntShoot, NULL, S_GRUNT_ATK3),
    [S_GRUNT_ATK3] = A(SPR_GRUNT_A, 8, A_FaceTarget, NULL, S_GRUNT_RUN1),
    [S_GRUNT_PAIN] = A(SPR_GRUNT_P, 4, NULL, NULL, S_GRUNT_PAIN2),
    [S_GRUNT_PAIN2] = A(SPR_GRUNT_P, 4, A_Pain, NULL, S_GRUNT_RUN1),
    [S_GRUNT_DIE1] = A(SPR_GRUNT_D1, 6, A_Scream, NULL, S_GRUNT_DIE2),
    [S_GRUNT_DIE2] = A(SPR_GRUNT_D2, 6, A_Fall, NULL, S_GRUNT_DIE3),
    [S_GRUNT_DIE3] = A(SPR_GRUNT_D3, 6, NULL, NULL, S_GRUNT_DEAD),
    [S_GRUNT_DEAD] = A(SPR_GRUNT_D4, -1, NULL, NULL, S_NULL),
    /* hound: fast, low, telegraphs its bite by rearing back */
    [S_HOUND_STND] = A(SPR_HOUND_S, 10, A_Look, NULL, S_HOUND_STND),
    [S_HOUND_RUN1] = A(SPR_HOUND_W1, 4, NULL, T_Chase, S_HOUND_RUN2),
    [S_HOUND_RUN2] = A(SPR_HOUND_W2, 4, NULL, T_Chase, S_HOUND_RUN3),
    [S_HOUND_RUN3] = A(SPR_HOUND_W1, 4, NULL, T_Chase, S_HOUND_RUN4),
    [S_HOUND_RUN4] = A(SPR_HOUND_W2, 4, NULL, T_Chase, S_HOUND_RUN1),
    [S_HOUND_ATK1] = A(SPR_HOUND_A, 7, A_FaceTarget, NULL, S_HOUND_ATK2),
    [S_HOUND_ATK2] = A(SPR_HOUND_W1, 4, A_Bite, NULL, S_HOUND_ATK3),
    [S_HOUND_ATK3] = A(SPR_HOUND_S, 6, NULL, NULL, S_HOUND_RUN1),
    [S_HOUND_PAIN] = A(SPR_HOUND_P, 5, A_Pain, NULL, S_HOUND_RUN1),
    [S_HOUND_DIE1] = A(SPR_HOUND_D1, 6, A_Scream, NULL, S_HOUND_DIE2),
    [S_HOUND_DIE2] = A(SPR_HOUND_D2, 6, A_Fall, NULL, S_HOUND_DEAD),
    [S_HOUND_DEAD] = A(SPR_HOUND_D3, -1, NULL, NULL, S_NULL),
    /* spitter: glowing windup, then a slow dodgeable fireball */
    [S_SPIT_STND] = A(SPR_SPIT_S, 10, A_Look, NULL, S_SPIT_STND),
    [S_SPIT_RUN1] = A(SPR_SPIT_W1, 7, NULL, T_Chase, S_SPIT_RUN2),
    [S_SPIT_RUN2] = A(SPR_SPIT_S, 7, NULL, T_Chase, S_SPIT_RUN3),
    [S_SPIT_RUN3] = A(SPR_SPIT_W2, 7, NULL, T_Chase, S_SPIT_RUN4),
    [S_SPIT_RUN4] = A(SPR_SPIT_S, 7, NULL, T_Chase, S_SPIT_RUN1),
    [S_SPIT_ATK1] = AB(SPR_SPIT_A1, 16, A_FaceTarget, NULL, S_SPIT_ATK2),
    [S_SPIT_ATK2] = AB(SPR_SPIT_A2, 6, A_Spit, NULL, S_SPIT_ATK3),
    [S_SPIT_ATK3] = A(SPR_SPIT_S, 8, NULL, NULL, S_SPIT_RUN1),
    [S_SPIT_PAIN] = A(SPR_SPIT_P, 6, A_Pain, NULL, S_SPIT_RUN1),
    [S_SPIT_DIE1] = A(SPR_SPIT_D1, 6, A_Scream, NULL, S_SPIT_DIE2),
    [S_SPIT_DIE2] = A(SPR_SPIT_D2, 6, A_Fall, NULL, S_SPIT_DIE3),
    [S_SPIT_DIE3] = A(SPR_SPIT_D3, 6, NULL, NULL, S_SPIT_DEAD),
    [S_SPIT_DEAD] = A(SPR_SPIT_D4, -1, NULL, NULL, S_NULL),
    /* brute: slow and tough, long windup into a three-round burst */
    [S_BRUTE_STND] = A(SPR_BRUTE_S, 10, A_Look, NULL, S_BRUTE_STND),
    [S_BRUTE_RUN1] = A(SPR_BRUTE_W1, 9, NULL, T_Chase, S_BRUTE_RUN2),
    [S_BRUTE_RUN2] = A(SPR_BRUTE_S, 9, NULL, T_Chase, S_BRUTE_RUN3),
    [S_BRUTE_RUN3] = A(SPR_BRUTE_W2, 9, NULL, T_Chase, S_BRUTE_RUN4),
    [S_BRUTE_RUN4] = A(SPR_BRUTE_S, 9, NULL, T_Chase, S_BRUTE_RUN1),
    [S_BRUTE_ATK1] = A(SPR_BRUTE_A, 20, A_FaceTarget, NULL, S_BRUTE_ATK2),
    [S_BRUTE_ATK2] = AB(SPR_BRUTE_F, 4, A_BruteShoot, NULL, S_BRUTE_ATK3),
    [S_BRUTE_ATK3] = A(SPR_BRUTE_A, 4, A_FaceTarget, NULL, S_BRUTE_ATK4),
    [S_BRUTE_ATK4] = AB(SPR_BRUTE_F, 4, A_BruteShoot, NULL, S_BRUTE_ATK5),
    [S_BRUTE_ATK5] = A(SPR_BRUTE_A, 4, A_FaceTarget, NULL, S_BRUTE_ATK6),
    [S_BRUTE_ATK6] = AB(SPR_BRUTE_F, 4, A_BruteShoot, NULL, S_BRUTE_ATK7),
    [S_BRUTE_ATK7] = A(SPR_BRUTE_A, 12, NULL, NULL, S_BRUTE_RUN1),
    [S_BRUTE_PAIN] = A(SPR_BRUTE_P, 6, A_Pain, NULL, S_BRUTE_RUN1),
    [S_BRUTE_DIE1] = A(SPR_BRUTE_D1, 8, A_Scream, NULL, S_BRUTE_DIE2),
    [S_BRUTE_DIE2] = A(SPR_BRUTE_D2, 8, A_Fall, NULL, S_BRUTE_DIE3),
    [S_BRUTE_DIE3] = A(SPR_BRUTE_D3, 8, NULL, NULL, S_BRUTE_DEAD),
    [S_BRUTE_DEAD] = A(SPR_BRUTE_D4, -1, NULL, NULL, S_NULL),
    /* the warden: two telegraphed rockets per volley; the episode ends when it falls */
    [S_WARDEN_STND] = A(SPR_WARDEN_S, 10, A_Look, NULL, S_WARDEN_STND),
    [S_WARDEN_RUN1] = A(SPR_WARDEN_W1, 8, NULL, T_Chase, S_WARDEN_RUN2),
    [S_WARDEN_RUN2] = A(SPR_WARDEN_S, 8, NULL, T_Chase, S_WARDEN_RUN3),
    [S_WARDEN_RUN3] = A(SPR_WARDEN_W2, 8, NULL, T_Chase, S_WARDEN_RUN4),
    [S_WARDEN_RUN4] = A(SPR_WARDEN_S, 8, NULL, T_Chase, S_WARDEN_RUN1),
    [S_WARDEN_ATK1] = A(SPR_WARDEN_A, 18, A_FaceTarget, NULL, S_WARDEN_ATK2),
    [S_WARDEN_ATK2] = AB(SPR_WARDEN_F, 6, A_WardenRocket, NULL, S_WARDEN_ATK3),
    [S_WARDEN_ATK3] = A(SPR_WARDEN_A, 14, A_FaceTarget, NULL, S_WARDEN_ATK4),
    [S_WARDEN_ATK4] = AB(SPR_WARDEN_F, 6, A_WardenRocket, NULL, S_WARDEN_ATK5),
    [S_WARDEN_ATK5] = A(SPR_WARDEN_A, 16, NULL, NULL, S_WARDEN_RUN1),
    [S_WARDEN_PAIN] = A(SPR_WARDEN_P, 8, A_Pain, NULL, S_WARDEN_RUN1),
    [S_WARDEN_DIE1] = A(SPR_WARDEN_D1, 12, A_Scream, NULL, S_WARDEN_DIE2),
    [S_WARDEN_DIE2] = A(SPR_WARDEN_D2, 12, A_Fall, NULL, S_WARDEN_DIE3),
    [S_WARDEN_DIE3] = A(SPR_WARDEN_D3, 12, NULL, NULL, S_WARDEN_DEAD),
    [S_WARDEN_DEAD] = A(SPR_WARDEN_D4, -1, A_BossDeath, NULL, S_NULL),
    /* missiles and explosions */
    [S_FIREBALL1] = AB(SPR_FIREBALL1, 4, NULL, T_Missile, S_FIREBALL2),
    [S_FIREBALL2] = AB(SPR_FIREBALL2, 4, NULL, T_Missile, S_FIREBALL1),
    [S_ROCKET1] = AB(SPR_ROCKET, -1, NULL, T_Missile, S_ROCKET1),
    [S_EXPL1] = AB(SPR_EXPL1, 5, A_Explode, NULL, S_EXPL2),
    [S_EXPL2] = AB(SPR_EXPL2, 5, NULL, NULL, S_EXPL3),
    [S_EXPL3] = AB(SPR_EXPL3, 5, NULL, NULL, S_NULL),
    [S_BARREL] = A(SPR_BARREL, -1, NULL, NULL, S_NULL),
    [S_BEXPL1] = A(SPR_BARREL, 5, A_Scream, NULL, S_BEXPL2),
    [S_BEXPL2] = AB(SPR_EXPL1, 5, A_Explode, NULL, S_BEXPL3),
    [S_BEXPL3] = AB(SPR_EXPL2, 5, NULL, NULL, S_BEXPL4),
    [S_BEXPL4] = AB(SPR_EXPL3, 5, NULL, NULL, S_NULL),
    [S_PUFF1] = AB(SPR_PUFF1, 3, NULL, NULL, S_PUFF2),
    [S_PUFF2] = A(SPR_PUFF2, 3, NULL, NULL, S_PUFF3),
    [S_PUFF3] = A(SPR_PUFF3, 4, NULL, NULL, S_NULL),
    [S_BLOOD1] = A(SPR_BLOOD1, 4, NULL, NULL, S_BLOOD2),
    [S_BLOOD2] = A(SPR_BLOOD2, 4, NULL, NULL, S_BLOOD3),
    [S_BLOOD3] = A(SPR_BLOOD3, 4, NULL, NULL, S_NULL),
    [S_STIM] = A(SPR_STIM, -1, NULL, NULL, S_NULL),
    [S_MEDKIT] = A(SPR_MEDKIT, -1, NULL, NULL, S_NULL),
    [S_ARMOR] = A(SPR_ARMOR, -1, NULL, NULL, S_NULL),
    [S_CLIP] = A(SPR_CLIP, -1, NULL, NULL, S_NULL),
    [S_SHELLS] = A(SPR_SHELLS, -1, NULL, NULL, S_NULL),
    [S_KEYR] = AB(SPR_KEYR, -1, NULL, NULL, S_NULL),
    [S_KEYB] = AB(SPR_KEYB, -1, NULL, NULL, S_NULL),
    [S_KEYY] = AB(SPR_KEYY, -1, NULL, NULL, S_NULL),
    [S_SHOTGUN] = A(SPR_SHOTGUN, -1, NULL, NULL, S_NULL),
    [S_CHAINGUN] = A(SPR_CHAINGUN, -1, NULL, NULL, S_NULL),
    [S_LAUNCHER] = A(SPR_LAUNCHER, -1, NULL, NULL, S_NULL),
    [S_ROCKETAMMO] = A(SPR_ROCKETAMMO, -1, NULL, NULL, S_NULL),
    [S_ROCKETBOX] = A(SPR_ROCKETBOX, -1, NULL, NULL, S_NULL),
    [S_BULLETBOX] = A(SPR_BULLETBOX, -1, NULL, NULL, S_NULL),
    [S_LAMP] = AB(SPR_LAMP, -1, NULL, NULL, S_NULL),
    [S_PILLAR] = A(SPR_PILLAR, -1, NULL, NULL, S_NULL),
    /* player weapons */
    [S_KNIFE] = W(SPR_KNIFE, 1, W_Ready, S_KNIFE),
    [S_KNIFE1] = W(SPR_KNIFE_A1, 3, NULL, S_KNIFE2),
    [S_KNIFE2] = W(SPR_KNIFE_A2, 4, W_Knife, S_KNIFE3),
    [S_KNIFE3] = W(SPR_KNIFE_A1, 4, NULL, S_KNIFE4),
    [S_KNIFE4] = W(SPR_KNIFE, 4, W_ReFire, S_KNIFE),
    [S_PISTOL] = W(SPR_PISTOL, 1, W_Ready, S_PISTOL),
    [S_PISTOL1] = W(SPR_PISTOL, 2, NULL, S_PISTOL2),
    [S_PISTOL2] = WB(SPR_PISTOL_F1, 3, W_FirePistol, S_PISTOL3),
    [S_PISTOL3] = W(SPR_PISTOL_F2, 3, NULL, S_PISTOL4),
    [S_PISTOL4] = W(SPR_PISTOL, 5, W_ReFire, S_PISTOL),
    [S_SGUN] = W(SPR_SGUN, 1, W_Ready, S_SGUN),
    [S_SGUN1] = W(SPR_SGUN, 3, NULL, S_SGUN2),
    [S_SGUN2] = WB(SPR_SGUN_F, 4, W_FireShotgun, S_SGUN3),
    [S_SGUN3] = W(SPR_SGUN, 5, NULL, S_SGUN4),
    [S_SGUN4] = W(SPR_SGUN_R1, 5, W_Cock, S_SGUN5),
    [S_SGUN5] = W(SPR_SGUN_R2, 7, NULL, S_SGUN6),
    [S_SGUN6] = W(SPR_SGUN_R1, 5, NULL, S_SGUN7),
    [S_SGUN7] = W(SPR_SGUN, 6, W_ReFire, S_SGUN),
    [S_CGUN] = W(SPR_CGUN, 1, W_Ready, S_CGUN),
    [S_CGUN1] = WB(SPR_CGUN_F1, 4, W_FireChaingun, S_CGUN2),
    [S_CGUN2] = WB(SPR_CGUN_F2, 4, W_FireChaingun, S_CGUN3),
    [S_CGUN3] = W(SPR_CGUN, 1, W_ReFire, S_CGUN),
    [S_RL] = W(SPR_RLAUNCH, 1, W_Ready, S_RL),
    [S_RL1] = W(SPR_RLAUNCH, 8, NULL, S_RL2),
    [S_RL2] = WB(SPR_RLAUNCH_F, 12, W_FireRocket, S_RL3), /* refire skips S_RL3's tics, so the cooldown lives here */
    [S_RL3] = W(SPR_RLAUNCH, 10, W_ReFire, S_RL),
};

#define ITEM(st, fl) {st, 0, 0, 0, 0, 0, 0, 0, MF_PICKUP | (fl), 0, FRACUNIT / 4, 0, 0, 0}
#define FX(st) {st, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
const mobjinfo_t mobjinfo[MT_COUNT] = {
    /* spawn, see, pain, attack, melee, death, health, painchance, flags, speed, radius, sounds */
    [MT_GRUNT] = {S_GRUNT_STND, S_GRUNT_RUN1, S_GRUNT_PAIN, S_GRUNT_ATK1, 0, S_GRUNT_DIE1, 20, 170,
                  MF_SOLID | MF_SHOOTABLE | MF_COUNTKILL, FRACUNIT * 5 / 100, FRACUNIT * 3 / 10,
                  SFX_GRUNT_SEE, SFX_GRUNT_PAIN, SFX_GRUNT_DIE},
    [MT_HOUND] = {S_HOUND_STND, S_HOUND_RUN1, S_HOUND_PAIN, 0, S_HOUND_ATK1, S_HOUND_DIE1, 25, 120,
                  MF_SOLID | MF_SHOOTABLE | MF_COUNTKILL, FRACUNIT * 11 / 100, FRACUNIT * 3 / 10,
                  SFX_HOUND_SEE, SFX_HOUND_PAIN, SFX_HOUND_DIE},
    [MT_SPITTER] = {S_SPIT_STND, S_SPIT_RUN1, S_SPIT_PAIN, S_SPIT_ATK1, 0, S_SPIT_DIE1, 45, 140,
                    MF_SOLID | MF_SHOOTABLE | MF_COUNTKILL, FRACUNIT * 5 / 100, FRACUNIT * 3 / 10,
                    SFX_SPIT_SEE, SFX_SPIT_PAIN, SFX_SPIT_DIE},
    [MT_BRUTE] = {S_BRUTE_STND, S_BRUTE_RUN1, S_BRUTE_PAIN, S_BRUTE_ATK1, 0, S_BRUTE_DIE1, 150, 40,
                  MF_SOLID | MF_SHOOTABLE | MF_COUNTKILL, FRACUNIT * 4 / 100, FRACUNIT * 4 / 10,
                  SFX_BRUTE_SEE, SFX_BRUTE_PAIN, SFX_BRUTE_DIE},
    [MT_WARDEN] = {S_WARDEN_STND, S_WARDEN_RUN1, S_WARDEN_PAIN, S_WARDEN_ATK1, 0, S_WARDEN_DIE1, 900, 20,
                   MF_SOLID | MF_SHOOTABLE | MF_COUNTKILL, FRACUNIT * 45 / 1000, FRACUNIT * 45 / 100,
                   SFX_WARDEN_SEE, SFX_BRUTE_PAIN, SFX_WARDEN_DIE},
    [MT_WROCKET] = {S_ROCKET1, 0, 0, 0, 0, S_EXPL1, 0, 0, MF_MISSILE, FRACUNIT * 2 / 10, FRACUNIT / 8, SFX_ROCKET, 0, SFX_EXPLODE},
    [MT_FIREBALL] = {S_FIREBALL1, 0, 0, 0, 0, S_EXPL1, 0, 0, MF_MISSILE, FRACUNIT * 16 / 100, FRACUNIT / 8, SFX_FIREBALL, 0, SFX_EXPLODE},
    [MT_ROCKET] = {S_ROCKET1, 0, 0, 0, 0, S_EXPL1, 0, 0, MF_MISSILE, FRACUNIT * 35 / 100, FRACUNIT / 8, SFX_ROCKET, 0, SFX_EXPLODE},
    [MT_PUFF] = FX(S_PUFF1),
    [MT_BLOOD] = FX(S_BLOOD1),
    [MT_STIM] = ITEM(S_STIM, MF_COUNTITEM),
    [MT_MEDKIT] = ITEM(S_MEDKIT, MF_COUNTITEM),
    [MT_ARMOR] = ITEM(S_ARMOR, MF_COUNTITEM),
    [MT_CLIP] = ITEM(S_CLIP, MF_COUNTITEM),
    [MT_SHELLS] = ITEM(S_SHELLS, MF_COUNTITEM),
    [MT_KEYR] = ITEM(S_KEYR, 0),
    [MT_KEYB] = ITEM(S_KEYB, 0),
    [MT_KEYY] = ITEM(S_KEYY, 0),
    [MT_SHOTGUN] = ITEM(S_SHOTGUN, MF_COUNTITEM),
    [MT_CHAINGUN] = ITEM(S_CHAINGUN, MF_COUNTITEM),
    [MT_LAUNCHER] = ITEM(S_LAUNCHER, MF_COUNTITEM),
    [MT_ROCKETAMMO] = ITEM(S_ROCKETAMMO, MF_COUNTITEM),
    [MT_ROCKETBOX] = ITEM(S_ROCKETBOX, MF_COUNTITEM),
    [MT_BULLETBOX] = ITEM(S_BULLETBOX, MF_COUNTITEM),
    [MT_LAMP] = {S_LAMP, 0, 0, 0, 0, 0, 0, 0, 0, 0, FRACUNIT / 4, 0, 0, 0},
    [MT_PILLAR] = {S_PILLAR, 0, 0, 0, 0, 0, 0, 0, MF_SOLID, 0, FRACUNIT * 3 / 10, 0, 0, 0},
    [MT_BARREL] = {S_BARREL, 0, 0, 0, 0, S_BEXPL1, 20, 0, MF_SOLID | MF_SHOOTABLE, 0, FRACUNIT * 3 / 10, 0, 0, SFX_EXPLODE},
};

/* public view of the state table for the renderer */
int16_t g_state_sprite(int st) { return sdef[st].sprite; }
int g_state_bright(int st) { return sdef[st].bright; }

typedef struct { int ammo, per, ready, attack; } weaponinfo_t;
static const weaponinfo_t weaponinfo[WP_COUNT] = {
    [WP_KNIFE] = {AM_NONE, 0, S_KNIFE, S_KNIFE1},
    [WP_PISTOL] = {AM_BULLET, 1, S_PISTOL, S_PISTOL1},
    [WP_SHOTGUN] = {AM_SHELL, 1, S_SGUN, S_SGUN1},
    [WP_CHAINGUN] = {AM_BULLET, 1, S_CGUN, S_CGUN1},
    [WP_LAUNCHER] = {AM_ROCKET, 1, S_RL, S_RL1},
};
int g_weapon_ammo(int w) { return weaponinfo[w].ammo; }

/* ---------------------------------------------------------------- helpers */

static int P_Random(void) { return rng_next(&g.rng); }

static void sound_at(int sfx, fixed_t x, fixed_t y)
{
    if (g.nsnd < (int)(sizeof g.snd / sizeof g.snd[0])) g.snd[g.nsnd++] = (sndev_t){(int16_t)sfx, x, y, 0};
}

static void sound_local(int sfx)
{
    if (g.nsnd < (int)(sizeof g.snd / sizeof g.snd[0])) g.snd[g.nsnd++] = (sndev_t){(int16_t)sfx, 0, 0, 1};
}

static void message(const char *m)
{
    snprintf(g.msg, sizeof g.msg, "%s", m);
    g.msgtics = TICRATE * 3;
}

static door_t *door_at(int tx, int ty)
{
    int d = g.doorat[ty][tx];
    return d >= 0 ? &g.doors[d] : NULL;
}

static bool in_map(int tx, int ty) { return tx >= 0 && ty >= 0 && tx < g.w && ty < g.h; }

static bool tile_blocks_move(int tx, int ty)
{
    if (!in_map(tx, ty)) return true;
    switch (g.tile[ty][tx]) {
    case TL_EMPTY: return false;
    case TL_DOOR: return door_at(tx, ty)->open < FRACUNIT;
    default: return true;
    }
}

/* Does the point block shots/sight? Doors block only on their mid-plane, minus the slid-open part. */
static bool point_blocks_shot(fixed_t x, fixed_t y, fixed_t step)
{
    int tx = ftoi(x), ty = ftoi(y);
    if (!in_map(tx, ty)) return true;
    switch (g.tile[ty][tx]) {
    case TL_EMPTY: return false;
    case TL_DOOR: {
        door_t *d = door_at(tx, ty);
        fixed_t across = d->vertical ? x - itof(tx) : y - itof(ty);
        fixed_t along = d->vertical ? y - itof(ty) : x - itof(tx);
        if (fabs_(across - FRACUNIT / 2) > step) return false;
        return along >= d->open;
    }
    default: return true;
    }
}

static bool line_clear(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    fixed_t dist = fhypot(x2 - x1, y2 - y1);
    const fixed_t step = FRACUNIT / 8;
    int n = dist / step;
    if (n == 0) return true;
    fixed_t sx = (x2 - x1) / n, sy = (y2 - y1) / n;
    fixed_t x = x1, y = y1;
    for (int i = 1; i < n; i++) {
        x += sx;
        y += sy;
        if (point_blocks_shot(x, y, step)) return false;
    }
    return true;
}

static actor_t *spawn(int type, fixed_t x, fixed_t y)
{
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *a = &g.actors[i];
        if (a->inuse) continue;
        memset(a, 0, sizeof *a);
        a->inuse = 1;
        a->type = (uint8_t)type;
        a->x = x;
        a->y = y;
        a->flags = mobjinfo[type].flags;
        a->health = mobjinfo[type].health;
        a->state = mobjinfo[type].spawnstate;
        a->tics = sdef[a->state].tics;
        a->movedir = -1;
        return a;
    }
    return NULL;
}

/* Doom-style: run entry actions, skip through zero-tic states. */
static void set_state(actor_t *a, int st)
{
    do {
        if (st == S_NULL) {
            a->inuse = 0;
            return;
        }
        a->state = (int16_t)st;
        a->tics = sdef[st].tics;
        if (sdef[st].action) sdef[st].action(a);
        if (!a->inuse) return;
        st = sdef[st].next;
    } while (a->tics == 0);
}

static fixed_t radius_of(const actor_t *a) { return mobjinfo[a->type].radius; }

#define PLAYER_RADIUS (FRACUNIT * 3 / 10)

static bool box_free_of_tiles(fixed_t x, fixed_t y, fixed_t r)
{
    int x0 = ftoi(x - r), x1 = ftoi(x + r - 1), y0 = ftoi(y - r), y1 = ftoi(y + r - 1);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            if (tile_blocks_move(tx, ty)) return false;
    return true;
}

static bool boxes_overlap(fixed_t ax, fixed_t ay, fixed_t ar, fixed_t bx, fixed_t by, fixed_t br)
{
    return fabs_(ax - bx) < ar + br && fabs_(ay - by) < ar + br;
}

/* Move a box if nothing blocks it. self = NULL means the player. */
static bool try_move(actor_t *self, fixed_t nx, fixed_t ny, fixed_t r)
{
    if (!box_free_of_tiles(nx, ny, r)) return false;
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *o = &g.actors[i];
        if (!o->inuse || !(o->flags & MF_SOLID) || o == self) continue;
        if (boxes_overlap(nx, ny, r, o->x, o->y, radius_of(o))) return false;
    }
    if (self && !g.player.dead && boxes_overlap(nx, ny, r, g.player.x, g.player.y, PLAYER_RADIUS)) return false;
    return true;
}

static bool actor_in_tile(int tx, int ty)
{
    fixed_t cx = itof(tx) + FRACUNIT / 2, cy = itof(ty) + FRACUNIT / 2;
    if (boxes_overlap(g.player.x, g.player.y, PLAYER_RADIUS, cx, cy, FRACUNIT / 2)) return true;
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *o = &g.actors[i];
        if (o->inuse && (o->flags & MF_SOLID) && boxes_overlap(o->x, o->y, radius_of(o), cx, cy, FRACUNIT / 2))
            return true;
    }
    return false;
}

/* ---------------------------------------------------------------- doors and pushwalls */

#define DOOR_SPEED (FRACUNIT / 16)
#define DOOR_WAIT (TICRATE * 4)

static void door_open(door_t *d)
{
    if (d->state == 0 || d->state == 3) {
        if (d->state == 0) sound_at(SFX_DOOR_OPEN, itof(d->tx) + FRACUNIT / 2, itof(d->ty) + FRACUNIT / 2);
        d->state = 1;
    } else if (d->state == 2)
        d->timer = DOOR_WAIT;
}

static void doors_tick(void)
{
    for (int i = 0; i < g.ndoors; i++) {
        door_t *d = &g.doors[i];
        switch (d->state) {
        case 1:
            d->open += DOOR_SPEED;
            if (d->open >= FRACUNIT) {
                d->open = FRACUNIT;
                d->state = 2;
                d->timer = DOOR_WAIT;
            }
            break;
        case 2:
            if (--d->timer <= 0) {
                if (actor_in_tile(d->tx, d->ty))
                    d->timer = TICRATE / 2;
                else {
                    d->state = 3;
                    sound_at(SFX_DOOR_CLOSE, itof(d->tx) + FRACUNIT / 2, itof(d->ty) + FRACUNIT / 2);
                }
            }
            break;
        case 3:
            if (actor_in_tile(d->tx, d->ty)) {
                d->state = 1;
                break;
            }
            d->open -= DOOR_SPEED;
            if (d->open <= 0) {
                d->open = 0;
                d->state = 0;
            }
            break;
        default: break;
        }
    }
}

static void push_mark(int type)
{
    pushwall_t *p = &g.push;
    int steps = ftoi(p->pos);
    int cx = p->tx + p->dx * steps, cy = p->ty + p->dy * steps;
    g.tile[cy][cx] = (uint8_t)type;
    g.tex[cy][cx] = (uint8_t)p->tex;
    if (p->pos % FRACUNIT) {
        g.tile[cy + p->dy][cx + p->dx] = (uint8_t)type;
        g.tex[cy + p->dy][cx + p->dx] = (uint8_t)p->tex;
    }
}

static void push_start(int tx, int ty, int dx, int dy)
{
    if (g.push.active) return;
    int dist = 0;
    while (dist < 2) {
        int nx = tx + dx * (dist + 1), ny = ty + dy * (dist + 1);
        if (!in_map(nx, ny) || g.tile[ny][nx] != TL_EMPTY || actor_in_tile(nx, ny)) break;
        dist++;
    }
    if (dist == 0) {
        sound_local(SFX_NOWAY);
        return;
    }
    g.push = (pushwall_t){1, tx, ty, dx, dy, g.tex[ty][tx], 0, dist};
    g.tile[ty][tx] = TL_EMPTY;
    push_mark(TL_PUSHMOVING);
    g.secrets++;
    message("A SECRET IS REVEALED!");
    sound_at(SFX_PUSHWALL, itof(tx) + FRACUNIT / 2, itof(ty) + FRACUNIT / 2);
}

static void push_tick(void)
{
    pushwall_t *p = &g.push;
    if (!p->active) return;
    push_mark(TL_EMPTY);
    p->pos += FRACUNIT / 32;
    if (p->pos >= itof(p->dist)) {
        p->pos = itof(p->dist);
        p->active = 0;
        push_mark(TL_WALL);
        return;
    }
    push_mark(TL_PUSHMOVING);
}

/* ---------------------------------------------------------------- noise: wake enemies in the connected area */

static void make_noise(fixed_t x, fixed_t y)
{
    static uint8_t seen[MAPH][MAPW];
    static uint16_t queue[MAPW * MAPH];
    memset(seen, 0, sizeof seen);
    int head = 0, tail = 0, sx = ftoi(x), sy = ftoi(y);
    seen[sy][sx] = 1;
    queue[tail++] = (uint16_t)(sy * MAPW + sx);
    while (head < tail) {
        int t = queue[head++], tx = t % MAPW, ty = t / MAPW;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            int nx = tx + dx[k], ny = ty + dy[k];
            if (!in_map(nx, ny) || seen[ny][nx]) continue;
            int tl = g.tile[ny][nx];
            if (tl != TL_EMPTY && !(tl == TL_DOOR && door_at(nx, ny)->open > 0)) continue;
            seen[ny][nx] = 1;
            queue[tail++] = (uint16_t)(ny * MAPW + nx);
        }
    }
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *a = &g.actors[i];
        if (a->inuse && (a->flags & MF_COUNTKILL) && !a->awake && !(a->flags & MF_AMBUSH) && a->health > 0 &&
            seen[ftoi(a->y)][ftoi(a->x)])
            a->awake = 2; /* woken by noise; A_Look reacts next time it runs */
    }
}

/* ---------------------------------------------------------------- damage */

static void damage_player(int dmg)
{
    player_t *p = &g.player;
    if (p->dead) return;
    static const int skillmul[SK_COUNT] = {2, 4, 5};
    dmg = dmg * skillmul[g.skill] / 4;
    if (dmg < 1) dmg = 1;
    if (p->armor) {
        int saved = dmg / 3;
        if (saved > p->armor) saved = p->armor;
        p->armor -= saved;
        dmg -= saved;
    }
    p->health -= dmg;
    p->damagecount += dmg;
    if (p->damagecount > 40) p->damagecount = 40;
    if (p->health <= 0) {
        p->health = 0;
        p->dead = 1;
        sound_local(SFX_PLAYER_DIE);
    } else
        sound_local(SFX_PLAYER_PAIN);
}

static void wake(actor_t *a)
{
    if (a->awake == 1) return;
    a->awake = 1;
    set_state(a, mobjinfo[a->type].seestate);
}

static void damage_actor(actor_t *a, int dmg)
{
    if (!(a->flags & MF_SHOOTABLE)) return;
    a->health -= dmg;
    if (a->health <= 0) {
        a->flags &= ~MF_SHOOTABLE;
        if (a->flags & MF_COUNTKILL) g.kills++;
        set_state(a, mobjinfo[a->type].deathstate);
        return;
    }
    if (!(a->flags & MF_COUNTKILL)) return;
    if (P_Random() < mobjinfo[a->type].painchance) {
        a->awake = 1;
        set_state(a, mobjinfo[a->type].painstate);
        return;
    }
    wake(a);
}

/* Instant-hit trace from (x,y) along ang. Returns 0 = nothing, 1 = wall, 2 = something shootable. */
static int hitscan(fixed_t x, fixed_t y, angle_t ang, fixed_t range, int dmg)
{
    fixed_t dx = fcos(ang), dy = fsin(ang);
    const fixed_t step = FRACUNIT / 16;
    fixed_t walld = range;
    for (fixed_t d = step; d < range; d += step)
        if (point_blocks_shot(x + fmul(dx, d), y + fmul(dy, d), step)) {
            walld = d;
            break;
        }
    actor_t *best = NULL;
    fixed_t bestd = walld;
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *a = &g.actors[i];
        if (!a->inuse || !(a->flags & (MF_SHOOTABLE | MF_SOLID))) continue;
        fixed_t rx = a->x - x, ry = a->y - y;
        fixed_t along = fmul(rx, dx) + fmul(ry, dy);
        if (along <= 0 || along >= bestd) continue;
        fixed_t perp = fabs_(fmul(rx, dy) - fmul(ry, dx));
        if (perp < radius_of(a)) {
            best = a;
            bestd = along;
        }
    }
    if (best && (best->flags & MF_SHOOTABLE)) {
        fixed_t bd = bestd - FRACUNIT / 8;
        if (best->flags & MF_COUNTKILL) spawn(MT_BLOOD, x + fmul(dx, bd), y + fmul(dy, bd));
        else spawn(MT_PUFF, x + fmul(dx, bd), y + fmul(dy, bd));
        damage_actor(best, dmg);
        return 2;
    }
    if (!best && walld >= range) return 0;
    fixed_t pd = bestd - step;
    spawn(MT_PUFF, x + fmul(dx, pd), y + fmul(dy, pd));
    return 1;
}

/* Splash damage around (x,y): full at the centre, none at 2 tiles; walls block it. */
static void radius_attack(fixed_t x, fixed_t y, int maxdmg)
{
    const fixed_t range = itof(2);
    if (maxdmg <= 0) return;
    player_t *p = &g.player;
    fixed_t d = fhypot(p->x - x, p->y - y);
    if (d < range && line_clear(x, y, p->x, p->y)) damage_player(maxdmg - fmul(maxdmg, fdiv(d, range)));
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *a = &g.actors[i];
        if (!a->inuse || !(a->flags & MF_SHOOTABLE)) continue;
        d = fhypot(a->x - x, a->y - y) - radius_of(a);
        if (d < 0) d = 0;
        if (d < range && line_clear(x, y, a->x, a->y)) damage_actor(a, maxdmg - fmul(maxdmg, fdiv(d, range)));
    }
}

/* ---------------------------------------------------------------- enemy AI */

enum { DI_E, DI_SE, DI_S, DI_SW, DI_W, DI_NW, DI_N, DI_NE, DI_NONE = -1 };
#define DIAG 46341 /* 1/sqrt(2) */
static const fixed_t xspeed[8] = {FRACUNIT, DIAG, 0, -DIAG, -FRACUNIT, -DIAG, 0, DIAG};
static const fixed_t yspeed[8] = {0, DIAG, FRACUNIT, DIAG, 0, -DIAG, -FRACUNIT, -DIAG};

static bool sees_player(actor_t *a, bool fov)
{
    player_t *p = &g.player;
    if (p->dead) return false;
    if (fov && fhypot(p->x - a->x, p->y - a->y) > itof(2)) {
        angle_t rel = fatan2(p->y - a->y, p->x - a->x) - a->angle;
        if (rel > ANG90 && rel < ANG270) return false;
    }
    return line_clear(a->x, a->y, p->x, p->y);
}

static void A_Look(actor_t *a)
{
    if (a->awake == 2 || sees_player(a, true)) {
        sound_at(mobjinfo[a->type].seesound, a->x, a->y);
        a->reactiontime = 4 + (P_Random() & 15);
        wake(a);
    }
}

static void A_FaceTarget(actor_t *a) { a->angle = fatan2(g.player.y - a->y, g.player.x - a->x); }

/* 0 = blocked, 1 = moved, 2 = waiting for a door */
static int move_dir(actor_t *a)
{
    if (a->movedir == DI_NONE) return 0;
    fixed_t sp = mobjinfo[a->type].speed;
    fixed_t nx = a->x + fmul(sp, xspeed[a->movedir]), ny = a->y + fmul(sp, yspeed[a->movedir]);
    fixed_t r = radius_of(a);
    if (try_move(a, nx, ny, r)) {
        a->x = nx;
        a->y = ny;
        a->angle = (angle_t)a->movedir * ANG45;
        return 1;
    }
    /* opens doors in the way */
    int x0 = ftoi(nx - r), x1 = ftoi(nx + r - 1), y0 = ftoi(ny - r), y1 = ftoi(ny + r - 1);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            if (in_map(tx, ty) && g.tile[ty][tx] == TL_DOOR && !door_at(tx, ty)->lock) {
                door_open(door_at(tx, ty));
                return 2;
            }
    return 0;
}

static bool try_dir(actor_t *a, int dir)
{
    a->movedir = dir;
    if (!move_dir(a)) return false;
    a->movecount = 8 + (P_Random() & 31);
    return true;
}

static void new_chase_dir(actor_t *a)
{
    int olddir = a->movedir, turnaround = olddir == DI_NONE ? DI_NONE : (olddir + 4) & 7;
    fixed_t dx = g.player.x - a->x, dy = g.player.y - a->y;
    int d1 = dx > FRACUNIT / 4 ? DI_E : dx < -FRACUNIT / 4 ? DI_W : DI_NONE;
    int d2 = dy > FRACUNIT / 4 ? DI_S : dy < -FRACUNIT / 4 ? DI_N : DI_NONE;
    if (d1 != DI_NONE && d2 != DI_NONE) {
        static const int diag[2][2] = {{DI_NW, DI_NE}, {DI_SW, DI_SE}}; /* [south][east] */
        int dd = diag[d2 == DI_S][d1 == DI_E];
        if (dd != turnaround && try_dir(a, dd)) return;
    }
    if (P_Random() > 200 || fabs_(dy) > fabs_(dx)) {
        int t = d1;
        d1 = d2;
        d2 = t;
    }
    if (d1 != DI_NONE && d1 != turnaround && try_dir(a, d1)) return;
    if (d2 != DI_NONE && d2 != turnaround && try_dir(a, d2)) return;
    if (olddir != DI_NONE && try_dir(a, olddir)) return;
    int start = P_Random() & 7;
    for (int i = 0; i < 8; i++) {
        int d = (start + i) & 7;
        if (d != turnaround && try_dir(a, d)) return;
    }
    if (turnaround != DI_NONE && try_dir(a, turnaround)) return;
    a->movedir = DI_NONE;
}

static bool check_attack(actor_t *a)
{
    if (!sees_player(a, false)) return false;
    int d = fhypot(g.player.x - a->x, g.player.y - a->y) / (FRACUNIT / 16) - 32; /* 1/16 tiles */
    if (d < 0) d = 0;
    if (d > 200) d = 200;
    return P_Random() >= d;
}

static bool in_melee_range(actor_t *a)
{
    fixed_t reach = radius_of(a) + PLAYER_RADIUS + FRACUNIT / 4;
    return !g.player.dead && boxes_overlap(a->x, a->y, reach, g.player.x, g.player.y, 0);
}

static void T_Chase(actor_t *a)
{
    const mobjinfo_t *info = &mobjinfo[a->type];
    if (a->reactiontime) a->reactiontime--;
    if (g.player.dead) a->movecount = 0;
    if (info->meleestate && in_melee_range(a)) {
        set_state(a, info->meleestate);
        return;
    }
    if (--a->movecount < 0) {
        if (info->attackstate && !g.player.dead && a->reactiontime == 0 && check_attack(a)) {
            set_state(a, mobjinfo[a->type].attackstate);
            a->movecount = 4 + (P_Random() & 15);
            return;
        }
        new_chase_dir(a);
        return;
    }
    int m = move_dir(a);
    if (m == 0) new_chase_dir(a);
}

static void A_GruntShoot(actor_t *a)
{
    A_FaceTarget(a);
    sound_at(SFX_ENEMYFIRE, a->x, a->y);
    if (!sees_player(a, false)) return;
    player_t *p = &g.player;
    int tiles = ftoi(fhypot(p->x - a->x, p->y - a->y));
    int chance = 190 - tiles * 16;
    if (fhypot(p->momx, p->momy) > FRACUNIT / 20) chance -= 60; /* moving targets are harder to hit */
    if (chance < 16) chance = 16;
    if (P_Random() >= chance) return;
    int dmg = 2 + P_Random() % 8;
    if (tiles < 2) dmg += 4;
    damage_player(dmg);
}

static void A_BruteShoot(actor_t *a)
{
    A_FaceTarget(a);
    sound_at(SFX_CHAINGUN, a->x, a->y);
    if (!sees_player(a, false)) return;
    player_t *p = &g.player;
    int chance = 170 - ftoi(fhypot(p->x - a->x, p->y - a->y)) * 12;
    if (fhypot(p->momx, p->momy) > FRACUNIT / 20) chance -= 50;
    if (chance < 24) chance = 24;
    if (P_Random() < chance) damage_player(3 + P_Random() % 9);
}

static void A_Bite(actor_t *a)
{
    A_FaceTarget(a);
    sound_at(SFX_HOUND_BITE, a->x, a->y);
    if (in_melee_range(a)) damage_player(4 + P_Random() % 9);
}

static void spawn_missile(int type, fixed_t x, fixed_t y, angle_t ang, int owner)
{
    fixed_t c = fcos(ang), s = fsin(ang), ahead = FRACUNIT * 4 / 10;
    actor_t *m = spawn(type, x + fmul(c, ahead), y + fmul(s, ahead));
    if (!m) return;
    m->owner = owner;
    m->angle = ang;
    m->momx = fmul(c, mobjinfo[type].speed);
    m->momy = fmul(s, mobjinfo[type].speed);
    sound_at(mobjinfo[type].seesound, x, y);
}

static void A_WardenRocket(actor_t *a)
{
    A_FaceTarget(a);
    spawn_missile(MT_WROCKET, a->x, a->y, a->angle, (int)(a - g.actors));
}

static void A_BossDeath(actor_t *a)
{
    (void)a;
    message("THE WARDEN HAS FALLEN!");
    if (!g.exiting) g.exiting = 1;
}

static void A_Spit(actor_t *a)
{
    A_FaceTarget(a);
    spawn_missile(MT_FIREBALL, a->x, a->y, a->angle, (int)(a - g.actors));
}

static void explode_missile(actor_t *m)
{
    m->momx = m->momy = 0;
    m->flags &= ~MF_MISSILE;
    sound_at(mobjinfo[m->type].deathsound, m->x, m->y);
    set_state(m, mobjinfo[m->type].deathstate);
}

static void A_Explode(actor_t *a) { radius_attack(a->x, a->y, a->type == MT_FIREBALL ? 0 : a->type == MT_WROCKET ? 40 : 128); }

/* Missiles move in two half steps so fast rockets don't skip through thin things. */
static void T_Missile(actor_t *m)
{
    if (!(m->flags & MF_MISSILE)) return;
    for (int step = 0; step < 2; step++) {
        fixed_t nx = m->x + m->momx / 2, ny = m->y + m->momy / 2;
        if (tile_blocks_move(ftoi(nx), ftoi(ny))) {
            explode_missile(m);
            return;
        }
        fixed_t r = radius_of(m);
        bool hostile = m->owner >= 0; /* fired by a monster */
        if (hostile && !g.player.dead && boxes_overlap(nx, ny, r, g.player.x, g.player.y, PLAYER_RADIUS)) {
            m->x = nx;
            m->y = ny;
            damage_player((m->type == MT_WROCKET ? 5 : 3) * (1 + P_Random() % 8));
            explode_missile(m);
            return;
        }
        for (int i = 0; i < MAXACTORS; i++) {
            actor_t *o = &g.actors[i];
            if (!o->inuse || o == m || i == m->owner || !(o->flags & (MF_SOLID | MF_SHOOTABLE))) continue;
            if (hostile && (o->flags & MF_COUNTKILL)) continue; /* monster missiles pass through monsters */
            if (!boxes_overlap(nx, ny, r, o->x, o->y, radius_of(o))) continue;
            m->x = nx;
            m->y = ny;
            damage_actor(o, (m->type == MT_ROCKET ? 20 : 3) * (1 + P_Random() % 8));
            explode_missile(m);
            return;
        }
        m->x = nx;
        m->y = ny;
    }
}

static void A_Pain(actor_t *a) { sound_at(mobjinfo[a->type].painsound, a->x, a->y); }
static void A_Scream(actor_t *a) { sound_at(mobjinfo[a->type].deathsound, a->x, a->y); }

static void A_Fall(actor_t *a)
{
    a->flags &= ~MF_SOLID;
    a->flags |= MF_CORPSE;
    int drop = a->type == MT_GRUNT ? MT_CLIP : a->type == MT_BRUTE ? MT_BULLETBOX : -1;
    if (drop >= 0) {
        actor_t *c = spawn(drop, a->x, a->y);
        if (c) c->flags = (c->flags & ~MF_COUNTITEM) | MF_DROPPED;
    }
}

/* ---------------------------------------------------------------- player weapons */

static void set_wstate(player_t *p, int st)
{
    do {
        p->wstate = (int16_t)st;
        p->wtics = sdef[st].tics;
        if (sdef[st].waction) sdef[st].waction(p);
        if (p->wstate != st) return; /* action changed state */
        st = sdef[st].next;
    } while (p->wtics == 0);
}

static bool weapon_has_ammo(const player_t *p, int w)
{
    return weaponinfo[w].ammo == AM_NONE || p->ammo[weaponinfo[w].ammo] >= weaponinfo[w].per;
}

/* auto-switch when empty; never picks the launcher, so nobody blows themselves up by surprise */
static int best_weapon(player_t *p)
{
    static const int pref[] = {WP_CHAINGUN, WP_SHOTGUN, WP_PISTOL, WP_KNIFE};
    for (int i = 0; i < 4; i++)
        if ((p->weaponowned & (1 << pref[i])) && weapon_has_ammo(p, pref[i])) return pref[i];
    return WP_KNIFE;
}

static bool has_ammo(player_t *p) { return weapon_has_ammo(p, p->readyweapon); }

static void use_ammo(player_t *p)
{
    const weaponinfo_t *wi = &weaponinfo[p->readyweapon];
    if (wi->ammo != AM_NONE) p->ammo[wi->ammo] -= wi->per;
}

static void W_Ready(player_t *p)
{
    if (p->pendingweapon != p->readyweapon || p->wy > 0 || p->dead) return;
    if (p->attackdown) {
        if (has_ammo(p))
            set_wstate(p, weaponinfo[p->readyweapon].attack);
        else if (best_weapon(p) != p->readyweapon)
            p->pendingweapon = best_weapon(p);
    } else
        p->refire = 0;
}

static void W_ReFire(player_t *p)
{
    if (p->attackdown && has_ammo(p) && p->pendingweapon == p->readyweapon && !p->dead) {
        p->refire++;
        set_wstate(p, weaponinfo[p->readyweapon].attack);
    } else
        p->refire = 0;
}

static void W_Knife(player_t *p)
{
    int hit = hitscan(p->x, p->y, p->angle + (angle_t)((P_Random() - P_Random()) * (1 << 18)), FRACUNIT * 11 / 10,
                      2 * (1 + P_Random() % 10));
    sound_local(hit == 2 ? SFX_KNIFEHIT : SFX_KNIFE);
}

static void W_FireChaingun(player_t *p)
{
    if (!has_ammo(p)) return;
    use_ammo(p);
    p->flash = 2;
    sound_local(SFX_CHAINGUN);
    make_noise(p->x, p->y);
    angle_t a = p->angle;
    if (p->refire) a += (angle_t)((P_Random() - P_Random()) * (1 << 18));
    hitscan(p->x, p->y, a, itof(48), 5 * (1 + P_Random() % 3));
}

static void W_FireRocket(player_t *p)
{
    use_ammo(p);
    p->flash = 3;
    make_noise(p->x, p->y);
    spawn_missile(MT_ROCKET, p->x, p->y, p->angle, -1);
}

static void W_FirePistol(player_t *p)
{
    use_ammo(p);
    p->flash = 3;
    sound_local(SFX_PISTOL);
    make_noise(p->x, p->y);
    angle_t a = p->angle;
    if (p->refire) a += (angle_t)((P_Random() - P_Random()) * (1 << 18));
    hitscan(p->x, p->y, a, itof(48), 5 * (1 + P_Random() % 3));
}

static void W_FireShotgun(player_t *p)
{
    use_ammo(p);
    p->flash = 4;
    sound_local(SFX_SHOTGUN);
    make_noise(p->x, p->y);
    for (int i = 0; i < 7; i++)
        hitscan(p->x, p->y, p->angle + (angle_t)((P_Random() - P_Random()) * (1 << 19)), itof(48),
                5 * (1 + P_Random() % 3));
}

static void W_Cock(player_t *p)
{
    (void)p;
    sound_local(SFX_SGCOCK);
}

#define WEAPON_BOTTOM (FRACUNIT * 96)
#define WEAPON_SPEED (FRACUNIT * 8)

static void weapon_tick(player_t *p)
{
    bool switching = p->pendingweapon != p->readyweapon || p->dead;
    bool ready = sdef[p->wstate].waction == W_Ready;
    if (switching && ready) {
        p->wy += WEAPON_SPEED;
        if (p->wy >= WEAPON_BOTTOM) {
            p->wy = WEAPON_BOTTOM;
            if (p->dead) return;
            p->readyweapon = p->pendingweapon;
            set_wstate(p, weaponinfo[p->readyweapon].ready);
        }
        return;
    }
    if (p->wy > 0) {
        p->wy -= WEAPON_SPEED;
        if (p->wy < 0) p->wy = 0;
    }
    if (p->wtics > 0 && --p->wtics == 0) set_wstate(p, sdef[p->wstate].next);
}

/* ---------------------------------------------------------------- player */

void g_new_player(player_t *p)
{
    memset(p, 0, sizeof *p);
    p->health = 100;
    p->ammo[AM_BULLET] = 50;
    p->maxammo[AM_BULLET] = 200;
    p->maxammo[AM_SHELL] = 50;
    p->maxammo[AM_ROCKET] = 50;
    p->weaponowned = 1 << WP_KNIFE | 1 << WP_PISTOL;
    p->readyweapon = p->pendingweapon = WP_PISTOL;
    p->wstate = S_PISTOL;
}

static void use_line(player_t *p)
{
    fixed_t dx = fcos(p->angle), dy = fsin(p->angle);
    for (fixed_t d = FRACUNIT / 4; d <= FRACUNIT * 5 / 4; d += FRACUNIT / 16) {
        int tx = ftoi(p->x + fmul(dx, d)), ty = ftoi(p->y + fmul(dy, d));
        if (!in_map(tx, ty)) return;
        switch (g.tile[ty][tx]) {
        case TL_EMPTY: continue;
        case TL_DOOR: {
            door_t *dr = door_at(tx, ty);
            static const char *need[4] = {"", "YOU NEED THE RED KEY", "YOU NEED THE BLUE KEY", "YOU NEED THE YELLOW KEY"};
            if (dr->lock && !(p->keys & (1 << (dr->lock - 1)))) {
                message(need[dr->lock]);
                sound_local(SFX_NOWAY);
                return;
            }
            if (dr->state == 2 && !actor_in_tile(tx, ty)) {
                dr->state = 3;
                sound_at(SFX_DOOR_CLOSE, itof(tx) + FRACUNIT / 2, itof(ty) + FRACUNIT / 2);
            } else
                door_open(dr);
            return;
        }
        case TL_PUSH: {
            int ptx = ftoi(p->x), pty = ftoi(p->y), ddx = tx - ptx, ddy = ty - pty;
            /* push along the dominant axis away from the player */
            if (ddx && ddy) {
                if (fabs_(dx) > fabs_(dy)) ddy = 0;
                else ddx = 0;
            }
            push_start(tx, ty, ddx > 0 ? 1 : ddx < 0 ? -1 : 0, ddy > 0 ? 1 : ddy < 0 ? -1 : 0);
            return;
        }
        case TL_EXIT:
            if (!g.exiting) {
                g.tex[ty][tx] = WT_EXIT_ON;
                sound_local(SFX_SWITCH);
                g.exiting = 1;
            }
            return;
        default: sound_local(SFX_NOWAY); return;
        }
    }
}

static void give_ammo(player_t *p, int type, int n, bool *took)
{
    if (p->ammo[type] >= p->maxammo[type]) return;
    p->ammo[type] += n;
    if (p->ammo[type] > p->maxammo[type]) p->ammo[type] = p->maxammo[type];
    *took = true;
}

static void touch_item(player_t *p, actor_t *a)
{
    bool took = false, dropped = (a->flags & MF_DROPPED) != 0;
    int sfx = SFX_ITEM;
    const char *msg = "";
    switch (a->type) {
    case MT_STIM:
        if (p->health < 100) {
            p->health = p->health + 10 > 100 ? 100 : p->health + 10;
            took = true;
            msg = "PICKED UP A STIMPACK";
        }
        break;
    case MT_MEDKIT:
        if (p->health < 100) {
            p->health = p->health + 25 > 100 ? 100 : p->health + 25;
            took = true;
            msg = "PICKED UP A MEDKIT";
        }
        break;
    case MT_ARMOR:
        if (p->armor < 100) {
            p->armor = 100;
            took = true;
            msg = "PICKED UP ARMOR";
        }
        break;
    case MT_CLIP:
        give_ammo(p, AM_BULLET, dropped ? 5 : 10, &took);
        msg = "PICKED UP A CLIP";
        break;
    case MT_SHELLS:
        give_ammo(p, AM_SHELL, 4, &took);
        msg = "PICKED UP SHELLS";
        break;
    case MT_KEYR: case MT_KEYB: case MT_KEYY: {
        static const char *names[3] = {"PICKED UP THE RED KEY", "PICKED UP THE BLUE KEY", "PICKED UP THE YELLOW KEY"};
        int k = a->type - MT_KEYR;
        p->keys |= 1 << k;
        took = true;
        msg = names[k];
        sfx = SFX_WPNUP;
        break;
    }
    case MT_SHOTGUN: case MT_CHAINGUN: case MT_LAUNCHER: {
        static const struct { int mt, wp, ammo, n; const char *msg; } wp[] = {
            {MT_SHOTGUN, WP_SHOTGUN, AM_SHELL, 8, "YOU GOT THE SHOTGUN!"},
            {MT_CHAINGUN, WP_CHAINGUN, AM_BULLET, 20, "YOU GOT THE CHAINGUN!"},
            {MT_LAUNCHER, WP_LAUNCHER, AM_ROCKET, 2, "YOU GOT THE ROCKET LAUNCHER!"},
        };
        int i = a->type == MT_SHOTGUN ? 0 : a->type == MT_CHAINGUN ? 1 : 2;
        give_ammo(p, wp[i].ammo, wp[i].n, &took);
        if (!(p->weaponowned & (1 << wp[i].wp))) {
            p->weaponowned |= 1 << wp[i].wp;
            p->pendingweapon = wp[i].wp;
            took = true;
        }
        msg = wp[i].msg;
        sfx = SFX_WPNUP;
        break;
    }
    case MT_ROCKETAMMO:
        give_ammo(p, AM_ROCKET, 1, &took);
        msg = "PICKED UP A ROCKET";
        break;
    case MT_ROCKETBOX:
        give_ammo(p, AM_ROCKET, 5, &took);
        msg = "PICKED UP A CRATE OF ROCKETS";
        break;
    case MT_BULLETBOX:
        give_ammo(p, AM_BULLET, dropped ? 25 : 50, &took);
        msg = "PICKED UP A BOX OF BULLETS";
        break;
    default: return;
    }
    if (!took) return;
    if (a->flags & MF_COUNTITEM) g.items++;
    a->inuse = 0;
    p->bonuscount = 6;
    message(msg);
    sound_local(sfx);
}

static void player_tick(const ticcmd_t *cmd)
{
    player_t *p = &g.player;
    if (p->damagecount) p->damagecount--;
    if (p->bonuscount) p->bonuscount--;
    if (p->flash) p->flash--;

    if (p->dead) {
        p->deadtics++;
        p->momx = fmul(p->momx, 0xC000);
        p->momy = fmul(p->momy, 0xC000);
        if ((cmd->buttons & BT_USE) && p->deadtics > TICRATE) g.exiting = -1; /* restart level */
        weapon_tick(p);
        return;
    }

    p->angle += (angle_t)cmd->turn << 16;
    const fixed_t k = 20;
    fixed_t c = fcos(p->angle), s = fsin(p->angle);
    p->momx += fmul(c, cmd->forward * k) - fmul(s, cmd->side * k);
    p->momy += fmul(s, cmd->forward * k) + fmul(c, cmd->side * k);
    const fixed_t maxmom = FRACUNIT / 4;
    if (p->momx > maxmom) p->momx = maxmom;
    if (p->momx < -maxmom) p->momx = -maxmom;
    if (p->momy > maxmom) p->momy = maxmom;
    if (p->momy < -maxmom) p->momy = -maxmom;

    fixed_t nx = p->x + p->momx, ny = p->y + p->momy;
    if (try_move(NULL, nx, ny, PLAYER_RADIUS)) {
        p->x = nx;
        p->y = ny;
    } else if (try_move(NULL, nx, p->y, PLAYER_RADIUS)) {
        p->x = nx;
        p->momy = 0;
    } else if (try_move(NULL, p->x, ny, PLAYER_RADIUS)) {
        p->y = ny;
        p->momx = 0;
    } else
        p->momx = p->momy = 0;

    p->momx = fmul(p->momx, 0xE800);
    p->momy = fmul(p->momy, 0xE800);
    if (fabs_(p->momx) < 64) p->momx = 0;
    if (fabs_(p->momy) < 64) p->momy = 0;
    p->bob = fhypot(p->momx, p->momy);

    if (cmd->buttons & BT_USE) {
        if (!p->usedown) use_line(p);
        p->usedown = 1;
    } else
        p->usedown = 0;
    p->attackdown = (cmd->buttons & BT_ATTACK) != 0;

    if (cmd->weapon >= 1 && cmd->weapon <= WP_COUNT && (p->weaponowned & (1 << (cmd->weapon - 1))))
        p->pendingweapon = cmd->weapon - 1;
    weapon_tick(p);

    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *a = &g.actors[i];
        if (a->inuse && (a->flags & MF_PICKUP) &&
            boxes_overlap(p->x, p->y, PLAYER_RADIUS, a->x, a->y, radius_of(a)))
            touch_item(p, a);
    }
}

/* ---------------------------------------------------------------- ticker */

void g_ticker(const ticcmd_t *cmd)
{
    g.nsnd = 0;
    if (g.msgtics) g.msgtics--;
    player_tick(cmd);
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *a = &g.actors[i];
        if (!a->inuse) continue;
        if (sdef[a->state].think) sdef[a->state].think(a);
        if (!a->inuse) continue;
        if (a->tics > 0 && --a->tics == 0) set_state(a, sdef[a->state].next);
    }
    doors_tick();
    push_tick();
    g.leveltime++;
    if (g.exiting > 0) g.exiting++;
}

uint32_t g_checksum(void)
{
    uint32_t h = 2166136261u;
#define MIX(v) (h = (h ^ (uint32_t)(v)) * 16777619u)
    const player_t *p = &g.player;
    MIX(p->x); MIX(p->y); MIX(p->momx); MIX(p->momy); MIX(p->angle); MIX(p->health); MIX(p->armor);
    for (int i = 0; i < AM_COUNT; i++) MIX(p->ammo[i]);
    MIX(p->weaponowned); MIX(p->readyweapon); MIX(p->wstate); MIX(p->wy); MIX(p->keys); MIX(p->dead);
    for (int i = 0; i < MAXACTORS; i++) {
        const actor_t *a = &g.actors[i];
        if (!a->inuse) continue;
        MIX(i); MIX(a->type); MIX(a->state); MIX(a->tics); MIX(a->x); MIX(a->y); MIX(a->health); MIX(a->flags);
        MIX(a->movedir); MIX(a->angle);
    }
    for (int i = 0; i < g.ndoors; i++) { MIX(g.doors[i].open); MIX(g.doors[i].state); }
    MIX(g.push.pos); MIX(g.rng); MIX(g.leveltime); MIX(g.kills); MIX(g.items); MIX(g.secrets);
    for (int y = 0; y < g.h; y++)
        for (int x = 0; x < g.w; x++) MIX(g.tile[y][x]);
#undef MIX
    return h;
}

/* ---------------------------------------------------------------- level loading */

static int lookup(const char *v, const char *const *names, int n)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(v, names[i])) return i;
    return 0;
}

bool g_load_level(const char *text, int skill, int levelnum, const player_t *carry)
{
    static const char *floors[FT_COUNT] = {"stone", "dirt", "metal", "panel", "rock"};
    memset(&g, 0, sizeof g);
    g.skill = skill;
    g.levelnum = levelnum;
    g.rng = 0x1234567u + (uint32_t)levelnum * 7919u;
    if (carry) {
        g.player = *carry;
        g.player.keys = 0;
    } else
        g_new_player(&g.player);
    g.player.momx = g.player.momy = 0;
    g.player.damagecount = g.player.bonuscount = g.player.flash = 0;
    g.player.wstate = (int16_t)weaponinfo[g.player.readyweapon].ready;
    g.player.wtics = 1;
    g.player.pendingweapon = g.player.readyweapon;
    g.player.wy = 0;
    g.ceiltex = FT_CEIL_PANEL;
    memset(g.doorat, -1, sizeof g.doorat);

    /* header lines, then "map:" rows, then an optional "skill:" overlay of the same shape where
     * 1 = easy only, 2 = medium and hard, 3 = hard only (applies to the thing in that cell) */
    static char rows[MAPH][MAPW + 1], skillrows[MAPH][MAPW + 1];
    memset(rows, 0, sizeof rows);
    memset(skillrows, 0, sizeof skillrows);
    const char *s = text;
    char line[256];
    int row = 0, srow = 0, section = 0, facing = 0;
    while (*s) {
        int n = 0;
        while (*s && *s != '\n') {
            if (n < 255 && *s != '\r') line[n++] = *s;
            s++;
        }
        if (*s) s++;
        line[n] = 0;
        if (!strcmp(line, "map:")) {
            section = 1;
            continue;
        }
        if (!strcmp(line, "skill:")) {
            section = 2;
            continue;
        }
        if (section == 1 && row < MAPH) {
            snprintf(rows[row++], MAPW + 1, "%.64s", line);
            continue;
        }
        if (section == 2 && srow < MAPH) {
            snprintf(skillrows[srow++], MAPW + 1, "%.64s", line);
            continue;
        }
        char key[32], val[64];
        if (line[0] == ';' || sscanf(line, "%31[^:]: %63[^\n]", key, val) != 2) continue;
        if (!strcmp(key, "name")) snprintf(g.levelname, sizeof g.levelname, "%.31s", val);
        else if (!strcmp(key, "music")) snprintf(g.music, sizeof g.music, "%.31s", val);
        else if (!strcmp(key, "floor")) g.floortex = lookup(val, floors, FT_COUNT);
        else if (!strcmp(key, "ceil")) g.ceiltex = lookup(val, floors, FT_COUNT);
        else if (!strcmp(key, "ambient")) g.ambient = atoi(val);
        else if (!strcmp(key, "par")) g.par = atoi(val);
        else if (!strcmp(key, "facing")) facing = atoi(val);
    }
    for (int y = 0; y < row; y++) {
        int n = (int)strlen(rows[y]);
        if (n > g.w) g.w = n;
        for (int x = 0; x < n; x++) {
            char ch = rows[y][x], sk = skillrows[y][x];
            fixed_t cx = itof(x) + FRACUNIT / 2, cy = itof(y) + FRACUNIT / 2;
            static const char wallch[] = "#WMBGT";
            const char *w = ch ? strchr(wallch, ch) : NULL;
            if (w) {
                g.tile[y][x] = TL_WALL;
                g.tex[y][x] = (uint8_t)(w - wallch);
                continue;
            }
            switch (ch) {
            case 'X': g.tile[y][x] = TL_EXIT; g.tex[y][x] = WT_EXIT; break;
            case '$': g.tile[y][x] = TL_PUSH; g.totalsecrets++; break;
            case 'D': case 'R': case 'U': case 'Y':
                if (g.ndoors < MAXDOORS) {
                    g.tile[y][x] = TL_DOOR;
                    g.doorat[y][x] = (int8_t)g.ndoors;
                    door_t *d = &g.doors[g.ndoors++];
                    d->tx = (uint8_t)x;
                    d->ty = (uint8_t)y;
                    d->lock = ch == 'R' ? 1 : ch == 'U' ? 2 : ch == 'Y' ? 3 : 0;
                }
                break;
            case 'P': g.player.x = cx; g.player.y = cy; break;
            default: {
                static const char thingch[] = "ghHacs123SlobdfkCLrqmw";
                static const int thingmt[] = {MT_GRUNT, MT_STIM, MT_MEDKIT, MT_ARMOR, MT_CLIP, MT_SHELLS,
                                              MT_KEYR, MT_KEYB, MT_KEYY, MT_SHOTGUN, MT_LAMP, MT_PILLAR, MT_BARREL,
                                              MT_HOUND, MT_SPITTER, MT_BRUTE, MT_CHAINGUN, MT_LAUNCHER,
                                              MT_ROCKETAMMO, MT_ROCKETBOX, MT_BULLETBOX, MT_WARDEN};
                const char *t = ch ? strchr(thingch, ch) : NULL;
                if (!t) break;
                if ((sk == '1' && skill != SK_EASY) || (sk == '2' && skill == SK_EASY) || (sk == '3' && skill != SK_HARD))
                    break;
                actor_t *a = spawn(thingmt[t - thingch], cx, cy);
                if (!a) break;
                if (a->flags & MF_COUNTKILL) g.totalkills++;
                if (a->flags & MF_COUNTITEM) g.totalitems++;
            }
            }
        }
    }
    g.h = row;
    if (!g.w || !g.h || !g.player.x) {
        message("BAD LEVEL: NO MAP OR NO PLAYER START");
        return false;
    }
    g.player.angle = (angle_t)facing * ANG90;
    /* door orientation from neighbours; pushwall texture from a neighbouring wall */
    for (int i = 0; i < g.ndoors; i++) {
        door_t *d = &g.doors[i];
        d->vertical = !(tile_blocks_move(d->tx - 1, d->ty) && tile_blocks_move(d->tx + 1, d->ty));
    }
    for (int y = 0; y < g.h; y++)
        for (int x = 0; x < g.w; x++)
            if (g.tile[y][x] == TL_PUSH) {
                static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++)
                    if (in_map(x + dx[k], y + dy[k]) && g.tile[y + dy[k]][x + dx[k]] == TL_WALL) {
                        g.tex[y][x] = g.tex[y + dy[k]][x + dx[k]];
                        break;
                    }
            }
    /* enemies start facing the player start */
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *a = &g.actors[i];
        if (a->inuse && (a->flags & MF_COUNTKILL)) a->angle = fatan2(g.player.y - a->y, g.player.x - a->x);
    }
    return true;
}
