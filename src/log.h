#ifndef LOG_H
#define LOG_H

#include <time.h>

/* Called once at the end of a guided session */
void log_session(const char *preset,
                 int inhale_s, int exhale_s,
                 int duration_target_s,
                 int duration_actual_s,
                 int breaths,
                 double completion_pct,
                 const char *status);   /* "complete" | "interrupted" */

/* Print the log path to stdout and exit */
void log_print_path(void);

#endif /* LOG_H */
