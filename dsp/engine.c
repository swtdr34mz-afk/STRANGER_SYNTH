/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (OFFWORLD) */
/* OFFWORLD's sound engine (engine.h). Single precision only, no double, no libc.
 * The render works in chunks of 32 frames: the envelopes, the pitches, the filters' coefficients and the effects'
 * modulation are updated per chunk, the oscillators, filters and effects per sample. */
#include "engine.h"
#include "fastmath.h"
#include "plat.h"

#define SR ENG_SR
#define CH 32u                           /* a chunk: 0.73 ms */

/* ------------------------------------------------------------- the macros --- */
/* The zones' centres are where the curves below have their breakpoints: at a zone every parameter a macro moves is
 * exactly the value chosen for it there; between two zones they move from one to the other. */
const macro_t MACROS[4] = {
    {"SWELL", 5, {12, 45, 75, 105, 127}, {"STAB", "BRASS", "SWELL", "TIDE", "ETERNAL"}},
    {"DRIFT", 5, {0, 35, 70, 105, 127}, {"CLEAN", "CS", "ENSEMBLE", "WIDE", "BROKEN"}},
    {"SPACE", 5, {15, 50, 85, 115, 127}, {"ROOM", "HALL", "CATHEDRAL", "CANYON", "FREEZE"}},
    {"RAIN", 5, {0, 35, 70, 105, 127}, {"DRY", "DRIZZLE", "RAIN", "STORM", "FLOOD"}},
};

/* name, o2 semitones, o2 pulse, width, o2 level, sub, cutoff, envelope octaves, resonance, velocity, macros */
const preset_t PRESETS[NPRESETS] = {
    {"OFFWORLD BRASS", 0.0f, 0, 0.50f, 1.00f, 0.35f, 260.0f, 4.6f, 0.20f, 0.60f, {45, 35, 85, 0}},
    {"NEON TOWER", 7.0f, 0, 0.50f, 0.60f, 0.20f, 600.0f, 3.6f, 0.25f, 0.70f, {45, 35, 50, 0}},
    {"PYRAMID HALL", 12.0f, 0, 0.50f, 0.55f, 0.30f, 300.0f, 4.0f, 0.10f, 0.50f, {105, 70, 115, 35}},
    {"LAST SUNRISE", 19.0f, 0, 0.50f, 0.42f, 0.15f, 650.0f, 2.4f, 0.12f, 0.50f, {105, 70, 50, 35}},
    {"SKYLINE", -12.0f, 0, 0.50f, 0.70f, 0.00f, 900.0f, 3.0f, 0.30f, 0.80f, {12, 70, 50, 35}},
    {"DEEP SECTOR", -12.0f, 1, 0.55f, 0.80f, 0.60f, 140.0f, 3.6f, 0.45f, 0.40f, {105, 105, 85, 105}},
    {"ACID SKY", 0.0f, 0, 0.50f, 0.50f, 0.20f, 180.0f, 5.0f, 0.78f, 0.80f, {45, 35, 85, 35}},
};

/* a curve: breakpoints (x 0..127, y), x rising, straight lines between them */
typedef struct {
    uint8_t x;
    float y;
} bp_t;
#define NBP(t) (sizeof(t) / sizeof((t)[0]))
#define CURVE(t, x) curve((t), NBP(t), (x))

static float curve(const bp_t *t, uint32_t n, float x)
{
    uint32_t i;
    if (x <= (float)t[0].x)
        return t[0].y;
    for (i = 1; i < n; i++)
        if (x <= (float)t[i].x) {
            float a = (x - (float)t[i - 1].x) / (float)(t[i].x - t[i - 1].x);
            return t[i - 1].y + a * (t[i].y - t[i - 1].y);
        }
    return t[n - 1].y;
}

/* SWELL: the attacks (amp, filter), the filter's fall and where it stays, the release */
static const bp_t SW_AA[] = {{0, 0.004f}, {12, 0.010f}, {45, 0.12f}, {75, 0.9f}, {105, 3.5f}, {127, 7.0f}};
static const bp_t SW_FA[] = {{0, 0.010f}, {12, 0.030f}, {45, 0.35f}, {75, 1.8f}, {105, 5.0f}, {127, 9.0f}};
static const bp_t SW_FD[] = {{0, 0.15f}, {12, 0.25f}, {45, 1.2f}, {75, 2.5f}, {105, 4.0f}, {127, 6.0f}};
static const bp_t SW_FS[] = {{0, 0.20f}, {12, 0.25f}, {45, 0.45f}, {75, 0.60f}, {105, 0.75f}, {127, 0.85f}};
static const bp_t SW_R[] = {{0, 0.35f}, {12, 0.5f}, {45, 1.4f}, {75, 2.5f}, {105, 4.0f}, {127, 7.0f}};
static const bp_t SW_ENV[] = {{0, 1.0f}, {45, 1.0f}, {75, 0.92f}, {105, 0.82f}, {127, 0.76f}};
/* DRIFT: the oscillators apart (cents), the random drift (cents), the ensemble, the stereo spread, the tape's wow, grit */
static const bp_t DR_DET[] = {{0, 1.0f}, {35, 7.0f}, {70, 10.0f}, {105, 16.0f}, {127, 28.0f}};
static const bp_t DR_RND[] = {{0, 0.0f}, {35, 3.0f}, {70, 4.0f}, {105, 6.0f}, {127, 14.0f}};
static const bp_t DR_ENS[] = {{0, 0.0f}, {35, 0.35f}, {70, 0.70f}, {105, 0.85f}, {127, 0.90f}};
static const bp_t DR_SPR[] = {{0, 0.0f}, {35, 0.3f}, {70, 0.5f}, {105, 0.9f}, {127, 0.9f}};
static const bp_t DR_WOW[] = {{0, 0.0f}, {105, 0.0f}, {127, 18.0f}};
static const bp_t DR_DRV[] = {{0, 1.0f}, {105, 1.0f}, {127, 2.2f}};
/* SPACE: the room's size, its decay (T60), the wet share, its damping, the delay's send and feedback */
static const bp_t SP_SIZE[] = {{0, 0.45f}, {15, 0.6f}, {50, 1.0f}, {85, 1.6f}, {115, 2.3f}, {127, 2.3f}};
static const bp_t SP_T60[] = {{0, 0.8f}, {15, 1.5f}, {50, 4.0f}, {85, 10.0f}, {115, 22.0f}, {126, 30.0f}};
static const bp_t SP_WET[] = {{0, 0.08f}, {15, 0.18f}, {50, 0.35f}, {85, 0.50f}, {115, 0.60f}, {127, 0.65f}};
static const bp_t SP_DAMP[] = {{0, 5000.0f}, {50, 6500.0f}, {85, 5000.0f}, {115, 3800.0f}, {127, 4000.0f}};
static const bp_t SP_DSEND[] = {{0, 0.0f}, {15, 0.0f}, {50, 0.12f}, {85, 0.18f}, {115, 0.25f}, {127, 0.20f}};
static const bp_t SP_DFB[] = {{0, 0.30f}, {50, 0.35f}, {85, 0.45f}, {115, 0.55f}, {127, 0.50f}};
/* RAIN: the hiss, drops a second, the rumble, thunders a second */
static const bp_t RN_HISS[] = {{0, 0.0f}, {35, 0.010f}, {70, 0.022f}, {105, 0.035f}, {127, 0.050f}};
static const bp_t RN_DROPS[] = {{0, 0.0f}, {35, 25.0f}, {70, 110.0f}, {105, 260.0f}, {127, 420.0f}};
static const bp_t RN_RUMB[] = {{0, 0.0f}, {35, 0.004f}, {70, 0.012f}, {105, 0.025f}, {127, 0.035f}};
static const bp_t RN_THUN[] = {{0, 0.0f}, {95, 0.0f}, {105, 0.06f}, {127, 0.12f}};

/* --------------------------------------------------------------- helpers --- */
static float kchunk(float tau_s) { return tau_s <= 0.0f ? 1.0f : 1.0f - fm_expf(-(float)CH / (tau_s * SR)); }
static float onepole(float hz) { return 1.0f - fm_expf(-FM_TWO_PI * fm_minf(hz, 18000.0f) / SR); }
static float hz_of(float midi) { return 440.0f * fm_exp2f((midi - 69.0f) * (1.0f / 12.0f)); }
static void zero(void *d, uint32_t n)
{
    uint8_t *p = (uint8_t *)d;
    while (n--)
        *p++ = 0;
}

