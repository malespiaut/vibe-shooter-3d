/* Headless determinism test: runs the sim on scripted input and compares a state checksum. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"
#include "game.h"

#define EXPECTED 0x5bce9303u /* update with: demo_test --print */

static char *read_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    if (fread(s, 1, (size_t)n, f) != (size_t)n) n = 0;
    s[n] = 0;
    fclose(f);
    return s;
}

static int check_math(void)
{
    int bad = 0;
    bad |= fsin(ANG90) != FRACUNIT;
    bad |= fcos(0) != FRACUNIT;
    bad |= fsin(ANG180) != 0;
    bad |= fhypot(itof(3), itof(4)) != itof(5);
    bad |= fatan2(itof(1), itof(1)) != ANG45;
    bad |= fatan2(0, itof(-1)) != ANG180;
    bad |= fatan2(itof(-1), 0) != ANG270;
    bad |= ftoi(-FRACUNIT / 2) != -1;
    if (bad) printf("fixed math self-check FAILED\n");
    return bad;
}

int main(int argc, char **argv)
{
    int print = argc > 1 && !strcmp(argv[1], "--print");
    fixed_init();
    if (check_math()) return 1;
    static ticcmd_t cmds[TICRATE * 60 * 60];
    int skill, level;
    char *demo = read_text("assets/demos/demo1.txt");
    int n = demo ? demo_parse(demo, &skill, &level, cmds, (int)(sizeof cmds / sizeof cmds[0])) : -1;
    if (n <= 0) {
        printf("cannot read demo\n");
        return 1;
    }
    char path[64];
    snprintf(path, sizeof path, "assets/levels/e1m%d.txt", level + 1);
    char *text = read_text(path);
    if (!text || !g_load_level(text, skill, level, NULL)) {
        printf("cannot load %s\n", path);
        return 1;
    }
    player_t carry = g.player;
    uint32_t hash = 0;
    for (int tic = 0; tic < n && g.exiting <= TICRATE; tic++) {
        g_ticker(&cmds[tic]);
        if (g.exiting < 0) { /* same death-restart rule as main.c */
            g_load_level(text, skill, level, &carry);
            carry = g.player;
        }
        hash = hash * 31u + g_checksum();
    }
    printf("checksum %08x  (%d tics, kills %d/%d, items %d/%d, secrets %d/%d, health %d)\n", hash, n, g.kills,
           g.totalkills, g.items, g.totalitems, g.secrets, g.totalsecrets, g.player.health);
    free(text);
    free(demo);
    if (print) return 0;
    if (hash != EXPECTED) {
        printf("MISMATCH: expected %08x\n", EXPECTED);
        return 1;
    }
    return 0;
}
