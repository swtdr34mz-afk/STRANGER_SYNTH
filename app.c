/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (OFFWORLD) */
/* OFFWORLD: input, the knobs and their sweetspots, DRONE, EPIC, the low split and the melody that follows it,
 * pressure, the pages, settings. Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. The sound is dsp/engine.c, the film on the screen film.c.
 *
 * Controls (v0.2)
 *   the keys        the brass, with velocity (harder = brighter) and pressure (PRESSURE); OCT- / OCT+ the octave
 *   K1..K4          MAIN: SWELL, DRIFT, SPACE, RAIN, macros resting on sweetspots
 *                   SPLIT (SEL): SPLIT (off, 1..3 octaves down), POINT, SCALE, FOLLOW (OFF, SNAP, WHITE)
 *                   PRESSURE (GLO): PRESSURE (OFF, FILTER, SWELL, POWER, VIBRATO), AMOUNT, TONE, VIBRATO
 *   SEL             the SPLIT page (again: MAIN); held + a key: the split there
 *   GLO             the PRESSURE page (again: MAIN)       HOME  MAIN; held 1 s: the preset's knobs as they came
 *   PRESET          the preset (each keeps its own knobs)
 *   SELECT          TONE              ALGORITHM  VIBRATO
 *   PLAY            DRONE: every note stays (a key with no key down on its side starts a new chord), a deep root
 *   SEQ             LOW HOLD: the low split's notes stay (a new low note replaces them); the right hand plays free
 *   ARP             the ARPEGGIATOR on / off (with the split on it plays the low split, else every key);
 *                   held 1 s: EPIC (root, fifth and octave from one finger)
 *   LFO             the ARP page: RATE, PATTERN, OCTAVES, GATE; SELECT there: the tempo (BPM; MIDI clock in wins)
 *   REC held 1 s    every note let go
 * The melody: with the split on and FOLLOW on, the right hand plays in SCALE from the low split's note (the lowest
 * that sounds, or the last one): SNAP moves every key to the scale's note at or below it, WHITE puts the scale on the
 * white keys (the black keys rest). */
#include "fastmath.h"
#include "engine.h"

#define SET_MAGIC 0x31595353u            /* "SSY1": STRANGER SYNTH's settings record (an OFFWORLD's is not taken over) */
#define NCUE 8
#define CUE_NOTES 6
typedef struct {
    uint32_t magic;
    uint8_t m[NPRESETS][4];              /* every preset's knobs */
    uint8_t preset;
    int8_t tone;                         /* -50 .. 50 */
    uint8_t vib;                         /* 0 .. 100 */
    int8_t octave;                       /* -2 .. 1 */
    uint8_t epic;
    uint8_t split_oct;                   /* 0 off, 1..3 octaves down */
    uint8_t split_key;                   /* the right part's first key 1..26 */
    uint8_t scale;                       /* SCALES */
    uint8_t follow;                      /* FOLLOW_* */
    uint8_t press;                       /* PRESS_* */
    uint8_t pamt;                        /* 0 .. 100 */
    uint8_t glide_l, mode_l;             /* GLIDE: the low split's time 0..100 and GM_* */
    uint8_t glide_h, mode_h;             /* and the right hand's (the whole keyboard with the split off) */
    uint8_t city;                        /* WORLD: the scenes' sound 0..100 */
    uint8_t story;                       /* the night as a story: on / off */
    uint8_t logs;                        /* the little stories on the screen: on / off */
    uint8_t cue_fade;                    /* CUES: how long a change slides 0..100 (0.3 .. 30 s) */
    uint8_t cue_hold;                    /* how long each cue stays with AUTO, s 4..60 */
    uint8_t cue_auto;
    uint8_t cue_n[NCUE];                 /* each cue's notes: how many, which (MIDI) */
    uint8_t cue_p[NCUE][CUE_NOTES];
    uint8_t arp;                         /* the ARPEGGIATOR on / off */
    uint8_t arp_rate, arp_pat, arp_oct;  /* RATE_*, ARP_*, octaves 1..4 */
    uint8_t arp_gate;                    /* 5 .. 100 % of a step */
    uint8_t bpm;                         /* its own tempo 40 .. 240 */
    uint8_t rsv[2];
} settings_t;

static settings_t set;
static uint8_t set_dirty;
static uint32_t dirty_at;

#define DEEP_ID 999                      /* the drone's deep root */
#define MIDI_ID 300                      /* MIDI notes: 300 + note (+100, +200: EPIC's fifth and octave) */

#define C_CYAN RGB(60, 230, 255)
#define C_MAG RGB(255, 60, 170)
#define C_AMBER RGB(255, 170, 60)
#define C_FIRE RGB(255, 120, 30)
#define C_RAIN RGB(110, 130, 168)
#define C_RED RGB(255, 40, 40)

/* ------------------------------------------------------------------ scales --- */
typedef struct {
    const char *name;
    uint8_t n;
    uint8_t st[7];
} scale_t;
static const scale_t SCALES[] = {
    {"MINOR", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"DORIAN", 7, {0, 2, 3, 5, 7, 9, 10}},
    {"PHRYGIAN", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"HARM MINOR", 7, {0, 2, 3, 5, 7, 8, 11}},
    {"MAJOR", 7, {0, 2, 4, 5, 7, 9, 11}},
    {"LYDIAN", 7, {0, 2, 4, 6, 7, 9, 11}},
    {"PENTATONIC", 5, {0, 3, 5, 7, 10}},
    {"ARABIC", 7, {0, 1, 4, 5, 7, 8, 11}},
};
#define NSCALE (sizeof SCALES / sizeof SCALES[0])
enum { FOLLOW_OFF, FOLLOW_SNAP, FOLLOW_WHITE, NFOLLOW };
/* GLIDE's modes: POLY every note its own voice, gliding from the last note of its side; MONO one voice a side, a new
 * key slides it there (no new attack while one sounds), always gliding; LEGATO as MONO, but gliding only legato */
enum { GM_POLY, GM_MONO, GM_LEGATO, NGMODE };
static const char *const GMODE_NAME[NGMODE] = {"POLY", "MONO", "LEGATO"};
static const char *const FOLLOW_NAME[NFOLLOW] = {"OFF", "SNAP", "WHITE"};
static const char *const PRESS_NAME[NPRESS] = {"OFF", "FILTER", "SWELL", "POWER", "VIBRATO"};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* --------------------------------------------------------------- settings --- */
static void settings_v4(void)            /* the parts 0.4 added, as they start */
{
    uint32_t i;
    set.city = 60;
    set.story = 1;
    set.logs = 1;
    set.cue_fade = 45;
    set.cue_hold = 16;
    set.cue_auto = 0;
    for (i = 0; i < NCUE; i++)
        set.cue_n[i] = 0;
}

static void settings_v2(void)            /* the parts 0.2 added, as they start */
{
    set.split_oct = 0;
    set.split_key = 7;                   /* F3..B3 left, C4 the right's first */
    set.scale = 0;
    set.follow = FOLLOW_WHITE;
    set.press = PRESS_FILTER;
    set.pamt = 70;
}

static void settings_v3(void)            /* GLIDE, as it starts: off, every note its own voice */
{
    set.glide_l = set.glide_h = 0;
    set.mode_l = set.mode_h = 0;
}

static void settings_defaults(void)
{
    uint32_t i, k;
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    for (i = 0; i < NPRESETS; i++)
        for (k = 0; k < 4u; k++)
            set.m[i][k] = PRESETS[i].m[k];
    set.vib = 30;
    settings_v2();
    settings_v3();
    settings_v4();
    set.arp = 0;
    set.arp_rate = RATE_16;
    set.arp_pat = ARP_UP;
    set.arp_oct = 1;
    set.arp_gate = 50;
    set.bpm = 110;
}

static int cues_ok(void)
{
    uint32_t i;
    for (i = 0; i < NCUE; i++)
        if (set.cue_n[i] > CUE_NOTES)
            return 0;
    return 1;
}

static int settings_ok(void)
{
    uint32_t i, k;
    for (i = 0; i < NPRESETS; i++)
        for (k = 0; k < 4u; k++)
            if (set.m[i][k] > 127u)
                return 0;
    return set.magic == SET_MAGIC && set.preset < NPRESETS && set.tone >= -50 && set.tone <= 50 && set.vib <= 100u &&
           set.octave >= -2 && set.octave <= 1 && set.epic <= 1u && set.split_oct <= 3u && set.split_key >= 1u &&
           set.split_key < NKEYS && set.scale < NSCALE && set.follow < NFOLLOW && set.press < NPRESS &&
           set.pamt <= 100u && set.glide_l <= 100u && set.glide_h <= 100u && set.mode_l < NGMODE && set.mode_h < NGMODE &&
           set.city <= 100u && set.story <= 1u && set.logs <= 1u && set.cue_fade <= 100u && set.cue_hold >= 4u &&
           set.cue_hold <= 60u && set.cue_auto <= 1u && cues_ok() && set.arp <= 1u && set.arp_rate < NRATE &&
           set.arp_pat < NARPPAT && set.arp_oct >= 1u && set.arp_oct <= 4u && set.arp_gate >= 5u && set.arp_gate <= 100u &&
           set.bpm >= 40u && set.bpm <= 240u;
}

static void settings_load(void)
{
    int r;
    memset(&set, 0, sizeof set);
    r = plat_store_load(OBJ_SETTINGS, &set, sizeof set);
    if (r != (int)sizeof(settings_t) || set.magic != SET_MAGIC || !settings_ok())
        settings_defaults();
}

static void dirty(void)
{
    set_dirty = 1;
    dirty_at = plat_ms();
}

static void settings_save(void)
{
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0)
        set_dirty = 0;
}

#define KM (set.m[set.preset])           /* the picked preset's knobs */
#define PR (PRESETS[set.preset])

static void sound(void);                 /* (after the night: RAIN follows it) */

/* ------------------------------------------------------- knobs: sweetspots --- */
#define REST 5
static int32_t knob_pos[4];

static int32_t pos_max(const macro_t *mc) { return 127 + mc->nzones * REST; }

static int32_t val_of_pos(const macro_t *mc, int32_t p)
{
    uint32_t z;
    for (z = 0; z < mc->nzones; z++) {
        int32_t c = mc->centre[z];
        if (p <= c)
            return p;
        if (p <= c + REST)
            return c;
        p -= REST;
    }
    return p > 127 ? 127 : p;
}

static int32_t pos_of_val(const macro_t *mc, int32_t v)
{
    int32_t p = v;
    uint32_t z;
    for (z = 0; z < mc->nzones; z++) {
        if (mc->centre[z] < v)
            p += REST;
        else if (mc->centre[z] == v)
            p += REST / 2;
    }
    return p;
}

static int zone_at(const macro_t *mc, int32_t v)
{
    uint32_t z;
    for (z = 0; z < mc->nzones; z++)
        if (mc->centre[z] == v)
            return (int)z;
    return -1;
}

