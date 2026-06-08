#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "audio.h"

SoundMode g_sound_mode = SOUND_BELL;

static char g_player[256] = ""; /* path to paplay or aplay */

/* Pre-generated tone file paths: [0]=inhale 800Hz, [1]=exhale 400Hz, [2]=hold 600Hz */
static char g_tone_files[3][64];
static int  g_tones_ready = 0;

/* ------------------------------------------------------------------ WAV gen */

/* Write little-endian 16-bit */
static void write_le16(unsigned char *buf, unsigned short v)
{
    buf[0] = (unsigned char)(v & 0xff);
    buf[1] = (unsigned char)((v >> 8) & 0xff);
}

/* Write little-endian 32-bit */
static void write_le32(unsigned char *buf, unsigned int v)
{
    buf[0] = (unsigned char)(v & 0xff);
    buf[1] = (unsigned char)((v >> 8) & 0xff);
    buf[2] = (unsigned char)((v >> 16) & 0xff);
    buf[3] = (unsigned char)((v >> 24) & 0xff);
}

/*
 * Generate a sine-wave WAV (16-bit, mono, 44100 Hz).
 * freq_hz: frequency; dur_ms: duration; amp: 0..1
 * Returns malloc'd buffer; *out_size = total bytes.
 */
unsigned char *audio_gen_wav(double freq_hz, int dur_ms, double amp,
                              size_t *out_size)
{
    const int sample_rate = 44100;
    const int fade_ms     = 10;

    int total_samples = (int)((long long)sample_rate * dur_ms / 1000);
    int fade_samples  = (int)((long long)sample_rate * fade_ms / 1000);

    /* WAV header = 44 bytes */
    size_t data_bytes = (size_t)total_samples * 2;  /* 16-bit */
    size_t total      = 44 + data_bytes;
    unsigned char *buf = malloc(total);
    if (!buf) return NULL;
    memset(buf, 0, total);

    /* RIFF header */
    memcpy(buf +  0, "RIFF", 4);
    write_le32(buf +  4, (unsigned int)(total - 8));
    memcpy(buf +  8, "WAVE", 4);
    memcpy(buf + 12, "fmt ", 4);
    write_le32(buf + 16, 16);              /* chunk size */
    write_le16(buf + 20, 1);              /* PCM */
    write_le16(buf + 22, 1);              /* mono */
    write_le32(buf + 24, (unsigned int)sample_rate);
    write_le32(buf + 28, (unsigned int)(sample_rate * 2)); /* byte rate */
    write_le16(buf + 32, 2);              /* block align */
    write_le16(buf + 34, 16);             /* bits per sample */
    memcpy(buf + 36, "data", 4);
    write_le32(buf + 40, (unsigned int)data_bytes);

    /* samples */
    int i;
    for (i = 0; i < total_samples; i++) {
        double envelope = amp;
        /* fade in */
        if (i < fade_samples)
            envelope = amp * ((double)i / fade_samples);
        /* fade out */
        else if (i >= total_samples - fade_samples)
            envelope = amp * ((double)(total_samples - 1 - i) / fade_samples);

        double sample = envelope * sin(2.0 * M_PI * freq_hz * i / sample_rate);
        short s = (short)(sample * 32767.0);
        unsigned char *p = buf + 44 + i * 2;
        p[0] = (unsigned char)(s & 0xff);
        p[1] = (unsigned char)((s >> 8) & 0xff);
    }

    *out_size = total;
    return buf;
}

/* ------------------------------------------------------------------ helpers */

/* Write a WAV buffer to a temp file; stores path in dst (must be >= 64 bytes).
 * Returns 0 on success, -1 on failure. */
