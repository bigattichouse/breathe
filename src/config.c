#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>

#include "config.h"
#include "engine.h"

static int valid_config_name(const char *name)
{
    for (const char *p = name; *p; p++)
        if (*p == '"' || *p == '\n' || *p == '\r' || *p == '=') return 0;
    return strlen(name) > 0 && strlen(name) < 64;
}

static const char *config_path(void)
{
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/.breatherc", home);
    return path;
}

/*
 * Config file format:
 *   engine "name" = inhale:4, hold:7, exhale:8
 *   program "name" = engine:box rounds:5
 */
int config_load(void)
{
    FILE *f = fopen(config_path(), "r");
    if (!f) return 0;  /* not an error if missing */

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* skip comments and blanks */
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#') continue;

        /* Determine directive */
        int is_engine  = strncasecmp(p, "engine",  6) == 0;
        int is_program = strncasecmp(p, "program", 7) == 0;
        if (!is_engine && !is_program) continue;

        p += is_engine ? 6 : 7;
        while (*p == ' ' || *p == '\t') p++;

        /* Read quoted name */
        if (*p != '"') continue;
        p++;
        char name[64];
        int ni = 0;
        while (*p && *p != '"' && ni < 63)
            name[ni++] = *p++;
        name[ni] = '\0';
        if (*p == '"') p++;

        /* Skip to '=' */
        while (*p && *p != '=') p++;
        if (*p != '=') continue;
        p++;
        while (*p == ' ' || *p == '\t') p++;

        if (is_engine)
            engine_add(name, p);
        else
            program_add(name, p);
    }

    fclose(f);
    return 0;
}



int config_save_engine(const char *name, const char *spec)
{
    if (!valid_config_name(name)) return -1;
    const char *path = config_path();
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "engine \"%s\" = %s\n", name, spec);
    fclose(f);
    return 0;
}

int config_save_program(const char *name, const char *spec)
{
    if (!valid_config_name(name)) return -1;
    const char *path = config_path();
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "program \"%s\" = %s\n", name, spec);
    fclose(f);
    return 0;
}

/* Remove lines matching the given name from the config file */
static int config_delete_entry(const char *directive, const char *name)
{
    const char *path = config_path();
    char tmppath[520];
    snprintf(tmppath, sizeof(tmppath), "%s.tmp", path);

    FILE *in = fopen(path, "r");
    if (!in) return -1;
    FILE *out = fopen(tmppath, "w");
    if (!out) { fclose(in); return -1; }

    char line[512];
    char needle[128];
    snprintf(needle, sizeof(needle), "%s \"%s\"", directive, name);

    while (fgets(line, sizeof(line), in)) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncasecmp(p, needle, strlen(needle)) == 0)
            continue;  /* skip this line */
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    if (rename(tmppath, path) != 0) {
        unlink(tmppath);
        return -1;
    }
    return 0;
}

int config_delete_engine(const char *name)
{
    return config_delete_entry("engine", name);
}

int config_delete_program(const char *name)
{
    return config_delete_entry("program", name);
}

