#include "fixed.h"

fixed_t finesine[FINEANGLES * 5 / 4];
fixed_t *finecosine = &finesine[FINEANGLES / 4];

/* Quarter-wave sine via integer Taylor series in Q30: identical on every compiler. */
void fixed_init(void)
{
    const int64_t one = (int64_t)1 << 30;
    const int64_t half_pi = 1686629713; /* pi/2 in Q30 */
    fixed_t q[FINEANGLES / 4 + 1];
    for (int i = 0; i <= FINEANGLES / 4; i++) {
        int64_t x = half_pi * i / (FINEANGLES / 4);
        int64_t x2 = x * x / one;
        int64_t t = x, sum = x;
        for (int n = 1; n <= 6; n++) {
            t = t * x2 / one / ((2 * n) * (2 * n + 1));
            sum += (n & 1) ? -t : t;
        }
        q[i] = (fixed_t)((sum + (1 << 13)) / (1 << 14));
    }
    for (int i = 0; i < FINEANGLES * 5 / 4; i++) {
        int a = i & FINEMASK, quad = a / (FINEANGLES / 4), r = a % (FINEANGLES / 4);
        fixed_t v = (quad & 1) ? q[FINEANGLES / 4 - r] : q[r];
        finesine[i] = quad >= 2 ? -v : v;
    }
}

uint32_t isqrt64(uint64_t v)
{
    uint64_t res = 0, bit = (uint64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= res + bit) {
            v -= res + bit;
            res = (res >> 1) + bit;
        } else
            res >>= 1;
        bit >>= 2;
    }
    return (uint32_t)res;
}

fixed_t fhypot(fixed_t dx, fixed_t dy)
{
    uint64_t x = (uint64_t)fabs_(dx), y = (uint64_t)fabs_(dy);
    return (fixed_t)isqrt64(x * x + y * y);
}

angle_t fatan2(fixed_t dy, fixed_t dx)
{
    int64_t ax = fabs_(dx), ay = fabs_(dy);
    if (ax == 0 && ay == 0) return 0;
    int swap = ay > ax;
    if (swap) {
        int64_t t = ax;
        ax = ay;
        ay = t;
    }
    /* largest k in [0, 512] with tan(k) <= ay/ax */
    int lo = 0, hi = FINEANGLES / 8;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if ((int64_t)finesine[mid] * ax <= (int64_t)finecosine[mid] * ay)
            lo = mid;
        else
            hi = mid - 1;
    }
    angle_t a = (angle_t)lo << ANGLETOFINESHIFT;
    if (swap) a = ANG90 - a;
    if (dx < 0) a = ANG180 - a;
    if (dy < 0) a = 0u - a;
    return a;
}
