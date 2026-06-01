#ifndef ENGINE_H
#define ENGINE_H

#include <stddef.h>

typedef enum {
    PHASE_INHALE,
    PHASE_EXHALE,
    PHASE_HOLD,
    PHASE_RAPID
} PhaseType;

typedef enum {
    DUR_FIXED,      /* countdown, auto-advance */
    DUR_TARGET,     /* countdown; spacebar held extends past target (count-up) */
    DUR_USER_HELD,  /* pure count-up; user releases key to advance */
    DUR_COUNT       /* for RAPID: n breath cycles then advance */
} DurType;

typedef struct {
    PhaseType type;
    DurType   dur_type;
    int       value;   /* seconds for DUR_FIXED/TARGET/USER_HELD, count for DUR_COUNT */
} Phase;

/* An engine is a named sequence of phases */
typedef struct {
    char   name[64];
    Phase  phases[16];
    int    phase_count;
    int    is_builtin;  /* 1 = cannot be deleted */
} Engine;

/* A program ties an engine to a duration/rounds, with optional per-round targets */
typedef struct {
    char   name[64];
    char   engine_name[64];
    int    duration_min;  /* 0 if rounds-based */
    int    rounds;        /* 0 if duration-based */
    int    hold_targets[8]; /* per-round hold targets for tummo-style (0 = use engine default) */
    int    hold_target_count;
    int    is_builtin;
} Program;

/* Registry */
#define MAX_ENGINES  64
#define MAX_PROGRAMS 64

extern Engine   g_engines[MAX_ENGINES];
extern int      g_engine_count;
extern Program  g_programs[MAX_PROGRAMS];
extern int      g_program_count;

void engine_init_builtins(void);

Engine  *engine_find(const char *name);
Program *program_find(const char *name);

/* Returns 0 on success, -1 if name already taken */
int engine_add(const char *name, const char *spec);
int program_add(const char *name, const char *spec);

int engine_delete(const char *name);   /* returns -1 if builtin or not found */
int program_delete(const char *name);

void engine_list(void);
void program_list(void);

/* Select default program by time of day */
Program *program_default(void);

/* Expose for testing */
int    engine_validate_ratio(int inhale_s, int exhale_s); /* 1=valid, 0=invalid */
double engine_phase_progress(PhaseType type, double elapsed, double duration);
int    engine_parse_spec(const char *spec, Phase *out, int max_phases);
void   engine_reset(void);  /* reset g_engine_count/g_program_count to 0 */

#endif /* ENGINE_H */
