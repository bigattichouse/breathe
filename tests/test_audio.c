#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

#include "test_runner.h"
#include "../src/audio.c"

/* Helper: read little-endian 16-bit */
static unsigned short le16(const unsigned char *p)
{
    return (unsigned short)(p[0] | (p[1] << 8));
}

/* Helper: read little-endian 32-bit */
static unsigned int le32(const unsigned char *p)
{
    return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

int main(void)
{
    /* Disable sound so no processes are spawned */
    audio_init(1, 0);

    /* ------------------------------------------------------------------ */
    SUITE("audio_gen_wav basic");
    {
        size_t size = 0;
        unsigned char *wav = audio_gen_wav(800.0, 100, 0.3, &size);
        ASSERT_NOTNULL(wav, "gen_wav returns non-NULL");

        /* 44100 * 100 / 1000 = 4410 samples * 2 bytes = 8820 data bytes
           + 44 header = 8864 total */
        size_t expected = 44 + 44100 * 100 / 1000 * 2;
        ASSERT_EQ((int)size, (int)expected, "WAV total size = 8864");

        /* RIFF header checks */
        ASSERT_EQ(wav[0], 'R', "byte 0 == R");
        ASSERT_EQ(wav[1], 'I', "byte 1 == I");
        ASSERT_EQ(wav[2], 'F', "byte 2 == F");
        ASSERT_EQ(wav[3], 'F', "byte 3 == F");

        ASSERT_EQ(wav[8],  'W', "byte 8  == W");
        ASSERT_EQ(wav[9],  'A', "byte 9  == A");
        ASSERT_EQ(wav[10], 'V', "byte 10 == V");
        ASSERT_EQ(wav[11], 'E', "byte 11 == E");

        ASSERT_EQ(wav[12], 'f', "byte 12 == f");
        ASSERT_EQ(wav[13], 'm', "byte 13 == m");
        ASSERT_EQ(wav[14], 't', "byte 14 == t");
        ASSERT_EQ(wav[15], ' ', "byte 15 == space");

        ASSERT_EQ(wav[36], 'd', "byte 36 == d");
        ASSERT_EQ(wav[37], 'a', "byte 37 == a");
        ASSERT_EQ(wav[38], 't', "byte 38 == t");
        ASSERT_EQ(wav[39], 'a', "byte 39 == a");

        /* Sample rate at offset 24 */
        ASSERT_EQ((int)le32(wav + 24), 44100, "sample rate == 44100");

        /* Channels at offset 22 */
        ASSERT_EQ((int)le16(wav + 22), 1, "channels == 1 (mono)");

        /* Bits per sample at offset 34 */
        ASSERT_EQ((int)le16(wav + 34), 16, "bits per sample == 16");

        /* Data size at offset 40: 44100 * 100 / 1000 * 2 = 8820 */
        ASSERT_EQ((int)le32(wav + 40), 8820, "data size == 8820");

        /* First sample: fade-in from 0, so first sample value should be ~0
           (sample 0: envelope = amp * (0/fade_samples) = 0) */
        short first_sample = (short)(wav[44] | (wav[45] << 8));
        ASSERT_EQ(first_sample, 0, "first sample == 0 (fade-in)");

        free(wav);
    }

    /* ------------------------------------------------------------------ */
    SUITE("audio_gen_wav variations");
    {
        size_t size = 0;
        unsigned char *wav = audio_gen_wav(400.0, 100, 0.3, &size);
        ASSERT_NOTNULL(wav, "gen_wav 400Hz non-NULL");
        free(wav);
    }
    {
        /* 200ms: data = 44100 * 200 / 1000 * 2 = 17640 */
        size_t size = 0;
        unsigned char *wav = audio_gen_wav(800.0, 200, 0.3, &size);
        ASSERT_NOTNULL(wav, "gen_wav 200ms non-NULL");
        ASSERT_EQ((int)le32(wav + 40), 17640, "200ms data size == 17640");
        free(wav);
    }

    /* ------------------------------------------------------------------ */
    SUITE("audio_cycle_mode");
    /* audio_init(1, 0) set SOUND_OFF; reset to SOUND_BELL (default) for cycle test */
    g_sound_mode = SOUND_BELL;
    ASSERT_EQ((int)g_sound_mode, (int)SOUND_BELL, "start: SOUND_BELL");
    audio_cycle_mode();
    ASSERT_EQ((int)g_sound_mode, (int)SOUND_ON, "after 1st cycle: SOUND_ON");
    audio_cycle_mode();
    ASSERT_EQ((int)g_sound_mode, (int)SOUND_OFF, "after 2nd cycle: SOUND_OFF");
    audio_cycle_mode();
    ASSERT_EQ((int)g_sound_mode, (int)SOUND_BELL, "after 3rd cycle: SOUND_BELL");

    /* ------------------------------------------------------------------ */
    SUITE("audio_init and audio_mode_label");
    audio_init(1, 0);
    ASSERT_EQ((int)g_sound_mode, (int)SOUND_OFF, "audio_init(1, 0) sets SOUND_OFF");

    g_sound_mode = SOUND_ON;
    ASSERT_STR(audio_mode_label(), "sound", "SOUND_ON label == sound");

    g_sound_mode = SOUND_BELL;
    ASSERT_STR(audio_mode_label(), "bell", "SOUND_BELL label == bell");

    g_sound_mode = SOUND_OFF;
    ASSERT_STR(audio_mode_label(), "silent", "SOUND_OFF label == silent");

    TEST_RESULTS();
}
