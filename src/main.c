#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <math.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>

#include "engine.h"
#include "config.h"
#include "log.h"
#include "audio.h"
#include "tui.h"
#include "measure.h"

/* ----------------------------------------------------------------- globals */

static struct termios g_orig_termios;
static int            g_raw_mode = 0;
volatile int          g_interrupted = 0;

/* Session state (for signal handler) */
static const char    *g_preset_name      = "unknown";
static double         g_elapsed_s        = 0.0;
static int            g_breath_count     = 0;
static int            g_duration_s       = 0;
static int            g_no_log           = 0;
static int            g_inhale_s         = 5;
static int            g_exhale_s         = 5;

/* ---------------------------------------------------------------- termios */

static void term_raw(void)
{
    if (g_raw_mode) return;
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    struct termios raw = g_orig_termios;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON);  /* keep ISIG so Ctrl-C delivers SIGINT */
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw_mode = 1;
}

static void term_restore(void)
{
    if (!g_raw_mode) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
    g_raw_mode = 0;
}

/* ----------------------------------------------------------------- signal */

static void on_sigint(int sig)
{
    (void)sig;
    g_interrupted = 1;
}

static void cleanup_and_exit(int complete)
{
    term_restore();
    tui_cleanup();

    const char *status = complete ? "complete" : "interrupted";
    tui_summary(g_preset_name, g_elapsed_s, g_breath_count, status);

    if (!g_no_log) {
        double pct = (g_duration_s > 0)
                     ? (g_elapsed_s / g_duration_s * 100.0) : 100.0;
        if (pct > 100.0) pct = 100.0;
        log_session(g_preset_name,
                    g_inhale_s, g_exhale_s,
                    g_duration_s,
                    (int)g_elapsed_s,
                    g_breath_count,
                    pct,
                    status);
    }
    exit(0);
}

/* ---------------------------------------------------------------- timing */

static double mono_secs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ---------------------------------------------------------------- key read */

static int read_key(char *buf, int maxlen)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = { 0, 0 };
    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (r <= 0) return 0;
    int n = (int)read(STDIN_FILENO, buf, (size_t)maxlen);
    return (n > 0) ? n : 0;
}

/* ---------------------------------------------------------------- session loop */

typedef struct {
    Engine  *engine;
    Program *program;
    int      inhale_s;
    int      exhale_s;
    int      duration_s;   /* 0 if rounds-based */
    int      rounds;       /* 0 if duration-based */
    int      no_log;
    const char *preset_name;
} Session;

/*
 * Compute progress (0..1) within a phase based on elapsed time and phase type.
 * For inhale: fills 0→1; for exhale: fills 1→0.
 * For rapid: simulates mini cycles.
 */
static double phase_progress(PhaseType type, double elapsed, double duration)
{
    if (duration <= 0) return 0.0;
    double t = elapsed / duration;
    if (t < 0) t = 0;
    if (t > 1) t = 1;

    switch (type) {
        case PHASE_INHALE: return t;
        case PHASE_EXHALE: return 1.0 - t;
        case PHASE_HOLD:   return 1.0;  /* stays full */
        case PHASE_RAPID: {
            /* Rapid: 1.5s inhale + 1.0s exhale = 2.5s cycle */
            double cycle = 2.5;
            double pos   = fmod(elapsed, cycle);
            if (pos < 1.5) return pos / 1.5;
            else           return 1.0 - ((pos - 1.5) / 1.0);
        }
        default: return 0.0;
    }
}

