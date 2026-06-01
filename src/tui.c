#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tui.h"
#include "engine.h"
#include "audio.h"

#define BAR_WIDTH 30

/* ANSI helpers */
#define RESET   "\033[0m"
#define CYAN    "\033[36m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define MAGENTA "\033[35m"
#define DIM     "\033[2m"
#define BOLD    "\033[1m"

static int g_initialized = 0;

void tui_init(void)
{
    /* Hide cursor */
    printf("\033[?25l");
    fflush(stdout);
    g_initialized = 1;
}

void tui_cleanup(void)
{
    if (!g_initialized) return;
    /* Show cursor, reset color, move to new line */
    printf(RESET "\033[?25h\n");
    fflush(stdout);
    g_initialized = 0;
}

/* ----------------------------------------------------------------- bar draw */

static void draw_bar(PhaseType phase, double progress, int pulse_on,
                     int rapid_n, int rapid_total)
{
    const char *color;
    switch (phase) {
        case PHASE_INHALE: color = CYAN;    break;
        case PHASE_EXHALE: color = GREEN;   break;
        case PHASE_HOLD:   color = YELLOW;  break;
        case PHASE_RAPID:  color = MAGENTA; break;
        default:           color = RESET;   break;
    }

    /* For rapid: use current cycle position (sub-phase progress) */
    double fill = progress;
    if (fill < 0.0) fill = 0.0;
    if (fill > 1.0) fill = 1.0;

    /* For hold: pulse effect — if pulse_on is 0, dim */
    int filled = (int)(fill * BAR_WIDTH + 0.5);
    if (filled > BAR_WIDTH) filled = BAR_WIDTH;

    printf("  %s", color);

    if (phase == PHASE_HOLD && !pulse_on) {
        printf(DIM);
    }

    int i;
    for (i = 0; i < BAR_WIDTH; i++) {
        if (i < filled)
            printf("\xe2\x96\x88");  /* U+2588 FULL BLOCK */
        else
            printf("\xe2\x96\x91");  /* U+2591 LIGHT SHADE */
    }
    printf(RESET);

    if (phase == PHASE_RAPID) {
        printf("  %d / %d", rapid_n, rapid_total);
    }
}

/* ----------------------------------------------------------------- render */

void tui_render(PhaseType phase,
                double     progress,
                double     remaining_s,
                int        rapid_n,
                int        rapid_total,
                double     elapsed_total_s,
                int        duration_s,
                const char *preset_name,
                int        inhale_s,
                int        exhale_s,
                TuiStatus  status,
                int        pulse_on)
{
    /* Time display */
    int elapsed_min = (int)(elapsed_total_s / 60);
    int elapsed_sec = (int)elapsed_total_s % 60;
    (void)duration_s;  /* reserved for future progress bar display */

    /* Status icon */
    const char *status_icon;
    if (status == TUI_STATUS_PAUSED)
        status_icon = "\xe2\x80\x96";   /* ‖ */
    else if (status == TUI_STATUS_MUTED || g_sound_mode == SOUND_OFF)
        status_icon = "\xf0\x9f\x94\x87"; /* 🔇 */
    else
        status_icon = "\xe2\x97\x8f";   /* ● */

    /* Phase label */
    const char *phase_label;
    switch (phase) {
        case PHASE_INHALE: phase_label = "INHALE"; break;
        case PHASE_EXHALE: phase_label = "EXHALE"; break;
        case PHASE_HOLD:   phase_label = "HOLD";   break;
        case PHASE_RAPID:  phase_label = "RAPID";  break;
        default:           phase_label = "";        break;
    }

    /* Remaining time display */
    char time_str[32];
    if (remaining_s < 0) {
        /* count-up extension mode */
        snprintf(time_str, sizeof(time_str), "+%ds", (int)(-remaining_s));
    } else {
        snprintf(time_str, sizeof(time_str), "%ds", (int)(remaining_s + 0.99));
    }

    /* Header line: preset · ratio · elapsed [status] */
    printf("\r\033[K");  /* carriage return, clear line */
    printf("%s \xc2\xb7 %d:%d \xc2\xb7 %02d:%02d   [%s]\n",
           preset_name, inhale_s, exhale_s,
           elapsed_min, elapsed_sec,
           status_icon);

    /* Phase label line */
    printf("\r\033[K");
    printf("         %s  %s\n", phase_label, time_str);

    /* Bar line */
    printf("\r\033[K");
    draw_bar(phase, progress, pulse_on, rapid_n, rapid_total);
    printf("\n");

    /* Help line */
    printf("\r\033[K");
    printf("  space pause \xc2\xb7 s mute \xc2\xb7 q quit");

    /* Move cursor back to top of our 4-line block.
     * We printed 3 newlines (header, phase, bar) and left the help line
     * without a trailing newline, so net movement is 3 lines down. */
    printf("\033[3A\r");
    fflush(stdout);
}

/* ---------------------------------------------------------------- summary */

void tui_summary(const char *preset_name,
                 double elapsed_s,
                 int breaths,
                 const char *status)
{
    /* Position after our 4-line block.
     * Cursor is at line 0 of block after cleanup's \n moves us to line 1.
     * Down 2 more lands on line 3 (help), then \n moves past it. */
    printf("\033[2B\n");
    printf(RESET);
    printf("Session complete: %s\n", preset_name);
    int m = (int)(elapsed_s / 60);
    int s = (int)elapsed_s % 60;
    printf("Duration: %02d:%02d  Breaths: %d  Status: %s\n",
           m, s, breaths, status);
    fflush(stdout);
}

/* ---------------------------------------------------------------- safety */

void tui_print_safety(void)
{
    printf(BOLD "Safety Information" RESET "\n\n");
    printf("Breathe is a breathing exercise guide. Please read before use:\n\n");
    printf("CARDIAC: Breathing exercises affect heart rate and blood pressure.\n");
    printf("Consult your doctor if you have any cardiac conditions.\n\n");
    printf("RESPIRATORY: Hyperventilation techniques (e.g. Tummo/Wim Hof style)\n");
    printf("can cause lightheadedness or loss of consciousness.\n");
    printf("NEVER practice breath retention near water or while driving.\n");
    printf("Stop immediately if you feel faint, dizzy, or experience chest pain.\n\n");
    printf("This software is for wellness purposes only and is not medical advice.\n");
}

/* ---------------------------------------------------------------- version */

void tui_print_version(void)
{
    printf("breathe 1.0.0\n");
    printf("Based on https://github.com/marekkowalczyk/breathe-cli\n");
}
