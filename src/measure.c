#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

#include "measure.h"

extern volatile sig_atomic_t g_interrupted;  /* set by main.c's SIGINT handler */

#define MAX_PHASES 1024

typedef enum {
    MPHASE_NONE,
    MPHASE_INHALE,
    MPHASE_EXHALE,
    MPHASE_HOLD
} MPhaseType;

typedef struct {
    MPhaseType type;
    double     duration_ms;
    int        flagged_tap;
} MRecord;

static MRecord g_records[MAX_PHASES];
static int     g_record_count = 0;

static struct timespec g_phase_start;
static MPhaseType      g_current_phase = MPHASE_NONE;
static struct timespec g_session_start;

/* Milliseconds since epoch (for display) */
static double ms_since(struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec  - start->tv_sec)  * 1000.0 +
           (now.tv_nsec - start->tv_nsec) / 1.0e6;
}


static void end_phase(void)
{
    if (g_current_phase == MPHASE_NONE) return;
    if (g_record_count >= MAX_PHASES) return;

    double dur = ms_since(&g_phase_start);
    MRecord r;
    r.type        = g_current_phase;
    r.duration_ms = dur;
    r.flagged_tap = (dur < 300.0) ? 1 : 0;

    g_records[g_record_count++] = r;
    g_current_phase = MPHASE_NONE;
}

static void start_phase(MPhaseType t)
{
    end_phase();
    g_current_phase = t;
    clock_gettime(CLOCK_MONOTONIC, &g_phase_start);
}

/* Terminal raw mode (managed by main.c, but we need select() reads here) */
static int read_key_nonblock(char *buf, int buflen)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = { 0, 0 };
    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (r <= 0) return 0;
    int n = (int)read(STDIN_FILENO, buf, (size_t)buflen);
    return (n > 0) ? n : 0;
}

static const char *phase_name(MPhaseType t)
{
    switch (t) {
        case MPHASE_INHALE: return "INHALE";
        case MPHASE_EXHALE: return "exhale";
        case MPHASE_HOLD:   return "hold";
        default:            return "---";
    }
}

static void write_measurements_csv(void)
{
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char path[512];
    snprintf(path, sizeof(path), "%s/.breathe_measurements.csv", home);

    /* Generate session_id: 6 hex chars (XOR with pid to avoid same-second collisions) */
    char session_id[8];
    snprintf(session_id, sizeof(session_id), "%06x",
             (unsigned int)((time(NULL) ^ (unsigned)getpid()) & 0xFFFFFF));

    /* Check if header needed */
    FILE *check = fopen(path, "r");
    int need_header = (check == NULL);
    if (check) fclose(check);

    FILE *f = fopen(path, "a");
    if (!f) return;

    if (need_header)
        fprintf(f, "date,time,session_id,phase,duration_ms,flagged_tap\n");

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char date[16], tim[16];
    strftime(date, sizeof(date), "%Y-%m-%d", tm);
    strftime(tim,  sizeof(tim),  "%H:%M:%S", tm);

    int i;
    for (i = 0; i < g_record_count; i++) {
        const char *pn;
        switch (g_records[i].type) {
            case MPHASE_INHALE: pn = "inhale"; break;
            case MPHASE_EXHALE: pn = "exhale"; break;
            case MPHASE_HOLD:   pn = "hold";   break;
            default:            pn = "unknown";
        }
        fprintf(f, "%s,%s,%s,%s,%.1f,%d\n",
                date, tim, session_id, pn,
                g_records[i].duration_ms,
                g_records[i].flagged_tap);
    }
    fclose(f);
}

static void print_summary(void)
{
    /* Compute per-type stats */
    typedef struct { double sum; double min; double max; int count; } Stat;
    Stat stats[4];
    int i;
    for (i = 0; i < 4; i++) {
        stats[i].sum   = 0;
        stats[i].min   = 1e18;
        stats[i].max   = 0;
        stats[i].count = 0;
    }

    for (i = 0; i < g_record_count; i++) {
        int idx = (int)g_records[i].type;
        if (idx < 1 || idx > 3) continue;
        stats[idx].sum += g_records[i].duration_ms;
        if (g_records[i].duration_ms < stats[idx].min)
            stats[idx].min = g_records[i].duration_ms;
        if (g_records[i].duration_ms > stats[idx].max)
            stats[idx].max = g_records[i].duration_ms;
        stats[idx].count++;
    }

    double total_s = ms_since(&g_session_start) / 1000.0;
    int inhale_count = stats[MPHASE_INHALE].count;
    double bpm = (total_s > 0 && inhale_count > 0)
                 ? ((double)inhale_count / total_s * 60.0) : 0.0;

    printf("\n\033[1mSession Summary\033[0m\n");
    printf("─────────────────────────────────────────\n");

    const char *type_names[] = { "", "INHALE", "exhale", "hold" };
    int t;
    for (t = 1; t <= 3; t++) {
        if (stats[t].count == 0) continue;
        double avg = stats[t].sum / stats[t].count;
        printf("  %-8s  min:%.1fs  avg:%.1fs  max:%.1fs  n:%d\n",
               type_names[t],
               stats[t].min / 1000.0,
               avg           / 1000.0,
               stats[t].max / 1000.0,
               stats[t].count);
    }
    printf("─────────────────────────────────────────\n");
    printf("  bpm: %.1f\n", bpm);
    printf("\n");
}

