#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "test_runner.h"
#include "../src/engine.c"

int main(void)
{
    /* ------------------------------------------------------------------ */
    SUITE("engine_init_builtins");
    engine_reset();
    engine_init_builtins();

    ASSERT_GT(g_engine_count,  5, "g_engine_count >= 6");
    ASSERT_GT(g_program_count, 5, "g_program_count >= 6");

    /* ------------------------------------------------------------------ */
    SUITE("engine_find");
    ASSERT_NOTNULL(engine_find("resonance"),    "find resonance");
    ASSERT_NOTNULL(engine_find("RESONANCE"),    "find RESONANCE case-insensitive");
    ASSERT_NULL   (engine_find("nonexistent"),  "find nonexistent == NULL");

    /* ------------------------------------------------------------------ */
    SUITE("resonance engine structure");
    {
        Engine *e = engine_find("resonance");
        ASSERT_NOTNULL(e, "resonance not NULL");
        ASSERT_EQ(e->phase_count, 2, "resonance phase_count == 2");
        ASSERT_EQ((int)e->phases[0].type,     (int)PHASE_INHALE, "resonance ph0 type INHALE");
        ASSERT_EQ((int)e->phases[0].dur_type, (int)DUR_FIXED,    "resonance ph0 DUR_FIXED");
        ASSERT_EQ(e->phases[0].value, 5, "resonance ph0 value 5");
        ASSERT_EQ((int)e->phases[1].type,     (int)PHASE_EXHALE, "resonance ph1 type EXHALE");
        ASSERT_EQ((int)e->phases[1].dur_type, (int)DUR_FIXED,    "resonance ph1 DUR_FIXED");
        ASSERT_EQ(e->phases[1].value, 5, "resonance ph1 value 5");
    }

    /* ------------------------------------------------------------------ */
    SUITE("calm engine structure");
    {
        Engine *e = engine_find("calm");
        ASSERT_NOTNULL(e, "calm not NULL");
        ASSERT_EQ((int)e->phases[0].type, (int)PHASE_INHALE, "calm ph0 INHALE");
        ASSERT_EQ(e->phases[0].value, 4, "calm ph0 inhale:4");
        ASSERT_EQ((int)e->phases[1].type, (int)PHASE_EXHALE, "calm ph1 EXHALE");
        ASSERT_EQ(e->phases[1].value, 6, "calm ph1 exhale:6");
    }

    /* ------------------------------------------------------------------ */
    SUITE("box engine structure");
    {
        Engine *e = engine_find("box");
        ASSERT_NOTNULL(e, "box not NULL");
        ASSERT_EQ(e->phase_count, 4, "box phase_count == 4");
        ASSERT_EQ((int)e->phases[0].type, (int)PHASE_INHALE, "box ph0 INHALE");
        ASSERT_EQ(e->phases[0].value, 4, "box ph0 value 4");
        ASSERT_EQ((int)e->phases[1].type, (int)PHASE_HOLD,   "box ph1 HOLD");
        ASSERT_EQ(e->phases[1].value, 4, "box ph1 value 4");
        ASSERT_EQ((int)e->phases[2].type, (int)PHASE_EXHALE, "box ph2 EXHALE");
        ASSERT_EQ(e->phases[2].value, 4, "box ph2 value 4");
        ASSERT_EQ((int)e->phases[3].type, (int)PHASE_HOLD,   "box ph3 HOLD");
        ASSERT_EQ(e->phases[3].value, 4, "box ph3 value 4");
    }

    /* ------------------------------------------------------------------ */
    SUITE("tummo engine structure");
    {
        Engine *e = engine_find("tummo");
        ASSERT_NOTNULL(e, "tummo not NULL");
        ASSERT_EQ(e->phase_count, 4, "tummo phase_count == 4");
        ASSERT_EQ((int)e->phases[0].type,     (int)PHASE_RAPID, "tummo ph0 RAPID");
        ASSERT_EQ((int)e->phases[0].dur_type, (int)DUR_COUNT,   "tummo ph0 DUR_COUNT");
        ASSERT_EQ(e->phases[0].value, 30, "tummo ph0 value 30");
        ASSERT_EQ((int)e->phases[1].type,     (int)PHASE_HOLD,   "tummo ph1 HOLD");
        ASSERT_EQ((int)e->phases[1].dur_type, (int)DUR_TARGET,   "tummo ph1 DUR_TARGET");
        ASSERT_EQ(e->phases[1].value, 30, "tummo ph1 value 30");
    }

    /* ------------------------------------------------------------------ */
    SUITE("energize engine structure");
    {
        Engine *e = engine_find("energize");
        ASSERT_NOTNULL(e, "energize not NULL");
        ASSERT_EQ(e->phase_count, 3, "energize phase_count == 3");
        ASSERT_EQ((int)e->phases[0].type,     (int)PHASE_INHALE, "energize ph0 INHALE");
        ASSERT_EQ((int)e->phases[0].dur_type, (int)DUR_FIXED,    "energize ph0 DUR_FIXED");
        ASSERT_EQ(e->phases[0].value, 6, "energize ph0 value 6");
        ASSERT_EQ((int)e->phases[1].type,     (int)PHASE_HOLD,   "energize ph1 HOLD");
        ASSERT_EQ((int)e->phases[1].dur_type, (int)DUR_FIXED,    "energize ph1 DUR_FIXED");
        ASSERT_EQ(e->phases[1].value, 2, "energize ph1 value 2");
        ASSERT_EQ((int)e->phases[2].type,     (int)PHASE_EXHALE, "energize ph2 EXHALE");
        ASSERT_EQ((int)e->phases[2].dur_type, (int)DUR_FIXED,    "energize ph2 DUR_FIXED");
        ASSERT_EQ(e->phases[2].value, 4, "energize ph2 value 4");
        ASSERT_EQ(e->is_builtin, 1, "energize is_builtin==1");
    }

    /* ------------------------------------------------------------------ */
    SUITE("program_find");
    {
        Program *p = program_find("balanced");
        ASSERT_NOTNULL(p, "find balanced");
        ASSERT_STR(p->engine_name, "resonance", "balanced engine_name==resonance");
        ASSERT_EQ(p->duration_min, 10, "balanced duration_min==10");
    }
    {
        Program *p = program_find("tummo");
        ASSERT_NOTNULL(p, "find tummo");
        ASSERT_EQ(p->rounds, 3, "tummo rounds==3");
        ASSERT_EQ(p->hold_target_count, 3, "tummo hold_target_count==3");
        ASSERT_EQ(p->hold_targets[0], 30, "tummo hold_targets[0]==30");
        ASSERT_EQ(p->hold_targets[1], 60, "tummo hold_targets[1]==60");
        ASSERT_EQ(p->hold_targets[2], 90, "tummo hold_targets[2]==90");
    }
    ASSERT_NOTNULL(program_find("TUMMO"), "find TUMMO case-insensitive");
    {
        Program *p = program_find("afternoon");
        ASSERT_NOTNULL(p, "find afternoon");
        ASSERT_STR(p->engine_name, "energize", "afternoon engine_name==energize");
        ASSERT_EQ(p->duration_min, 5, "afternoon duration_min==5");
        ASSERT_EQ(p->is_builtin, 1, "afternoon is_builtin==1");
    }

    /* ------------------------------------------------------------------ */
    SUITE("builtin protection");
    {
        Engine *e = engine_find("resonance");
        ASSERT_EQ(e->is_builtin, 1, "resonance is_builtin==1");
        ASSERT_EQ(engine_delete("resonance"), -1, "delete resonance returns -1");
    }
    {
        Program *p = program_find("balanced");
        ASSERT_EQ(p->is_builtin, 1, "balanced is_builtin==1");
        ASSERT_EQ(program_delete("balanced"), -1, "delete balanced returns -1");
    }

    /* ------------------------------------------------------------------ */
    SUITE("engine_add / engine_delete");
    ASSERT_EQ(engine_add("test478", "inhale:4, hold:7, exhale:8"), 0,
              "engine_add test478 returns 0");
    {
        Engine *e = engine_find("test478");
        ASSERT_NOTNULL(e, "find test478 after add");
        ASSERT_EQ(e->phase_count, 3, "test478 phase_count==3");
        ASSERT_EQ((int)e->phases[0].type, (int)PHASE_INHALE, "test478 ph0 INHALE");
        ASSERT_EQ(e->phases[0].value, 4, "test478 ph0 value 4");
        ASSERT_EQ((int)e->phases[1].type, (int)PHASE_HOLD, "test478 ph1 HOLD");
        ASSERT_EQ(e->phases[1].value, 7, "test478 ph1 value 7");
        ASSERT_EQ((int)e->phases[2].type, (int)PHASE_EXHALE, "test478 ph2 EXHALE");
        ASSERT_EQ(e->phases[2].value, 8, "test478 ph2 value 8");
    }
    ASSERT_EQ(engine_add("test478", "inhale:4"), -1,
              "engine_add duplicate returns -1");
    ASSERT_EQ(engine_delete("test478"), 0, "engine_delete test478 returns 0");
    ASSERT_NULL(engine_find("test478"), "find test478 after delete == NULL");
    ASSERT_EQ(engine_delete("nonexistent"), -1, "delete nonexistent returns -1");

    /* ------------------------------------------------------------------ */
    SUITE("program_add / program_delete");
    ASSERT_EQ(program_add("mybox", "engine:box rounds:5"), 0,
              "program_add mybox returns 0");
    {
        Program *p = program_find("mybox");
        ASSERT_NOTNULL(p, "find mybox after add");
        ASSERT_EQ(p->rounds, 5, "mybox rounds==5");
    }
    ASSERT_EQ(program_delete("mybox"), 0, "program_delete mybox returns 0");
    ASSERT_NULL(program_find("mybox"), "find mybox after delete == NULL");

    /* ------------------------------------------------------------------ */
    SUITE("engine_parse_spec");
    {
        Phase phases[16];
        int n;

        n = engine_parse_spec("inhale:4, hold:7, exhale:8", phases, 16);
        ASSERT_EQ(n, 3, "parse 3-phase spec returns 3");
        ASSERT_EQ((int)phases[0].type,     (int)PHASE_INHALE, "parsed ph0 INHALE");
        ASSERT_EQ((int)phases[0].dur_type, (int)DUR_FIXED,    "parsed ph0 DUR_FIXED");
        ASSERT_EQ(phases[0].value, 4, "parsed ph0 value 4");
        ASSERT_EQ((int)phases[1].type,  (int)PHASE_HOLD, "parsed ph1 HOLD");
        ASSERT_EQ(phases[1].value, 7, "parsed ph1 value 7");
        ASSERT_EQ((int)phases[2].type,  (int)PHASE_EXHALE, "parsed ph2 EXHALE");
        ASSERT_EQ(phases[2].value, 8, "parsed ph2 value 8");

        n = engine_parse_spec("inhale:4, hold:4, exhale:4, hold:4", phases, 16);
        ASSERT_EQ(n, 4, "parse 4-phase box spec returns 4");

        n = engine_parse_spec("rapid:count(30)", phases, 16);
        ASSERT_EQ(n, 1, "parse rapid:count(30) returns 1");
        ASSERT_EQ((int)phases[0].type,     (int)PHASE_RAPID, "rapid type RAPID");
        ASSERT_EQ((int)phases[0].dur_type, (int)DUR_COUNT,   "rapid DUR_COUNT");
        ASSERT_EQ(phases[0].value, 30, "rapid value 30");

        n = engine_parse_spec("hold:target(60)", phases, 16);
        ASSERT_EQ(n, 1, "parse hold:target(60) returns 1");
        ASSERT_EQ((int)phases[0].type,     (int)PHASE_HOLD,   "hold target type HOLD");
        ASSERT_EQ((int)phases[0].dur_type, (int)DUR_TARGET,   "hold target DUR_TARGET");
        ASSERT_EQ(phases[0].value, 60, "hold target value 60");

        n = engine_parse_spec("", phases, 16);
        ASSERT_EQ(n, 0, "parse empty spec returns 0");

        n = engine_parse_spec("inhale:3s, exhale:5s", phases, 16);
        ASSERT_EQ(n, 2, "parse with 's' suffix returns 2");
        ASSERT_EQ(phases[0].value, 3, "parsed 's' suffix inhale value 3");
        ASSERT_EQ(phases[1].value, 5, "parsed 's' suffix exhale value 5");
    }

    /* ------------------------------------------------------------------ */
    SUITE("engine_validate_ratio");
    ASSERT_EQ(engine_validate_ratio(5,  5),  1, "5:5 valid");
    ASSERT_EQ(engine_validate_ratio(2,  6),  0, "2:6 invalid (inhale < 3)");
    ASSERT_EQ(engine_validate_ratio(5, 11),  0, "5:11 invalid (exhale > 10)");
    ASSERT_EQ(engine_validate_ratio(3,  4),  0, "3:4 invalid (sum=7 < 8)");
    ASSERT_EQ(engine_validate_ratio(3,  5),  1, "3:5 valid (sum=8 minimum)");
    ASSERT_EQ(engine_validate_ratio(10, 10), 1, "10:10 valid");
    ASSERT_EQ(engine_validate_ratio(11,  5), 0, "11:5 invalid (inhale > 10)");

    /* ------------------------------------------------------------------ */
    SUITE("engine_phase_progress");
    ASSERT_NEAR(engine_phase_progress(PHASE_INHALE, 0.0, 5.0), 0.0, 1e-9,
                "inhale t=0 progress 0.0");
    ASSERT_NEAR(engine_phase_progress(PHASE_INHALE, 2.5, 5.0), 0.5, 1e-9,
                "inhale t=0.5 progress 0.5");
    ASSERT_NEAR(engine_phase_progress(PHASE_INHALE, 5.0, 5.0), 1.0, 1e-9,
                "inhale t=1 progress 1.0");
    ASSERT_NEAR(engine_phase_progress(PHASE_EXHALE, 0.0, 5.0), 1.0, 1e-9,
                "exhale t=0 progress 1.0");
    ASSERT_NEAR(engine_phase_progress(PHASE_EXHALE, 2.5, 5.0), 0.5, 1e-9,
                "exhale t=0.5 progress 0.5");
    ASSERT_NEAR(engine_phase_progress(PHASE_EXHALE, 5.0, 5.0), 0.0, 1e-9,
                "exhale t=1 progress 0.0");
    ASSERT_NEAR(engine_phase_progress(PHASE_HOLD, 2.5, 5.0), 1.0, 1e-9,
                "hold always 1.0");
    ASSERT_NEAR(engine_phase_progress(PHASE_RAPID, 0.0,  10.0), 0.0,    1e-9,
                "rapid t=0 progress 0.0");
    ASSERT_NEAR(engine_phase_progress(PHASE_RAPID, 1.0,  10.0), 0.5,    1e-6,
                "rapid t=1.0 mid-inhale 0.5");
    ASSERT_NEAR(engine_phase_progress(PHASE_RAPID, 2.0,  10.0), 1.0,    1e-9,
                "rapid t=2.0 peak 1.0");
    ASSERT_NEAR(engine_phase_progress(PHASE_RAPID, 2.75, 10.0), 0.5,    1e-9,
                "rapid t=2.75 mid-exhale 0.5");
    ASSERT_NEAR(engine_phase_progress(PHASE_RAPID, 3.5,  10.0), 0.0,    1e-9,
                "rapid t=3.5 end-of-cycle 0.0");

    TEST_RESULTS();
}