static inline float flush_dn(float x) { return (x > -1e-15f && x < 1e-15f) ? 0.0f : x; }   /* no denormals */

static uint32_t rng = 0x2545F491u;
static inline float white(void)          /* -1 .. 1 */
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(int32_t)rng * (1.0f / 2147483648.0f);
}
static inline float rndu(void) { return 0.5f + 0.5f * white(); }   /* 0 .. 1 */

/* the band-limited saw: a naive ramp with its jump smoothed (polyBLEP) */
static inline float blep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}
static inline float saw(float ph, float dt) { return 2.0f * ph - 1.0f - blep(ph, dt); }
static inline float pulse(float ph, float dt, float pw)
{
    float p2 = ph + pw;
    if (p2 >= 1.0f)
        p2 -= 1.0f;
    return saw(ph, dt) - saw(p2, dt);
}

/* ---------------------------------------------------------------- params --- */
typedef struct {
    /* the timbre */
    float o2_semi, pw, o2_lvl, sub, cut_hz, env_oct, res_k, bright;
    uint8_t o2_pulse;
    /* SWELL: per-chunk coefficients */
    float ka_a, kd_a, s_a, kr_a, ka_f, kd_f, s_f, kr_f;
    /* DRIFT */
    float det_c, rnd_c, ens, spread, wow_c, drive;
    /* SPACE */
    float size, t60, wet, damp_k, dsend, dfb;
    uint8_t freeze;
    /* RAIN */
    float hiss, drop_p, rumble, thunder_p;
    /* TONE and VIBRATO */
    float tone_mul, vib_c;
    /* PRESSURE: what pressing harder does (PRESS_*), how much 0..1 */
    uint8_t press;
    float pamt;
} prm_t;
/* P_next: written by the main loop (its seq odd while it writes); T: the render's copy (the target); P: what plays,
 * its continuous values gliding to T a block at a time (a preset changed: no jump, no click) */
static prm_t P, T, P_next;
static volatile uint32_t p_seq, p_took;

void eng_sound(int preset, const uint8_t m[4], float tone, float vib, int press, float pamt)
{
    const preset_t *pr = &PRESETS[(uint32_t)preset < NPRESETS ? preset : 0];
    prm_t *q = &P_next;
    float sw = (float)m[0], dr = (float)m[1], sp = (float)m[2], rn = (float)m[3], atk;
    p_seq++;                             /* odd: being written */
    q->o2_semi = pr->o2_semi;
    q->o2_pulse = pr->o2_pulse;
    q->pw = pr->pw;
    q->o2_lvl = pr->o2_lvl;
    q->sub = pr->sub;
    q->cut_hz = pr->cut_hz;
    q->env_oct = pr->env_oct * CURVE(SW_ENV, sw);
    q->res_k = 2.0f - 1.9f * fm_clampf(pr->res, 0.0f, 0.92f);
    q->bright = pr->bright;
    /* the attack aims at 1.3 and stops at 1: it takes 1.47 time constants */
    atk = CURVE(SW_AA, sw) / 1.47f;
    q->ka_a = kchunk(atk);
    q->kd_a = kchunk(1.2f);
    q->s_a = 0.9f;
    q->kr_a = kchunk(CURVE(SW_R, sw) / 4.6f);   /* the release heard about as long as asked (to -40 dB) */
    q->ka_f = kchunk(CURVE(SW_FA, sw) / 1.47f);
    q->kd_f = kchunk(CURVE(SW_FD, sw) / 3.0f);
    q->s_f = CURVE(SW_FS, sw);
    q->kr_f = kchunk(CURVE(SW_R, sw) / 4.0f);
    q->det_c = CURVE(DR_DET, dr);
    q->rnd_c = CURVE(DR_RND, dr);
    q->ens = CURVE(DR_ENS, dr);
    q->spread = CURVE(DR_SPR, dr);
    q->wow_c = CURVE(DR_WOW, dr);
    q->drive = CURVE(DR_DRV, dr);
    q->size = CURVE(SP_SIZE, sp);
    q->t60 = CURVE(SP_T60, sp);
    q->wet = CURVE(SP_WET, sp);
    q->damp_k = onepole(CURVE(SP_DAMP, sp));
    q->dsend = CURVE(SP_DSEND, sp);
    q->dfb = CURVE(SP_DFB, sp);
    q->freeze = m[2] >= 127u;
    q->hiss = CURVE(RN_HISS, rn);
    q->drop_p = CURVE(RN_DROPS, rn) / SR;
    q->rumble = CURVE(RN_RUMB, rn);
    q->thunder_p = CURVE(RN_THUN, rn) / SR * (float)CH;   /* tested once a chunk */
    q->tone_mul = fm_exp2f(1.6f * fm_clampf(tone, -1.0f, 1.0f));
    q->vib_c = 28.0f * fm_clampf(vib, 0.0f, 1.0f);
    q->press = (uint8_t)((uint32_t)press < NPRESS ? press : 0);
    q->pamt = fm_clampf(pamt, 0.0f, 1.0f);
    p_seq++;                             /* even: done */
}

/* the render's copy, when the main loop is not writing it (a torn copy is dropped: the next block takes it) */
static void params_take(void)
{
    uint32_t s0 = p_seq;
    prm_t c;
    if (s0 == p_took || (s0 & 1u))
        return;
    c = P_next;
    if (p_seq != s0)
        return;
    T = c;
    p_took = s0;
}

/* P glides to T: the continuous values about 40 ms, the rest at once */
static void params_glide(void)
{
    const float a = 0.14f;
    prm_t c = T;
#define G(f) c.f = P.f + (T.f - P.f) * a
    G(o2_semi); G(pw); G(o2_lvl); G(sub); G(cut_hz); G(env_oct); G(res_k); G(bright); G(det_c); G(rnd_c); G(ens); G(spread);
    G(wow_c); G(drive); G(size); G(t60); G(wet); G(damp_k); G(dsend); G(dfb); G(hiss); G(rumble); G(tone_mul);
    G(vib_c); G(pamt);
#undef G
    P = c;
}

/* ---------------------------------------------------------------- voices --- */
typedef struct {
    int id;
    uint8_t on, rel, sa, sf, kill;
    uint32_t age;                        /* chunks since its start */
    uint32_t born;                       /* the note count at its start (the oldest goes first) */
    float pitch, vel;
    float p1, p2, ps;                    /* the phases */
    float ea, ea_prev, ef;               /* the amp and filter envelopes */
    float a1, b1, a2, b2;                /* the two filter stages' states */
    float drift, drift_to, vib_e;
    float press, ptarget;                /* the key's pressure, gliding to ptarget */
    float pt, kg;                        /* GLIDE: the pitch it heads for, how fast (a share a chunk; 1 at once) */
    uint8_t steal;                       /* taken for a new note: fading out fast, then the pending note starts */
    float c_prev[6], vg_prev;            /* last chunk's filter coefficients and level: the samples glide from them */
    uint8_t c_ok;
    int pend_id;
    float pend_pitch, pend_vel, pend_from, pend_kg;
    uint32_t rng;                        /* its own noise (a voice may run on the second core) */
} voice_t;
static voice_t V[ENG_NV];
static const float PANS[ENG_NV] = {-0.9f, 0.9f, -0.5f, 0.5f, -0.2f, 0.2f, -0.7f, 0.7f};

