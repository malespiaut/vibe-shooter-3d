#include "demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int demo_parse(const char *text, int *skill, int *level, ticcmd_t *out, int max)
{
    int n = 0, used;
    if (sscanf(text, "ironhold-demo 1 skill %d level %d%n", skill, level, &used) != 2) return -1;
    text += used;
    int f, s, t, b, w;
    while (n < max && sscanf(text, "%d %d %d %d %d%n", &f, &s, &t, &b, &w, &used) == 5) {
        out[n++] = (ticcmd_t){(int8_t)f, (int8_t)s, (int16_t)t, (uint8_t)b, (uint8_t)w};
        text += used;
    }
    return n;
}

char *demo_format(int skill, int level, const ticcmd_t *cmds, int n, long *len)
{
    size_t cap = 64 + (size_t)n * 32;
    char *buf = malloc(cap);
    long l = snprintf(buf, cap, "ironhold-demo 1\nskill %d\nlevel %d\n", skill, level);
    for (int i = 0; i < n; i++)
        l += snprintf(buf + l, cap - (size_t)l, "%d %d %d %d %d\n", cmds[i].forward, cmds[i].side, cmds[i].turn,
                      cmds[i].buttons, cmds[i].weapon);
    *len = l;
    return buf;
}