static int zone_near(const macro_t *mc, int32_t v)
{
    uint32_t z;
    int best = 0;
    int32_t d = 999;
    for (z = 0; z < mc->nzones; z++) {
        int32_t e = v - mc->centre[z];
        if (e < 0)
            e = -e;
        if (e < d) {
            d = e;
            best = (int)z;
        }
    }
    return best;
}

static void knobs_from_settings(void)
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        knob_pos[k] = pos_of_val(&MACROS[k], KM[k]);
}

/* ---------------------------------------------------------------------- UI --- */
enum { PG_MAIN, PG_SPLIT, PG_PRESS, PG_GLIDE, PG_WORLD, PG_CUES, PG_ARP, NPAGE };
enum { SIDE_L, SIDE_R };
static struct {
    char msg[2][24];
    uint32_t msg_until;
    int8_t zone_k, zone_z;
    uint32_t zone_until;
    uint32_t knob_at[4];
    uint32_t keys, btn;                  /* held last frame */
    uint32_t latched;                    /* keys whose notes stay (DRONE, LOW HOLD) */
    uint32_t sounding;                   /* keys whose notes sound now (held or latched) */
    float kp[NKEYS];                     /* the pitch each key started at */
    uint8_t prs[NKEYS];                  /* each key's pressure as last sent */
    uint8_t drone, lowhold, page;
    uint8_t root;                        /* the melody's root: the low split's note, 0..11 */
    float deep;                          /* the deep root's pitch (0: none) */
    uint32_t rec_t, home_t, sel_t, arp_t;
    uint8_t sel_used;
    uint32_t enc_at[NE];
    int32_t enc_acc[4];
    uint8_t redraw, head_redraw;
    uint8_t intro;
    uint32_t intro_t0;
    uint32_t scene_at;
    uint8_t win[NKEYS];                  /* each key's light 0..255 (the film's lights follow them) */
    uint8_t mono[2];                     /* MONO / LEGATO: the key whose note sounds on that side, + 1 (0: none) */
    float last_p[2];                     /* the last pitch played on each side (where a glide starts) */
    uint8_t has_last[2];
    uint8_t kvel[NKEYS];
    uint32_t ptime[NKEYS], pcount;       /* the order the keys went down (MONO goes back to the last one held) */
    uint8_t memory;                      /* MEMORY: the last seconds frozen */
    uint8_t sheet;                       /* the controls drawn over the screen (HOME twice) */
    uint32_t home_last;
    int32_t story_off;                   /* the night: where it is, ms (the clock runs from 19:00) */
    uint8_t rain_now;                    /* RAIN as it plays (with the story: the night's weather) */
    uint8_t cue_sel, cue_cur;            /* CUES: the slot picked (0: none / stop), the one sounding */
    uint8_t cue_pn;                      /* the notes sounding from the cues, their pitches */
    uint8_t cue_pp[CUE_NOTES];
    uint32_t cue_at;
    uint32_t save_t;
    uint8_t ducking;
    uint32_t duck_t;
    uint32_t rnd;
} ui;

static uint32_t urand(void)
{
    ui.rnd = ui.rnd * 1664525u + 1013904223u;
    return ui.rnd >> 8;
}

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void signed_text(int32_t v, char *b)
{
    if (v >= 0)
        *b++ = '+';
    int_text(v, b);
}

static void note_text(int32_t midi, char *b)   /* C4 = 60 */
{
    b[0] = 0;
    text_cat(b, NOTE_NAME[midi % 12]);
    int_text(midi / 12 - 1, b + (NOTE_NAME[midi % 12][1] ? 2 : 1));
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1200u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static int32_t accel(int k, int32_t e, int32_t mid, int32_t fast)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_at[k];
    ui.enc_at[k] = now;
    if (dt < 35u)
        return e * fast;
    if (dt < 80u)
        return e * mid;
    return e;
}

/* ------------------------------------------------------------------ notes --- */
static int split_on(void) { return set.split_oct != 0u; }
static int side_of(uint32_t k) { return split_on() && k < set.split_key ? SIDE_L : SIDE_R; }
static uint32_t side_mask(int side)
{
    uint32_t all = (1u << NKEYS) - 1u, left = split_on() ? (1u << set.split_key) - 1u : 0u;
    return side == SIDE_L ? left : all & ~left;
}
static int holds(int side) { return ui.drone || (side == SIDE_L && ui.lowhold && split_on()); }
static int is_white(uint32_t k)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS;
    uint32_t i;
    for (i = 0; i < 16u; i++)
        if (WK[i] == k)
            return 1;
    return 0;
}

/* what key k plays now: 0 = nothing (a black key in WHITE) */
static int key_pitch(uint32_t k, float *p)
{
    float nat = 53.0f + (float)k + 12.0f * (float)set.octave;
    const scale_t *sc = &SCALES[set.scale % NSCALE];
    if (!split_on()) {
        *p = nat;
        return 1;
    }
    if (k < set.split_key) {             /* the low split: octaves down */
        *p = nat - 12.0f * (float)set.split_oct;
        return 1;
    }
    if (set.follow == FOLLOW_SNAP) {     /* to the scale's note at or below it */
        int32_t n = (int32_t)nat, rel = ((n - (int32_t)ui.root) % 12 + 12) % 12, best = 0, i;
        for (i = 0; i < sc->n; i++)
            if (sc->st[i] <= rel)
                best = sc->st[i];
        *p = (float)(n - rel + best);
        return 1;
    }
    if (set.follow == FOLLOW_WHITE) {    /* the scale on the white keys, from the root near the first one */
        uint32_t j, first = 0, d = 0;
        int32_t p0, base;
        if (!is_white(k))
            return 0;
        for (j = set.split_key; j < NKEYS; j++)
            if (is_white(j)) {
                first = j;
                break;
            }
        for (j = first; j < k; j++)
            d += (uint32_t)is_white(j);
        p0 = 53 + (int32_t)first + 12 * set.octave;
        base = p0 - (((p0 - (int32_t)ui.root) % 12 + 12) % 12);
        if (p0 - base > 6)
            base += 12;
        *p = (float)(base + 12 * (int32_t)(d / sc->n) + sc->st[d % sc->n]);
        return 1;
    }
    *p = nat;
    return 1;
}

/* the ARPEGGIATOR: with the split on it takes the low split's notes, else every key's. Its keys start no voice of
 * their own; what they hold (pressed, or staying with LOW HOLD / DRONE) goes to the engine, which plays it */
static int arp_side(int side) { return set.arp && (!split_on() || side == SIDE_L); }
static int arp_key(uint32_t k) { return arp_side(side_of(k)); }
static void arp_cfg(void)
{
    eng_arp(set.arp, set.arp_rate, set.arp_pat, set.arp_oct, (float)set.arp_gate / 100.0f, (float)set.bpm);
}
static void arp_push(void)               /* the arp's notes, in the order they were played */
{
    float p[ARP_MAX], v[ARP_MAX];
    uint8_t ks[ARP_MAX];
    uint32_t k, n = 0, i, j;
    for (k = 0; k < NKEYS && n < ARP_MAX; k++)
        if ((ui.sounding >> k & 1u) && arp_key(k))
            ks[n++] = (uint8_t)k;
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && ui.ptime[ks[j]] < ui.ptime[ks[j - 1]]; j--) {
            uint8_t t = ks[j];
            ks[j] = ks[j - 1];
            ks[j - 1] = t;
        }
    for (i = 0; i < n; i++) {
        p[i] = ui.kp[ks[i]];
        v[i] = (float)ui.kvel[ks[i]] / 127.0f;
    }
    eng_arp_notes(p, v, (int)n);
}

static int gmode(int side) { return side == SIDE_L ? set.mode_l : set.mode_h; }
static float gl_secs(uint32_t v) { return (float)(v * v) * (3.0f / 10000.0f); }   /* 0..100: 0..3 s, fine at the start */
static float gtime(int side) { return gl_secs(side == SIDE_L ? set.glide_l : set.glide_h); }

static int follows(uint32_t k) { return split_on() && set.follow != FOLLOW_OFF && k >= set.split_key; }

/* EPIC's fifth above pitch p: a perfect fifth, but where FOLLOW is on, the scale's note nearest to it (the scale
 * decides: a B in A minor gets its F or G, never an F#) */
static float fifth_of(uint32_t k, float p)
{
    const scale_t *sc = &SCALES[set.scale % NSCALE];
    int32_t t = (int32_t)p + 7, best = t, bd = 99, c, i;
    if (!follows(k))
        return p + 7.0f;
    for (c = t - 2; c <= t + 2; c++) {
        int32_t rel = ((c - (int32_t)ui.root) % 12 + 12) % 12, d = c > t ? c - t : t - c;
        for (i = 0; i < sc->n; i++)
            if (sc->st[i] == rel && (d < bd || (d == bd && c > best))) {
                bd = d;
                best = c;
            }
    }
    return (float)best;
}

/* a key's notes (the root, and with EPIC the fifth and the octave) */
static void notes_on(uint32_t k, float p, float v, float from, float g)
{
    static const float VS[3] = {1.0f, 0.85f, 0.8f};
    float iv[3];
    uint32_t i, n = set.epic ? 3u : 1u;
    iv[0] = 0.0f;
    iv[1] = fifth_of(k, p) - p;
    iv[2] = 12.0f;
    for (i = 0; i < n; i++) {
        if (from != p)
            eng_note_glide((int)(k + 100u * i), p + iv[i], v * VS[i], from + iv[i], g);
        else
            eng_note_on((int)(k + 100u * i), p + iv[i], v * VS[i]);
    }
}
static void notes_move(uint32_t m, uint32_t k, float p, float v, float g)
{
    float iv[3];
    uint32_t i, n = set.epic ? 3u : 1u;
    iv[0] = 0.0f;
    iv[1] = fifth_of(k, p) - p;
    iv[2] = 12.0f;
    for (i = 0; i < n; i++)
        eng_note_move((int)(m + 100u * i), (int)(k + 100u * i), p + iv[i], v, g, 0);
}

static void key_start(uint32_t k, uint8_t vel)
{
    float p, v = (float)vel / 127.0f, g;
    int side = side_of(k), mode = gmode(side);
    uint32_t m;
    if (!key_pitch(k, &p))
        return;
    g = gtime(side);
    ui.kp[k] = p;
    ui.kvel[k] = vel;
    ui.ptime[k] = ++ui.pcount;
    if (arp_side(side)) {                /* the arp plays it */
        ui.sounding |= 1u << k;
        arp_push();
        return;
    }
    m = ui.mono[side] ? ui.mono[side] - 1u : 99u;
    if (mode != GM_POLY && m < NKEYS && m != k && (ui.sounding >> m & 1u)) {   /* legato: the voice slides here */
        notes_move(m, k, p, v, g);
        ui.sounding &= ~(1u << m);
        ui.latched &= ~(1u << m);
    } else {
        float from = g > 0.0f && mode != GM_LEGATO && ui.has_last[side] ? ui.last_p[side] : p;
        notes_on(k, p, v, from, g);
    }
    if (mode != GM_POLY)
        ui.mono[side] = (uint8_t)(k + 1u);
    ui.last_p[side] = p;
    ui.has_last[side] = 1;
    ui.sounding |= 1u << k;
}