/* the queue: main loop -> render */
enum { EV_ON, EV_OFF, EV_ALL_OFF, EV_PANIC, EV_PRESS, EV_MOVE, EV_WORLD, EV_SFX, EV_MEM };
typedef struct {
    uint8_t type, retrig;
    int16_t id, id2;
    float pitch, vel, from, g;
} ev_t;
#define NQ 64u
static ev_t Q[NQ];
static volatile uint32_t q_w, q_r;
static ev_t *push(uint8_t type, int id, float pitch, float vel)
{
    static ev_t spare;
    uint32_t w = q_w, nx = (w + 1u) % NQ;
    if (nx == q_r)
        return &spare;                   /* full: dropped (never, at the keys' pace) */
    Q[w].type = type;
    Q[w].id = (int16_t)id;
    Q[w].id2 = (int16_t)id;
    Q[w].pitch = pitch;
    Q[w].vel = vel;
    Q[w].from = pitch;
    Q[w].g = 0.0f;
    Q[w].retrig = 1;
    return &Q[w];
}
static void commit(void) { q_w = (q_w + 1u) % NQ; }
void eng_note_on(int id, float pitch, float vel)
{
    push(EV_ON, id, pitch, vel);
    commit();
}
void eng_note_glide(int id, float pitch, float vel, float from, float glide_s)
{
    ev_t *e = push(EV_ON, id, pitch, vel);
    e->from = from;
    e->g = glide_s;
    commit();
}
void eng_note_move(int old_id, int id, float pitch, float vel, float glide_s, int retrig)
{
    ev_t *e = push(EV_MOVE, id, pitch, vel);
    e->id2 = (int16_t)old_id;
    e->g = glide_s;
    e->retrig = (uint8_t)(retrig != 0);
    commit();
}
void eng_note_off(int id)
{
    push(EV_OFF, id, 0.0f, 0.0f);
    commit();
}
void eng_all_off(void)
{
    push(EV_ALL_OFF, 0, 0.0f, 0.0f);
    commit();
}
void eng_panic(void)
{
    push(EV_PANIC, 0, 0.0f, 0.0f);
    commit();
}
void eng_world(int scene, float level)
{
    push(EV_WORLD, scene, 0.0f, level);
    commit();
}
void eng_sfx(int type, float a, float b)
{
    push(EV_SFX, type, a, b);
    commit();
}
void eng_memory(int on)
{
    push(EV_MEM, on, 0.0f, 0.0f);
    commit();
}
void eng_press(int id, float p)
{
    push(EV_PRESS, id, 0.0f, p);
    commit();
}

static uint32_t notes, thunders;
static volatile uint8_t duck_on;

static float kglide(float g) { return g <= 0.0005f ? 1.0f : kchunk(g / 3.0f); }   /* there in about g seconds */

static float vrand(voice_t *v)           /* the voice's own noise, 0..1 (either core may call it) */
{
    v->rng ^= v->rng << 13;
    v->rng ^= v->rng >> 17;
    v->rng ^= v->rng << 5;
    return (float)(v->rng >> 8) * (1.0f / 16777216.0f);
}

static void fresh(voice_t *v, int pick)  /* a silent voice: from zero, its phases scattered (no two alike) */
{
    if (!v->rng)
        v->rng = 0x9E3779B9u * (uint32_t)(pick + 1);
    v->ea = v->ea_prev = v->ef = 0.0f;
    v->a1 = v->b1 = v->a2 = v->b2 = 0.0f;
    v->c_ok = 0;
    v->p1 = vrand(v);
    v->p2 = vrand(v);
    v->ps = 0.0f;
    v->drift = v->drift_to = vrand(v) - 0.5f;
    v->press = 0.0f;
}

static void begin(voice_t *v, int id, float pitch, float vel, float from, float kg)
{
    v->on = 1;
    v->rel = v->kill = v->steal = 0;
    v->sa = v->sf = 0;
    v->id = id;
    v->age = 0;
    v->born = notes++;
    v->pt = pitch;
    v->pitch = kg < 1.0f ? from : pitch;
    v->kg = kg;
    v->vel = fm_clampf(vel, 0.05f, 1.0f);
    v->vib_e = 0.0f;
    v->ptarget = 0.0f;
}

static void start(int id, float pitch, float vel, float from, float glide_s)
{
    int i, pick = -1;
    voice_t *v;
    float kg = kglide(glide_s);
    for (i = 0; i < ENG_NV; i++)         /* the same note again: struck again, where it is */
        if (V[i].on && V[i].id == id && !V[i].steal) {
            begin(&V[i], id, pitch, vel, V[i].pitch, kg < 1.0f ? kg : 1.0f);
            return;
        }
    {                                    /* a free voice, in the half with fewer sounding (the cores share the work) */
        int n0 = 0, n1 = 0, h, first;
        for (i = 0; i < ENG_NV; i++)
            if (V[i].on)
                *(i < ENG_NV / 2 ? &n0 : &n1) += 1;
        first = n1 < n0 ? ENG_NV / 2 : 0;
        for (h = 0; h < 2; h++) {
            int b0 = h == 0 ? first : ENG_NV / 2 - first;
            for (i = b0; i < b0 + ENG_NV / 2; i++)
                if (!V[i].on) {
                    fresh(&V[i], i);
                    begin(&V[i], id, pitch, vel, from, kg);
                    return;
                }
        }
    }
    {                                    /* all busy: the quietest let-go one, else the oldest */
        float lo = 9.0f;
        for (i = 0; i < ENG_NV; i++)
            if (V[i].rel && V[i].ea < lo) {
                lo = V[i].ea;
                pick = i;
            }
        if (pick < 0) {
            uint32_t old = 0xFFFFFFFFu;
            for (i = 0; i < ENG_NV; i++)
                if (V[i].born < old) {
                    old = V[i].born;
                    pick = i;
                }
        }
    }
    v = &V[pick];
    if (v->ea < 0.003f) {
        fresh(v, pick);
        begin(v, id, pitch, vel, from, kg);
        return;
    }
    /* still sounding: it fades out in a few ms first, then the new note starts clean (no jump, no click) */
    v->steal = 1;
    v->rel = 0;
    v->id = id;
    v->born = notes++;
    v->pend_id = id;
    v->pend_pitch = pitch;
    v->pend_vel = vel;
    v->pend_from = from;
    v->pend_kg = kg;
}

/* ------------------------------------------------------------------ the world --- */
/* What the screen shows, heard: a bed of sound for each scene (traffic, a crowd, wind, a hologram's hum, the plant, TV
 * static, neon buzzing, wind on a roof) and things that happen in it (a car flying past, a sign crackling, a flame, a
 * siren far off, a bowl set down). All synthesized here, all into the room. The app says which scene and when. */
typedef struct {
    float lp_hz, hp_hz, noise, am_rate, am_depth, hum_hz, hum, buzz;
} bed_t;
static const bed_t BEDS[NWORLD] = {
    {420.0f, 60.0f, 0.050f, 0.20f, 0.30f, 0.0f, 0.0f, 0.0f},       /* SKYLINE: traffic far below */
    {1500.0f, 320.0f, 0.040f, 4.5f, 0.75f, 0.0f, 0.0f, 0.0f},      /* NOODLE ROW: voices, a crowd */
    {900.0f, 120.0f, 0.075f, 0.15f, 0.50f, 0.0f, 0.0f, 0.0f},      /* NIGHT FLIGHT: the wind */
    {2500.0f, 600.0f, 0.008f, 6.0f, 0.50f, 220.0f, 0.014f, 0.3f},  /* HOLO KOI: the hologram's hum */
    {260.0f, 40.0f, 0.065f, 0.10f, 0.40f, 55.0f, 0.018f, 0.5f},    /* REFINERY: the plant */
    {9000.0f, 2500.0f, 0.020f, 0.0f, 0.0f, 60.0f, 0.008f, 0.6f},   /* TV WALL: static, mains hum */
    {600.0f, 80.0f, 0.020f, 0.30f, 0.30f, 100.0f, 0.012f, 1.0f},   /* NEON ALLEY: the tubes buzzing */
    {700.0f, 150.0f, 0.060f, 0.25f, 0.60f, 0.0f, 0.0f, 0.0f},      /* ROOFTOP: wind */
};

#define MEM_N 66150u                     /* MEMORY: 3 s at 22,050 Hz, 8 bits (square-law) */
static int8_t mem_buf[MEM_N] PLAT_POOL;
#define NGRAIN 4

static struct {
    int scene, want;
    float level, gain;                   /* the bed: its level (CITY), how far faded in (a scene change: out, in) */
    float lp_k, hp_k, lp1, lp2, hp1, am_ph, am, am_to, hum_ph;
    /* a car flying past: where it is (0..1), how fast, from which side */
    float fly_t, fly_dt, fly_dir, fly_bp1, fly_bp2, fly_ph;
    /* a sign's crackle, a flame, a siren, a bowl */
    float zap_e, zap_ph, zap_lp, flame_e, flame_lp, flame_atk, siren_t, siren_dt, siren_pan, siren_ph, clink_e, clink_ph, clink_inc, clink_pan;
    /* MEMORY */
    uint32_t mem_w;
    uint8_t mem_on, mem_half;
    float mem_g, mem_to;
    float gpos[NGRAIN], gu[NGRAIN], gdu[NGRAIN], gpan[NGRAIN];
} W;

static int8_t mem_enc(float x)           /* -2..2 into 8 bits, finer near zero */
{
    float a = fm_clampf(fm_fabsf(x) * 0.5f, 0.0f, 1.0f);
    int32_t q = (int32_t)(fm_sqrtf(a) * 127.0f);
    return (int8_t)(x < 0.0f ? -q : q);
}
static float mem_dec(int8_t q)
{
    float a = (float)q * (1.0f / 127.0f);
    return (a < 0.0f ? -a * a : a * a) * 2.0f;
}

