/* 16.16 fixed point, binary angles, trig tables, deterministic RNG. */
#ifndef FIXED_H
#define FIXED_H

#include <stdint.h>

typedef int32_t fixed_t;
typedef uint32_t angle_t; /* full circle = 2^32 */

#define FRACBITS 16
#define FRACUNIT (1 << FRACBITS)
#define FINEANGLES 4096
#define FINEMASK (FINEANGLES - 1)
#define ANGLETOFINESHIFT 20
#define ANG45 0x20000000u
#define ANG90 0x40000000u
#define ANG180 0x80000000u
#define ANG270 0xC0000000u

extern fixed_t finesine[FINEANGLES * 5 / 4];
extern fixed_t *finecosine;

void fixed_init(void);

static inline fixed_t fmul(fixed_t a, fixed_t b) { return (fixed_t)(((int64_t)a * b) / FRACUNIT); }

static inline fixed_t fdiv(fixed_t a, fixed_t b)
{
    if (b == 0) return a < 0 ? -INT32_MAX : INT32_MAX;
    int64_t r = ((int64_t)a * FRACUNIT) / b;
    if (r > INT32_MAX) return INT32_MAX;
    if (r < -INT32_MAX) return -INT32_MAX;
    return (fixed_t)r;
}

static inline fixed_t itof(int i) { return (fixed_t)(i * FRACUNIT); }

/* floor to integer, correct for negatives without shifting them */
static inline int ftoi(fixed_t f) { return f >= 0 ? f / FRACUNIT : -((-f + FRACUNIT - 1) / FRACUNIT); }

static inline fixed_t fabs_(fixed_t f) { return f < 0 ? -f : f; }

static inline fixed_t fsin(angle_t a) { return finesine[a >> ANGLETOFINESHIFT]; }
static inline fixed_t fcos(angle_t a) { return finecosine[a >> ANGLETOFINESHIFT]; }

uint32_t isqrt64(uint64_t v);
/* length of (dx,dy) in fixed */
fixed_t fhypot(fixed_t dx, fixed_t dy);
/* angle of vector (dx,dy) */
angle_t fatan2(fixed_t dy, fixed_t dx);

/* Deterministic game RNG (LCG). State lives in game state. */
static inline int rng_next(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return (int)(*s >> 24); /* 0..255 */
}

#endif
