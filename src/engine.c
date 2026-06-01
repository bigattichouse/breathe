#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#include "engine.h"

Engine   g_engines[MAX_ENGINES];
int      g_engine_count = 0;
Program  g_programs[MAX_PROGRAMS];
int      g_program_count = 0;

/* ------------------------------------------------------------------ helpers */

static void str_lower(char *dst, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n - 1 && src[i]; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* ---------------------------------------------------------------- built-ins */

static void add_engine_builtin(const char *name, const char *desc,
                               Phase *phases, int count)
{
    if (g_engine_count >= MAX_ENGINES) return;
    Engine *e = &g_engines[g_engine_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name,        name, sizeof(e->name)        - 1);
    strncpy(e->description, desc, sizeof(e->description) - 1);
    memcpy(e->phases, phases, count * sizeof(Phase));
    e->phase_count = count;
    e->is_builtin  = 1;
}

static void set_phase_hints(const char *h_inhale, const char *h_exhale,
                             const char *h_hold, const char *h_rapid)
{
    Engine *e = &g_engines[g_engine_count - 1];
    strncpy(e->phase_hints[PHASE_INHALE], h_inhale, sizeof(e->phase_hints[0]) - 1);
    strncpy(e->phase_hints[PHASE_EXHALE], h_exhale, sizeof(e->phase_hints[0]) - 1);
    strncpy(e->phase_hints[PHASE_HOLD],   h_hold,   sizeof(e->phase_hints[0]) - 1);
    strncpy(e->phase_hints[PHASE_RAPID],  h_rapid,  sizeof(e->phase_hints[0]) - 1);
}

static void add_program_builtin(const char *name, const char *desc,
                                const char *engine,
                                int dur_min, int rounds,
                                int *hold_targets, int htcount)
{
    if (g_program_count >= MAX_PROGRAMS) return;
    Program *p = &g_programs[g_program_count++];
    memset(p, 0, sizeof(*p));
    strncpy(p->name,        name,   sizeof(p->name)        - 1);
    strncpy(p->description, desc,   sizeof(p->description) - 1);
    strncpy(p->engine_name, engine, sizeof(p->engine_name) - 1);
    p->duration_min = dur_min;
    p->rounds       = rounds;
    p->is_builtin   = 1;
    if (hold_targets && htcount > 0) {
        int i;
        for (i = 0; i < htcount && i < 8; i++)
            p->hold_targets[i] = hold_targets[i];
        p->hold_target_count = htcount;
    }
}

void engine_init_builtins(void)
{
    {
        Phase ph[2] = {
            { PHASE_INHALE, DUR_FIXED, 5 },
            { PHASE_EXHALE, DUR_FIXED, 5 }
        };
        add_engine_builtin("resonance",
            "Paced 5s inhale / 5s exhale. Targets ~6 breaths/min, the "
            "cardiovascular resonance frequency linked to improved heart-rate "
            "variability and parasympathetic tone.",
            ph, 2);
        set_phase_hints(
            "In through your nose... let your belly rise.",
            "Out through your nose... let your belly fall.",
            "Hold gently... stay relaxed.",
            "");
    }
    {
        Phase ph[2] = {
            { PHASE_INHALE, DUR_FIXED, 4 },
            { PHASE_EXHALE, DUR_FIXED, 6 }
        };
        add_engine_builtin("calm",
            "Exhale-weighted 4s inhale / 6s exhale. The longer exhale "
            "emphasises parasympathetic activation, making it well-suited "
            "for winding down before sleep.",
            ph, 2);
        set_phase_hints(
            "In through your nose... let your belly rise.",
            "Out through your nose... let your belly fall.",
            "Hold gently... stay relaxed.",
            "");
    }
    {
        Phase ph[2] = {
            { PHASE_INHALE, DUR_FIXED, 4 },
            { PHASE_EXHALE, DUR_FIXED, 6 }
        };
        add_engine_builtin("extended",
            "Same 4:6 ratio as calm but used for longer 20-minute sessions "
            "matching the Bernardi resonance-breathing clinical trial protocol.",
            ph, 2);
        set_phase_hints(
            "In through your nose... let your belly rise.",
            "Out through your nose... let your belly fall.",
            "Hold gently... stay relaxed.",
            "");
    }
    {
        Phase ph[4] = {
            { PHASE_INHALE, DUR_FIXED, 4 },
            { PHASE_HOLD,   DUR_FIXED, 4 },
            { PHASE_EXHALE, DUR_FIXED, 4 },
            { PHASE_HOLD,   DUR_FIXED, 4 }
        };
        add_engine_builtin("box",
            "Equal 4s inhale / hold / exhale / hold (box or square breathing). "
            "Widely used by military and first-responders for rapid stress "
            "regulation and mental focus under pressure.",
            ph, 4);
        set_phase_hints(
            "In through your nose... let your belly rise.",
            "Out through your nose... let your belly fall.",
            "Hold gently... stay relaxed.",
            "");
    }
    {
        Phase ph[4] = {
            { PHASE_RAPID,  DUR_COUNT,  30 },
            { PHASE_HOLD,   DUR_TARGET, 30 },
            { PHASE_INHALE, DUR_FIXED,  4  },
            { PHASE_HOLD,   DUR_FIXED,  15 }
        };
        add_engine_builtin("tummo",
            "Tibetan 'inner fire' breathwork (popularised by Wim Hof): 30 rapid "
            "power breaths followed by an exhale retention hold, then a deep "
            "recovery inhale held 15s. Activates the sympathetic system and "
            "elevates core temperature. Do not practise near water.",
            ph, 4);
        set_phase_hints(
            "Deep in through your nose... fill completely.",
            "",
            "Hold... stay relaxed.",
            "Breathe through your mouth, pumping your belly...");
    }
    {
        Phase ph[3] = {
            { PHASE_INHALE, DUR_FIXED, 6 },
            { PHASE_HOLD,   DUR_FIXED, 2 },
            { PHASE_EXHALE, DUR_FIXED, 4 }
        };
        add_engine_builtin("energize",
            "Inhale-dominant 6s inhale / 2s hold / 4s exhale. The extended "
            "inhale activates the sympathetic nervous system, countering the "
            "mid-afternoon energy dip without caffeine.",
            ph, 3);
        set_phase_hints(
            "In through your nose... let your belly rise.",
            "Out through your nose... let your belly fall.",
            "Hold gently... stay alert.",
            "");
    }

    /* Programs */
    add_program_builtin("balanced",
        "10-minute resonance session. Good all-purpose default — auto-selected "
        "before noon.",
        "resonance", 10, 0, NULL, 0);
    add_program_builtin("calm",
        "15-minute exhale-weighted session. Auto-selected after 5 pm for "
        "evening wind-down.",
        "calm", 15, 0, NULL, 0);
    add_program_builtin("extended",
        "20-minute resonance session matching clinical trial durations. "
        "Auto-selected 12–2 pm.",
        "extended", 20, 0, NULL, 0);
    add_program_builtin("afternoon",
        "5-minute energizing session for the mid-afternoon slump. "
        "Auto-selected 2–5 pm.",
        "energize", 5, 0, NULL, 0);
    add_program_builtin("box",
        "10-minute box-breathing session for focus or acute stress relief.",
        "box", 10, 0, NULL, 0);

    {
        int targets[3] = { 30, 60, 90 };
        add_program_builtin("tummo",
            "3-round Tummo session with escalating exhale holds (30s / 60s / 90s). "
            "Consult --safety before use.",
            "tummo", 0, 3, targets, 3);
    }
}

/* -------------------------------------------------------------- lookup */

Engine *engine_find(const char *name)
{
    int i;
    char lower[64];
    str_lower(lower, name, sizeof(lower));
    for (i = 0; i < g_engine_count; i++) {
        char en[64];
        str_lower(en, g_engines[i].name, sizeof(en));
        if (strcmp(en, lower) == 0) return &g_engines[i];
    }
    return NULL;
}

Program *program_find(const char *name)
{
    int i;
    char lower[64];
    str_lower(lower, name, sizeof(lower));
    for (i = 0; i < g_program_count; i++) {
        char pn[64];
        str_lower(pn, g_programs[i].name, sizeof(pn));
        if (strcmp(pn, lower) == 0) return &g_programs[i];
    }
    return NULL;
}

/* -------------------------------------------------------------- spec parser */

/*
 * Spec format: "inhale:4, hold:7, exhale:8, hold:4"
 * phase keywords: inhale, exhale, hold, rapid
 * value optionally suffixed with 's' (seconds) or 'm' (minutes)
 * hold can have :target suffix: "hold:target(30)"
 */
int engine_parse_spec(const char *spec, Phase *out, int max_phases)
{
    const char *p = spec;
    int count = 0;

    while (*p && count < max_phases) {
        p = skip_ws(p);
        if (!*p) break;

        /* Skip commas */
        if (*p == ',') { p++; continue; }

        /* Read keyword */
        char kw[32];
        int ki = 0;
        while (*p && *p != ':' && *p != ',' && !isspace((unsigned char)*p) && ki < 31)
            kw[ki++] = *p++;
        kw[ki] = '\0';

        Phase ph;
        memset(&ph, 0, sizeof(ph));

        char kwl[32];
        str_lower(kwl, kw, sizeof(kwl));
        if (strcmp(kwl, "inhale") == 0) ph.type = PHASE_INHALE;
        else if (strcmp(kwl, "exhale") == 0) ph.type = PHASE_EXHALE;
        else if (strcmp(kwl, "hold")   == 0) ph.type = PHASE_HOLD;
        else if (strcmp(kwl, "rapid")  == 0) ph.type = PHASE_RAPID;
        else { /* unknown, skip to comma */
            while (*p && *p != ',') p++;
            continue;
        }

        ph.dur_type = DUR_FIXED;
        ph.value    = 4;

        if (*p == ':') {
            p++;
            p = skip_ws(p);

            /* Check for "target" keyword */
            if (strncasecmp(p, "target", 6) == 0) {
                ph.dur_type = DUR_TARGET;
                p += 6;
                if (*p == '(') {
                    p++;
                    ph.value = atoi(p);
                    while (*p && *p != ')') p++;
                    if (*p == ')') p++;
                }
                if (ph.value <= 0) ph.value = 5;
            } else if (strncasecmp(p, "count", 5) == 0) {
                ph.dur_type = DUR_COUNT;
                p += 5;
                if (*p == '(') {
                    p++;
                    ph.value = atoi(p);
                    while (*p && *p != ')') p++;
                    if (*p == ')') p++;
                }
                if (ph.value <= 0) ph.value = 1;
            } else {
                int v = atoi(p);
                while (*p && (isdigit((unsigned char)*p) || *p == '-')) p++;
                if (*p == 's') p++;
                else if (*p == 'm') { v *= 60; p++; }
                ph.value = (v > 0) ? v : 4;
                ph.dur_type = DUR_FIXED;
            }
        }

        out[count++] = ph;

        /* advance to next comma */
        while (*p && *p != ',') p++;
    }

    return count;
}

int engine_add(const char *name, const char *spec)
{
    if (engine_find(name)) return -1;
    if (g_engine_count >= MAX_ENGINES) return -1;

    Engine *e = &g_engines[g_engine_count];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->phase_count = engine_parse_spec(spec, e->phases, 16);
    e->is_builtin  = 0;
    g_engine_count++;
    return 0;
}

/*
 * Program spec: "engine:box rounds:5" or "engine:resonance duration:10m"
 */
int program_add(const char *name, const char *spec)
{
    if (program_find(name)) return -1;
    if (g_program_count >= MAX_PROGRAMS) return -1;

    Program *p = &g_programs[g_program_count];
    memset(p, 0, sizeof(*p));
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->is_builtin = 0;

    const char *q = spec;
    while (*q) {
        q = skip_ws(q);
        if (!*q) break;

        char kw[32];
        int ki = 0;
        while (*q && *q != ':' && !isspace((unsigned char)*q) && ki < 31)
            kw[ki++] = *q++;
        kw[ki] = '\0';

        if (*q == ':') q++;

        char val[64];
        int vi = 0;
        while (*q && !isspace((unsigned char)*q) && vi < 63)
            val[vi++] = *q++;
        val[vi] = '\0';

        char kwl[32];
        str_lower(kwl, kw, sizeof(kwl));
        if (strcmp(kwl, "engine") == 0) {
            snprintf(p->engine_name, sizeof(p->engine_name), "%s", val);
        } else if (strcmp(kwl, "duration") == 0) {
            int v = atoi(val);
            if (strchr(val, 'm')) v = atoi(val);
            p->duration_min = v;
        } else if (strcmp(kwl, "rounds") == 0) {
            p->rounds = atoi(val);
        }
    }
    g_program_count++;
    return 0;
}

int engine_delete(const char *name)
{
    int i;
    for (i = 0; i < g_engine_count; i++) {
        if (strcasecmp(g_engines[i].name, name) == 0) {
            if (g_engines[i].is_builtin) return -1;
            memmove(&g_engines[i], &g_engines[i+1],
                    (g_engine_count - i - 1) * sizeof(Engine));
            g_engine_count--;
            return 0;
        }
    }
    return -1;
}

int program_delete(const char *name)
{
    int i;
    for (i = 0; i < g_program_count; i++) {
        if (strcasecmp(g_programs[i].name, name) == 0) {
            if (g_programs[i].is_builtin) return -1;
            memmove(&g_programs[i], &g_programs[i+1],
                    (g_program_count - i - 1) * sizeof(Program));
            g_program_count--;
            return 0;
        }
    }
    return -1;
}

void engine_list(void)
{
    int i, j;
    printf("Engines:\n");
    for (i = 0; i < g_engine_count; i++) {
        Engine *e = &g_engines[i];
        printf("\n  \033[1m%s\033[0m%s\n    Phases: ",
               e->name, e->is_builtin ? "" : "  (user)");
        for (j = 0; j < e->phase_count; j++) {
            Phase *ph = &e->phases[j];
            const char *pname = (ph->type == PHASE_INHALE) ? "inhale" :
                                (ph->type == PHASE_EXHALE) ? "exhale" :
                                (ph->type == PHASE_HOLD)   ? "hold"   : "rapid";
            if (j > 0) printf(", ");
            if (ph->dur_type == DUR_COUNT)
                printf("%s:count(%d)", pname, ph->value);
            else if (ph->dur_type == DUR_TARGET)
                printf("%s:target(%ds)", pname, ph->value);
            else
                printf("%s:%ds", pname, ph->value);
        }
        printf("\n");
        if (e->description[0])
            printf("    %s\n", e->description);
    }
}

void program_list(void)
{
    int i;
    printf("Programs:\n");
    for (i = 0; i < g_program_count; i++) {
        Program *p = &g_programs[i];
        printf("\n  \033[1m%s\033[0m%s\n    ",
               p->name, p->is_builtin ? "" : "  (user)");
        if (p->rounds > 0)
            printf("engine:%-10s rounds:%d\n", p->engine_name, p->rounds);
        else
            printf("engine:%-10s duration:%d min\n", p->engine_name, p->duration_min);
        if (p->description[0])
            printf("    %s\n", p->description);
    }
}

Program *program_default(void)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    int hour = tm->tm_hour;

    const char *name;
    if (hour < 12)
        name = "balanced";
    else if (hour < 14)
        name = "extended";
    else if (hour < 17)
        name = "afternoon";
    else
        name = "calm";

    return program_find(name);
}

int engine_validate_ratio(int in, int ex)
{
    if (in < 3 || in > 10) return 0;
    if (ex < 3 || ex > 10) return 0;
    if (in + ex < 8) return 0;
    return 1;
}

double engine_phase_progress(PhaseType type, double elapsed, double duration)
{
    if (duration <= 0) return 0.0;
    double t = elapsed / duration;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    switch (type) {
        case PHASE_INHALE: return t;
        case PHASE_EXHALE: return 1.0 - t;
        case PHASE_HOLD:   return 1.0;
        case PHASE_RAPID: {
            double cycle = 2.5;
            double pos = fmod(elapsed, cycle);
            if (pos < 1.5) return pos / 1.5;
            else           return 1.0 - ((pos - 1.5) / 1.0);
        }
        default: return 0.0;
    }
}

void engine_reset(void)
{
    g_engine_count = 0;
    g_program_count = 0;
}