static void grain_new(int g)
{
    W.gpos[g] = rndu() * (float)(MEM_N - 6000u);
    W.gu[g] = 0.0f;
    W.gdu[g] = 1.0f / (0.18f * SR * (0.7f + 0.6f * rndu()));   /* 0.13 .. 0.23 s */
    W.gpan[g] = rndu();
}

static void world_event(int type, float a, float b)
{
    if (type == SFX_FLYBY) {
        W.fly_t = 0.0f;
        W.fly_dt = 1.0f / (fm_maxf(b, 0.5f) * SR);
        W.fly_dir = a < 0.0f ? -1.0f : 1.0f;
    } else if (type == SFX_ZAP)
        W.zap_e = 1.0f;
    else if (type == SFX_FLAME) {
        W.flame_atk = 0.0f;
        W.flame_e = 0.001f;
    } else if (type == SFX_SIREN) {
        W.siren_t = 0.0001f;
        W.siren_dt = 1.0f / (7.0f * SR);
        W.siren_pan = fm_clampf(a, 0.0f, 1.0f);
    } else if (type == SFX_CLINK) {
        W.clink_e = 1.0f;
        W.clink_inc = (2600.0f + 1600.0f * rndu()) / SR;
        W.clink_pan = rndu();
    }
}

static void world_chunk(void)            /* the bed's filters and the scene change, once a chunk */
{
    const bed_t *b = &BEDS[(uint32_t)W.scene % NWORLD];
    float to = W.want != W.scene ? 0.0f : W.level;
    W.gain += (to - W.gain) * 0.02f;
    if (W.want != W.scene && W.gain < 0.003f) {
        W.scene = W.want;
        b = &BEDS[(uint32_t)W.scene % NWORLD];
    }
    W.lp_k = onepole(b->lp_hz);
    W.hp_k = onepole(b->hp_hz);
    W.mem_g += (W.mem_to - W.mem_g) * (W.mem_to > W.mem_g ? 0.012f : 0.0035f);
}

/* the world's sound, a sample: into l / r, and what the room should hear of it into *send */
static inline void world_sample(float *l, float *r, float *send)
{
    const bed_t *b = &BEDS[(uint32_t)W.scene % NWORLD];
    float x, y = 0.0f, yl = 0.0f, yr = 0.0f, g = W.gain;
    if (g > 0.0005f) {                   /* the bed: band-limited noise, swelling and falling, and a hum */
        x = white();
        W.lp1 += W.lp_k * (x - W.lp1);
        W.lp2 += W.lp_k * (W.lp1 - W.lp2);
        W.hp1 += W.hp_k * (W.lp2 - W.hp1);
        if (b->am_rate > 0.0f) {
            W.am_ph += b->am_rate / SR;
            if (W.am_ph >= 1.0f) {
                W.am_ph -= 1.0f;
                W.am_to = rndu();
            }
            W.am += (W.am_to - W.am) * 0.0006f;
        } else
            W.am = 1.0f;
        y = (W.lp2 - W.hp1) * 2.2f * b->noise * (1.0f - b->am_depth + b->am_depth * W.am);
        if (b->hum > 0.0f) {
            float h;
            W.hum_ph += b->hum_hz / SR;
            if (W.hum_ph >= 1.0f)
                W.hum_ph -= 1.0f;
            h = fm_sin_turns(W.hum_ph);
            h += b->buzz * (h * h * h * 2.0f - h);   /* buzzier with BUZZ */
            y += h * b->hum;
        }
        y *= g;
        yl = yr = y;
    }
    if (W.fly_dt > 0.0f) {               /* the car: from one side to the other, loudest close, its pitch falling past */
        float t = W.fly_t, d = 2.0f * t - 1.0f, amp = 1.0f / (1.0f + 10.0f * d * d), pan = 0.5f + 0.5f * d * W.fly_dir;
        float dop = d < 0.0f ? 1.12f : 0.88f, n = white(), k1 = 0.18f * dop, tone;
        W.fly_bp1 += k1 * (n - W.fly_bp1);
        W.fly_bp2 += 0.05f * (W.fly_bp1 - W.fly_bp2);
        W.fly_ph += 85.0f * dop / SR;
        if (W.fly_ph >= 1.0f)
            W.fly_ph -= 1.0f;
        tone = ((W.fly_bp1 - W.fly_bp2) * 1.6f + 0.35f * fm_sin_turns(W.fly_ph)) * amp * 0.10f * W.level;
        yl += tone * (1.0f - pan);
        yr += tone * pan;
        W.fly_t += W.fly_dt;
        if (W.fly_t >= 1.0f)
            W.fly_dt = 0.0f;
    }
    if (W.zap_e > 0.001f) {              /* a tube crackling */
        float z;
        W.zap_ph += 120.0f / SR;
        if (W.zap_ph >= 1.0f)
            W.zap_ph -= 1.0f;
        z = (W.zap_ph < 0.5f ? 1.0f : -1.0f);   /* a tube's hum, filtered round */
        W.zap_lp += 0.02f * (z - W.zap_lp);
        z = W.zap_lp * fm_sin_turns(W.zap_e * 0.5f) * 0.03f * W.level;
        yl += z;
        yr += z * 0.6f;
        W.zap_e *= 0.99992f;
    }
    if (W.flame_e > 0.0f) {              /* a flame shooting up: a roar that opens and dies */
        float n = white(), f;
        if (W.flame_atk < 1.0f) {
            W.flame_atk += 1.0f / (0.15f * SR);
            W.flame_e = W.flame_atk;
        } else
            W.flame_e *= 0.99995f;
        W.flame_lp += (0.01f + 0.05f * W.flame_e) * (n - W.flame_lp);
        f = W.flame_lp * 3.0f * W.flame_e * 0.09f * W.level;
        yl += f;
        yr += f;
        if (W.flame_e < 0.001f)
            W.flame_e = 0.0f;
    }
    if (W.siren_t > 0.0f) {              /* a siren far off: two tones wailing, rising and fading */
        float env = fm_sin_turns(W.siren_t * 0.5f), f = 700.0f + 230.0f * fm_sin_turns(W.siren_t * 2.4f), sg;
        W.siren_ph += f / SR;
        if (W.siren_ph >= 1.0f)
            W.siren_ph -= 1.0f;
        sg = (fm_sin_turns(W.siren_ph) + 0.2f * fm_sin_turns(W.siren_ph * 2.0f - (float)(int)(W.siren_ph * 2.0f))) * env *
             0.022f * W.level;
        yl += sg * (1.0f - W.siren_pan);
        yr += sg * W.siren_pan;
        W.siren_t += W.siren_dt;
        if (W.siren_t >= 1.0f)
            W.siren_t = 0.0f;
    }
    if (W.clink_e > 0.001f) {            /* a bowl set down */
        float c;
        W.clink_ph += W.clink_inc;
        if (W.clink_ph >= 1.0f)
            W.clink_ph -= 1.0f;
        c = (fm_sin_turns(W.clink_ph) + 0.4f * fm_sin_turns(W.clink_ph * 2.7f - (float)(int)(W.clink_ph * 2.7f))) *
            W.clink_e * 0.02f * W.level;
        yl += c * (1.0f - W.clink_pan);
        yr += c * W.clink_pan;
        W.clink_e *= 0.99965f;
    }
    *l += yl;
    *r += yr;
    *send = 0.5f * (yl + yr);
}

/* MEMORY: recording what plays (when frozen: not); the frozen seconds played back as a cloud of grains */
static inline void memory_sample(float in, float *l, float *r, float *send)
{
    uint32_t g;
    float sl = 0.0f, sr = 0.0f;
    if (!W.mem_on) {
        W.mem_half ^= 1u;                /* every other sample: 22,050 Hz */
        if (W.mem_half) {
            mem_buf[W.mem_w] = mem_enc(in);
            if (++W.mem_w >= MEM_N)
                W.mem_w = 0;
        }
    }
    if (W.mem_g < 0.0005f)
        return;
    for (g = 0; g < NGRAIN; g++) {
        float u = W.gu[g], w = 4.0f * u * (1.0f - u), at = W.gpos[g] + u * 0.18f * SR * 0.5f, fr, x;
        uint32_t i0 = (uint32_t)at, i1;
        if (i0 >= MEM_N)
            i0 -= MEM_N;
        i1 = i0 + 1u >= MEM_N ? 0u : i0 + 1u;
        fr = at - (float)(uint32_t)at;
        x = (mem_dec(mem_buf[i0]) * (1.0f - fr) + mem_dec(mem_buf[i1]) * fr) * w * w;
        sl += x * (1.0f - W.gpan[g]);
        sr += x * W.gpan[g];
        W.gu[g] += W.gdu[g];
        if (W.gu[g] >= 1.0f)
            grain_new((int)g);
    }
    sl *= W.mem_g * 0.9f;
    sr *= W.mem_g * 0.9f;
    *l += sl;
    *r += sr;
    *send += 0.5f * (sl + sr);
}

