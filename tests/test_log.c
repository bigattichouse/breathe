#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_runner.h"
#include "../src/log.c"

/* Count lines in a file */
static int count_lines(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int lines = 0;
    char buf[512];
    while (fgets(buf, sizeof(buf), f)) {
        /* Only count non-empty lines */
        if (buf[0] != '\0' && buf[0] != '\n')
            lines++;
    }
    fclose(f);
    return lines;
}

/* Check if a file contains a substring */
static int file_contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[1024];
    int found = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, needle)) { found = 1; break; }
    }
    fclose(f);
    return found;
}

int main(void)
{
    /* Set up temp HOME */
    char tmpdir[] = "/tmp/breathe_logtest_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "mkdtemp failed\n");
        return 1;
    }
    setenv("HOME", tmpdir, 1);

    char logpath[600];
    snprintf(logpath, sizeof(logpath), "%s/.breathe_log.csv", tmpdir);

    /* Make sure file doesn't exist yet */
    unlink(logpath);

    /* ------------------------------------------------------------------ */
    SUITE("log_session creates file");
    log_session("balanced", 5, 5, 600, 587, 58, 97.8, "complete");
    {
        FILE *f = fopen(logpath, "r");
        ASSERT_NOTNULL(f, "log file created after log_session");
        if (f) fclose(f);
    }

    /* ------------------------------------------------------------------ */
    SUITE("log file header");
    ASSERT(file_contains(logpath, "date,time,preset,ratio,"),
           "log header contains date,time,preset,ratio");

    /* ------------------------------------------------------------------ */
    SUITE("log file line count after first session");
    ASSERT_EQ(count_lines(logpath), 2,
              "file has 2 lines after first session (header + data)");

    /* ------------------------------------------------------------------ */
    SUITE("log CSV row content");
    ASSERT(file_contains(logpath, "balanced"), "CSV row contains preset 'balanced'");
    ASSERT(file_contains(logpath, "5:5"),      "CSV row contains ratio '5:5'");
    ASSERT(file_contains(logpath, "600"),      "CSV row contains duration_target_s '600'");
    ASSERT(file_contains(logpath, "complete"), "CSV row contains status 'complete'");

    /* ------------------------------------------------------------------ */
    SUITE("log_session appends");
    log_session("calm", 4, 6, 900, 900, 90, 100.0, "complete");
    ASSERT_EQ(count_lines(logpath), 3,
              "file has 3 lines after second session");

    /* ------------------------------------------------------------------ */
    SUITE("completion_pct format");
    /* 97.8 should appear as "97.8" (one decimal place) */
    ASSERT(file_contains(logpath, "97.8"), "completion_pct formatted as '97.8'");

    /* ------------------------------------------------------------------ */
    SUITE("ratio format with colon separator");
    /* First session used 5:5, second used 4:6 */
    ASSERT(file_contains(logpath, "4:6"), "second session ratio '4:6' in log");

    /* Cleanup */
    unlink(logpath);
    rmdir(tmpdir);

    TEST_RESULTS();
}