static void key_stop(uint32_t k)
{
    int side = side_of(k);
    if (arp_side(side)) {
        ui.sounding &= ~(1u << k);
        ui.prs[k] = 0;
        arp_push();
        return;
    }
    eng_note_off((int)k);
    eng_note_off(100 + (int)k);
    eng_note_off(200 + (int)k);
    ui.sounding &= ~(1u << k);
    ui.prs[k] = 0;
    if (ui.mono[side] == k + 1u)
        ui.mono[side] = 0;
}

/* the melody's root: the lowest note of the low split that sounds (else it stays the last) */
static void root_update(void)
{
    uint32_t k, left = side_mask(SIDE_L) & ui.sounding;
    if (!left)
        return;
    for (k = 0; k < NKEYS; k++)
        if (left >> k & 1u) {
            uint8_t r = (uint8_t)(((int32_t)ui.kp[k] % 12 + 12) % 12);
            if (r != ui.root) {
                ui.root = r;
                ui.redraw = 1;
            }
            return;
        }
}

/* the deep root under a drone: an octave below the lowest note that stays */
static void deep_update(void)
{
    float want = 0.0f;
    uint32_t k;
    if (ui.drone)
        for (k = 0; k < NKEYS; k++)
            if ((ui.latched & ui.sounding) >> k & 1u) {
                want = ui.kp[k] - 12.0f;
                break;
            }
    if (want == ui.deep)
        return;
    if (ui.deep > 0.0f && want > 0.0f && set.glide_l + set.glide_h) {   /* it slides with the glide */
        eng_note_move(DEEP_ID, DEEP_ID, want, 0.65f, gtime(split_on() ? SIDE_L : SIDE_R), 0);
        ui.deep = want;
        return;
    }
    if (ui.deep > 0.0f)
        eng_note_off(DEEP_ID);
    if (want > 0.0f)
        eng_note_on(DEEP_ID, want, 0.65f);
    ui.deep = want;
}

static void latched_release(uint32_t mask)   /* the latched notes in mask let go */
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        if ((ui.latched & mask) >> k & 1u)
            key_stop(k);
    ui.latched &= ~mask;
}

static void key_press(uint32_t k, uint8_t vel, uint32_t down_before)
{
    int side = side_of(k);
    if (holds(side)) {
        if (!(down_before & side_mask(side))) {   /* no key down on its side: a new chord there (MONO: it slides) */
            uint32_t keep = gmode(side) != GM_POLY && ui.mono[side] ? 1u << (ui.mono[side] - 1u) : 0u;
            latched_release(side_mask(side) & ~keep);
        }
        ui.latched |= 1u << k;
    }
    key_start(k, vel);
    if (!(ui.sounding >> k & 1u))
        ui.latched &= ~(1u << k);        /* (a resting black key) */
    root_update();
    deep_update();
}

static void key_release(uint32_t k)
{
    int side = side_of(k);
    if (holds(side) && (ui.latched >> k & 1u)) {
        /* it stays */
    } else if (!arp_side(side) && gmode(side) != GM_POLY && ui.mono[side] == k + 1u) {   /* MONO: back to the last key still held */
        uint32_t j, best = 99u, bt = 0, held = ui.keys & side_mask(side) & ~(1u << k);
        float p;
        for (j = 0; j < NKEYS; j++)
            if ((held >> j & 1u) && ui.ptime[j] > bt) {
                bt = ui.ptime[j];
                best = j;
            }
        if (best < NKEYS && key_pitch(best, &p)) {
            notes_move(k, best, p, (float)ui.kvel[best] / 127.0f, gtime(side));
            ui.sounding = (ui.sounding & ~(1u << k)) | (1u << best);
            ui.kp[best] = p;
            ui.mono[side] = (uint8_t)(best + 1u);
            ui.last_p[side] = p;
            ui.prs[k] = 0;
        } else
            key_stop(k);
    } else if (ui.sounding >> k & 1u)
        key_stop(k);
    if ((ui.latched >> k & 1u) && ui.prs[k]) {   /* it stays, but no longer pressed */
        eng_press((int)k, 0.0f);
        eng_press(100 + (int)k, 0.0f);
        eng_press(200 + (int)k, 0.0f);
        ui.prs[k] = 0;
    }
    root_update();
    deep_update();
}

static void holds_changed(void)          /* DRONE or LOW HOLD switched: what no longer holds lets go */
{
    uint32_t keep = 0;
    if (holds(SIDE_L))
        keep |= side_mask(SIDE_L);
    if (holds(SIDE_R))
        keep |= side_mask(SIDE_R);
    latched_release(~keep & ~ui.keys);
    ui.latched &= keep;
    ui.latched |= ui.keys & ui.sounding & keep;   /* the keys down now stay */
    arp_push();
    root_update();
    deep_update();
    ui.head_redraw = 1;
}

static void all_off(void)
{
    eng_all_off();
    ui.latched = 0;
    ui.sounding = 0;
    ui.deep = 0.0f;
    arp_push();
}

/* the ARPEGGIATOR on / off: what sounds lets go, the keys still down start again the new way */
static void arp_toggle(void)
{
    uint32_t k, held = ui.keys;
    for (k = 0; k < NKEYS; k++)
        if (ui.sounding >> k & 1u)
            key_stop(k);
    ui.latched = 0;
    ui.mono[0] = ui.mono[1] = 0;
    set.arp = (uint8_t)!set.arp;
    arp_cfg();
    for (k = 0; k < NKEYS; k++)
        if (held >> k & 1u)
            key_press(k, ui.kvel[k] ? ui.kvel[k] : 100u, held & ((1u << k) - 1u));
    arp_push();
    dirty();
    app_say("ARP", set.arp ? "ON" : "OFF");
    ui.head_redraw = 1;
}

/* ------------------------------------------------------------------- cues --- */
/* CUES: up to eight chords kept (SAVE on the CUES page keeps what sounds), played as a film score: one slides into
 * the next over FADE (each note to its neighbour: the lowest to the lowest), by hand (CUE) or by itself (AUTO, each
 * HOLD seconds). Their notes are their own voices (ids 820.., with EPIC 840.. and 860..), under what the keys play. */
#define CUE_ID 820
static float cue_fade_s(void) { return 0.3f + (float)(set.cue_fade * set.cue_fade) * 0.00297f; }

static void cue_stop(void)
{
    uint32_t j;
    for (j = 0; j < CUE_NOTES; j++) {
        eng_note_off(CUE_ID + (int)j);
        eng_note_off(CUE_ID + 20 + (int)j);
        eng_note_off(CUE_ID + 40 + (int)j);
    }
    ui.cue_cur = 0;
    ui.cue_pn = 0;
}

static void cue_play(uint32_t c)         /* c 1..NCUE (0: stop; 0xFF: the first one kept) */
{
    uint32_t j, nb, na = ui.cue_pn, n;
    float g = cue_fade_s();
    if (c == 0xFFu)
        for (c = 1; c <= NCUE && !set.cue_n[c - 1]; c++) {
        }
    if (c == 0u || c > NCUE || !set.cue_n[c - 1]) {
        if (c == 0u)
            cue_stop();
        return;
    }
    nb = set.cue_n[c - 1];
    n = na > nb ? na : nb;
    for (j = 0; j < n; j++) {
        float b = (float)set.cue_p[c - 1][j < nb ? j : nb - 1];
        uint32_t e, ne = set.epic ? 3u : 1u;
        static const float IV[3] = {0.0f, 7.0f, 12.0f};
        for (e = 0; e < 3u; e++) {
            int id = CUE_ID + (int)(20u * e + j);
            if (j >= nb || e >= ne)
                eng_note_off(id);
            else if (j < na)             /* its neighbour slides here */
                eng_note_move(id, id, b + IV[e], 0.8f, g, 0);
            else                         /* a new note, sliding in from the old chord's top */
                eng_note_glide(id, b + IV[e], 0.75f, na ? (float)ui.cue_pp[na - 1] + IV[e] : b + IV[e], g);
        }
    }
    for (j = 0; j < nb; j++)
        ui.cue_pp[j] = set.cue_p[c - 1][j];
    ui.cue_pn = (uint8_t)nb;
    ui.cue_cur = (uint8_t)c;
    ui.cue_at = plat_ms();
    ui.redraw = 1;
}

static void cue_store(void)              /* what sounds now (the keys', held or staying) into the slot picked */
{
    uint32_t k, n = 0, c = ui.cue_sel, i, j;
    uint8_t p[CUE_NOTES];
    char t[24];
    if (!c)                              /* none picked: the first empty one */
        for (c = 1; c <= NCUE && set.cue_n[c - 1]; c++) {
        }
    if (c > NCUE)
        c = NCUE;
    for (k = 0; k < NKEYS && n < CUE_NOTES; k++)
        if (ui.sounding >> k & 1u)
            p[n++] = (uint8_t)clamp((int32_t)ui.kp[k], 12, 120);
    for (i = 1; i < n; i++)              /* low to high */
        for (j = i; j > 0 && p[j - 1] > p[j]; j--) {
            uint8_t x = p[j];
            p[j] = p[j - 1];
            p[j - 1] = x;
        }
    set.cue_n[c - 1] = (uint8_t)n;
    for (i = 0; i < n; i++)
        set.cue_p[c - 1][i] = p[i];
    ui.cue_sel = (uint8_t)(c < NCUE ? c + 1u : c);   /* ready for the next */
    dirty();
    t[0] = 0;
    text_cat(t, "CUE ");
    int_text((int32_t)c, t + 4);
    if (n) {
        char x[8];
        int_text((int32_t)n, x);
        text_cat(x, " NOTES");
        app_say(t, x);
    } else
        app_say(t, "EMPTY");
}

static void cue_tick(uint32_t now)       /* AUTO: the next cue kept, after HOLD */
{
    uint32_t c, i;
    if (!set.cue_auto || !ui.cue_cur || now - ui.cue_at < (uint32_t)set.cue_hold * 1000u)
        return;
    for (i = 1; i <= NCUE; i++) {
        c = (ui.cue_cur - 1u + i) % NCUE + 1u;
        if (set.cue_n[c - 1]) {
            cue_play(c);
            ui.cue_sel = (uint8_t)c;
            return;
        }
    }
}

/* ------------------------------------------------------------------ the night --- */
/* STORY: a night told over 24 minutes, 19:00 to 07:00 (two minutes an hour): dusk, the night, rain, the storm at
 * one, the storm passing, dawn; then dusk again. RAIN is how wild that night gets at most. */
