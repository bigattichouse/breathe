# breathe

A terminal breathing guide. Runs a timed, animated breathing session with audio cues.

```
breathe [OPTIONS] [PRESET]
```

## Quick start

```sh
make
./breathe           # auto-selects a program based on time of day
./breathe calm      # explicit program
./breathe box       # box breathing
```

## Programs

Programs are timed sessions. The default program is chosen by time of day.

| Program    | Engine     | Duration | Auto-selected     | Purpose                              |
|------------|------------|----------|-------------------|--------------------------------------|
| `balanced` | resonance  | 10 min   | before noon       | All-purpose HRV and focus            |
| `extended` | extended   | 20 min   | 12–2 pm           | Clinical-length resonance session    |
| `afternoon`| energize   | 5 min    | 2–5 pm            | Mid-afternoon energy pickup          |
| `calm`     | calm       | 15 min   | after 5 pm        | Evening wind-down before sleep       |
| `box`      | box        | 10 min   | —                 | Stress relief and acute focus        |
| `tummo`    | tummo      | 3 rounds | —                 | Wim Hof-style sympathetic activation |

## Engines

Engines are named breath-phase sequences. Programs reference engines.

| Engine      | Pattern                              | Effect                                     |
|-------------|--------------------------------------|--------------------------------------------|
| `resonance` | 5s in / 5s out                       | ~6 breaths/min, improves HRV               |
| `calm`      | 4s in / 6s out                       | Exhale-dominant, parasympathetic           |
| `extended`  | 4s in / 6s out                       | Same as calm, for longer sessions          |
| `box`       | 4s in / 4s hold / 4s out / 4s hold  | Square breathing, focus and stress relief  |
| `energize`  | 6s in / 2s hold / 4s out            | Inhale-dominant, sympathetic activation    |
| `tummo`     | 30 rapid / hold / 4s in / 15s hold  | Tibetan inner-fire; see `--safety`         |

## TUI layout

```
balanced · 5:5 · 00:12   [●]
         INHALE  4s
  ████████████████░░░░░░░░░░░░░░
  In through your nose... let your belly rise.
  space pause · s mute · q quit
```

Each engine carries per-phase technique hints shown dimmed below the progress bar:

- Standard engines: nasal breathing, diaphragmatic (belly) expansion
- `tummo` rapid phase: mouth breathing, belly pump
- User-defined engines: no hint shown

## Options

```
-p, --preset NAME      Select program or engine by name
-d, --duration MINS    Override session duration (1–60)
    --ratio IN:EX      Custom inhale:exhale ratio (e.g. 4:6)
    --inhale SECS      Custom inhale duration
    --exhale SECS      Custom exhale duration
-n, --no-sound         Disable audio cues
-q, --quiet            Suppress TUI (plain output)
    --no-log           Do not write to session log
-m, --measure          Measure mode: tap spacebar to record your own rhythm
    --list             List all programs and engines
    --safety           Print safety information
```

## Custom engines and programs

```sh
# Define a custom engine (4-7-8 relaxation)
breathe --create-engine my478 "inhale:4, hold:7, exhale:8"

# Define a custom program using it
breathe --create-program sleep "engine:my478 duration:10"

# Run it
breathe sleep

# Delete
breathe --delete-engine my478
breathe --delete-program sleep
```

## Session log

Sessions are appended to `~/.local/share/breathe/sessions.csv`:

```
date,time,preset,ratio,duration_target_s,duration_actual_s,completion_pct,breaths,status
```

## Safety

Tummo-style breath retention can cause lightheadedness or loss of consciousness.
**Never practise breath retention near water or while driving.**
Run `breathe --safety` for full guidance before using the `tummo` program.

## Build and test

```sh
make          # build ./breathe
make test     # run all tests
```

Requires a C99 compiler and `libc` with POSIX extensions. No other dependencies.

## Credits

Based on [breathe-cli](https://github.com/marekkowalczyk/breathe-cli) by Marek Kowalczyk.
