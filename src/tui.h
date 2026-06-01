#ifndef TUI_H
#define TUI_H

#include "engine.h"

typedef enum {
    TUI_STATUS_ACTIVE,
    TUI_STATUS_PAUSED,
    TUI_STATUS_MUTED
} TuiStatus;

/* Called once at start of session */
void tui_init(void);

/* Called on exit / signal */
void tui_cleanup(void);

/* Render one frame of the guided-breathing screen.
   progress: 0.0 (start of phase) .. 1.0 (end of phase)
   remaining_s: seconds left in phase (or count-up "+Xs" when negative)
   phase_index: current phase index within engine (for bar color)
   rapid_n: current breath count within rapid phase (0 if not rapid)
   rapid_total: total target for rapid phase
   elapsed_total_s: total session elapsed seconds
   duration_s: total session target duration in seconds (0 if rounds)
   preset_name: program/preset label
   inhale_s, exhale_s: ratio for header
   status: active/paused/muted indicator
   pulse_on: for hold-pulse, alternates every 500ms
*/
void tui_render(PhaseType   phase,
                double      progress,
                double      remaining_s,
                int         rapid_n,
                int         rapid_total,
                double      elapsed_total_s,
                int         duration_s,
                const char *preset_name,
                int         inhale_s,
                int         exhale_s,
                TuiStatus   status,
                int         pulse_on,
                const char *hint);

/* Print end-of-session summary */
void tui_summary(const char *preset_name,
                 double elapsed_s,
                 int breaths,
                 const char *status);

/* Print safety text and exit */
void tui_print_safety(void);

/* Print version */
void tui_print_version(void);

#endif /* TUI_H */