static void events(void)
{
    while (q_r != q_w) {
        ev_t e = Q[q_r];
        int i;
        q_r = (q_r + 1u) % NQ;
        if (e.type == EV_ON)
            start(e.id, e.pitch, e.vel, e.from, e.g);
        else if (e.type == EV_MOVE) {    /* legato: the old note's voice slides to the new one */
            int done = 0;
            for (i = 0; i < ENG_NV && !done; i++)
                if (V[i].on && V[i].id == e.id2) {
                    voice_t *v = &V[i];
                    float kg = kglide(e.g);
                    if (v->steal) {
                        v->pend_id = v->id = e.id;
                        v->pend_pitch = e.pitch;
                    } else {
                        v->id = e.id;
                        v->pt = e.pitch;
                        v->kg = kg;
                        if (kg >= 1.0f)
                            v->pitch = e.pitch;
                        if (e.retrig || v->rel) {
                            v->rel = v->kill = 0;
                            v->sa = v->sf = 0;
                            v->age = 0;
                            v->vel = fm_clampf(e.vel, 0.05f, 1.0f);
                        }
                    }
                    done = 1;
                }
            if (!done)
                start(e.id, e.pitch, e.vel, e.pitch, 0.0f);
        } else if (e.type == EV_OFF) {
            for (i = 0; i < ENG_NV; i++)
                if (V[i].on && V[i].id == e.id) {
                    if (V[i].steal) {    /* let go before it began: it only fades */
                        V[i].steal = 0;
                        V[i].kill = 1;
                    }
                    V[i].rel = 1;
                }
        } else if (e.type == EV_WORLD) {
            W.want = e.id;
            W.level = fm_clampf(e.vel, 0.0f, 1.0f);
        } else if (e.type == EV_SFX) {
            world_event(e.id, e.pitch, e.vel);
        } else if (e.type == EV_MEM) {
            if (e.id && !W.mem_on) {
                int g;
                W.mem_on = 1;
                for (g = 0; g < NGRAIN; g++) {
                    grain_new(g);
                    W.gu[g] = (float)g / (float)NGRAIN;
                }
                W.mem_to = 1.0f;
            } else if (!e.id) {
                W.mem_to = 0.0f;
                W.mem_on = 0;    /* (it records again; the cloud fades over ~2 s) */
            }
        } else if (e.type == EV_PRESS) {
            for (i = 0; i < ENG_NV; i++)
                if (V[i].on && V[i].id == e.id)
                    V[i].ptarget = fm_clampf(e.vel, 0.0f, 1.0f);
        } else if (e.type == EV_ALL_OFF) {
            for (i = 0; i < ENG_NV; i++)
                V[i].rel = 1;
        } else {
            for (i = 0; i < ENG_NV; i++)
                V[i].on = 0;
        }
    }
}

void eng_shed(void)                      /* (audio interrupt) the quietest voice goes quickly */
{
    int i, pick = -1;
    float lo = 9.0f;
    for (i = 0; i < ENG_NV; i++)
        if (V[i].on && !V[i].kill && (V[i].rel ? V[i].ea * 0.25f : V[i].ea) < lo) {
            lo = V[i].rel ? V[i].ea * 0.25f : V[i].ea;
            pick = i;
        }
    if (pick >= 0) {
        V[pick].kill = 1;
        V[pick].rel = 1;
    }
}

/* --------------------------------------------------------------- effects --- */
#define ENS_N 2048u                      /* the ensemble's line: 46 ms */
static float ens_buf[ENS_N] PLAT_POOL;
#define DL_L 15876u                      /* the delay: 0.36 s left, 0.54 s right */
#define DL_R 23814u
static int16_t dl_l[DL_L] PLAT_POOL, dl_r[DL_R] PLAT_POOL;
/* the reverb: eight lines, their lengths at size 1 (samples), up to 2.4 times as long */
static const uint16_t RV_BASE[8] = {1153, 1361, 1597, 1847, 2083, 2399, 2711, 3023};
#define RV_MAXF 2.4f
#define RV_TOTAL 39000u                  /* the eight lines at 2.4 x and a little spare */
static int16_t rv_mem[RV_TOTAL] PLAT_POOL;
static const uint16_t AP_LEN[4] = {142, 107, 379, 277};   /* the diffusers before it */
static float ap_buf[142 + 107 + 379 + 277] PLAT_POOL;

static struct {
    uint32_t ens_w;
    float ens_ph, ens_phf;
    uint32_t dl_wl, dl_wr;
    float dl_lpl, dl_lpr;
    uint32_t rv_off[8], rv_max[8], rv_w[8];
    float rv_len[8], rv_lp[8], rv_g[8], rv_mod;
    float rv_hpl, rv_hpr;
    uint32_t ap_off[4], ap_w[4];
    /* rain */
    float hl_lp, hl_hp, hr_lp, hr_hp, r1, r2;
    float drop_e[3], drop_to[3], drop_pan[3], drop_hp[3], drop_lp[3];
    float th_e, th_atk, th_lp1, th_lp2, th_crk, th_c1;
    uint8_t th_on;
    /* the rest */
    float lfo, wow1, wow2;
    float level, gain_s, lim_env, gr_min, duck;
} E;

void eng_init(void)
{
    uint32_t i, off = 0;
    zero(V, sizeof V);
    zero(&E, sizeof E);
    zero(ens_buf, sizeof ens_buf);
    zero(dl_l, sizeof dl_l);
    zero(dl_r, sizeof dl_r);
    zero(rv_mem, sizeof rv_mem);
    zero(ap_buf, sizeof ap_buf);
    zero(mem_buf, sizeof mem_buf);
    zero(&W, sizeof W);
    for (i = 0; i < 8u; i++) {
        E.rv_off[i] = off;
        E.rv_max[i] = (uint32_t)((float)RV_BASE[i] * RV_MAXF) + 8u;
        off += E.rv_max[i];
        E.rv_len[i] = (float)RV_BASE[i];
    }
    off = 0;
    for (i = 0; i < 4u; i++) {
        E.ap_off[i] = off;
        off += AP_LEN[i];
    }
    q_w = q_r = 0;
    E.gr_min = 1.0f;
    E.duck = 1.0f;
    eng_sound(0, PRESETS[0].m, 0.0f, 0.3f, PRESS_FILTER, 0.7f);
    params_take();
    P = T;
}

