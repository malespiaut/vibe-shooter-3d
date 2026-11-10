/* Demos: a skill, a level and one ticcmd per tic, stored as text. */
#ifndef DEMO_H
#define DEMO_H

#include "game.h"

/* Returns the number of tics parsed (up to max), or -1 if the header is bad. */
int demo_parse(const char *text, int *skill, int *level, ticcmd_t *out, int max);
/* Returns a malloc'd text; *len gets its length. */
char *demo_format(int skill, int level, const ticcmd_t *cmds, int n, long *len);

#endif
