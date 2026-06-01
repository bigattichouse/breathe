#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"

static const char *log_path(void)
{
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/.breathe_log.csv", home);
    return path;
}

static void ensure_header(const char *path)
{
    /* Check if file exists */
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return; }

    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "date,time,preset,ratio,duration_target_s,duration_actual_s,"
               "breaths,completion_pct,status\n");
    fclose(f);
}

void log_session(const char *preset,
                 int inhale_s, int exhale_s,
                 int duration_target_s,
                 int duration_actual_s,
                 int breaths,
                 double completion_pct,
                 const char *status)
{
    const char *path = log_path();
    ensure_header(path);

    FILE *f = fopen(path, "a");
    if (!f) return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char date[16], tim[16];
    strftime(date, sizeof(date), "%Y-%m-%d", tm);
    strftime(tim,  sizeof(tim),  "%H:%M:%S", tm);

    char ratio[16];
    snprintf(ratio, sizeof(ratio), "%d:%d", inhale_s, exhale_s);

    fprintf(f, "%s,%s,%s,%s,%d,%d,%d,%.1f,%s\n",
            date, tim, preset, ratio,
            duration_target_s, duration_actual_s,
            breaths, completion_pct, status);

    fclose(f);
}

void log_print_path(void)
{
    printf("%s\n", log_path());
}
