/* The ONLY file allowed to include SDL. Acts as the "video card" and "sound card". */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static uint32_t pal32[256];
static SDL_AudioStream *astream;
static void (*mixfn)(int16_t *out, int frames);
static char basepath[1024];

bool plat_init(const char *title)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_CreateWindowAndRenderer(title, SCREENW * 3, SCREENH * 3, SDL_WINDOW_RESIZABLE, &win, &ren)) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(ren, 1);
    SDL_SetRenderLogicalPresentation(ren, SCREENW, SCREENH, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, SCREENW, SCREENH);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_SetWindowRelativeMouseMode(win, true);
    const char *bp = SDL_GetBasePath();
    if (bp) SDL_strlcpy(basepath, bp, sizeof basepath);
    return true;
}

void plat_shutdown(void)
{
    if (astream) SDL_DestroyAudioStream(astream);
    SDL_Quit();
}

void plat_set_palette(const uint8_t rgb[256 * 3])
{
    for (int i = 0; i < 256; i++)
        pal32[i] = 0xff000000u | (uint32_t)rgb[i * 3] << 16 | (uint32_t)rgb[i * 3 + 1] << 8 | rgb[i * 3 + 2];
}

void plat_present(const uint8_t fb[SCREENW * SCREENH])
{
    void *pixels;
    int pitch;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch)) {
        for (int y = 0; y < SCREENH; y++) {
            uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
            const uint8_t *src = fb + y * SCREENW;
            for (int x = 0; x < SCREENW; x++) row[x] = pal32[src[x]];
        }
        SDL_UnlockTexture(tex);
    }
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

static void set_key(plat_input *in, int k, bool down)
{
    if (k <= 0 || k >= PK_COUNT) return;
    if (down && !in->down[k]) in->pressed[k] = 1;
    in->down[k] = down;
}

bool plat_poll(plat_input *in)
{
    bool run = true;
    memset(in->pressed, 0, sizeof in->pressed);
    in->mouse_dx = in->mouse_dy = 0;
    in->down[PK_WHEELUP] = in->down[PK_WHEELDOWN] = 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT: run = false; break;
        case SDL_EVENT_KEY_DOWN:
            if (!e.key.repeat) set_key(in, e.key.scancode, true);
            break;
        case SDL_EVENT_KEY_UP: set_key(in, e.key.scancode, false); break;
        case SDL_EVENT_MOUSE_MOTION:
            in->mouse_dx += (int)e.motion.xrel;
            in->mouse_dy += (int)e.motion.yrel;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button >= 1 && e.button.button <= 3) {
                static const int map[4] = {0, PK_MOUSE1, PK_MOUSE3, PK_MOUSE2};
                set_key(in, map[e.button.button], e.button.down);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (e.wheel.y > 0) set_key(in, PK_WHEELUP, true);
            if (e.wheel.y < 0) set_key(in, PK_WHEELDOWN, true);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: memset(in->down, 0, sizeof in->down); break;
        default: break;
        }
    }
    return run;
}

void plat_audio_lock(void)
{
    if (astream) SDL_LockAudioStream(astream);
}

void plat_audio_unlock(void)
{
    if (astream) SDL_UnlockAudioStream(astream);
}

static void SDLCALL audio_cb(void *ud, SDL_AudioStream *s, int additional, int total)
{
    (void)ud;
    (void)total;
    static int16_t buf[4096 * 2];
    int frames = additional / 4;
    while (frames > 0) {
        int n = frames > 4096 ? 4096 : frames;
        mixfn(buf, n);
        SDL_PutAudioStreamData(s, buf, n * 4);
        frames -= n;
    }
}

void plat_set_audio_callback(void (*mix)(int16_t *out, int frames), int rate)
{
    mixfn = mix;
    SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, rate};
    astream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_cb, NULL);
    if (!astream) {
        fprintf(stderr, "audio: %s\n", SDL_GetError());
        return;
    }
    SDL_ResumeAudioStreamDevice(astream);
}

uint64_t plat_ticks_us(void)
{
    return SDL_GetTicksNS() / 1000;
}

/* Files live next to the executable; fall back to the working directory. */
long plat_read_file(const char *path, void **data)
{
    char full[2048];
    size_t size;
    SDL_snprintf(full, sizeof full, "%s%s", basepath, path);
    void *d = SDL_LoadFile(full, &size);
    if (!d) d = SDL_LoadFile(path, &size);
    if (!d) return -1;
    void *out = malloc(size + 1); /* +1 so text files can be NUL-terminated */
    memcpy(out, d, size);
    ((char *)out)[size] = 0;
    SDL_free(d);
    *data = out;
    return (long)size;
}

bool plat_write_file(const char *path, const void *data, long size)
{
    char full[2048];
    SDL_snprintf(full, sizeof full, "%s%s", basepath, path);
    return SDL_SaveFile(full, data, (size_t)size);
}
