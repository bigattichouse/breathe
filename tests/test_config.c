#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_runner.h"
#include "../src/engine.c"
#include "../src/config.c"

int main(void)
{
    /* Set up a temp HOME so we don't touch ~/.breatherc */
    char tmpdir[] = "/tmp/breathe_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "mkdtemp failed\n");
        return 1;
    }
    setenv("HOME", tmpdir, 1);

    /* ------------------------------------------------------------------ */
    SUITE("config_load with no file");
    engine_reset();
    engine_init_builtins();
    ASSERT_EQ(config_load(), 0, "config_load() with no file returns 0");

    /* ------------------------------------------------------------------ */
    SUITE("config_save_engine / config_load roundtrip");
    engine_reset();
    engine_init_builtins();
    ASSERT_EQ(config_save_engine("mytest", "inhale:3, exhale:5"), 0,
              "config_save_engine returns 0");
    ASSERT_EQ(config_load(), 0, "config_load after save returns 0");
    {
        Engine *e = engine_find("mytest");
        ASSERT_NOTNULL(e, "engine mytest found after load");
        ASSERT_EQ(e->phase_count, 2, "mytest phase_count==2");
        ASSERT_EQ((int)e->phases[0].type, (int)PHASE_INHALE, "mytest ph0 INHALE");
        ASSERT_EQ(e->phases[0].value, 3, "mytest ph0 value 3");
        ASSERT_EQ((int)e->phases[1].type, (int)PHASE_EXHALE, "mytest ph1 EXHALE");
        ASSERT_EQ(e->phases[1].value, 5, "mytest ph1 value 5");
    }

    /* ------------------------------------------------------------------ */
    SUITE("config_delete_engine");
    ASSERT_EQ(config_delete_engine("mytest"), 0, "config_delete_engine returns 0");
    /* reset and reload; mytest should be gone */
    engine_reset();
    engine_init_builtins();
    ASSERT_EQ(config_load(), 0, "config_load after delete returns 0");
    ASSERT_NULL(engine_find("mytest"), "mytest not found after delete+reload");

    /* ------------------------------------------------------------------ */
    SUITE("config_save_program / config_load roundtrip");
    ASSERT_EQ(config_save_program("myprog", "engine:box rounds:3"), 0,
              "config_save_program returns 0");
    engine_reset();
    engine_init_builtins();
    ASSERT_EQ(config_load(), 0, "config_load after save_program returns 0");
    {
        Program *p = program_find("myprog");
        ASSERT_NOTNULL(p, "program myprog found after load");
        ASSERT_EQ(p->rounds, 3, "myprog rounds==3");
    }

    /* ------------------------------------------------------------------ */
    SUITE("config_delete_program");
    ASSERT_EQ(config_delete_program("myprog"), 0, "config_delete_program returns 0");
    engine_reset();
    engine_init_builtins();
    ASSERT_EQ(config_load(), 0, "config_load after delete_program returns 0");
    ASSERT_NULL(program_find("myprog"), "myprog not found after delete+reload");

    /* ------------------------------------------------------------------ */
    SUITE("config file handles comments and blank lines");
    {
        /* Write a config file with comments and blanks */
        char cfgpath[600];
        snprintf(cfgpath, sizeof(cfgpath), "%s/.breatherc", tmpdir);
        FILE *f = fopen(cfgpath, "w");
        if (f) {
            fprintf(f, "# This is a comment\n");
            fprintf(f, "\n");
            fprintf(f, "engine \"commenttest\" = inhale:4, exhale:6\n");
            fprintf(f, "# another comment\n");
            fprintf(f, "\n");
            fclose(f);
        }
        engine_reset();
        engine_init_builtins();
        ASSERT_EQ(config_load(), 0, "config_load with comments returns 0");
        ASSERT_NOTNULL(engine_find("commenttest"),
                       "engine commenttest loaded despite comments/blanks");
    }

    /* ------------------------------------------------------------------ */
    SUITE("config save multiple engines appends all");
    {
        char cfgpath[600];
        snprintf(cfgpath, sizeof(cfgpath), "%s/.breatherc", tmpdir);
        /* Remove existing file */
        unlink(cfgpath);
        ASSERT_EQ(config_save_engine("eng1", "inhale:4, exhale:4"), 0,
                  "save eng1 returns 0");
        ASSERT_EQ(config_save_engine("eng2", "inhale:5, exhale:5"), 0,
                  "save eng2 returns 0");
        engine_reset();
        engine_init_builtins();
        ASSERT_EQ(config_load(), 0, "config_load with two engines returns 0");
        ASSERT_NOTNULL(engine_find("eng1"), "eng1 found");
        ASSERT_NOTNULL(engine_find("eng2"), "eng2 found");
    }

    /* Cleanup */
    {
        char cfgpath[600];
        snprintf(cfgpath, sizeof(cfgpath), "%s/.breatherc", tmpdir);
        unlink(cfgpath);
        rmdir(tmpdir);
    }

    TEST_RESULTS();
}