static void run_session(Session *s)
{
    int      phase_idx   = 0;
    int      round_num   = 0;
    int      rapid_n     = 0;          /* breath counter within rapid phase */
    double   phase_start = mono_secs();
    double   session_start = phase_start;
    int      paused      = 0;
    double   pause_start = 0;
    double   total_pause = 0;
    int      extending   = 0;          /* spacebar held in DUR_TARGET */
    double   extend_start = 0;
    double   extend_total = 0;

    /* For audio: fire cue at start of each new phase */
    int      cue_fired   = 0;
    int      last_rapid_cycle = 0;

    Engine  *e = s->engine;
    Phase   *cur_phase = &e->phases[phase_idx];

    /* Phase effective duration */
    double phase_dur = (double)cur_phase->value;

    /* First cue */
    audio_play_cue((cur_phase->type == PHASE_INHALE) ? 0 :
                   (cur_phase->type == PHASE_EXHALE) ? 1 : 2);
    cue_fired = 1;

    while (1) {
        /* Check signal */
        if (g_interrupted) {
            cleanup_and_exit(0);
        }

        double now         = mono_secs();
        double session_elapsed = (now - session_start) - total_pause;
        g_elapsed_s = session_elapsed;

        /* Key handling */
        char keybuf[8];
        int  kn = read_key(keybuf, sizeof(keybuf));
        if (kn > 0) {
            char ch = keybuf[0];
            if (ch == 'q' || ch == 'Q') {
                cleanup_and_exit(0);
            } else if (ch == 's' || ch == 'S') {
                audio_cycle_mode();
                cue_fired = 0;  /* refire on next phase start */
            } else if (ch == ' ') {
                if (paused) {
                    /* Resume: reset to inhale phase */
                    paused         = 0;
                    total_pause   += now - pause_start;
                    phase_idx      = 0;
                    cur_phase      = &e->phases[0];
                    phase_dur      = (double)cur_phase->value;
                    phase_start    = now;
                    rapid_n        = 0;
                    extending      = 0;
                    extend_total   = 0;
                    cue_fired      = 0;
                    audio_play_cue(0);
                    cue_fired = 1;
                } else {
                    /* Pause */
                    paused     = 1;
                    pause_start = now;
                }
            }

            /* Spacebar hold for DUR_TARGET extension */
            if (!paused && cur_phase->dur_type == DUR_TARGET) {
                /* In raw mode spacebar press/release can't be tracked
                   directly; we use space as a toggle for extending */
                if (ch == ' ' && !paused) {
                    if (!extending) {
                        extending    = 1;
                        extend_start = now;
                    }
                }
            }
        }

        if (paused) {
            /* Still render paused state */
            int pulse_on = ((int)(now * 2) % 2) == 0;
            double remaining = phase_dur - (phase_start - session_start);
            tui_render(cur_phase->type, 1.0, remaining,
                       rapid_n, cur_phase->type == PHASE_RAPID ? cur_phase->value : 0,
                       session_elapsed,
                       s->duration_s,
                       s->preset_name,
                       s->inhale_s, s->exhale_s,
                       TUI_STATUS_PAUSED, pulse_on);

            struct timespec ts = { 0, 16000000L };
            nanosleep(&ts, NULL);
            continue;
        }

        double phase_elapsed = (now - phase_start);

        /* Phase advance logic */
        int advance_phase = 0;

        switch (cur_phase->dur_type) {
            case DUR_FIXED:
                if (phase_elapsed >= phase_dur)
                    advance_phase = 1;
                break;

            case DUR_TARGET:
                /* count down to target; if extending, don't auto-advance */
                if (!extending && phase_elapsed >= phase_dur)
                    advance_phase = 1;
                break;

            case DUR_USER_HELD:
                /* advance when user releases key (handled via spacebar) */
                break;

            case DUR_COUNT:
                /* RAPID: count breaths */
                if (cur_phase->type == PHASE_RAPID) {
                    double cycle_s  = 2.5;
                    int    cycle_n  = (int)(phase_elapsed / cycle_s);
                    if (cycle_n != last_rapid_cycle) {
                        last_rapid_cycle = cycle_n;
                        rapid_n = cycle_n;
                    }
                    if (rapid_n >= cur_phase->value)
                        advance_phase = 1;
                }
                break;
        }

        if (advance_phase) {
            /* Handle per-round hold targets for tummo */
            phase_idx++;
            if (phase_idx >= e->phase_count) {
                phase_idx = 0;
                round_num++;
                g_breath_count++;  /* count a full cycle */

                /* Duration check */
                if (s->duration_s > 0 && session_elapsed >= s->duration_s) {
                    cleanup_and_exit(1);
                    return;
                }
                /* Rounds check */
                if (s->rounds > 0 && round_num >= s->rounds) {
                    cleanup_and_exit(1);
                    return;
                }
            } else {
                /* If we just finished an exhale in a non-tummo engine,
                   count a breath */
                if (cur_phase->type == PHASE_EXHALE) {
                    g_breath_count++;
                    /* Duration check mid-cycle */
                    if (s->duration_s > 0 && session_elapsed >= s->duration_s) {
                        cleanup_and_exit(1);
                        return;
                    }
                }
            }

            cur_phase     = &e->phases[phase_idx];
            phase_dur     = (double)cur_phase->value;

            /* Override hold target for tummo per round */
            if (cur_phase->dur_type == DUR_TARGET &&
                s->program && round_num < s->program->hold_target_count) {
                phase_dur = (double)s->program->hold_targets[round_num];
            }

            phase_start   = now;
            phase_elapsed = 0.0;
            rapid_n       = 0;
            last_rapid_cycle = 0;
            extending     = 0;
            extend_total  = 0;
            cue_fired     = 0;

            /* Play cue for new phase */
            int cue_type = (cur_phase->type == PHASE_INHALE) ? 0 :
                           (cur_phase->type == PHASE_EXHALE) ? 1 : 2;
            audio_play_cue(cue_type);
            cue_fired = 1;
        }

        /* Compute display values */
        double remaining;
        if (cur_phase->dur_type == DUR_TARGET && extending) {
            /* show +Ns count-up since target reached */
            extend_total = (now - extend_start);
            remaining    = -extend_total;
        } else {
            remaining = phase_dur - phase_elapsed;
            if (remaining < 0) remaining = 0;
        }

        double progress = phase_progress(cur_phase->type, phase_elapsed, phase_dur);

        int pulse_on = 1;
        if (cur_phase->type == PHASE_HOLD) {
            pulse_on = ((int)(now * 2) % 2) == 0;
        }

        TuiStatus tstatus = TUI_STATUS_ACTIVE;
        if (g_sound_mode == SOUND_OFF) tstatus = TUI_STATUS_MUTED;

        int r_total = (cur_phase->type == PHASE_RAPID) ? cur_phase->value : 0;

        tui_render(cur_phase->type, progress, remaining,
                   rapid_n, r_total,
                   session_elapsed, s->duration_s,
                   s->preset_name,
                   s->inhale_s, s->exhale_s,
                   tstatus, pulse_on);

        (void)cue_fired;

        struct timespec ts = { 0, 16000000L };  /* ~60fps */
        nanosleep(&ts, NULL);
    }
}

