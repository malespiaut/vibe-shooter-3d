/* The ONLY interface between the game and the platform. */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#define SCREENW 320
#define SCREENH 240

/* Key codes are USB HID usage IDs (a hardware standard). */
enum {
    PK_A = 4, PK_Z = 29,
    PK_1 = 30, PK_0 = 39,
    PK_RETURN = 40, PK_ESCAPE = 41, PK_BACKSPACE = 42, PK_TAB = 43, PK_SPACE = 44,
    PK_MINUS = 45, PK_EQUALS = 46, PK_LBRACKET = 47, PK_RBRACKET = 48,
    PK_SEMICOLON = 51, PK_APOSTROPHE = 52, PK_GRAVE = 53, PK_COMMA = 54, PK_PERIOD = 55, PK_SLASH = 56,
    PK_F1 = 58, PK_F12 = 69,
    PK_PAUSE = 72, PK_INSERT = 73, PK_HOME = 74, PK_PAGEUP = 75, PK_DELETE = 76, PK_END = 77, PK_PAGEDOWN = 78,
    PK_RIGHT = 79, PK_LEFT = 80, PK_DOWN = 81, PK_UP = 82,
    PK_LCTRL = 224, PK_LSHIFT = 225, PK_LALT = 226, PK_RCTRL = 228, PK_RSHIFT = 229, PK_RALT = 230,
    /* mouse buttons share the key space so they can be bound like keys */
    PK_MOUSE1 = 500, PK_MOUSE2 = 501, PK_MOUSE3 = 502, PK_WHEELUP = 503, PK_WHEELDOWN = 504,
    PK_COUNT = 512
};

typedef struct {
    uint8_t down[PK_COUNT];     /* held this frame */
    uint8_t pressed[PK_COUNT];  /* went down since last poll */
    int mouse_dx, mouse_dy;
} plat_input;

bool plat_init(const char *title);
void plat_shutdown(void);
void plat_set_palette(const uint8_t rgb[256 * 3]);
void plat_present(const uint8_t fb[SCREENW * SCREENH]);
bool plat_poll(plat_input *in); /* false = quit requested */
void plat_audio_lock(void);
void plat_audio_unlock(void);
void plat_set_audio_callback(void (*mix)(int16_t *out, int frames), int rate);
uint64_t plat_ticks_us(void);
long plat_read_file(const char *path, void **data); /* caller frees; -1 on error */
bool plat_write_file(const char *path, const void *data, long size);

#endif
