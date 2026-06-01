#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>

typedef enum {
    SOUND_ON,       /* play WAV via paplay/aplay */
    SOUND_BELL,     /* terminal bell only */
    SOUND_OFF       /* silence */
} SoundMode;

extern SoundMode g_sound_mode;

/* Find a suitable player binary; called once at startup */
void audio_init(int no_sound_flag);

/* Play a tone cue: phase_type 0=inhale, 1=exhale, 2=hold */
void audio_play_cue(int phase_type);

/* Cycle: on -> bell -> off -> on */
void audio_cycle_mode(void);

const char *audio_mode_label(void);

/* WAV generation exposed for testing */
unsigned char *audio_gen_wav(double freq_hz, int dur_ms, double amp, size_t *out_size);

#endif /* AUDIO_H */
