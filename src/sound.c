/* Sound: effects synthesized at startup, our own mixer, and a 4-channel tracker-style music synth. */
#include "sound.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "platform.h"

/* ---------------------------------------------------------------- effect synthesis */

typedef struct {
    int16_t *data;
    int len;
} sample_t;

static sample_t sfx[SFX_COUNT];
static uint32_t nseed = 1;

static float noise(void)
{
    nseed = nseed * 1664525u + 1013904223u;
    return (float)(nseed >> 8) / 8388608.0f - 1.0f;
}

typedef struct { float b0, b1, b2, a1, a2, x1, x2, y1, y2; } biquad;

static biquad bandpass(float f, float q)
{
    float w = 6.2831853f * f / MIXRATE, al = sinf(w) / (2 * q), a0 = 1 + al;
    return (biquad){al / a0, 0, -al / a0, -2 * cosf(w) / a0, (1 - al) / a0, 0, 0, 0, 0};
}

static float bq(biquad *b, float x)
{
    float y = b->b0 * x + b->b1 * b->x1 + b->b2 * b->x2 - b->a1 * b->y1 - b->a2 * b->y2;
    b->x2 = b->x1;
    b->x1 = x;
    b->y2 = b->y1;
    b->y1 = y;
    return y;
}

static void finish(int id, float *buf, int n, float gain)
{
    float peak = 1e-6f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(buf[i]));
    sfx[id].data = malloc((size_t)n * sizeof(int16_t));
    sfx[id].len = n;
    for (int i = 0; i < n; i++) {
        float fade = i > n - 64 ? (n - i) / 64.0f : 1; /* no click at the end */
        sfx[id].data[i] = (int16_t)(buf[i] / peak * gain * fade * 32000);
    }
    free(buf);
}

static float *alloc_buf(float secs, int *n)
{
    *n = (int)(secs * MIXRATE);
    return calloc((size_t)*n, sizeof(float));
}

/* gunshot: filtered noise crack + low thump */
static void gunshot(int id, float secs, float decay, float bright, float thump_hz, float thump_amt, float gain)
{
    int n;
    float *b = alloc_buf(secs, &n), lp = 0, ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / MIXRATE;
        float a = 1 - expf(-6.2831853f * (bright * expf(-t * 12) + 300) / MIXRATE);
        lp += a * (noise() - lp);
        float f = thump_hz * (0.35f + 0.65f * expf(-t * 25));
        ph += f / MIXRATE;
        b[i] = lp * expf(-t * decay) * 1.4f + sinf(ph * 6.2831853f) * expf(-t * decay * 0.8f) * thump_amt;
    }
    finish(id, b, n, gain);
}

/* voiced grunt: sawtooth through two formant filters with a pitch glide */
static void voice(int id, float secs, float f0a, float f0b, float fm1, float fm2, float breath, float vib, float gain)
{
    int n;
    float *b = alloc_buf(secs, &n), ph = 0;
    biquad f1 = bandpass(fm1, 4), f2 = bandpass(fm2, 6);
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float f0 = f0a + (f0b - f0a) * t + sinf(i * 6.2831853f * 7 / MIXRATE) * vib;
        ph += f0 / MIXRATE;
        float saw = 2 * (ph - floorf(ph)) - 1 + noise() * breath;
        float env = fminf(1, t * 20) * (1 - t) * (1 - t * 0.3f);
        b[i] = (bq(&f1, saw) * 1.2f + bq(&f2, saw) * 0.7f) * env;
    }
    finish(id, b, n, gain);
}

static void tones(int id, const float *freqs, int count, float each, float gain)
{
    int n;
    float *b = alloc_buf(each * count, &n), ph = 0;
    for (int i = 0; i < n; i++) {
        int k = i / (int)(each * MIXRATE);
        float t = (float)(i % (int)(each * MIXRATE)) / (each * MIXRATE);
        ph += freqs[k < count ? k : count - 1] / MIXRATE;
        b[i] = ((ph - floorf(ph)) < 0.5f ? 0.6f : -0.6f) * (1 - t * 0.7f);
    }
    finish(id, b, n, gain);
}

/* machinery: low hum + rumble, with an optional clunk at the end */
static void machine(int id, float secs, float hum, float clunk, float gain)
{
    int n;
    float *b = alloc_buf(secs, &n), lp = 0, ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / n, ts = (float)i / MIXRATE;
        lp += 0.04f * (noise() - lp);
        ph += hum / MIXRATE;
        float env = fminf(1, t * 8) * fminf(1, (1 - t) * 6);
        float v = (lp * 3 + (2 * (ph - floorf(ph)) - 1) * 0.25f) * env * (0.8f + 0.2f * sinf(ts * 50));
        float tc = ts - secs * 0.85f;
        if (clunk > 0 && tc > 0) v += sinf(tc * 6.2831853f * 70) * expf(-tc * 30) * clunk + noise() * expf(-tc * 60) * clunk * 0.5f;
        b[i] = v;
    }
    finish(id, b, n, gain);
}