static int write_tone_file(double freq_hz, char *dst, size_t dst_len)
{
    size_t wav_size = 0;
    unsigned char *wav = audio_gen_wav(freq_hz, 100, 0.3, &wav_size);
    if (!wav) return -1;

    /* Use a template that mkstemps can fill */
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "/tmp/breathe_tone_XXXXXX.wav");
    int fd = mkstemps(tmp, 4);
    if (fd < 0) { free(wav); return -1; }

    ssize_t written = write(fd, wav, wav_size);
    close(fd);
    free(wav);

    if (written < 0) { unlink(tmp); return -1; }

    snprintf(dst, dst_len, "%s", tmp);
    return 0;
}

/* ------------------------------------------------------------------ player */

/*
 * Default mode is terminal bell (safe over SSH — the \a travels through the
 * connection and rings on the local terminal).  Pass force_sound_flag=1 via
 * --sound to opt into WAV audio via aplay/paplay instead.
 */
void audio_init(int no_sound_flag, int force_sound_flag)
{
    if (no_sound_flag) {
        g_sound_mode = SOUND_OFF;
        return;
    }

    if (!force_sound_flag) return; /* default: terminal bell, no player needed */

    /* --sound: find a WAV player */
    const char *players[] = {
        "/usr/bin/paplay",
        "/usr/bin/aplay",
        "/bin/aplay",
        NULL
    };
    int i;
    for (i = 0; players[i]; i++) {
        if (access(players[i], X_OK) == 0) {
            strncpy(g_player, players[i], sizeof(g_player) - 1);
            break;
        }
    }

    if (g_player[0] == '\0') return; /* no player found; stay with bell */

    /* Pre-generate tone files: inhale=800Hz, exhale=400Hz, hold=600Hz */
    double freqs[3] = { 800.0, 400.0, 600.0 };
    int ok = 1;
    for (i = 0; i < 3; i++) {
        if (write_tone_file(freqs[i], g_tone_files[i], sizeof(g_tone_files[i])) != 0) {
            ok = 0;
            break;
        }
    }
    if (!ok) {
        for (i = 0; i < 3; i++) {
            if (g_tone_files[i][0]) {
                unlink(g_tone_files[i]);
                g_tone_files[i][0] = '\0';
            }
        }
        return; /* stay with bell */
    }
    g_tones_ready = 1;
    g_sound_mode = SOUND_ON;
}

void audio_cleanup(void)
{
    int i;
    for (i = 0; i < 3; i++) {
        if (g_tone_files[i][0]) {
            unlink(g_tone_files[i]);
            g_tone_files[i][0] = '\0';
        }
    }
    g_tones_ready = 0;
}

void audio_play_cue(int phase_type)
{
    if (g_sound_mode == SOUND_OFF) return;

    if (g_sound_mode == SOUND_BELL || g_player[0] == '\0') {
        if (write(STDOUT_FILENO, "\a", 1) < 0) { /* ignore */ }
        return;
    }

    if (!g_tones_ready) return;

    /* 0=inhale 800Hz, 1=exhale 400Hz, 2=hold 600Hz */
    int idx = (phase_type == 0) ? 0 :
              (phase_type == 1) ? 1 : 2;

    const char *path = g_tone_files[idx];

    /* Fork and exec player — file remains on disk until audio_cleanup() */
    pid_t pid = fork();
    if (pid == 0) {
        /* child: redirect stdout/stderr to /dev/null */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execl(g_player, g_player, path, (char*)NULL);
        _exit(1);
    }
    /* parent: SIGCHLD=SIG_IGN (set in main.c) auto-reaps the child */
}

void audio_cycle_mode(void)
{
    if (g_sound_mode == SOUND_BELL)
        g_sound_mode = SOUND_ON;
    else if (g_sound_mode == SOUND_ON)
        g_sound_mode = SOUND_OFF;
    else
        g_sound_mode = SOUND_BELL;
}

const char *audio_mode_label(void)
{
    if (g_sound_mode == SOUND_ON)   return "sound";
    if (g_sound_mode == SOUND_BELL) return "bell";
    return "silent";
}
