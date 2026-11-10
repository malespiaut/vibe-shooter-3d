/* Level validator: every level must be closed, consistent, and finishable.
 *   levelcheck FILE...   exit status 1 if any level has an error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

static int check(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("%s: cannot open\n", path);
        return 1;
    }
    static char text[65536];
    text[fread(text, 1, sizeof text - 1, f)] = 0;
    fclose(f);
    if (!g_load_level(text, SK_HARD, 0, NULL)) {
        printf("%s: %s\n", path, g.msg);
        return 1;
    }
    int errors = 0;
    for (int y = 0; y < g.h; y++)
        for (int x = 0; x < g.w; x++) {
            int edge = x == 0 || y == 0 || x == g.w - 1 || y == g.h - 1;
            if (edge && (g.tile[y][x] == TL_EMPTY || g.tile[y][x] == TL_DOOR)) {
                printf("%s: open map edge at %d,%d\n", path, x, y);
                errors++;
            }
            if (g.tile[y][x] == TL_DOOR) {
                int we = g.tile[y][x - 1] && g.tile[y][x - 1] != TL_DOOR && g.tile[y][x + 1] && g.tile[y][x + 1] != TL_DOOR;
                int ns = g.tile[y - 1][x] && g.tile[y - 1][x] != TL_DOOR && g.tile[y + 1][x] && g.tile[y + 1][x] != TL_DOOR;
                if (we == ns) {
                    printf("%s: door at %d,%d needs walls on exactly one axis\n", path, x, y);
                    errors++;
                }
            }
        }
    /* pillars are permanent and solid: they count as walls for reachability */
    static uint8_t pillar[MAPH][MAPW];
    memset(pillar, 0, sizeof pillar);
    for (int i = 0; i < MAXACTORS; i++)
        if (g.actors[i].inuse && g.actors[i].type == MT_PILLAR) pillar[g.actors[i].y / FRACUNIT][g.actors[i].x / FRACUNIT] = 1;
    /* flood fill from the start, picking up keys and reopening the search when one unlocks doors */
    static uint8_t seen[MAPH][MAPW];
    int keys = 0, exit_ok = 0, changed = 1;
    while (changed) {
        changed = 0;
        memset(seen, 0, sizeof seen);
        static int q[MAPW * MAPH];
        int head = 0, tail = 0, sx = g.player.x / FRACUNIT, sy = g.player.y / FRACUNIT;
        seen[sy][sx] = 1;
        q[tail++] = sy * MAPW + sx;
        while (head < tail) {
            int t = q[head++], tx = t % MAPW, ty = t / MAPW;
            static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                int nx = tx + dx[k], ny = ty + dy[k];
                if (nx < 0 || ny < 0 || nx >= g.w || ny >= g.h || seen[ny][nx] || pillar[ny][nx]) continue;
                int tl = g.tile[ny][nx];
                if (tl == TL_EXIT) exit_ok = 1;
                if (tl == TL_DOOR) {
                    int lock = g.doors[g.doorat[ny][nx]].lock;
                    if (lock && !(keys & (1 << (lock - 1)))) continue;
                } else if (tl != TL_EMPTY && tl != TL_PUSH)
                    continue;
                seen[ny][nx] = 1;
                q[tail++] = ny * MAPW + nx;
            }
        }
        for (int i = 0; i < MAXACTORS; i++) {
            actor_t *a = &g.actors[i];
            if (!a->inuse || a->type < MT_KEYR || a->type > MT_KEYY) continue;
            int bit = 1 << (a->type - MT_KEYR);
            if (seen[a->y / FRACUNIT][a->x / FRACUNIT] && !(keys & bit)) {
                keys |= bit;
                changed = 1;
            }
        }
    }
    int boss = 0; /* a boss level ends when the boss dies, so it needs no exit switch */
    for (int i = 0; i < MAXACTORS; i++) boss |= g.actors[i].inuse && g.actors[i].type == MT_WARDEN;
    if (!exit_ok && !boss) {
        printf("%s: exit switch not reachable\n", path);
        errors++;
    }
    int unreachable = 0;
    for (int i = 0; i < MAXACTORS; i++) {
        actor_t *a = &g.actors[i];
        if (a->inuse && a->type != MT_PILLAR && !seen[a->y / FRACUNIT][a->x / FRACUNIT]) {
            printf("%s: thing type %d at %d,%d is unreachable\n", path, a->type, a->x / FRACUNIT, a->y / FRACUNIT);
            unreachable++;
        }
    }
    errors += unreachable;
    for (int i = 0; i < g.ndoors; i++)
        if (g.doors[i].lock && !(keys & (1 << (g.doors[i].lock - 1)))) {
            printf("%s: locked door at %d,%d has no reachable key\n", path, g.doors[i].tx, g.doors[i].ty);
            errors++;
        }
    printf("%s: %-24s %dx%d  kills %d  items %d  secrets %d  doors %d  %s\n", path, g.levelname, g.w, g.h,
           g.totalkills, g.totalitems, g.totalsecrets, g.ndoors, errors ? "FAIL" : "ok");
    return errors != 0;
}

int main(int argc, char **argv)
{
    fixed_init();
    int bad = 0;
    for (int i = 1; i < argc; i++) bad |= check(argv[i]);
    return bad;
}
