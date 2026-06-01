#ifndef MEASURE_H
#define MEASURE_H

/* Run the measure session.  Blocks until user presses X/Enter/Ctrl-C.
   Writes results to ~/.breathe_measurements.csv when done. */
void measure_run(void);

typedef struct {
    double min_ms, max_ms, sum_ms;
    int    count;
} MeasureStat;

/* compute stats over an array of (type, duration_ms) pairs
   types: 1=inhale 2=exhale 3=hold (matches MPhaseType values) */
void measure_compute_stats_raw(
    const int *types,
    const double *durations,
    int count,
    MeasureStat *inhale,
    MeasureStat *exhale,
    MeasureStat *hold);

#endif /* MEASURE_H */