#define NIGHT_MS (24u * 60u * 1000u)
static uint32_t night_ms(void) { return ((uint32_t)((int32_t)plat_ms() + ui.story_off)) % NIGHT_MS; }
static int32_t night_min(void) { return (int32_t)(night_ms() / 2000u); }   /* 0..719: minutes since 19:00 */
static int32_t story_step(void) { return night_min() / 10; }
static void story_set(int32_t step)      /* the clock to step x 10 minutes after 19:00 */
{
    ui.story_off += (int32_t)((uint32_t)step * 20000u) - (int32_t)night_ms();
}
/* the weather of the night, 0..1, by the hour */
static float night_rain(void)
{
    static const float W[13] = {0.0f, 0.05f, 0.25f, 0.35f, 0.55f, 0.75f, 0.95f, 1.0f, 0.8f, 0.5f, 0.3f, 0.12f, 0.0f};
    int32_t m = night_min();
    float h = (float)m / 60.0f;
    int32_t i = (int32_t)h;
    float f = h - (float)i;
    if (i >= 12)
        return W[12];
    return W[i] + f * (W[i + 1] - W[i]);
}
static uint8_t rain_eff(void) { return set.story ? (uint8_t)((float)KM[3] * night_rain() + 0.5f) : KM[3]; }

static void sound(void)
{
    uint8_t m[4];
    m[0] = KM[0];
    m[1] = KM[1];
    m[2] = KM[2];
    m[3] = ui.rain_now = rain_eff();     /* RAIN as it plays: the knob, or with STORY the night's share of it */
    eng_sound(set.preset, m, (float)set.tone / 50.0f, (float)set.vib / 100.0f, set.press, (float)set.pamt / 100.0f);
}

static void weather(void)                /* the night moves on: the rain with it */
{
    if (rain_eff() != ui.rain_now)
        sound();
}

/* ------------------------------------------------------------------ pages --- */
/* the knobs of SPLIT and PRESSURE: a value, its range, its text */
static const char *const PAGE_NAME[NPAGE] = {"MAIN", "SPLIT", "PRESSURE", "GLIDE", "WORLD", "CUES", "ARP"};
static const char *const RATE_NAME[NRATE] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char *const PAT_NAME[NARPPAT] = {"UP", "DOWN", "UP-DOWN", "PLAYED", "RANDOM"};
static const char *const PK_NAME[6][4] = {{"SPLIT", "POINT", "SCALE", "FOLLOW"},
                                          {"PRESSURE", "AMOUNT", "TONE", "VIBRATO"},
                                          {"LOW GLIDE", "LOW MODE", "GLIDE", "MODE"},
                                          {"CITY", "STORY", "LOGS", "TIME"},
                                          {"CUE", "FADE", "HOLD", "AUTO"},
                                          {"RATE", "PATTERN", "OCTAVES", "GATE"}};
static void cue_play(uint32_t c);
static void world_level(void);
static void story_set(int32_t step);
static int32_t story_step(void);

static int32_t pk_get(int pg, int k)
{
    if (pg == PG_SPLIT)
        return k == 0 ? set.split_oct : k == 1 ? set.split_key : k == 2 ? set.scale : set.follow;
    if (pg == PG_GLIDE)
        return k == 0 ? set.glide_l : k == 1 ? set.mode_l : k == 2 ? set.glide_h : set.mode_h;
    if (pg == PG_WORLD)
        return k == 0 ? set.city : k == 1 ? set.story : k == 2 ? set.logs : story_step();
    if (pg == PG_CUES)
        return k == 0 ? ui.cue_sel : k == 1 ? set.cue_fade : k == 2 ? set.cue_hold : set.cue_auto;
    if (pg == PG_ARP)
        return k == 0 ? set.arp_rate : k == 1 ? set.arp_pat : k == 2 ? set.arp_oct : set.arp_gate;
    return k == 0 ? set.press : k == 1 ? set.pamt : k == 2 ? set.tone : set.vib;
}

static void pk_range(int pg, int k, int32_t *lo, int32_t *hi)
{
    static const int8_t LO[6][4] = {{0, 1, 0, 0}, {0, 0, -50, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 4, 0}, {0, 0, 1, 5}};
    static const int8_t HI[6][4] = {{3, NKEYS - 1, NSCALE - 1, NFOLLOW - 1}, {NPRESS - 1, 100, 50, 100},
                                    {100, NGMODE - 1, 100, NGMODE - 1}, {100, 1, 1, 71}, {NCUE, 100, 60, 1},
                                    {NRATE - 1, NARPPAT - 1, 4, 100}};
    *lo = LO[pg - 1][k];
    *hi = HI[pg - 1][k];
}

static void pk_text(int pg, int k, char *t)
{
    int32_t v = pk_get(pg, k);
    t[0] = 0;
    if (pg == PG_ARP) {
        if (k == 0)
            text_cat(t, RATE_NAME[v]);
        else if (k == 1)
            text_cat(t, PAT_NAME[v]);
        else {
            int_text(v, t);
            text_cat(t, k == 2 ? (v > 1 ? " OCTS" : " OCT") : "%");
        }
        return;
    }
    if (pg == PG_SPLIT) {
        if (k == 0) {
            if (!v)
                text_cat(t, "OFF");
            else {
                signed_text(-v, t);
                text_cat(t, " OCT");
            }
        } else if (k == 1)
            note_text(53 + v, t);
        else if (k == 2)
            text_cat(t, SCALES[v].name);
        else
            text_cat(t, FOLLOW_NAME[v]);
    } else if (pg == PG_WORLD) {
        if (k == 0) {
            int_text(v, t);
            text_cat(t, "%");
        } else if (k == 3) {             /* the night's clock, 10 minutes a step from 19:00 */
            int32_t h = (19 + v / 6) % 24, mi = (v % 6) * 10;
            t[0] = (char)('0' + h / 10);
            t[1] = (char)('0' + h % 10);
            t[2] = ':';
            t[3] = (char)('0' + mi / 10);
            t[4] = '0';
            t[5] = 0;
        } else
            text_cat(t, v ? "ON" : "OFF");
    } else if (pg == PG_CUES) {
        if (k == 0) {
            if (!v)
                text_cat(t, "STOP");
            else {
                text_cat(t, "CUE ");
                int_text(v, t + 4);
                if (!set.cue_n[v - 1])
                    text_cat(t, " -");
            }
        } else if (k == 1) {
            uint32_t ds = (uint32_t)(3.0f + (float)(v * v) * 0.0297f);   /* tenths: 0.3 .. 30 s */
            int_text((int32_t)(ds / 10u), t);
            if (ds < 100u) {
                char d[3] = {'.', (char)('0' + ds % 10u), 0};
                text_cat(t, d);
            }
            text_cat(t, " S");
        } else if (k == 2) {
            int_text(v, t);
            text_cat(t, " S");
        } else
            text_cat(t, v ? "ON" : "OFF");
    } else if (pg == PG_GLIDE) {
        if (k & 1)
            text_cat(t, GMODE_NAME[v]);
        else if (!v)
            text_cat(t, "OFF");
        else {
            uint32_t ms = (uint32_t)(gl_secs((uint32_t)v) * 1000.0f + 0.5f);
            if (ms < 1000u) {
                int_text((int32_t)ms, t);
                text_cat(t, " MS");
            } else {
                char d[4];
                int_text((int32_t)(ms / 1000u), t);
                text_cat(t, ".");
                d[0] = (char)('0' + ms / 100u % 10u);
                d[1] = 0;
                text_cat(t, d);
                text_cat(t, " S");
            }
        }
    } else {
        if (k == 0)
            text_cat(t, PRESS_NAME[v]);
        else if (k == 2)
            signed_text(v, t);
        else {
            int_text(v, t);
            text_cat(t, "%");
        }
    }
}

static void pk_set(int pg, int k, int32_t v)
{
    char t[16];
    if (pg == PG_ARP) {
        if (k == 0)
            set.arp_rate = (uint8_t)v;
        else if (k == 1)
            set.arp_pat = (uint8_t)v;
        else if (k == 2)
            set.arp_oct = (uint8_t)v;
        else
            set.arp_gate = (uint8_t)v;
        arp_cfg();
    } else if (pg == PG_SPLIT) {
        if (k == 0) {
            if (set.split_oct && !v)     /* the split off: the low part's hold goes */
                ui.lowhold = 0;
            set.split_oct = (uint8_t)v;
        } else if (k == 1)
            set.split_key = (uint8_t)v;
        else if (k == 2)
            set.scale = (uint8_t)v;
        else {
            set.follow = (uint8_t)v;
            if (v && !set.split_oct) {   /* FOLLOW needs the low split (its note is the root): on, an octave down */
                set.split_oct = 1;
                holds_changed();
            }
        }
        if (k <= 1)
            holds_changed();
    } else if (pg == PG_WORLD) {
        if (k == 0) {
            set.city = (uint8_t)v;
            world_level();
        } else if (k == 1) {
            set.story = (uint8_t)v;
            sound();
        }
        else if (k == 2)
            set.logs = (uint8_t)v;
        else {
            story_set(v);
            sound();
        }
    } else if (pg == PG_CUES) {
        if (k == 0) {
            ui.cue_sel = (uint8_t)v;
            cue_play((uint32_t)v);       /* the cue picked plays, sliding from the last (0: they stop) */
        } else if (k == 1)
            set.cue_fade = (uint8_t)v;
        else if (k == 2)
            set.cue_hold = (uint8_t)v;
        else {
            set.cue_auto = (uint8_t)v;
            if (v && !ui.cue_cur)
                cue_play(0xFFu);         /* AUTO: from the first cue there is */
        }
    } else if (pg == PG_GLIDE) {
        if (k == 0)
            set.glide_l = (uint8_t)v;
        else if (k == 1)
            set.mode_l = (uint8_t)v;
        else if (k == 2)
            set.glide_h = (uint8_t)v;
        else
            set.mode_h = (uint8_t)v;
        if (k & 1)
            ui.mono[k == 1 ? SIDE_L : SIDE_R] = 0;
    } else {
        if (k == 0)
            set.press = (uint8_t)v;
        else if (k == 1)
            set.pamt = (uint8_t)v;
        else if (k == 2)
            set.tone = (int8_t)v;
        else
            set.vib = (uint8_t)v;
        sound();
    }
    dirty();
    pk_text(pg, k, t);
    app_say(PK_NAME[pg - 1][k], t);
}

static void page_knob(int k, int32_t e)
{
    int pg = ui.page;
    int32_t lo, hi, v = pk_get(pg, k), list = (pg == PG_SPLIT && k != 1) || (pg == PG_PRESS && k == 0) || (pg == PG_GLIDE && (k & 1)) ||
                                                 (pg == PG_WORLD && (k == 1 || k == 2)) || (pg == PG_CUES && (k == 0 || k == 3)) ||
                                                 (pg == PG_ARP && k < 3);
    pk_range(pg, k, &lo, &hi);
    if (list) {                          /* a list: two detents a step */
        ui.enc_acc[k] += e;
        if (ui.enc_acc[k] > -2 && ui.enc_acc[k] < 2)
            return;
        e = ui.enc_acc[k] > 0 ? 1 : -1;
        ui.enc_acc[k] = 0;
    } else
        e = accel(EN_K1 + k, e, 2, 4);
    v = clamp(v + e, lo, hi);
    ui.knob_at[k] = plat_ms();
    if (v != pk_get(pg, k))
        pk_set(pg, k, v);
    ui.redraw = 1;
}