static void make_sfx(void)
{
    gunshot(SFX_PISTOL, 0.4f, 11, 7000, 140, 1.0f, 1.0f);
    gunshot(SFX_SHOTGUN, 0.8f, 5, 5000, 90, 1.4f, 1.0f);
    gunshot(SFX_ENEMYFIRE, 0.3f, 18, 9000, 200, 0.5f, 0.7f);
    {
        int n;
        float *b = alloc_buf(0.32f, &n);
        for (int i = 0; i < n; i++) {
            float t = (float)i / MIXRATE, t2 = t - 0.16f;
            float v = noise() * expf(-t * 300) + sinf(t * 6.2831853f * 2300) * expf(-t * 80) * 0.4f;
            if (t2 > 0) v += noise() * expf(-t2 * 250) + sinf(t2 * 6.2831853f * 1800) * expf(-t2 * 70) * 0.4f;
            b[i] = v;
        }
        finish(SFX_SGCOCK, b, n, 0.5f);
    }
    voice(SFX_GRUNT_SEE, 0.42f, 190, 140, 700, 1150, 0.25f, 4, 0.8f);
    voice(SFX_GRUNT_PAIN, 0.25f, 260, 180, 800, 1300, 0.5f, 10, 0.8f);
    voice(SFX_GRUNT_DIE, 0.9f, 210, 70, 650, 1050, 0.4f, 12, 0.85f);
    voice(SFX_PLAYER_PAIN, 0.28f, 150, 105, 600, 1000, 0.4f, 6, 0.8f);
    voice(SFX_PLAYER_DIE, 1.3f, 170, 45, 550, 950, 0.5f, 9, 0.9f);
    static const float item[] = {880, 1320}, wpn[] = {523, 659, 784, 1046}, sw[] = {440, 660};
    tones(SFX_ITEM, item, 2, 0.06f, 0.3f);
    tones(SFX_WPNUP, wpn, 4, 0.07f, 0.3f);
    tones(SFX_SWITCH, sw, 2, 0.1f, 0.3f);
    machine(SFX_DOOR_OPEN, 0.55f, 55, 0, 0.6f);
    machine(SFX_DOOR_CLOSE, 0.55f, 50, 1.2f, 0.7f);
    machine(SFX_PUSHWALL, 1.6f, 35, 0.8f, 0.7f);
    gunshot(SFX_NOWAY, 0.18f, 30, 600, 80, 1.2f, 0.55f);
}

/* ---------------------------------------------------------------- music */

enum { MCH = 4, STEPS = 16, MAXPAT = 16, MAXORDER = 64 };
typedef struct { int8_t note[MCH][STEPS]; } pattern_t; /* -1 nothing, -2 note off, else midi note / drum id */

static pattern_t pats[MAXPAT];
static int order[MAXORDER], norder, songpos, songstep, steplen, stepctr;
static int playing;

typedef struct { float ph, freq, env, lp; int on; float t; int drum; } voice_t;
static voice_t mv[MCH];

static int parse_note(const char *tok, int ch)
{
    if (tok[0] == '.') return -1;
    if (tok[0] == '-') return -2;
    if (ch == 3) return tok[0]; /* drums: k s h o */
    static const int semis[7] = {9, 11, 0, 2, 4, 5, 7}; /* a b c d e f g */
    if (tok[0] < 'a' || tok[0] > 'g') return -1;
    int n = semis[tok[0] - 'a'], i = 1;
    if (tok[i] == '#') {
        n++;
        i++;
    }
    return (int8_t)(12 * (tok[i] - '0' + 1) + n);
}

void snd_music(const char *text)
{
    plat_audio_lock();
    playing = 0;
    norder = songpos = songstep = stepctr = 0;
    memset(mv, 0, sizeof mv);
    memset(pats, -1, sizeof pats);
    int bpm = 120, cur = -1;
    static const char *chname[MCH] = {"bass:", "lead:", "pad:", "drum:"};
    const char *s = text;
    while (s && *s) {
        char line[512];
        int n = 0;
        while (*s && *s != '\n') {
            if (n < 511) line[n++] = *s;
            s++;
        }
        if (*s) s++;
        line[n] = 0;
        if (!strncmp(line, "tempo:", 6)) bpm = atoi(line + 6);
        else if (!strncmp(line, "pattern ", 8)) cur = line[8] - 'A';
        else if (!strncmp(line, "order:", 6)) {
            for (char *p = line + 6; *p && norder < MAXORDER; p++)
                if (*p >= 'A' && *p < 'A' + MAXPAT) order[norder++] = *p - 'A';
        } else if (cur >= 0 && cur < MAXPAT)
            for (int c = 0; c < MCH; c++) {
                size_t l = strlen(chname[c]);
                if (strncmp(line, chname[c], l)) continue;
                char *p = line + l;
                for (int st = 0; st < STEPS; st++) {
                    while (*p == ' ') p++;
                    if (!*p) break;
                    pats[cur].note[c][st] = (int8_t)parse_note(p, c);
                    while (*p && *p != ' ') p++;
                }
            }
    }
    steplen = MIXRATE * 60 / (bpm * 4);
    playing = norder > 0;
    plat_audio_unlock();
}