/* ---------------------------------------------------------------- CLI */

static void usage(void)
{
    printf("Usage: breathe [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -p, --preset NAME          Select program preset\n");
    printf("  -d, --duration MINUTES     Session duration (1-60)\n");
    printf("      --ratio IN:EX          Inhale:exhale ratio (e.g. 4:6)\n");
    printf("      --inhale N             Inhale seconds (3-10)\n");
    printf("      --exhale N             Exhale seconds (3-10)\n");
    printf("  -n, --no-sound             Disable audio cues\n");
    printf("  -q, --quiet                Skip startup disclaimer\n");
    printf("      --no-log               Don't write to session log\n");
    printf("      --log                  Print log path and exit\n");
    printf("  -m, --measure              Measure mode\n");
    printf("      --safety               Print safety info and exit\n");
    printf("      --list                 List all presets\n");
    printf("      --list-engines         List engines\n");
    printf("      --list-programs        List programs\n");
    printf("      --version              Print version and exit\n");
    printf("      --create-engine NAME SPEC\n");
    printf("      --create-program NAME SPEC\n");
    printf("      --delete-engine NAME\n");
    printf("      --delete-program NAME\n");
}

static int validate_inhale_exhale(int in, int ex)
{
    if (in < 3 || in > 10) {
        fprintf(stderr, "Error: inhale must be 3-10 seconds\n");
        return 0;
    }
    if (ex < 3 || ex > 10) {
        fprintf(stderr, "Error: exhale must be 3-10 seconds\n");
        return 0;
    }
    if (in + ex < 8) {
        fprintf(stderr, "Error: inhale + exhale must be >= 8 seconds\n");
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    /* Initialize engine registry */
    engine_init_builtins();

    /* Load user config */
    config_load();

    /* Default config */
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Parse arguments */
    int i;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--version") == 0) {
            tui_print_version();
            return 0;
        } else if (strcmp(arg, "--safety") == 0) {
            tui_print_safety();
            return 0;
        } else if (strcmp(arg, "--log") == 0) {
            log_print_path();
            return 0;
        } else if (strcmp(arg, "--list") == 0 ||
                   strcmp(arg, "--list-programs") == 0) {
            program_list();
            return 0;
        } else if (strcmp(arg, "--list-engines") == 0) {
            engine_list();
            return 0;
        } else if (strcmp(arg, "--no-sound") == 0 ||
                   strcmp(arg, "-n") == 0) {
            cfg.no_sound = 1;
        } else if (strcmp(arg, "--quiet") == 0 ||
                   strcmp(arg, "-q") == 0) {
            cfg.quiet = 1;
        } else if (strcmp(arg, "--no-log") == 0) {
            cfg.no_log = 1;
        } else if (strcmp(arg, "--measure") == 0 ||
                   strcmp(arg, "-m") == 0) {
            cfg.measure_mode = 1;
        } else if ((strcmp(arg, "--preset") == 0 ||
                    strcmp(arg, "-p") == 0) && i + 1 < argc) {
            strncpy(cfg.preset, argv[++i], sizeof(cfg.preset) - 1);
        } else if ((strcmp(arg, "--duration") == 0 ||
                    strcmp(arg, "-d") == 0) && i + 1 < argc) {
            cfg.duration_min = atoi(argv[++i]);
            if (cfg.duration_min < 1 || cfg.duration_min > 60) {
                fprintf(stderr, "Error: duration must be 1-60 minutes\n");
                return 1;
            }
        } else if (strcmp(arg, "--ratio") == 0 && i + 1 < argc) {
            const char *ratio = argv[++i];
            sscanf(ratio, "%d:%d", &cfg.ratio_in, &cfg.ratio_ex);
            cfg.ratio_set = 1;
        } else if (strcmp(arg, "--inhale") == 0 && i + 1 < argc) {
            cfg.inhale_s = atoi(argv[++i]);
        } else if (strcmp(arg, "--exhale") == 0 && i + 1 < argc) {
            cfg.exhale_s = atoi(argv[++i]);
        } else if (strcmp(arg, "--create-engine") == 0 && i + 2 < argc) {
            const char *name = argv[++i];
            const char *spec = argv[++i];
            if (engine_find(name)) {
                fprintf(stderr, "Error: engine '%s' already exists\n", name);
                return 1;
            }
            if (engine_add(name, spec) == 0) {
                config_save_engine(name, spec);
                printf("Engine '%s' created.\n", name);
            } else {
                fprintf(stderr, "Error creating engine\n");
                return 1;
            }
            return 0;
        } else if (strcmp(arg, "--create-program") == 0 && i + 2 < argc) {
            const char *name = argv[++i];
            const char *spec = argv[++i];
            if (program_find(name)) {
                fprintf(stderr, "Error: program '%s' already exists\n", name);
                return 1;
            }
            if (program_add(name, spec) == 0) {
                config_save_program(name, spec);
                printf("Program '%s' created.\n", name);
            } else {
                fprintf(stderr, "Error creating program\n");
                return 1;
            }
            return 0;
        } else if (strcmp(arg, "--delete-engine") == 0 && i + 1 < argc) {
            const char *name = argv[++i];
            if (engine_delete(name) == 0) {
                config_delete_engine(name);
                printf("Engine '%s' deleted.\n", name);
            } else {
                fprintf(stderr, "Error: engine '%s' not found or is built-in\n", name);
                return 1;
            }
            return 0;
        } else if (strcmp(arg, "--delete-program") == 0 && i + 1 < argc) {
            const char *name = argv[++i];
            if (program_delete(name) == 0) {
                config_delete_program(name);
                printf("Program '%s' deleted.\n", name);
            } else {
                fprintf(stderr, "Error: program '%s' not found or is built-in\n", name);
                return 1;
            }
            return 0;
        } else if (arg[0] != '-') {
            /* Positional: treat as preset name */
            strncpy(cfg.preset, arg, sizeof(cfg.preset) - 1);
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            usage();
            return 1;
        }
    }

    /* Validate inhale/exhale if set */
    if (cfg.ratio_set) {
        if (!validate_inhale_exhale(cfg.ratio_in, cfg.ratio_ex)) return 1;
        cfg.inhale_s = cfg.ratio_in;
        cfg.exhale_s = cfg.ratio_ex;
    }
    if (cfg.inhale_s > 0 || cfg.exhale_s > 0) {
        int in = cfg.inhale_s > 0 ? cfg.inhale_s : 5;
        int ex = cfg.exhale_s > 0 ? cfg.exhale_s : 5;
        if (!validate_inhale_exhale(in, ex)) return 1;
        cfg.inhale_s = in;
        cfg.exhale_s = ex;
    }

    /* Measure mode */
    if (cfg.measure_mode) {
        if (!cfg.quiet) {
            printf("Measure mode: use arrow keys to track breathing phases.\n");
            printf("Up=inhale  Down=exhale  Left/Right=hold  X or Enter=end\n\n");
        }
        signal(SIGINT, on_sigint);
        term_raw();
        measure_run();
        term_restore();
        return 0;
    }

    /* Resolve program */
    Program *prog = NULL;
    if (cfg.preset[0]) {
        prog = program_find(cfg.preset);
        if (!prog) {
            fprintf(stderr, "Error: unknown preset '%s'\n", cfg.preset);
            return 1;
        }
    } else {
        prog = program_default();
    }

    if (!prog) {
        fprintf(stderr, "Error: no program found\n");
        return 1;
    }

    Engine *eng = engine_find(prog->engine_name);
    if (!eng) {
        fprintf(stderr, "Error: engine '%s' not found\n", prog->engine_name);
        return 1;
    }

    /* Build custom engine if --inhale/--exhale given */
    Engine custom_engine;
    if (cfg.inhale_s > 0 && cfg.exhale_s > 0) {
        memcpy(&custom_engine, eng, sizeof(Engine));
        /* Override inhale and exhale values */
        int j;
        for (j = 0; j < custom_engine.phase_count; j++) {
            if (custom_engine.phases[j].type == PHASE_INHALE)
                custom_engine.phases[j].value = cfg.inhale_s;
            else if (custom_engine.phases[j].type == PHASE_EXHALE)
                custom_engine.phases[j].value = cfg.exhale_s;
        }
        eng = &custom_engine;
    }

    /* Determine ratio for display */
    int display_inhale = cfg.inhale_s > 0 ? cfg.inhale_s : 0;
    int display_exhale = cfg.exhale_s > 0 ? cfg.exhale_s : 0;
    /* Find from engine if not overridden */
    if (display_inhale == 0 || display_exhale == 0) {
        int j;
        for (j = 0; j < eng->phase_count; j++) {
            if (eng->phases[j].type == PHASE_INHALE && display_inhale == 0)
                display_inhale = eng->phases[j].value;
            if (eng->phases[j].type == PHASE_EXHALE && display_exhale == 0)
                display_exhale = eng->phases[j].value;
        }
    }

    /* Duration */
    int duration_s = 0;
    int rounds     = prog->rounds;
    if (cfg.duration_min > 0) {
        duration_s = cfg.duration_min * 60;
        rounds     = 0;
    } else if (prog->duration_min > 0) {
        duration_s = prog->duration_min * 60;
    }

    /* Setup globals for signal handler */
    g_preset_name = prog->name;
    g_duration_s  = duration_s;
    g_no_log      = cfg.no_log;
    g_inhale_s    = display_inhale;
    g_exhale_s    = display_exhale;

    /* Safety disclaimer */
    if (!cfg.quiet) {
        printf("breathe: a terminal breathing guide. "
               "Run --safety for health warnings. "
               "Press q to quit.\n");
        fflush(stdout);
        /* Brief pause so user sees it */
        struct timespec ts = { 1, 0 };
        nanosleep(&ts, NULL);
    }

    /* Audio init */
    audio_init(cfg.no_sound);

    /* Setup signal handler */
    signal(SIGINT, on_sigint);

    /* Enter raw mode and init TUI */
    term_raw();
    tui_init();

    /* Run session */
    Session sess;
    memset(&sess, 0, sizeof(sess));
    sess.engine      = eng;
    sess.program     = prog;
    sess.inhale_s    = display_inhale;
    sess.exhale_s    = display_exhale;
    sess.duration_s  = duration_s;
    sess.rounds      = rounds;
    sess.no_log      = cfg.no_log;
    sess.preset_name = prog->name;

    run_session(&sess);

    /* Should not reach here normally */
    cleanup_and_exit(1);
    return 0;
}