static void page_go(int pg)
{
    ui.page = (uint8_t)pg;
    ui.zone_until = 0;
    app_say(PAGE_NAME[pg], pg == PG_MAIN ? PR.name : "");
}

/* ------------------------------------------------------------------ knobs --- */
static void knob(int k, int32_t e)
{
    const macro_t *mc = &MACROS[k];
    int32_t p, v;
    int zb, za;
    if (ui.page != PG_MAIN) {
        page_knob(k, e);
        return;
    }
    p = clamp(knob_pos[k] + accel(EN_K1 + k, e, 3, 6), 0, pos_max(mc));
    v = val_of_pos(mc, p);
    zb = zone_at(mc, KM[k]);
    za = zone_at(mc, v);
    knob_pos[k] = p;
    ui.knob_at[k] = plat_ms();
    ui.redraw = 1;
    if (v == KM[k])
        return;
    KM[k] = (uint8_t)v;
    sound();
    dirty();
    if (za >= 0 && za != zb) {
        ui.zone_k = (int8_t)k;
        ui.zone_z = (int8_t)za;
        ui.zone_until = plat_ms() + 700u;
        if (ann_on())
            ann_say(mc->zone[za], 0, ANN_NOW);
    }
}

static void preset_load(int32_t d)
{
    set.preset = (uint8_t)(((int32_t)set.preset + d % NPRESETS + NPRESETS) % NPRESETS);
    knobs_from_settings();
    sound();
    dirty();
    ui.zone_until = 0;
    app_say("PRESET", PR.name);
}

static void octave(int d)
{
    int32_t o = clamp(set.octave + d, -2, 1);
    char t[8];
    if (o == set.octave)
        return;
    set.octave = (int8_t)o;
    dirty();
    signed_text(o, t);
    app_say("OCTAVE", t);
    ui.head_redraw = 1;
}

static void pressure(void)               /* the held keys' pressure, to their notes (as it changes) */
{
    uint32_t k;
    if (set.press == PRESS_OFF)
        return;
    for (k = 0; k < NKEYS; k++) {
        uint8_t pr = (ui.keys >> k & 1u) && (ui.sounding >> k & 1u) ? touch_pressure(k) : 0u;
        int d = (int)pr - (int)ui.prs[k];
        if (d >= 3 || d <= -3 || (pr == 0u && ui.prs[k])) {
            float f = (float)pr / 127.0f;
            ui.prs[k] = pr;
            eng_press((int)k, f);
            eng_press(100 + (int)k, f);
            eng_press(200 + (int)k, f);
        }
    }
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, now = plat_ms(), down;
    int32_t e;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++)
        if (!(keys >> k & 1u) && (ui.keys >> k & 1u)) {
            ui.keys &= ~(1u << k);
            key_release(k);
        }
    down = ui.keys & keys;
    for (k = 0; k < NKEYS; k++) {        /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            int v = touch_vel(k);
            if (v <= 0)
                v = 100;
            if (btn >> B_SEL & 1u) {     /* SEL held: the split here (this key the right's first) */
                if (k >= 1u) {
                    char t[8];
                    if (!set.split_oct)
                        set.split_oct = 1;
                    set.split_key = (uint8_t)k;
                    holds_changed();
                    dirty();
                    note_text(53 + (int32_t)k, t);
                    app_say("SPLIT", t);
                }
                ui.sel_used = 1;
            } else
                key_press(k, (uint8_t)v, down);
            down |= bit;
            if (ui.intro)
                ui.intro = 2;
        }
    }
    if (keys != ui.keys)
        ui.redraw = 1;
    ui.keys = keys;
    pressure();
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (ui.intro)
            ui.intro = 2;
        if (ui.sheet) {                  /* the controls on the screen: any button closes them */
            ui.sheet = 0;
            ui.redraw = ui.head_redraw = 1;
            continue;
        }
        if (k == B_PLAY) {
            ui.drone = (uint8_t)!ui.drone;
            holds_changed();
            app_say("DRONE", ui.drone ? "ON" : "OFF");
        } else if (k == B_SEQ) {
            ui.lowhold = (uint8_t)!ui.lowhold;
            if (ui.lowhold && !set.split_oct) {   /* LOW HOLD needs the split: on, an octave down */
                set.split_oct = 1;
                dirty();
            }
            holds_changed();
            app_say("LOW HOLD", ui.lowhold ? "ON" : "OFF");
        } else if (k == B_ARP)
            ui.arp_t = now;
        else if (k == B_OCTDN || k == B_OCTUP)
            octave(k == B_OCTUP ? 1 : -1);
        else if (k == B_REC)
            ui.rec_t = now;
        else if (k == B_SAVE)
            ui.save_t = now;
        else if (k == B_FX)
            page_go(ui.page == PG_WORLD ? PG_MAIN : PG_WORLD);
        else if (k == B_ENV)
            page_go(ui.page == PG_CUES ? PG_MAIN : PG_CUES);
        else if (k == B_LFO)
            page_go(ui.page == PG_ARP ? PG_MAIN : PG_ARP);
        else if (k == B_HOME) {
            if (now - ui.home_last < 400u) {   /* twice: the controls, drawn */
                ui.sheet = 1;
                ui.home_last = 0;
                ui.home_t = 0;
                continue;
            }
            ui.home_last = now;
            ui.home_t = now;
            if (ui.page != PG_MAIN)
                page_go(PG_MAIN);
        } else if (k == B_SEL) {
            ui.sel_t = now;
            ui.sel_used = 0;
        } else if (k == B_GLO)
            page_go(ui.page == PG_PRESS ? PG_MAIN : PG_PRESS);
        else if (k == B_EDIT)
            page_go(ui.page == PG_GLIDE ? PG_MAIN : PG_GLIDE);
    }
    if (ui.sel_t && !(btn >> B_SEL & 1u)) {   /* SEL let go: the SPLIT page (again: MAIN), unless it set the split */
        ui.sel_t = 0;
        if (!ui.sel_used)
            page_go(ui.page == PG_SPLIT ? PG_MAIN : PG_SPLIT);
    }
    if (ui.save_t) {                     /* SAVE: what sounds kept as a cue; held 1 s on CUES: every cue cleared */
        if (!(btn >> B_SAVE & 1u)) {
            ui.save_t = 0;
            cue_store();
        } else if (ui.page == PG_CUES && now - ui.save_t >= 1000u) {
            uint32_t c;
            ui.save_t = 0;
            for (c = 0; c < NCUE; c++)
                set.cue_n[c] = 0;
            cue_stop();
            ui.cue_sel = 0;
            dirty();
            app_say("CUES", "CLEARED");
        }
    }
    if (ui.arp_t) {                      /* ARP: a tap the arpeggiator, held 1 s EPIC */
        if (!(btn >> B_ARP & 1u)) {
            ui.arp_t = 0;
            arp_toggle();
        } else if (now - ui.arp_t >= 1000u) {
            ui.arp_t = 0;
            set.epic = (uint8_t)!set.epic;
            dirty();
            app_say("EPIC", set.epic ? "ON" : "OFF");
            ui.head_redraw = 1;
        }
    }
    if (ui.rec_t) {
        if (!(btn >> B_REC & 1u)) {      /* a tap: MEMORY, the last seconds frozen / let go */
            ui.rec_t = 0;
            ui.memory = (uint8_t)!ui.memory;
            eng_memory(ui.memory);
            app_say("MEMORY", ui.memory ? "FROZEN" : "OFF");
        } else if (now - ui.rec_t >= 1000u) {
            ui.rec_t = 0;
            all_off();
            app_say("ALL NOTES", "OFF");
        }
    }
    if (ui.home_t) {
        if (!(btn >> B_HOME & 1u))
            ui.home_t = 0;
        else if (now - ui.home_t >= 1000u) {
            ui.home_t = 0;
            for (k = 0; k < 4u; k++)
                KM[k] = PR.m[k];
            knobs_from_settings();
            sound();
            dirty();
            app_say(PR.name, "RESET");
        }
    }
    for (k = 0; k < 4u; k++)
        if ((e = plat_enc(EN_K1 + (int)k)) != 0) {
            knob((int)k, e);
            if (ui.intro)
                ui.intro = 2;
        }
    if ((e = plat_enc(EN_SELECT)) != 0 && ui.page == PG_ARP) {   /* on the ARP page: the tempo */
        char t[8];
        set.bpm = (uint8_t)clamp((int32_t)set.bpm + accel(EN_SELECT, e, 2, 5), 40, 240);
        arp_cfg();
        dirty();
        int_text(set.bpm, t);
        app_say(eng_clock_ext() ? "BPM (EXT)" : "BPM", t);
        ui.redraw = 1;
    } else if (e != 0) {
        char t[8];
        set.tone = (int8_t)clamp((int32_t)set.tone + accel(EN_SELECT, e, 2, 4), -50, 50);
        sound();
        dirty();
        signed_text(set.tone, t);
        app_say("TONE", t);
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        char t[8];
        set.vib = (uint8_t)clamp((int32_t)set.vib + accel(EN_ALGO, e, 2, 4), 0, 100);
        sound();
        dirty();
        int_text(set.vib, t);
        text_cat(t, "%");
        app_say("VIBRATO", t);
    }
    if ((e = plat_enc(EN_PRESET)) != 0)
        preset_load(e > 0 ? 1 : -1);
    if (btn != ui.btn)
        ui.redraw = 1;
    ui.btn = btn;
}

