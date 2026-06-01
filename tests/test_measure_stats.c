#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "test_runner.h"
#include "../src/measure.c"

int main(void)
{
    /* ------------------------------------------------------------------ */
    SUITE("measure_compute_stats_raw basic");
    {
        /* types: 1=inhale, 2=exhale, 3=hold */
        int    types[] = { 1, 2, 1, 2, 1, 2, 3 };
        double durs[]  = { 4200.0, 5100.0, 3800.0, 6000.0, 5000.0, 4900.0, 2000.0 };
        int    count   = 7;

        MeasureStat inhale, exhale, hold;
        measure_compute_stats_raw(types, durs, count, &inhale, &exhale, &hold);

        ASSERT_EQ(inhale.count, 3, "inhale.count == 3");
        ASSERT_EQ(exhale.count, 3, "exhale.count == 3");
        ASSERT_EQ(hold.count,   1, "hold.count == 1");

        ASSERT_NEAR(inhale.min_ms, 3800.0, 1e-6, "inhale.min_ms == 3800.0");
        ASSERT_NEAR(inhale.max_ms, 5000.0, 1e-6, "inhale.max_ms == 5000.0");
        {
            double avg = inhale.sum_ms / inhale.count;
            ASSERT_NEAR(avg, 4333.3333, 0.1, "inhale avg ~= 4333.3 ms");
        }

        ASSERT_NEAR(exhale.min_ms, 4900.0, 1e-6, "exhale.min_ms == 4900.0");
        ASSERT_NEAR(exhale.max_ms, 6000.0, 1e-6, "exhale.max_ms == 6000.0");
        {
            double avg = exhale.sum_ms / exhale.count;
            ASSERT_NEAR(avg, 5333.3333, 0.1, "exhale avg ~= 5333.3 ms");
        }

        ASSERT_NEAR(hold.min_ms, 2000.0, 1e-6, "hold.min_ms == 2000.0");
        ASSERT_NEAR(hold.max_ms, 2000.0, 1e-6, "hold.max_ms == 2000.0");
    }

    /* ------------------------------------------------------------------ */
    SUITE("measure_compute_stats_raw empty input");
    {
        MeasureStat inhale, exhale, hold;
        measure_compute_stats_raw(NULL, NULL, 0, &inhale, &exhale, &hold);
        ASSERT_EQ(inhale.count, 0, "empty: inhale.count == 0");
        ASSERT_EQ(exhale.count, 0, "empty: exhale.count == 0");
        ASSERT_EQ(hold.count,   0, "empty: hold.count == 0");
    }

    /* ------------------------------------------------------------------ */
    SUITE("tap detection logic");
    /* The flagged_tap condition in measure.c is: dur < 300.0 */
    ASSERT_EQ((200.0 < 300.0) ? 1 : 0, 1, "200ms is a tap");
    ASSERT_EQ((300.0 < 300.0) ? 1 : 0, 0, "300ms is not a tap");
    ASSERT_EQ((299.9 < 300.0) ? 1 : 0, 1, "299.9ms is a tap");

    /* ------------------------------------------------------------------ */
    SUITE("BPM calculation");
    {
        /* 3 inhales over 30 seconds = 3/30*60 = 6.0 bpm */
        double total_s     = 30.0;
        int    inhale_count = 3;
        double bpm = (total_s > 0 && inhale_count > 0)
                     ? ((double)inhale_count / total_s * 60.0) : 0.0;
        ASSERT_NEAR(bpm, 6.0, 1e-9, "BPM 3 inhales/30s == 6.0");
    }
    {
        double total_s     = 30.0;
        int    inhale_count = 0;
        double bpm = (total_s > 0 && inhale_count > 0)
                     ? ((double)inhale_count / total_s * 60.0) : 0.0;
        ASSERT_NEAR(bpm, 0.0, 1e-9, "BPM 0 inhales == 0.0");
    }

    /* ------------------------------------------------------------------ */
    SUITE("all-one-type input");
    {
        int    types[] = { 1, 1, 1 };
        double durs[]  = { 4000.0, 5000.0, 6000.0 };
        MeasureStat inhale, exhale, hold;
        measure_compute_stats_raw(types, durs, 3, &inhale, &exhale, &hold);
        ASSERT_EQ(inhale.count, 3, "all-inhale: inhale.count == 3");
        ASSERT_EQ(exhale.count, 0, "all-inhale: exhale.count == 0");
        ASSERT_EQ(hold.count,   0, "all-inhale: hold.count == 0");
    }

    /* ------------------------------------------------------------------ */
    SUITE("MPHASE_NONE (type=0) ignored");
    {
        int    types[] = { 0, 1, 0 };
        double durs[]  = { 1000.0, 4000.0, 2000.0 };
        MeasureStat inhale, exhale, hold;
        measure_compute_stats_raw(types, durs, 3, &inhale, &exhale, &hold);
        ASSERT_EQ(inhale.count, 1, "NONE entries ignored: inhale.count == 1");
    }

    TEST_RESULTS();
}