/* one voice's chunk: envelopes, pitches and the filter's coefficients, then CH samples into L / R */
static void voice_chunk(voice_t *v, int slot, float *L, float *R, float lfo, float wow)
{
    float ea0, ea1, dea, f1, f2, fs, i1, i2, is, cents, fc, g, a1a, a2a, a3a, a1b, a2b, a3b, ka, pan, gl, gr, vamp;
    float pp, p_oct, p_amp, p_det, p_drv, drv;
    uint32_t s;
    /* the amp envelope (to the chunk's end; the samples ramp to it) */
    ea0 = v->ea;
    if (v->steal)
        v->ea += (0.0f - v->ea) * 0.35f; /* ~8 ms */
    else if (v->rel)
        v->ea += (0.0f - v->ea) * (v->kill ? 0.12f : P.kr_a);
    else if (v->sa == 0) {
        v->ea += (1.3f - v->ea) * P.ka_a;
        if (v->ea >= 1.0f) {
            v->ea = 1.0f;
            v->sa = 1;
        }
    } else
        v->ea += (P.s_a - v->ea) * P.kd_a;
    ea1 = v->ea;
    /* the filter's brass envelope */
    if (v->rel || v->steal)
        v->ef += (0.0f - v->ef) * P.kr_f;
    else if (v->sf == 0) {
        v->ef += (1.3f - v->ef) * P.ka_f;
        if (v->ef >= 1.0f) {
            v->ef = 1.0f;
            v->sf = 1;
        }
    } else
        v->ef += (P.s_f - v->ef) * P.kd_f;
    /* the pitch: the drift (a slow random walk), the delayed vibrato, the tape's wow */
    v->rng ^= v->rng << 13;
    v->rng ^= v->rng >> 17;
    v->rng ^= v->rng << 5;
    if ((v->rng >> 8) < (uint32_t)(0.0015f * 16777216.0f)) {
        v->rng ^= v->rng << 13;
        v->rng ^= v->rng >> 17;
        v->rng ^= v->rng << 5;
        v->drift_to = (float)(int32_t)v->rng * (1.0f / 2147483648.0f);
    }
    v->drift += (v->drift_to - v->drift) * 0.003f;
    if (v->age > 600u)                   /* the vibrato comes in after 0.45 s */
        v->vib_e += (1.0f - v->vib_e) * 0.0009f;
    v->age++;
    v->press += (v->ptarget - v->press) * 0.05f;   /* ~15 ms */
    v->pitch += (v->pt - v->pitch) * v->kg;          /* GLIDE */
    pp = v->press * P.pamt;
    p_oct = P.press == PRESS_FILTER ? 3.5f * pp : P.press == PRESS_SWELL ? 0.7f * pp : P.press == PRESS_POWER ? 1.5f * pp : 0.0f;
    p_amp = P.press == PRESS_SWELL ? 1.0f + 1.5f * pp : P.press == PRESS_POWER ? 1.0f + 0.8f * pp : 1.0f;
    p_det = P.press == PRESS_POWER ? 12.0f * pp : 0.0f;
    p_drv = P.press == PRESS_POWER ? 1.6f * pp : 0.0f;
    cents = v->drift * P.rnd_c + (v->vib_e * P.vib_c + (P.press == PRESS_VIBRATO ? 35.0f * pp : 0.0f)) * lfo + wow * P.wow_c;
    f1 = hz_of(v->pitch + (cents - 0.5f * (P.det_c + p_det)) * 0.01f);
    f2 = hz_of(v->pitch + P.o2_semi + (cents + 0.5f * (P.det_c + p_det)) * 0.01f);
    fs = f1 * 0.5f;
    i1 = f1 / SR;
    i2 = fm_minf(f2 / SR, 0.45f);
    is = fs / SR;
    /* the cutoff: closed, the envelope's opening (harder = more), the key's pitch, TONE */
    fc = P.cut_hz * P.tone_mul * fm_exp2f((v->pitch - 60.0f) * (0.5f / 12.0f)) *
         fm_exp2f(P.env_oct * v->ef * (1.0f - P.bright + P.bright * v->vel) + p_oct);
    fc = fm_clampf(fc, 30.0f, 15000.0f);
    g = fm_tanf(FM_PI * fc / SR);
    ka = 1.41f;                          /* the first stage plain, the second with the resonance */
    a1a = 1.0f / (1.0f + g * (g + ka));
    a2a = g * a1a;
    a3a = g * a2a;
    a1b = 1.0f / (1.0f + g * (g + P.res_k));
    a2b = g * a1b;
    a3b = g * a2b;
    pan = P.spread * PANS[slot];
    gl = 1.0f - (pan > 0.0f ? pan : 0.0f) * 0.75f;
    gr = 1.0f + (pan < 0.0f ? pan : 0.0f) * 0.75f;
    vamp = 0.13f * (0.55f + 0.45f * v->vel) * p_amp;
    drv = P.drive + p_drv;
    /* every value that changes a chunk at a time glides across the chunk, sample by sample: the filter's
     * coefficients (a fast brass attack moves the cutoff octaves in a few ms) and the level (pressure) */
    {
        float cn[6], d[6], c0[6], vg0 = v->c_ok ? v->vg_prev : vamp, dvg;
        uint32_t q;
        cn[0] = a1a; cn[1] = a2a; cn[2] = a3a; cn[3] = a1b; cn[4] = a2b; cn[5] = a3b;
        for (q = 0; q < 6u; q++) {
            c0[q] = v->c_ok ? v->c_prev[q] : cn[q];
            d[q] = (cn[q] - c0[q]) * (1.0f / (float)CH);
            v->c_prev[q] = cn[q];
        }
        v->c_ok = 1;
        dvg = (vamp - vg0) * (1.0f / (float)CH);
        v->vg_prev = vamp;
        a1a = c0[0]; a2a = c0[1]; a3a = c0[2]; a1b = c0[3]; a2b = c0[4]; a3b = c0[5];
        dea = (ea1 - ea0) * (1.0f / (float)CH);
        for (s = 0; s < CH; s++) {
        float o, x, v1, v2, v3, y, amp = (ea0 + dea * (float)(s + 1u)) * (vg0 + dvg * (float)(s + 1u));
        a1a += d[0]; a2a += d[1]; a3a += d[2]; a1b += d[3]; a2b += d[4]; a3b += d[5];
        o = saw(v->p1, i1);
        o += P.o2_lvl * (P.o2_pulse ? 0.5f * pulse(v->p2, i2, P.pw) : saw(v->p2, i2));
        {                                /* the sub: a parabola, near enough to a sine an octave down */
            float q = v->ps < 0.5f ? v->ps : v->ps - 0.5f, sn = 16.0f * q * (0.5f - q);
            o += P.sub * (v->ps < 0.5f ? sn : -sn);
        }
        v->p1 += i1;
        if (v->p1 >= 1.0f)
            v->p1 -= 1.0f;
        v->p2 += i2;
        if (v->p2 >= 1.0f)
            v->p2 -= 1.0f;
        v->ps += is;
        if (v->ps >= 1.0f)
            v->ps -= 1.0f;
        /* two state-variable low-passes (topology-preserving): 24 dB */
        x = o;
        v3 = x - v->b1;
        v1 = a1a * v->a1 + a2a * v3;
        v2 = v->b1 + a2a * v->a1 + a3a * v3;
        v->a1 = 2.0f * v1 - v->a1;
        v->b1 = 2.0f * v2 - v->b1;
        x = v2;
        v3 = x - v->b2;
        v1 = a1b * v->a2 + a2b * v3;
        v2 = v->b2 + a2b * v->a2 + a3b * v3;
        v->a2 = 2.0f * v1 - v->a2;
        v->b2 = 2.0f * v2 - v->b2;
        y = v2;
        if (drv > 1.001f)
            y = fm_tanhf(y * drv) / drv * 1.4f;
        y *= amp;
        L[s] += y * gl;
        R[s] += y * gr;
        }
    }
    v->a1 = flush_dn(v->a1);
    v->b1 = flush_dn(v->b1);
    v->a2 = flush_dn(v->a2);
    v->b2 = flush_dn(v->b2);
    if (v->steal && v->ea < 0.003f) {   /* faded: the pending note, clean */
        int slot2 = slot;
        fresh(v, slot2);
        begin(v, v->pend_id, v->pend_pitch, v->pend_vel, v->pend_from, v->pend_kg);
    } else if (v->rel && v->ea < 0.0008f)
        v->on = 0;
}

static inline float rv_read(uint32_t i, float d)
{
    const int16_t *b = rv_mem + E.rv_off[i];
    uint32_t mx = E.rv_max[i];
    float pos = (float)E.rv_w[i] + (float)mx - d, fr;
    uint32_t i0;
    if (pos >= (float)mx)
        pos -= (float)mx;
    i0 = (uint32_t)pos;
    fr = pos - (float)i0;
    {
        uint32_t i1 = i0 + 1u >= mx ? 0u : i0 + 1u;
        return ((float)b[i0] + fr * (float)(b[i1] - b[i0])) * (1.0f / 4096.0f);
    }
}

static inline int16_t q16(float x)      /* a reverb sample: +-8, rounded off softly from 4 (no hard clip) */
{
    if (x > 4.0f || x < -4.0f)
        x = (x > 0.0f ? 4.0f : -4.0f) + 3.9f * fm_tanhf((x - (x > 0.0f ? 4.0f : -4.0f)) * 0.26f);
    x *= 4096.0f;
    if (x > 32767.0f)
        x = 32767.0f;
    if (x < -32767.0f)
        x = -32767.0f;
    return (int16_t)x;
}

/* the voices: a block's worth, chunk by chunk; half on the second core (its own buffers) */
#define NBLK 512u                        /* frames a block at most */
#define NCHK (NBLK / CH)
#define NBLK_USE 256u                    /* the app's block (app.h APP_BLOCK): the pipeline needs them alike */
static float vb_l[2][NBLK], vb_r[2][NBLK];
static float blk_lfo[NCHK], blk_wow[NCHK];
static uint32_t blk_n;