static void lights(void)
{
    uint32_t btn = 0, dimb;
    if (ui.drone)
        btn |= LED_PLAY_GREEN;
    if (ui.lowhold)
        btn |= 1u << B_SEQ;
    if (set.epic)
        btn |= 1u << B_ARP;
    if (ui.page == PG_SPLIT)
        btn |= 1u << B_SEL;
    if (ui.page == PG_PRESS)
        btn |= 1u << B_GLO;
    if (ui.page == PG_GLIDE)
        btn |= 1u << B_EDIT;
    if (ui.page == PG_WORLD)
        btn |= 1u << B_FX;
    if (ui.page == PG_CUES)
        btn |= 1u << B_ENV;
    if (ui.memory)
        btn |= 1u << B_REC;
    if (set.octave < 0)
        btn |= 1u << B_OCTDN;
    if (set.octave > 0)
        btn |= 1u << B_OCTUP;
    dimb = (1u << B_PLAY) | (1u << B_ARP) | (1u << B_SEQ) | (1u << B_SEL) | (1u << B_GLO) | (1u << B_EDIT) | (1u << B_FX) | (1u << B_ENV) | (1u << B_SAVE) | (1u << B_OCTDN) |
           (1u << B_OCTUP) | (1u << B_REC) | (1u << B_HOME);
    {
        /* the keys: what sounds bright; with FOLLOW the scale's keys half lit, its roots bright; the low split glowing */
        uint32_t lit = ui.sounding | ui.keys, mid = 0, dim = side_mask(SIDE_L), k;
        if (split_on() && set.follow != FOLLOW_OFF) {
            const scale_t *sc = &SCALES[set.scale % NSCALE];
            for (k = set.split_key; k < NKEYS; k++) {
                float p;
                int32_t rel, i, in = 0;
                if (set.follow == FOLLOW_WHITE) {
                    if (!key_pitch(k, &p))
                        continue;
                } else
                    p = 53.0f + (float)k + 12.0f * (float)set.octave;
                rel = (((int32_t)p - (int32_t)ui.root) % 12 + 12) % 12;
                for (i = 0; i < sc->n; i++)
                    if (sc->st[i] == rel)
                        in = 1;
                if (!in)
                    continue;
                if (rel == 0)
                    lit |= 1u << k;
                else
                    mid |= 1u << k;
            }
        }
        plat_lights(btn | (ui.btn & ~(1u << B_PLAY)), lit, dimb, dim & ~lit, 1);
        plat_lights_mid(0, mid & ~lit);
    }
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ drawing --- */
static void ctext(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

static uint16_t mix565(uint16_t a, uint16_t b, uint32_t t)   /* a .. b, t 0..256 */
{
    int32_t ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
    int32_t br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31, tt = (int32_t)(t > 256u ? 256u : t);
    return (uint16_t)((((ar + ((br - ar) * tt >> 8)) & 31) << 11) | (((ag + ((bg - ag) * tt >> 8)) & 63) << 5) |
                      ((ab + ((bb - ab) * tt >> 8)) & 31));
}

/* rain, in Japanese (a neon sign): 13 x 10 */
static const char *const KANJI_AME[10] = {
    "#############", "      #      ", "#############", "#     #     #", "#  #  #  #  #",
    "#   # #   # #", "#     #     #", "#  #  #  #  #", "#   # #   # #", "#     #   ###",
};
static void draw_kanji(int32_t x, int32_t y, int32_t sc, uint16_t c)
{
    int32_t r, i;
    for (r = 0; r < 10; r++)
        for (i = 0; i < 13; i++)
            if (KANJI_AME[r][i] == '#')
                cv_rect(x + i * sc, y + r * sc, sc, sc, c);
}

static void neon_text(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, int lit)
{
    int32_t x = cx - text_w(f, s) / 2;
    if (!lit) {
        cv_text(x, y, f, s, RGB(30, 40, 60));
        return;
    }
    cv_text(x + 2, y + 2, f, s, RGB(120, 20, 90));
    cv_text(x - 1, y, f, s, RGB(20, 90, 120));
    cv_text(x, y, f, s, C_CYAN);
}

#include "film.c"                        /* the film: the scenes, the rain, the cuts */

static void world_level(void) { eng_world(film.scene, (float)set.city / 100.0f); }

static void draw_head(void)
{
    char t[8];
    int lit = !(film.flicker && (film.frame & 4u));
    int32_t x = 6;
    cv_begin(240, 24, C_BLACK);
    cv_text(x + 1, 5, &FONT_B, "STRAN", RGB(110, 10, 10));
    cv_text(x, 4, &FONT_B, "STRAN", C_RED);
    x += text_w(&FONT_B, "STRAN");
    cv_text(x + 1, 5, &FONT_B, "GER", lit ? RGB(110, 10, 10) : C_BLACK);
    cv_text(x, 4, &FONT_B, "GER", lit ? C_RED : RGB(60, 20, 20));
    cv_text(76, 7, &FONT_XS, "DRONE", ui.drone ? C_AMBER : RGB(60, 50, 50));
    cv_text(110, 7, &FONT_XS, "LOW", ui.lowhold ? C_AMBER : RGB(60, 50, 50));
    cv_text(132, 7, &FONT_XS, "ARP", set.arp ? C_RED : RGB(60, 40, 40));
    cv_text(154, 7, &FONT_XS, "EPIC", set.epic ? C_MAG : RGB(60, 40, 55));
    t[0] = 'O';
    t[1] = 'C';
    t[2] = 'T';
    signed_text(set.octave, t + 3);
    cv_text(236 - text_w(&FONT_XS, t), 7, &FONT_XS, t, RGB(150, 150, 170));
    cv_rect(0, 23, 240, 1, RGB(60, 20, 60));
    cv_blit(0, 0);
}

static void draw_knob(int k, int32_t x, int32_t y, uint32_t now)
{
    const macro_t *mc = &MACROS[k];
    int32_t v = KM[k], w = 54, z = zone_at(mc, v), zn = zone_near(mc, v), fill = (v * (w - 2)) / 127;
    uint32_t i;
    int hot = ui.knob_at[k] && now - ui.knob_at[k] < 600u;
    char t[8];
    cv_text(x, y, &FONT_XS, mc->name, hot ? C_WHITE : RGB(140, 140, 160));
    int_text(v, t);
    cv_text(x + w - text_w(&FONT_XS, t), y, &FONT_XS, t, hot ? C_CYAN : C_AMBER);
    cv_rect(x, y + 14, w, 8, RGB(50, 30, 60));
    cv_rect(x + 1, y + 15, w - 2, 6, C_BLACK);
    cv_rect(x + 1, y + 15, fill, 6, z >= 0 ? C_CYAN : C_MAG);
    for (i = 0; i < mc->nzones; i++) {
        int32_t tx = x + 1 + (mc->centre[i] * (w - 3)) / 127;
        cv_rect(tx, y + 11, 1, 3, (int)i == z ? C_CYAN : RGB(110, 90, 120));
    }
    ctext(x + w / 2, y + 26, &FONT_XS, mc->zone[z >= 0 ? z : zn], z >= 0 ? C_WHITE : RGB(90, 80, 100));
}

static void draw_pknob(int k, int32_t x, int32_t y, uint32_t now)   /* a page's knob: its name, its value */
{
    int32_t lo, hi, v = pk_get(ui.page, k), w = 54;
    int hot = ui.knob_at[k] && now - ui.knob_at[k] < 600u;
    char t[16];
    pk_range(ui.page, k, &lo, &hi);
    cv_text(x, y, &FONT_XS, PK_NAME[ui.page - 1][k], hot ? C_WHITE : RGB(140, 140, 160));
    cv_rect(x, y + 14, w, 8, RGB(50, 30, 60));
    cv_rect(x + 1, y + 15, w - 2, 6, C_BLACK);
    cv_rect(x + 1, y + 15, (v - lo) * (w - 2) / (hi - lo > 0 ? hi - lo : 1), 6, hot ? C_CYAN : C_MAG);
    pk_text(ui.page, k, t);
    ctext(x + w / 2, y + 26, &FONT_XS, t, hot ? C_CYAN : C_AMBER);
}

static void draw_panel(void)
{
    uint32_t now = plat_ms(), i;
    char t[40], n[8];
    cv_begin(240, 96, C_BLACK);
    cv_rect(0, 0, 240, 1, RGB(60, 20, 60));
    if (now < ui.msg_until) {
        int32_t x = cv_text(6, 5, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 5, &FONT_B, ui.msg[1], C_CYAN);
    } else if (ui.page != PG_MAIN) {
        cv_text(6, 5, &FONT_B, "//", C_MAG);
        cv_text(28, 5, &FONT_B, PAGE_NAME[ui.page], C_AMBER);
    } else {
        t[0] = '>';
        t[1] = ' ';
        t[2] = (char)('1' + set.preset);
        t[3] = ' ';
        t[4] = 0;
        cv_text(6, 5, &FONT_B, t, C_MAG);
        cv_text(6 + text_w(&FONT_B, t), 5, &FONT_B, PR.name, C_AMBER);
    }
    if (ui.page == PG_MAIN && now < ui.zone_until && ui.zone_k >= 0) {
        const macro_t *mc = &MACROS[ui.zone_k];
        ctext(120, 28, &FONT_XS, mc->name, RGB(140, 140, 160));
        ctext(120, 42, &FONT_M, mc->zone[ui.zone_z], C_CYAN);
        cv_rect(60, 70, 120, 1, C_MAG);
    } else {
        for (i = 0; i < 4u; i++) {
            if (ui.page == PG_MAIN)
                draw_knob((int)i, 4 + (int32_t)i * 59, 28, now);
            else
                draw_pknob((int)i, 4 + (int32_t)i * 59, 28, now);
        }
    }
    t[0] = 0;
    if (ui.page == PG_ARP) {             /* the tempo: its own, or EXT (MIDI clock in) */
        if (eng_clock_ext())
            text_cat(t, "EXT CLOCK  ");
        else {
            int_text(set.bpm, n);
            text_cat(t, n);
            text_cat(t, " BPM  ");
        }
    } else if (split_on() && set.follow != FOLLOW_OFF) {   /* the melody's key */
        text_cat(t, NOTE_NAME[ui.root]);
        text_cat(t, " ");
        text_cat(t, SCALES[set.scale].name);
        text_cat(t, "  ");
    } else {
        text_cat(t, "VOICES ");
        int_text(eng_voices(), n);
        text_cat(t, n);
        text_cat(t, "/8  ");
    }
    text_cat(t, "CPU ");
    int_text((int32_t)plat_cpu_pct(), n);
    text_cat(t, n);
    text_cat(t, "% XR ");                /* xruns: blocks the sound came late (a click each) */
    int_text((int32_t)plat_xruns(), n);
    text_cat(t, n);
    if (ui.page == PG_CUES) {            /* the eight cues: kept (amber), picked (frame), sounding (cyan) */
        uint32_t c;
        for (c = 0; c < NCUE; c++) {
            int32_t x = 6 + (int32_t)c * 29;
            uint16_t col = ui.cue_cur == c + 1u ? C_CYAN : set.cue_n[c] ? C_AMBER : RGB(40, 30, 50);
            if (ui.cue_sel == c + 1u)
                cv_rect(x - 1, 77, 26, 16, C_WHITE);
            cv_rect(x, 78, 24, 14, col);
            cv_rect(x + 1, 79, 22, 12, C_BLACK);
            n[0] = (char)('1' + c);
            n[1] = 0;
            cv_text(x + 3, 80, &FONT_XS, n, col);
            if (set.cue_n[c])
                cv_rect(x + 12, 84, (int32_t)set.cue_n[c] * 2, 3, col);
        }
        cv_blit(0, 144);
        return;
    }
    cv_text(6, 80, &FONT_XS, t, RGB(40, 140, 150));
    if (set.press != PRESS_OFF) {        /* the pressure: the hardest-pressed key */
        uint32_t k, pmax = 0;
        for (k = 0; k < NKEYS; k++)
            if (ui.prs[k] > pmax)
                pmax = ui.prs[k];
        cv_rect(200, 82, 34, 6, RGB(50, 30, 60));
        cv_rect(201, 83, (int32_t)(pmax * 32u / 127u), 4, C_MAG);
    }
    cv_blit(0, 144);
}

/* the controls, drawn: the FM-1 as a plan, every control named with what it does here, what is on lit */
static void sheet_btn(int32_t x, int32_t y, const char *name, const char *does, int on)
{
    uint16_t c = on ? C_CYAN : RGB(90, 70, 110);
    cv_rect(x, y, 56, 26, c);
    cv_rect(x + 1, y + 1, 54, 24, on ? RGB(10, 40, 50) : RGB(12, 8, 18));
    ctext(x + 28, y + 1, &FONT_XS, name, on ? C_WHITE : RGB(150, 140, 170));
    ctext(x + 28, y + 13, &FONT_XS, does, on ? C_CYAN : C_AMBER);
}

static void draw_sheet(void)
{
    static const char *const KN[NPAGE][4] = {
        {"SWELL", "DRIFT", "SPACE", "RAIN"},  {"SPLIT", "POINT", "SCALE", "FOLLOW"}, {"PRESS", "AMOUNT", "TONE", "VIB"},
        {"LO GLIDE", "LO MODE", "GLIDE", "MODE"}, {"CITY", "STORY", "LOGS", "TIME"},   {"CUE", "FADE", "HOLD", "AUTO"},
        {"RATE", "PATTERN", "OCTAVES", "GATE"},
    };
    uint32_t i;
    cv_begin(240, 120, C_BLACK);
    cv_text(4, 2, &FONT_B, "// CONTROLS", C_MAG);
    cv_text(158, 6, &FONT_XS, "BUTTON: BACK", RGB(110, 100, 130));
    for (i = 0; i < 3u; i++) {           /* the three encoders */
        static const char *const EN[3] = {"SELECT", "ALGO", "PRESET"}, *const ED[3] = {"TONE", "VIBRATO", "PRESET"};
        int32_t x = 4 + (int32_t)i * 79;
        cv_rect(x, 21, 8, 8, RGB(70, 70, 90));
        cv_text(x + 11, 19, &FONT_XS, EN[i], RGB(150, 140, 170));
        cv_text(x + 11, 30, &FONT_XS, ED[i], C_AMBER);
    }
    for (i = 0; i < 4u; i++) {           /* the four knobs, as this page has them */
        char d[3] = {'K', (char)('1' + i), 0};
        int32_t x = 4 + (int32_t)i * 59;
        cv_rect(x, 46, 8, 8, C_MAG);
        cv_text(x + 11, 44, &FONT_XS, d, RGB(150, 140, 170));
        cv_text(x + 11, 54, &FONT_XS, KN[ui.page][i], C_CYAN);
    }
    cv_rect(0, 66, 240, 1, RGB(60, 20, 60));
    sheet_btn(4, 68, "FX", "WORLD", ui.page == PG_WORLD);
    sheet_btn(62, 68, "SEL", "SPLIT", ui.page == PG_SPLIT);
    sheet_btn(120, 68, "ENV", "CUES", ui.page == PG_CUES);
    sheet_btn(178, 68, "EDIT", "GLIDE", ui.page == PG_GLIDE);
    sheet_btn(4, 94, "GLO", "PRESSURE", ui.page == PG_PRESS);
    sheet_btn(62, 94, "HOME", "MAIN", ui.page == PG_MAIN);
    sheet_btn(120, 94, "SAVE", "KEEP CUE", 0);
    sheet_btn(178, 94, "LFO", "ARP PAGE", ui.page == PG_ARP);
    cv_blit(0, 24);
    cv_begin(240, 96, C_BLACK);
    sheet_btn(4, 2, "PLAY", "DRONE", ui.drone);
    sheet_btn(62, 2, "SEQ", "LOW HOLD", ui.lowhold);
    sheet_btn(120, 2, "ARP", "ARP/EPIC", set.arp);
    sheet_btn(178, 2, "REC", "MEMORY", ui.memory);
    sheet_btn(4, 30, "OCT-/+", "OCTAVE", set.octave != 0);
    sheet_btn(62, 30, "REC 1s", "ALL OFF", 0);
    sheet_btn(120, 30, "HOME 1s", "RESET", 0);
    sheet_btn(178, 30, "HOME 2x", "THIS", 1);
    for (i = 0; i < NKEYS; i++) {        /* the keys: the low split, the scale */
        int32_t x = 4 + (int32_t)i * 8;
        uint16_t c = (side_mask(SIDE_L) >> i & 1u) ? C_AMBER : RGB(70, 70, 90);
        if (ui.sounding >> i & 1u)
            c = C_CYAN;
        cv_rect(x, 64, 7, 22, is_white(i) ? c : mix565(c, C_BLACK, 120));
    }
    cv_text(4, 86, &FONT_XS, split_on() ? "LOW SPLIT" : "KEYS", C_AMBER);
    cv_blit(0, 144);
}

/* the intro, 4 s: a terminal boots; the city rises out of the dark in the rain, searchlights sweep, a car crosses;
 * the title buzzes on letter by letter like neon tubes, rain in Japanese beside it; lightning; the name. Drawn in two
 * bands (the canvas holds 144 rows), each from the same picture in screen rows. */
#define INTRO_MS 4200u
static const char *const BOOT[4] = {"> POWER .......... OK", "> CITY GRID ...... ONLINE", "> WEATHER ........ RAIN",
                                    "> LINK ........... STRANGER"};

static void intro_band(int32_t y0, int32_t h, uint32_t ms)
{
    uint32_t i, glow = ms < 600u ? 0u : ms > 1800u ? 256u : (ms - 600u) * 256u / 1200u;
    int32_t y, rise = ms < 500u ? 0 : ms > 1700u ? 100 : (int32_t)((ms - 500u) / 12u);
    uint16_t sky0 = mix565(C_BLACK, RGB(6, 4, 22), glow), sky1 = mix565(C_BLACK, RGB(150, 64, 36), glow);
    int flash = ms >= 3000u && ms < 3120u;
    cv_begin(240, (uint32_t)h, C_BLACK);
    for (y = 0; y < h; y++) {            /* the sky, the smog lit from below */
        int32_t sy = y0 + y;
        uint32_t t = sy < 200 ? (uint32_t)(sy * sy) * 256u / 40000u : 256u;
        uint16_t c = mix565(sky0, sky1, t);
        if (flash)
            c = mix565(c, RGB(200, 190, 240), 150);
        cv_rect(0, y, 240, 1, c);
    }
    if (ms > 900u)                       /* searchlights */
        for (i = 0; i < 2u; i++) {
            int32_t bx = 60 + (int32_t)i * 120, dx = (int32_t)(90.0f * fm_sin_turns((float)ms * 0.00012f + (float)i * 0.4f));
            uint16_t c = mix565(C_BLACK, RGB(50, 50, 80), glow);
            cv_line(bx, 240 - y0, bx + dx, 60 - y0, c);
            cv_line(bx + 1, 240 - y0, bx + dx + 4, 60 - y0, c);
        }
    for (i = 0; i < 60u; i++) {          /* the far towers, rising */
        uint32_t q = hsh(i, 777u);
        int32_t th = (int32_t)(20u + q % 60u) * rise / 100, x = (int32_t)i * 4;
        cv_rect(x, 240 - th - y0, 4, th, RGB(22, 16, 42));
        if (th > 10 && (q & 3u) == 0u && ms > 1500u)
            cv_rect(x + 1, 240 - th + 4 + (int32_t)((q >> 4) & 15u) - y0, 1, 1, RGB(140, 90, 50));
    }
    for (i = 0; i < 9u; i++) {           /* the near towers, rising later, their windows coming on */
        uint32_t q = hsh(i, 991u);
        int32_t w = 18 + (int32_t)(q % 14u), th = (int32_t)(50u + (q >> 5) % 70u) * (rise > 30 ? rise - 30 : 0) / 70,
                x = (int32_t)i * 27 - 4 + (int32_t)(q % 7u), wy, wx;
        cv_rect(x, 240 - th - y0, w, th, RGB(10, 10, 24));
        for (wy = 240 - th + 5; wy < 236; wy += 7)
            for (wx = x + 3; wx < x + w - 3; wx += 5)
                if (hsh(q, (uint32_t)(wy * 7 + wx)) % 5u == 0u && ms > 1900u + (uint32_t)(240 - wy) * 6u)
                    cv_rect(wx, wy - y0, 2, 2, RGB(255, 190, 90));
        if ((q >> 9) & 1u)
            cv_rect(x, 240 - th - y0, 1, th, (q >> 10) & 1u ? C_CYAN : C_MAG);
    }
    if (ms > 1300u && ms < 3600u) {      /* a car crossing */
        int32_t cx = (int32_t)((ms - 1300u) / 9u) - 10, cy = 132 - y0;
        for (i = 1; i < 7u; i++)
            cv_rect(cx - (int32_t)i * 4, cy + 1, 3, 1, mix565(C_BLACK, C_CYAN, 220u - i * 30u));
        cv_rect(cx - 3, cy, 7, 3, RGB(60, 60, 80));
        cv_rect(cx + 4, cy + 1, 2, 1, RGB(255, 255, 230));
    }
    if (ms > 700u)                       /* rain */
        for (i = 0; i < 70u; i++) {
            uint32_t q = hsh(i, 4242u);
            int32_t x = (int32_t)(q % 250u), sp = 5 + (int32_t)((q >> 8) % 4u),
                    yy = (int32_t)((q >> 12) % 240u + ms / 33u * (uint32_t)sp) % 260 - 10;
            x -= yy / 7;
            cv_line(x, yy - y0, x - 1, yy + 6 - y0, mix565(C_BLACK, C_RAIN, glow));
        }
    if (flash && y0 == 0) {              /* the bolt */
        int32_t bx = 186, by;
        for (by = 0; by < 120; by += 12) {
            int32_t nx = bx + (int32_t)(hsh((uint32_t)by, 5u) % 13u) - 6;
            cv_line(bx, by, nx, by + 12, RGB(240, 235, 255));
            bx = nx;
        }
    }
    if (y0 == 0) {
        /* the terminal booting */
        for (i = 0; i < 4u; i++) {
            uint32_t t0 = 80u + i * 260u, n;
            char t[32];
            uint32_t k;
            if (ms < t0)
                break;
            n = (ms - t0) / 14u;
            for (k = 0; k < 31u && BOOT[i][k] && k < n; k++)
                t[k] = BOOT[i][k];
            t[k] = 0;
            cv_text(4, 4 + (int32_t)i * 12, &FONT_XS, t,
                    ms > 1700u ? mix565(RGB(40, 140, 150), C_BLACK, (ms - 1700u) / 4u > 230u ? 230u : (ms - 1700u) / 4u)
                               : RGB(60, 210, 220));
        }
        /* the title: each letter buzzes on, then holds */
        if (ms > 1700u) {
            static const char *const T = "STRANGER";
            const felucca_font_t *f = &APP_BIG_FONT;
            int32_t x = 120 - text_w(f, T) / 2;
            for (i = 0; T[i]; i++) {
                char c[2] = {T[i], 0};
                uint32_t on = 1700u + i * 110u, lit;
                lit = ms > on + 260u || (ms > on && ((ms / 40u + i * 7u) * 2654435761u >> 30) > 0u);
                if (ms > 3000u && ms < 3080u && i == 4u)
                    lit = 0;             /* one tube stutters in the lightning */
                if (lit) {
                    cv_text(x + 2, 72, f, c, RGB(120, 20, 90));
                    cv_text(x - 1, 70, f, c, RGB(20, 90, 120));
                    cv_text(x, 70, f, c, C_CYAN);
                } else if (ms > on)
                    cv_text(x, 70, f, c, RGB(30, 40, 60));
                x += text_w(f, c);
            }
            if (ms > 1700u && ms < 2400u) {   /* a scan passing over it */
                int32_t sy = 60 + (int32_t)((ms - 1700u) / 14u);
                cv_rect(0, sy, 240, 1, mix565(C_CYAN, C_BLACK, 120));
            }
        }
        if (ms > 2500u)                  /* the sign: rain */
            draw_kanji(200, 20, 2, ((ms / 60u) % 9u == 0u && ms < 3300u) ? RGB(80, 20, 60) : C_MAG);
        if (ms > 3150u) {
            const char *by = "by swtdr34mz";
            uint32_t n = (ms - 3150u) / 40u, k;
            char t[16];
            for (k = 0; k < 15u && by[k] && k < n; k++)
                t[k] = by[k];
            t[k] = 0;
            cv_text(120 - text_w(&FONT_XS, by) / 2, 108, &FONT_XS, t, RGB(170, 170, 190));
        }
    }
    for (y = (int32_t)((ms / 40u) & 3u); y < h; y += 4)   /* scanlines */
        px_dim_row(y);
    cv_blit(0, (uint32_t)y0);
}

static void draw_intro(uint32_t ms)
{
    intro_band(0, 144, ms);
    intro_band(144, 96, ms);
}

/* ------------------------------------------------------------- the contract --- */
void app_init(void)
{
    eng_init();
    plat_cpu2_set(1);                    /* half the voices on the second core (one core still works) */
    settings_load();
    knobs_from_settings();
    sound();
    arp_cfg();
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    eng_render(lr, n, (float)master_q12 * (1.0f / 4096.0f) * 8300000.0f);
}

void app_panic(void)
{
    eng_panic();
    ui.latched = ui.sounding = 0;
    ui.deep = 0.0f;
}

void app_overload(void) { eng_shed(); }

void app_splash(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_intro(INTRO_MS - 200u);
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.rnd = 0x51ED270Bu;
    film_init();
    ui.zone_k = -1;
    ui.redraw = ui.head_redraw = 1;
    ui.intro = 1;
    ui.intro_t0 = plat_ms();
}

void app_frame(void)
{
    uint32_t now = plat_ms(), pkt, k;
    int port;
    input();
    lights();
    while (plat_midi_in(&pkt, &port)) {  /* MIDI in plays the brass (any channel) */
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        int id = MIDI_ID + (int)d1;
        (void)port;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu) {   /* MIDI clock: the arp follows it */
            eng_clock(st == 0xF8u ? CLK_TICK : st == 0xFAu ? CLK_START : st == 0xFBu ? CLK_CONT : CLK_STOP);
            continue;
        }
        if ((st & 0xF0u) == 0x90u && d2) {
            eng_note_on(id, (float)d1, (float)d2 / 127.0f);
            if (set.epic) {
                eng_note_on(id + 100, (float)d1 + 7.0f, (float)d2 / 127.0f * 0.85f);
                eng_note_on(id + 200, (float)d1 + 12.0f, (float)d2 / 127.0f * 0.8f);
            }
        } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) {
            eng_note_off(id);
            eng_note_off(id + 100);
            eng_note_off(id + 200);
        } else if ((st & 0xF0u) == 0xA0u) {   /* polyphonic pressure: as the keys' */
            eng_press(id, (float)d2 / 127.0f);
            eng_press(id + 100, (float)d2 / 127.0f);
            eng_press(id + 200, (float)d2 / 127.0f);
        } else if ((st & 0xF0u) == 0xB0u && (d1 == 123u || d1 == 120u))
            eng_all_off();
    }
    if (ann_on() && ann_where()) {
        char t[64] = "STRANGER SYNTH. ";
        text_cat(t, PR.name);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(now);
    /* the settings to the flash: a write stops the sound for a moment (a click while it plays), so only in a quiet
     * moment (no note, no cue, no MEMORY), faded out first; the room and the rain come back a moment later */
    if (set_dirty && now - dirty_at > 3000u && eng_voices() == 0 && !ui.memory && !ui.cue_cur) {
        if (!ui.ducking) {
            eng_duck(1);
            ui.ducking = 1;
            ui.duck_t = now;
        } else if (eng_ducked() || now - ui.duck_t > 300u) {
            settings_save();
            eng_duck(0);
            ui.ducking = 0;
        }
    } else if (ui.ducking) {
        eng_duck(0);
        ui.ducking = 0;
    }
    if (ui.intro) {
        uint32_t ms = now - ui.intro_t0;
        if (ui.intro == 1 && ms < INTRO_MS) {
            if (now - ui.scene_at >= 33u) {
                ui.scene_at = now;
                draw_intro(ms);
            }
            return;
        }
        ui.intro = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.redraw = ui.head_redraw = 1;
        film_cut(now);
    }
    if ((ui.msg_until && now >= ui.msg_until) || (ui.zone_until && now >= ui.zone_until)) {
        if (now >= ui.msg_until)
            ui.msg_until = 0;
        if (now >= ui.zone_until)
            ui.zone_until = 0;
        ui.redraw = 1;
    }
    cue_tick(now);
    if ((now & 511u) < 20u)
        weather();
    if (ui.sheet) {                      /* the controls, drawn (a few times a second: what is on, live) */
        if (now - ui.scene_at >= 200u || ui.sheet == 1) {
            ui.scene_at = now;
            ui.sheet = 2;
            draw_sheet();
        }
        return;
    }
    if (now - ui.scene_at >= 40u) {      /* the film at 25 frames a second */
        uint8_t fl = film.flicker;
        ui.scene_at = now;
        for (k = 0; k < NKEYS; k++) {    /* the lights follow the notes, and fade */
            if (ui.sounding >> k & 1u)
                ui.win[k] = 255;
            else if (ui.win[k])
                ui.win[k] = (uint8_t)(ui.win[k] > 6u ? ui.win[k] - 6u : 0u);
        }
        film_frame(now);
        if (fl || film.flicker)
            ui.head_redraw = 1;
        if ((film.frame & 7u) == 0u)
            ui.redraw = 1;               /* the voices, the CPU */
    }
    if (ui.head_redraw) {
        ui.head_redraw = 0;
        draw_head();
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw_panel();
    }
}

/* ------------------------------------------------------------- backup ------ */
int app_backup_playing(void) { return ui.sounding != 0; }
void app_backup_flush(void)
{
    if (set_dirty)
        settings_save();
}
void app_backup_hold(int on) { (void)on; }
void app_backup_restored(void)
{
    settings_load();
    knobs_from_settings();
    sound();
    ui.redraw = ui.head_redraw = 1;
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "preset"))
        *got = set.preset;
    else if (!strcmp(what, "drone"))
        *got = ui.drone;
    else if (!strcmp(what, "lowhold"))
        *got = ui.lowhold;
    else if (!strcmp(what, "epic"))
        *got = set.epic;
    else if (!strcmp(what, "arp"))
        *got = set.arp;
    else if (!strcmp(what, "arp_steps"))
        *got = (int)eng_arp_steps();
    else if (!strcmp(what, "arp_pitch"))
        *got = (int)(eng_arp_pitch() + 0.5f);
    else if (!strcmp(what, "arp_rate"))
        *got = set.arp_rate;
    else if (!strcmp(what, "arp_pat"))
        *got = set.arp_pat;
    else if (!strcmp(what, "bpm"))
        *got = set.bpm;
    else if (!strcmp(what, "arp_oct"))
        *got = set.arp_oct;
    else if (!strcmp(what, "arp_ext"))
        *got = eng_clock_ext();
    else if (!strcmp(what, "arp_notes")) {   /* the keys the arp holds */
        uint32_t k, n = 0;
        for (k = 0; k < NKEYS; k++)
            n += (ui.sounding >> k & 1u) && arp_key(k);
        *got = (int)n;
    } else if (!strcmp(what, "voices_max_ok"))   /* the arp: never more than two of its voices at once */
        *got = eng_voices() <= 3;
    else if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "tone"))
        *got = set.tone;
    else if (!strcmp(what, "vib"))
        *got = set.vib;
    else if (!strcmp(what, "page"))
        *got = ui.page;
    else if (!strcmp(what, "split"))
        *got = set.split_oct ? set.split_key : 0;
    else if (!strcmp(what, "split_oct"))
        *got = set.split_oct;
    else if (!strcmp(what, "scale"))
        *got = set.scale;
    else if (!strcmp(what, "follow"))
        *got = set.follow;
    else if (!strcmp(what, "root"))
        *got = ui.root;
    else if (!strcmp(what, "glide_h"))
        *got = set.glide_h;
    else if (!strcmp(what, "mode_h"))
        *got = set.mode_h;
    else if (!strcmp(what, "mono"))
        *got = ui.mono[SIDE_R];
    else if (!strcmp(what, "cue"))
        *got = ui.cue_cur;
    else if (!strcmp(what, "memory"))
        *got = ui.memory;
    else if (!strcmp(what, "sheet"))
        *got = ui.sheet;
    else if (!strcmp(what, "story"))
        *got = set.story;
    else if (!strcmp(what, "press"))
        *got = set.press;
    else if (!strcmp(what, "pamt"))
        *got = set.pamt;
    else if (!strncmp(what, "kp", 2) && what[2])   /* kp0..kp26: the pitch a key started at */
        *got = (int)ui.kp[atoi(what + 2) % NKEYS];
    else if (!strncmp(what, "prs", 3) && what[3])
        *got = ui.prs[atoi(what + 3) % NKEYS];
    else if (!strcmp(what, "voices"))
        *got = eng_voices();
    else if (!strcmp(what, "notes"))
        *got = (int)eng_notes();
    else if (!strcmp(what, "thunder"))
        *got = (int)eng_thunder();
    else if (!strcmp(what, "latched"))
        *got = (int)ui.latched;
    else if (!strcmp(what, "sounding"))
        *got = (int)ui.sounding;
    else if (!strcmp(what, "deep"))
        *got = (int)ui.deep;
    else if (!strcmp(what, "intro"))
        *got = ui.intro;
    else if (!strcmp(what, "scene"))
        *got = film.scene;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (what[0] == 'm' && what[1] >= '1' && what[1] <= '4' && !what[2])
        *got = KM[what[1] - '1'];
    else if (!strncmp(what, "zone", 4) && what[4] >= '1' && what[4] <= '4')
        *got = zone_at(&MACROS[what[4] - '1'], KM[what[4] - '1']);
    else
        return 0;
    return 1;
}
#endif