static float midi_hz(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

static void music_step(void)
{
    const pattern_t *p = &pats[order[songpos]];
    for (int c = 0; c < MCH; c++) {
        int n = p->note[c][songstep];
        if (n == -1) continue;
        voice_t *v = &mv[c];
        if (n == -2) {
            v->on = 0;
            continue;
        }
        v->on = 1;
        v->env = 1;
        v->t = 0;
        if (c == 3) v->drum = n;
        else v->freq = midi_hz(n);
        if (c == 3) v->ph = 0;
    }
    if (++songstep == STEPS) {
        songstep = 0;
        if (++songpos >= norder) songpos = 0;
    }
}

static float music_sample(void)
{
    if (!playing) return 0;
    if (stepctr-- <= 0) {
        stepctr = steplen;
        music_step();
    }
    const float dt = 1.0f / MIXRATE;
    float out = 0;
    for (int c = 0; c < MCH; c++) {
        voice_t *v = &mv[c];
        if (v->env < 0.001f) continue;
        v->t += dt;
        float s = 0;
        if (c == 3) {
            switch (v->drum) {
            case 'k':
                v->ph += (45 + 120 * expf(-v->t * 30)) * dt;
                s = sinf(v->ph * 6.2831853f) * 1.2f;
                v->env *= 0.9994f;
                break;
            case 's':
                s = noise() * 0.6f + sinf(v->t * 6.2831853f * 190) * 0.4f * expf(-v->t * 20);
                v->env *= 0.9990f;
                break;
            case 'o':
            case 'h': {
                float x = noise();
                s = (x - v->lp) * 0.35f;
                v->lp = x;
                v->env *= v->drum == 'h' ? 0.9965f : 0.9993f;
                break;
            }
            default: v->env = 0; break;
            }
        } else {
            float vib = c == 1 && v->t > 0.15f ? 1 + 0.006f * sinf(v->t * 6.2831853f * 5.5f) : 1;
            v->ph += v->freq * vib * dt;
            float ph = v->ph - floorf(v->ph);
            if (c == 0) s = (ph < 0.3f ? 1 : -1) * 0.55f;
            else if (c == 1) s = (ph < 0.5f ? 1 : -1) * 0.3f;
            else s = (ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph) * 0.35f;
            v->lp += (c == 0 ? 0.12f : 0.3f) * (s - v->lp);
            s = v->lp;
            v->env *= v->on ? (c == 2 ? 0.99997f : 0.99985f) : 0.997f;
        }
        out += s * v->env;
    }
    return out * 0.25f;
}

/* ---------------------------------------------------------------- mixer */

#define NCHAN 16
typedef struct { const sample_t *s; int pos, vl, vr, id; } chan_t;
static chan_t chans[NCHAN];
static int sfxvol = 12, musvol = 9;

void snd_init(void) { make_sfx(); }

void snd_set_volume(int sfx_vol, int music_vol)
{
    plat_audio_lock();
    sfxvol = sfx_vol;
    musvol = music_vol;
    plat_audio_unlock();
}

void snd_play(int id, int vol, int sep)
{
    if (id <= SFX_NONE || id >= SFX_COUNT || !sfx[id].data || vol <= 0) return;
    plat_audio_lock();
    /* reuse: same effect already playing from the start, else a free channel, else the oldest */
    int best = 0, bestpos = -1;
    for (int i = 0; i < NCHAN; i++) {
        if (!chans[i].s) {
            best = i;
            break;
        }
        if (chans[i].pos > bestpos) {
            bestpos = chans[i].pos;
            best = i;
        }
    }
    int vl = vol * (255 - sep / 2) / 255, vr = vol * (128 + sep / 2) / 255;
    chans[best] = (chan_t){&sfx[id], 0, vl, vr, id};
    plat_audio_unlock();
}

void snd_mix(int16_t *out, int frames)
{
    for (int i = 0; i < frames; i++) {
        int32_t l = 0, r = 0;
        for (int c = 0; c < NCHAN; c++) {
            chan_t *ch = &chans[c];
            if (!ch->s) continue;
            int v = ch->s->data[ch->pos];
            l += v * ch->vl / 256;
            r += v * ch->vr / 256;
            if (++ch->pos >= ch->s->len) ch->s = NULL;
        }
        l = l * sfxvol / 15;
        r = r * sfxvol / 15;
        int32_t m = (int32_t)(music_sample() * 32000 * musvol / 15);
        l += m;
        r += m;
        out[i * 2] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        out[i * 2 + 1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
    }
}