static void voices_render(uint32_t from, uint32_t to, float *L, float *R)
{
    uint32_t c, i, s;
    for (s = 0; s < blk_n; s++)
        L[s] = R[s] = 0.0f;
    for (c = 0; c * CH < blk_n; c++)
        for (i = from; i < to; i++)
            if (V[i].on)
                voice_chunk(&V[i], (int)i, L + c * CH, R + c * CH, blk_lfo[c], blk_wow[c]);
}

/* With two cores the work is a pipeline: the second core renders this block's voices while the first runs the
 * effects on the last block's (one block later: 5.8 ms). Each core then carries about half. One core: both here. */
static uint8_t vb_w;                     /* the buffer the voices are written to this block */
static uint8_t vb_full;                  /* the other one holds a block of voices, ready for the effects */
static void job_voices(uint32_t arg) { voices_render(0, ENG_NV, vb_l[arg], vb_r[arg]); }

void eng_render(int32_t *lr, uint32_t n, float gain)
{
    uint32_t c0, s, i, c;
    float lvl = 0.0f, pmax = 0.0f, ens;
    int on2;
    uint8_t rd;
    if (n > NBLK)
        n = NBLK;
    params_take();
    params_glide();
    events();
    /* the shared movement, per chunk: the vibrato, the tape's wow */
    blk_n = n;
    for (c = 0; c * CH < n; c++) {
        E.lfo += 5.3f * (float)CH / SR;
        if (E.lfo >= 1.0f)
            E.lfo -= 1.0f;
        E.wow1 += 0.71f * (float)CH / SR;
        if (E.wow1 >= 1.0f)
            E.wow1 -= 1.0f;
        E.wow2 += 0.23f * (float)CH / SR;
        if (E.wow2 >= 1.0f)
            E.wow2 -= 1.0f;
        blk_lfo[c] = fm_sin_turns(E.lfo);
        blk_wow[c] = 0.7f * fm_sin_turns(E.wow1) + 0.3f * fm_sin_turns(E.wow2);
    }
    for (i = 0; i < ENG_NV; i++)         /* POWER: the hardest-pressed key opens the ensemble */
        if (V[i].on && V[i].press > pmax)
            pmax = V[i].press;
    on2 = plat_cpu2_on() && n == NBLK_USE && plat_cpu2_run(job_voices, vb_w);
    if (on2) {
        rd = (uint8_t)(vb_w ^ 1u);       /* the effects on the last block's voices, meanwhile */
        if (!vb_full) {
            for (s = 0; s < n; s++)
                vb_l[rd][s] = vb_r[rd][s] = 0.0f;
        }
    } else {
        job_voices(vb_w);
        rd = vb_w;
    }
    ens = P.press == PRESS_POWER ? fm_minf(1.0f, P.ens + 0.6f * pmax * P.pamt) : P.ens;
    for (c0 = 0; c0 < n; c0 += CH) {
        float L[CH], R[CH], ensd[3], rv_mod[8], dsend = P.dsend, dfb = P.dfb, wet = P.wet;
        uint32_t m = n - c0 < CH ? n - c0 : CH;
        world_chunk();
        for (s = 0; s < m; s++) {
            L[s] = vb_l[rd][c0 + s];
            R[s] = vb_r[rd][c0 + s];
        }
        /* the ensemble's three taps: 9 ms apart by a slow and a fast sway, a third of a turn apart */
        E.ens_ph += 0.55f * (float)CH / SR;
        if (E.ens_ph >= 1.0f)
            E.ens_ph -= 1.0f;
        E.ens_phf += 6.2f * (float)CH / SR;
        if (E.ens_phf >= 1.0f)
            E.ens_phf -= 1.0f;
        for (i = 0; i < 3u; i++) {
            float t = (float)i * (1.0f / 3.0f), a = E.ens_ph + t, b = E.ens_phf + t;
            if (a >= 1.0f)
                a -= 1.0f;
            if (b >= 1.0f)
                b -= 1.0f;
            ensd[i] = (0.009f + 0.0035f * fm_sin_turns(a) + 0.00035f * fm_sin_turns(b)) * SR;
        }
        /* the reverb's lines: toward the size asked (a few samples a chunk), three of them swaying; their gains */
        E.rv_mod += 0.37f * (float)CH / SR;
        if (E.rv_mod >= 1.0f)
            E.rv_mod -= 1.0f;
        for (i = 0; i < 8u; i++) {
            float to = (float)RV_BASE[i] * P.size, d = to - E.rv_len[i];
            E.rv_len[i] += fm_clampf(d, -2.0f, 2.0f);
            rv_mod[i] = E.rv_len[i];
            if (i == 1u || i == 4u || i == 6u) {
                float ph = E.rv_mod + (float)i * 0.29f;
                if (ph >= 1.0f)
                    ph -= 1.0f;
                rv_mod[i] += 6.0f * fm_sin_turns(ph);
            }
            rv_mod[i] = fm_clampf(rv_mod[i], 4.0f, (float)E.rv_max[i] - 4.0f);
            E.rv_g[i] = P.freeze ? 0.99997f : fm_exp2f(-9.966f * rv_mod[i] / (P.t60 * SR));
        }
        /* thunder, far off: now and then at STORM and FLOOD */
        if (!E.th_on && P.thunder_p > 0.0f && rndu() < P.thunder_p) {
            E.th_on = 1;
            E.th_atk = 0.0f;
            E.th_crk = 0.6f + 0.4f * rndu();
            thunders++;
        }
        for (s = 0; s < m; s++) {
            float msend, wsend, l = L[s], r = R[s], mono, t0, t1, t2, rain_l, rain_r, x, y[8], sum, wl, wr, rdl, rdr;
            /* the ensemble */
            mono = 0.5f * (l + r);
            ens_buf[E.ens_w] = mono;
            {
                float tap[3];
                for (i = 0; i < 3u; i++) {
                    float pos = (float)E.ens_w + (float)ENS_N - ensd[i], fr;
                    uint32_t i0, i1;
                    if (pos >= (float)ENS_N)
                        pos -= (float)ENS_N;
                    i0 = (uint32_t)pos;
                    fr = pos - (float)i0;
                    i1 = i0 + 1u >= ENS_N ? 0u : i0 + 1u;
                    tap[i] = ens_buf[i0] + fr * (ens_buf[i1] - ens_buf[i0]);
                }
                t0 = tap[0];
                t1 = tap[1];
                t2 = tap[2];
            }
            if (++E.ens_w >= ENS_N)
                E.ens_w = 0;
            l = l * (1.0f - 0.4f * ens) + ens * (t0 + 0.5f * t1) * 0.8f;
            r = r * (1.0f - 0.4f * ens) + ens * (t2 + 0.5f * t1) * 0.8f;
            /* MEMORY: what plays recorded; frozen, its cloud */
            msend = 0.0f;
            memory_sample(0.5f * (l + r), &l, &r, &msend);
            /* the rain: hiss (a band of noise each side), drops (ticks, here and there), the city's rumble, thunder */
            rain_l = rain_r = 0.0f;
            if (P.hiss > 0.0f || E.th_on) {
                float nl = white(), nr = white(), rum;
                E.hl_lp += 0.45f * (nl - E.hl_lp);
                E.hl_hp += 0.08f * (E.hl_lp - E.hl_hp);
                E.hr_lp += 0.45f * (nr - E.hr_lp);
                E.hr_hp += 0.08f * (E.hr_lp - E.hr_hp);
                rain_l = (E.hl_lp - E.hl_hp) * P.hiss;
                rain_r = (E.hr_lp - E.hr_hp) * P.hiss;
                if (rndu() < P.drop_p) {
                    for (i = 0; i < 3u; i++)
                        if (E.drop_e[i] < 0.02f) {
                            E.drop_e[i] = 0.0f;
                            E.drop_to[i] = (0.3f + 0.7f * rndu()) * P.hiss * 5.0f;
                            E.drop_pan[i] = rndu();
                            break;
                        }
                }
                for (i = 0; i < 3u; i++)   /* a drop: rises in ~1 ms, a soft band of noise, gone in ~5 ms */
                    if (E.drop_to[i] > 0.0f || E.drop_e[i] > 0.0005f) {
                        float d, bp;
                        if (E.drop_to[i] > 0.0f) {
                            E.drop_e[i] += (E.drop_to[i] - E.drop_e[i]) * 0.05f;
                            if (E.drop_e[i] > E.drop_to[i] * 0.9f)
                                E.drop_to[i] = 0.0f;
                        } else
                            E.drop_e[i] *= 0.9955f;
                        d = white() * E.drop_e[i];
                        E.drop_hp[i] += 0.35f * (d - E.drop_hp[i]);
                        E.drop_lp[i] += 0.08f * (E.drop_hp[i] - E.drop_lp[i]);
                        bp = E.drop_hp[i] - E.drop_lp[i];
                        rain_l += bp * (1.0f - E.drop_pan[i]);
                        rain_r += bp * E.drop_pan[i];
                    }
                E.r1 += 0.006f * (white() - E.r1);
                E.r2 += 0.006f * (E.r1 - E.r2);
                rum = E.r2 * 22.0f * P.rumble;
                if (E.th_on) {           /* a slow roll in, a long roll off, cracks at the start */
                    float tn;
                    if (E.th_atk < 1.0f) {
                        E.th_atk += 1.0f / (0.25f * SR);
                        E.th_e = E.th_atk;
                    } else {
                        E.th_e *= 0.999991f;   /* about 2.5 s */
                        if (E.th_e < 0.001f)
                            E.th_on = 0;
                    }
                    E.th_lp1 += 0.012f * (white() - E.th_lp1);
                    E.th_lp2 += 0.012f * (E.th_lp1 - E.th_lp2);
                    tn = E.th_lp2 * 9.0f * E.th_e;
                    if (E.th_crk > 0.01f) {
                        E.th_c1 += 0.2f * (white() - E.th_c1);
                        tn += E.th_c1 * E.th_crk * 0.4f * (0.5f + 0.5f * fm_sin_turns(E.th_atk * 3.0f - (float)(int)(E.th_atk * 3.0f)));
                        E.th_crk *= 0.99996f;
                    }
                    rum += tn * 0.35f;
                }
                rain_l += rum;
                rain_r += rum;
            }
            l += rain_l;
            r += rain_r;
            /* the world: the scene's sound */
            world_sample(&l, &r, &wsend);
            /* the delay: ping-pong, each repeat darker */
            rdl = (float)dl_l[E.dl_wl] * (1.0f / 16384.0f);
            rdr = (float)dl_r[E.dl_wr] * (1.0f / 16384.0f);
            E.dl_lpl += 0.25f * (rdl - E.dl_lpl);
            E.dl_lpr += 0.25f * (rdr - E.dl_lpr);
            {
                float in = 0.5f * (l + r) * dsend, a = (in + dfb * E.dl_lpr) * 16384.0f, b = dfb * E.dl_lpl * 16384.0f;
                dl_l[E.dl_wl] = (int16_t)(fm_tanhf(a * (1.0f / 32768.0f)) * 32767.0f);
                dl_r[E.dl_wr] = (int16_t)(fm_tanhf(b * (1.0f / 32768.0f)) * 32767.0f);
            }
            if (++E.dl_wl >= DL_L)
                E.dl_wl = 0;
            if (++E.dl_wr >= DL_R)
                E.dl_wr = 0;
            l += 0.8f * E.dl_lpl;
            r += 0.8f * E.dl_lpr;
            /* the reverb: the input diffused (four all-passes), then eight lines mixed by a Householder matrix, each
             * damped; FREEZE: no input, no loss */
            x = P.freeze ? 0.0f : (0.5f * (l + r) * 0.6f + 0.35f * (rain_l + rain_r) + 0.5f * wsend + 0.4f * msend);
            for (i = 0; i < 4u; i++) {
                float *b = ap_buf + E.ap_off[i], d = b[E.ap_w[i]], vv = x - 0.62f * d;
                b[E.ap_w[i]] = vv;
                x = d + 0.62f * vv;
                if (++E.ap_w[i] >= AP_LEN[i])
                    E.ap_w[i] = 0;
            }
            sum = 0.0f;
            for (i = 0; i < 8u; i++) {
                float yy;
                if (i == 1u || i == 4u || i == 6u)
                    yy = rv_read(i, rv_mod[i]);
                else {                   /* a whole sample's delay (it moves a sample at a time) */
                    uint32_t d = (uint32_t)rv_mod[i], mx = E.rv_max[i], at = E.rv_w[i] + mx - d;
                    if (at >= mx)
                        at -= mx;
                    yy = (float)rv_mem[E.rv_off[i] + at] * (1.0f / 4096.0f);
                }
                if (!P.freeze) {
                    E.rv_lp[i] += P.damp_k * (yy - E.rv_lp[i]);
                    yy = E.rv_lp[i];
                }
                y[i] = yy;
                sum += yy;
            }
            sum *= 0.25f;
            for (i = 0; i < 8u; i++) {
                int16_t *b = rv_mem + E.rv_off[i];
                b[E.rv_w[i]] = q16((y[i] - sum) * E.rv_g[i] + ((i & 1u) ? -x : x));
                if (++E.rv_w[i] >= E.rv_max[i])
                    E.rv_w[i] = 0;
            }
            wl = 0.45f * (y[0] - y[2] + y[4] + y[6]);
            wr = 0.45f * (y[1] + y[3] - y[5] + y[7]);
            E.rv_hpl += 0.008f * (wl - E.rv_hpl);
            wl -= E.rv_hpl;
            E.rv_hpr += 0.008f * (wr - E.rv_hpr);
            wr -= E.rv_hpr;
            l = l * (1.0f - 0.45f * wet) + wl * wet * 1.5f;
            r = r * (1.0f - 0.45f * wet) + wr * wet * 1.5f;
            /* the master: a limiter that turns it down (fast down, slow back up: no distortion), then a soft edge at
             * the very top for what slips through */
            {
                float pk = fm_maxf(fm_fabsf(l), fm_fabsf(r)), g;
                E.lim_env = pk > E.lim_env ? E.lim_env + (pk - E.lim_env) * 0.01f : E.lim_env * 0.99997f;
                g = E.lim_env > 0.55f ? 0.55f / E.lim_env : 1.0f;
                l *= g;
                r *= g;
                if (fm_fabsf(l) > 0.8f)
                    l = l > 0.0f ? 0.8f + 0.2f * fm_tanhf((l - 0.8f) * 5.0f) : -0.8f - 0.2f * fm_tanhf((-l - 0.8f) * 5.0f);
                if (fm_fabsf(r) > 0.8f)
                    r = r > 0.0f ? 0.8f + 0.2f * fm_tanhf((r - 0.8f) * 5.0f) : -0.8f - 0.2f * fm_tanhf((-r - 0.8f) * 5.0f);
                if (g < E.gr_min)
                    E.gr_min = g;
            }
            lvl = fm_maxf(lvl, fm_fabsf(l) + fm_fabsf(r));
            E.gain_s += (gain - E.gain_s) * 0.004f;
            E.duck += ((duck_on ? 0.0f : 1.0f) - E.duck) * 0.003f;   /* (a save to the flash: faded out first) */
            l *= E.duck;
            r *= E.duck;
            lr[2u * (c0 + s)] = (int32_t)(l * E.gain_s);
            lr[2u * (c0 + s) + 1u] = (int32_t)(r * E.gain_s);
        }
        for (i = 0; i < 8u; i++)
            E.rv_lp[i] = flush_dn(E.rv_lp[i]);
        E.dl_lpl = flush_dn(E.dl_lpl);
        E.dl_lpr = flush_dn(E.dl_lpr);
    }
    if (on2) {                           /* this block's voices: ready for the next */
        if (plat_cpu2_wait() != 0)
            job_voices(vb_w);            /* (the second core gave up: done here) */
        vb_w ^= 1u;
        vb_full = 1;
    } else
        vb_full = 0;
    E.level = lvl * 0.5f > E.level ? lvl * 0.5f : E.level * 0.97f;
}

void eng_duck(int on) { duck_on = (uint8_t)(on != 0); }
int eng_ducked(void) { return E.duck < 0.001f; }

int eng_voices(void)
{
    int i, k = 0;
    for (i = 0; i < ENG_NV; i++)
        if (V[i].on)
            k++;
    return k;
}
float eng_level(void) { return E.level; }
float eng_limit(void)                    /* the limiter's deepest cut since the last call, 0..1 (1: none) */
{
    float g = E.gr_min;
    E.gr_min = 1.0f;
    return g;
}
uint32_t eng_thunder(void) { return thunders; }
uint32_t eng_notes(void) { return notes; }
