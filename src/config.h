#ifndef CONFIG_H
#define CONFIG_H

/* Parsed CLI + config-file options for one session */
typedef struct {
    char   preset[64];      /* program name */
    int    duration_min;    /* 0 = use program default */
    int    inhale_s;        /* 0 = use engine default */
    int    exhale_s;        /* 0 = use engine default */
    int    no_sound;        /* --no-sound / -n */
    int    quiet;           /* --quiet / -q */
    int    no_log;          /* --no-log */
    int    measure_mode;    /* --measure / -m */
    /* internal: filled by CLI parser */
    int    ratio_set;       /* --ratio was given */
    int    ratio_in;
    int    ratio_ex;
} Config;

/* Parse ~/.breatherc into engine/program registries.
   Returns 0 on success. */
int config_load(void);

/* Save a new engine/program entry to ~/.breatherc */
int config_save_engine(const char *name, const char *spec);
int config_save_program(const char *name, const char *spec);

int config_delete_engine(const char *name);
int config_delete_program(const char *name);

#endif /* CONFIG_H */