static void render_measure(double session_s,
                           MPhaseType current_phase,
                           double current_phase_ms,
                           int show_last)
{
    int min = (int)(session_s / 60);
    int sec = (int)session_s % 60;

    printf("\r\033[K");
    printf("MEASURE                         %02d:%02d:%02d\n",
           0, min, sec);

    printf("\r\033[K");
    printf("─────────────────────────────────────────\n");

    /* Current phase */
    printf("\r\033[K");
    if (current_phase != MPHASE_NONE) {
        /* Bar: fill based on expected duration ~5s */
        double max_s = 10.0;
        double fill  = (current_phase_ms / 1000.0) / max_s;
        if (fill > 1.0) fill = 1.0;
        int filled = (int)(fill * 20);

        const char *color = (current_phase == MPHASE_INHALE) ? "\033[36m" :
                            (current_phase == MPHASE_EXHALE) ? "\033[32m" :
                            "\033[33m";

        printf("  %-8s%s", phase_name(current_phase), color);
        int i;
        for (i = 0; i < 20; i++)
            printf(i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91");
        printf("\033[0m    %.1fs\n",
               current_phase_ms / 1000.0);
    } else {
        printf("  (no active phase)\n");
    }

    printf("\r\033[K");
    printf("─────────────────────────────────────────\n");

    /* Last 5 completed phases */
    int start = g_record_count - show_last;
    if (start < 0) start = 0;
    int i;
    for (i = start; i < g_record_count; i++) {
        printf("\r\033[K");
        printf("  %-8s %.1fs%s\n",
               phase_name(g_records[i].type),
               g_records[i].duration_ms / 1000.0,
               g_records[i].flagged_tap ? " (tap)" : "");
    }
    /* Pad to always show 5 lines */
    int shown = g_record_count - start;
    for (i = shown; i < show_last; i++) {
        printf("\r\033[K\n");
    }

    printf("\r\033[K");
    printf("─────────────────────────────────────────\n");

    /* Averages */
    double sum_in = 0, sum_ex = 0, sum_hold = 0;
    int cnt_in = 0, cnt_ex = 0, cnt_hold = 0;
    for (i = 0; i < g_record_count; i++) {
        switch (g_records[i].type) {
            case MPHASE_INHALE: sum_in   += g_records[i].duration_ms; cnt_in++;   break;
            case MPHASE_EXHALE: sum_ex   += g_records[i].duration_ms; cnt_ex++;   break;
            case MPHASE_HOLD:   sum_hold += g_records[i].duration_ms; cnt_hold++; break;
            default: break;
        }
    }
    double avg_in   = cnt_in   ? sum_in   / cnt_in   / 1000.0 : 0;
    double avg_ex   = cnt_ex   ? sum_ex   / cnt_ex   / 1000.0 : 0;
    double avg_hold = cnt_hold ? sum_hold / cnt_hold / 1000.0 : 0;
    double bpm = (session_s > 0 && cnt_in > 0)
                 ? ((double)cnt_in / session_s * 60.0) : 0.0;

    printf("\r\033[K");
    printf("avg in:%.1fs  avg out:%.1fs  avg hold:%.1fs   bpm:%.1f\n",
           avg_in, avg_ex, avg_hold, bpm);

    printf("\r\033[K");
    printf("\xe2\x86\x91 inhale  \xe2\x86\x93 exhale  \xe2\x86\x90\xe2\x86\x92 hold  X end");

    /* Move cursor back to top */
    int lines = 4 /* header/sep/phase/sep */
              + show_last
              + 3; /* sep + avg + help */
    printf("\033[%dA\r", lines);
    fflush(stdout);
}

void measure_compute_stats_raw(const int *types, const double *durations, int count,
    MeasureStat *in, MeasureStat *ex, MeasureStat *hold)
{
    MeasureStat *s[4];
    int i;
    s[0] = NULL;
    s[1] = in;
    s[2] = ex;
    s[3] = hold;
    for (i = 1; i <= 3; i++) {
        if (s[i]) {
            s[i]->min_ms = 1e18;
            s[i]->max_ms = 0;
            s[i]->sum_ms = 0;
            s[i]->count  = 0;
        }
    }
    for (i = 0; i < count; i++) {
        int t = types[i];
        if (t < 1 || t > 3 || !s[t]) continue;
        s[t]->sum_ms += durations[i];
        if (durations[i] < s[t]->min_ms) s[t]->min_ms = durations[i];
        if (durations[i] > s[t]->max_ms) s[t]->max_ms = durations[i];
        s[t]->count++;
    }
}

void measure_run(void)
{
    /* Hide cursor */
    printf("\033[?25l");
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &g_session_start);

    /* Track which arrow keys are currently held */
    int held_up    = 0;
    int held_down  = 0;
    int held_lr    = 0;

    int done = 0;

    while (!done) {
        if (g_interrupted) { end_phase(); done = 1; break; }

        /* Non-blocking key read */
        char buf[8];
        int n = read_key_nonblock(buf, sizeof(buf));

        if (n > 0) {
            /* ESC sequence check */
            if (n >= 3 && buf[0] == '\033' && buf[1] == '[') {
                char arrow = buf[2];
                if (arrow == 'A') {          /* Up - inhale pressed */
                    if (!held_up) {
                        held_up = 1;
                        held_down = 0;
                        held_lr   = 0;
                        start_phase(MPHASE_INHALE);
                    }
                } else if (arrow == 'B') {   /* Down - exhale pressed */
                    if (!held_down) {
                        held_down = 1;
                        held_up   = 0;
                        held_lr   = 0;
                        start_phase(MPHASE_EXHALE);
                    }
                } else if (arrow == 'C' || arrow == 'D') {  /* Left/Right - hold */
                    if (!held_lr) {
                        held_lr  = 1;
                        held_up   = 0;
                        held_down = 0;
                        start_phase(MPHASE_HOLD);
                    }
                }
            } else {
                char ch = buf[0];
                /* Release detection: in raw mode, key-release isn't sent
                   so we use the heuristic that receiving any non-arrow
                   key while holding means release. For arrow keys we
                   handle via timing instead. */
                if (ch == 'x' || ch == 'X' || ch == '\r' || ch == '\n') {
                    end_phase();
                    done = 1;
                } else if (ch == 3) { /* Ctrl-C */
                    end_phase();
                    done = 1;
                } else if (ch == ' ') {
                    /* Space = release current phase */
                    end_phase();
                    held_up = held_down = held_lr = 0;
                }
            }
        } else {
            /* No key — if we were tracking a held state with no key
               arriving for a while, treat as release.
               Arrow key auto-repeat in terminals fires continuously;
               absence of keys means released. */
            if (held_up || held_down || held_lr) {
                /* Simple debounce: if no key for ~50ms, treat as released */
                /* We check by sleeping and re-reading */
                /* Actually in raw terminal, arrows repeat while held.
                   We'll just wait for next frame without ending phase. */
            }
        }

        /* Render current state */
        double session_ms = ms_since(&g_session_start);
        double session_s  = session_ms / 1000.0;
        double phase_ms   = 0;
        if (g_current_phase != MPHASE_NONE)
            phase_ms = ms_since(&g_phase_start);

        render_measure(session_s, g_current_phase, phase_ms, 5);

        /* ~60fps */
        struct timespec ts = { 0, 16000000L };
        nanosleep(&ts, NULL);

        /* Release detection: if no arrow received for 100ms while held,
           assume released. We track last-arrow time. */
        /* Simplified: reading will get repeated codes if held; if no
           code comes for 2 frames (~32ms), assume released. */
        static int no_arrow_frames = 0;
        if (n <= 0 && (held_up || held_down || held_lr)) {
            no_arrow_frames++;
            if (no_arrow_frames > 6) {  /* ~100ms */
                end_phase();
                held_up = held_down = held_lr = 0;
                no_arrow_frames = 0;
            }
        } else if (n > 0) {
            no_arrow_frames = 0;
        }
    }

    /* Move cursor past our rendered block */
    int show_last = 5;
    int lines = 4 + show_last + 3;
    printf("\033[%dB\n", lines);
    printf("\033[?25h");  /* show cursor */
    fflush(stdout);

    /* Write to CSV */
    write_measurements_csv();

    /* Print summary */
    print_summary();
}
