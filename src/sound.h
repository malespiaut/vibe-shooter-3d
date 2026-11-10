#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

#define MIXRATE 22050

void snd_init(void);
/* audio callback: mixes all channels + music into signed 16-bit stereo */
void snd_mix(int16_t *out, int frames);
/* vol 0..255, sep 0 = hard left .. 255 = hard right */
void snd_play(int sfx, int vol, int sep);
void snd_set_volume(int sfx_vol, int music_vol); /* 0..15 each */
/* parse and start a song from our text format; NULL stops the music */
void snd_music(const char *text);

#endif
