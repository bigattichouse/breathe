#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>

typedef enum {
    SOUND_BELL,     /* terminal bell \a — default, safe over SSH */
    SOUND_ON,       /* play WAV via paplay/aplay (--sound) */
    SOUND_OFF       /* silence */
} SoundMode;

extern SoundMode g_sound_mode;

/* Default mode is terminal bell. force_sound_flag (--sound) opts into WAV via aplay/paplay. */
void audio_init(int no_sound_flag, int force_sound_flag);

/* Unlink pre-generated tone files; call from cleanup_and_exit */
void audio_cleanup(void);

/* Play a tone cue: phase_type 0=inhale, 1=exhale, 2=hold */
void audio_play_cue(int phase_type);

/* Cycle: on -> bell -> off -> on */
void audio_cycle_mode(void);

const char *audio_mode_label(void);

/* WAV generation exposed for testing */
unsigned char *audio_gen_wav(double freq_hz, int dur_ms, double amp, size_t *out_size);

#endif /* AUDIO_H */
