/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (STRANGER SYNTH) */
/* STRANGER SYNTH's PERFORM effects, the tape, the sidechain (and the HEART, heart.c). Included by dsp/engine.c (one unit: the voices, the queue and the effects are in scope). */
/* -------------------------------------------------------------- perform --- */
/* STRANGER SYNTH's performance effects and the sidechain, on the whole sound at the end (before the limiter).
 *   TAPE STOP   the tape slows to a stop (and spins back up when let go)      REWIND  the tape runs back, once
 *   VHS         a worn video tape: wow and flutter, the highs gone, hiss      GATED   the 80s gated reverb
 *   ECHO        the delay thrown: a big send, long feedback                   LIGHTS  everything sinks under a low-pass
 *   WALKIE      a walkie-talkie: a narrow band, grit and static               RISER   the filters open over 8 s, noise
 * The tape: the last 1.5 s of the sound, mono, 22,050 Hz (OFFWORLD's MEMORY room). The SIDECHAIN ducks it all on each
 * beat (1/4, 1/8, 1/2 of the tempo or the MIDI clock), or on a kick from outside (KICK). */
#define TAPE_N 33075u
static int16_t tape_buf[TAPE_N] PLAT_POOL;
#define VHS_N 1024u
static float vhs_l[VHS_N] PLAT_POOL, vhs_r[VHS_N] PLAT_POOL;
static volatile uint32_t fx_mask;
static volatile float sc_depth_v;
static volatile uint8_t sc_rate_v;
enum { TP_LIVE, TP_STOP, TP_SPIN, TP_REW, TP_BACK };
static struct {
    uint32_t prev;
    float a[NFX];                        /* each effect's amount 0..1, gliding */
    uint32_t tw;                         /* the tape: where it writes */
    uint8_t half, tmode;
    float tacc, trd, rate, tmix, rw_t, tlp;
    uint32_t vw;                         /* VHS */
    float vph1, vph2, vlp_l, vlp_r;
    float gate_t, gate_g;                /* GATED */
    uint32_t notes_seen;
    float lo_l1, lo_l2, lo_r1, lo_r2, lo_k;   /* LIGHTS */
    float wk_h, wk_hp, wk_l;             /* WALKIE */
    float rise, rn_lp, rn_hp;            /* RISER */
    float sc_ph, sc_e, sc_g, sc_rel;     /* SIDECHAIN */
    uint32_t sc_tick;
} FX = {0};

static void fx_init(void)
{
    zero(tape_buf, sizeof tape_buf);
    zero(vhs_l, sizeof vhs_l);
    zero(vhs_r, sizeof vhs_r);
    zero(&FX, sizeof FX);
    FX.sc_g = FX.gate_g = 1.0f;
    FX.sc_rel = 0.999f;
}
void eng_fx(uint32_t mask) { fx_mask = mask; }
uint32_t eng_fx_busy(void) { return FX.tmode != TP_LIVE; }
float eng_fx_amt(int fx)
{
    if (fx == FX_TAPESTOP || fx == FX_REWIND)
        return FX.tmix;
    if (fx == FX_RISER)
        return FX.rise;
    return (uint32_t)fx < NFX ? FX.a[fx] : 0.0f;
}
void eng_sidechain(float depth, int rate)
{
    sc_depth_v = fm_clampf(depth, 0.0f, 1.0f);
    sc_rate_v = (uint8_t)((uint32_t)rate < NSC ? rate : SC_4);
}
void eng_sc_hit(void)
{
    push(EV_SC, 0, 0.0f, 0.0f);
    commit();
}
float eng_sc_gain(void) { return FX.sc_g; }
static void sc_hit(void) { FX.sc_e = 1.0f; }
static const uint8_t SC_TPB[NSC] = {24, 12, 48, 0, 0};   /* clock ticks a beat (KICK, HEART: none) */
static void hb_tick(int start);
static void sc_tick(int start)           /* (arp_clock) a MIDI clock tick, or START */
{
    hb_tick(start);
    uint8_t rt = sc_rate_v;
    if (start) {
        FX.sc_tick = 0;
        return;
    }
    if (SC_TPB[rt] && FX.sc_tick % SC_TPB[rt] == 0u)
        sc_hit();
    FX.sc_tick++;
}

#include "heart.c"


static inline int16_t tq(float x)
{
    float q = fm_clampf(x, -3.99f, 3.99f) * 8192.0f;
    return (int16_t)(q < 0.0f ? q - 0.5f : q + 0.5f);   /* (rounded) */
}
/* down to 22,050 Hz: a half-band of 15 taps (stop band -32 dB), so the highs do not fold back into the tape (a
 * plain average of two lets through what lies near 11 kHz, mirrored: grit when the tape plays back) */
static float th_h[16];
static uint32_t th_w;
static inline float tape_half_band(void)
{
    uint32_t w = th_w;
    return 0.5f * th_h[(w - 8u) & 15u] + 0.313926f * (th_h[(w - 7u) & 15u] + th_h[(w - 9u) & 15u])
           - 0.093404f * (th_h[(w - 5u) & 15u] + th_h[(w - 11u) & 15u])
           + 0.044110f * (th_h[(w - 3u) & 15u] + th_h[(w - 13u) & 15u])
           - 0.026488f * (th_h[(w - 1u) & 15u] + th_h[(w - 15u) & 15u]);
}
static inline float tape_at(float pos)
{
    uint32_t i0 = (uint32_t)pos, i1;
    float fr = pos - (float)i0;
    if (i0 >= TAPE_N)
        i0 -= TAPE_N;
    i1 = i0 + 1u >= TAPE_N ? 0u : i0 + 1u;
    return ((float)tape_buf[i0] * (1.0f - fr) + (float)tape_buf[i1] * fr) * (1.0f / 8192.0f);
}
static float tape_behind(float secs)     /* the read position secs behind the write head */
{
    float p = (float)FX.tw - secs * (SR * 0.5f);
    while (p < 0.0f)
        p += (float)TAPE_N;
    return p;
}

/* once a block: the effects switched, their amounts, the gate, the riser, the beat */
static void fx_block(uint32_t n)
{
    static const float GL[NFX] = {0, 0.06f, 0, 0.5f, 0.08f, 0.05f, 0.3f, 0};   /* how fast each comes and goes */
    uint32_t mask = fx_mask, up = mask & ~FX.prev, down = FX.prev & ~mask, i;
    float beat;
    FX.prev = mask;
    if (up >> FX_REWIND & 1u) {          /* REWIND: from now, back */
        FX.tmode = TP_REW;
        FX.rate = -0.6f;
        FX.trd = tape_behind(0.0f);
        FX.rw_t = 0.7f * SR;
    } else if ((up >> FX_TAPESTOP & 1u) && (FX.tmode == TP_LIVE || FX.tmode == TP_BACK)) {
        FX.tmode = TP_STOP;
        FX.rate = 1.0f;
        FX.trd = tape_behind(0.002f);
    }
    if ((down >> FX_TAPESTOP & 1u) && FX.tmode == TP_STOP) {   /* let go: it spins back up */
        if (FX.rate < 0.05f)
            FX.trd = tape_behind(0.25f);   /* (stopped a long while: from the recent sound) */
        FX.tmode = TP_SPIN;
    }
    for (i = 0; i < NFX; i++)
        if (GL[i] > 0.0f)
            FX.a[i] += ((float)(mask >> i & 1u) - FX.a[i]) * GL[i];
    FX.lo_k = 1.0f + (onepole(220.0f) - 1.0f) * FX.a[FX_LIGHTS];
    if (mask >> FX_RISER & 1u)
        FX.rise = fm_minf(1.0f, FX.rise + (float)n / (8.0f * SR));
    else
        FX.rise = fm_maxf(0.0f, FX.rise - (float)n / (0.4f * SR));
    fx_cut = fm_exp2f(4.5f * FX.rise * FX.rise);
    if (notes != FX.notes_seen) {        /* GATED: each new note opens the gate */
        FX.notes_seen = notes;
        FX.gate_t = 0.24f * SR;
    } else if (FX.gate_t > 0.0f)
        FX.gate_t -= (float)n;
    /* the beat (own tempo): the sidechain's hits */
    beat = SR * 60.0f / (A.bpm > 1.0f ? A.bpm : 120.0f);
    if (sc_rate_v == SC_8)
        beat *= 0.5f;
    else if (sc_rate_v == SC_2)
        beat *= 2.0f;
    FX.sc_rel = fm_expf(-1.0f / (fm_minf(0.5f * beat, 0.3f * SR) * 0.35f));
    hb_block(n, eng_clock_ext());
    if (!eng_clock_ext() && sc_rate_v != SC_KICK && sc_rate_v != SC_HEART) {
        FX.sc_ph += (float)n / beat;
        if (FX.sc_ph >= 1.0f) {
            FX.sc_ph -= (float)(int)FX.sc_ph;
            sc_hit();
        }
    }
}

/* each sample, at the end: the tape, the effects, the sidechain */
static inline void fx_sample(float *pl, float *pr)
{
    float l = *pl, r = *pr, mono, a;
    {                                    /* the sidechain (the synth only): down at each hit, back over half a beat */
        float d = sc_depth_v, to;
        FX.sc_e *= FX.sc_rel;
        to = 1.0f - d * FX.sc_e;
        FX.sc_g += (to - FX.sc_g) * (to < FX.sc_g ? 0.05f : 0.01f);
        l *= FX.sc_g;
        r *= FX.sc_g;
    }
    a = hb_sample();                     /* the HEART, under it all, in the middle */
    l += a;
    r += a;
    mono = 0.5f * (l + r);
    th_h[th_w++ & 15u] = mono;
    FX.half ^= 1u;                       /* the tape records, always (22,050 Hz) */
    if (FX.half)
        FX.tacc = mono;
    else {
        tape_buf[FX.tw] = tq(tape_half_band());
        if (++FX.tw >= TAPE_N)
            FX.tw = 0;
    }
    /* VHS: a short line swaying (wow 0.6 Hz, flutter 7 Hz), the highs off, hiss */
    vhs_l[FX.vw] = l;
    vhs_r[FX.vw] = r;
    if ((a = FX.a[FX_VHS]) > 0.002f) {
        float d, pos, fr, k, xl, xr;
        uint32_t i0, i1;
        FX.vph1 += 0.6f / SR;
        if (FX.vph1 >= 1.0f)
            FX.vph1 -= 1.0f;
        FX.vph2 += 7.0f / SR;
        if (FX.vph2 >= 1.0f)
            FX.vph2 -= 1.0f;
        d = 8.0f + a * (0.0045f * (1.0f + fm_sin_turns(FX.vph1)) + 0.0006f * (1.0f + fm_sin_turns(FX.vph2))) * SR;
        pos = (float)FX.vw + (float)VHS_N - d;
        if (pos >= (float)VHS_N)
            pos -= (float)VHS_N;
        i0 = (uint32_t)pos;
        fr = pos - (float)i0;
        i1 = i0 + 1u >= VHS_N ? 0u : i0 + 1u;
        xl = vhs_l[i0] + fr * (vhs_l[i1] - vhs_l[i0]);
        xr = vhs_r[i0] + fr * (vhs_r[i1] - vhs_r[i0]);
        k = 1.0f - 0.82f * a;            /* about 3 kHz at full */
        FX.vlp_l += k * (xl - FX.vlp_l);
        FX.vlp_r += k * (xr - FX.vlp_r);
        l = l + (FX.vlp_l - l) * a + white() * 0.004f * a;
        r = r + (FX.vlp_r - r) * a + white() * 0.004f * a;
    }
    if (++FX.vw >= VHS_N)
        FX.vw = 0;
    /* LIGHTS OUT: two poles down to 220 Hz */
    if (FX.lo_k < 0.999f) {
        FX.lo_l1 += FX.lo_k * (l - FX.lo_l1);
        FX.lo_l2 += FX.lo_k * (FX.lo_l1 - FX.lo_l2);
        FX.lo_r1 += FX.lo_k * (r - FX.lo_r1);
        FX.lo_r2 += FX.lo_k * (FX.lo_r1 - FX.lo_r2);
        l = FX.lo_l2 * (1.0f + 0.6f * FX.a[FX_LIGHTS]);
        r = FX.lo_r2 * (1.0f + 0.6f * FX.a[FX_LIGHTS]);
    } else {
        FX.lo_l1 = FX.lo_l2 = l;
        FX.lo_r1 = FX.lo_r2 = r;
    }
    /* WALKIE: 500 Hz .. 2.8 kHz, driven, static */
    if ((a = FX.a[FX_WALKIE]) > 0.002f) {
        float x = 0.5f * (l + r), y;
        FX.wk_hp += 0.069f * (x - FX.wk_hp);   /* (500 Hz) */
        y = x - FX.wk_hp;
        FX.wk_l += 0.33f * (y - FX.wk_l);      /* (2.8 kHz) */
        y = fm_tanhf(FX.wk_l * 4.0f) * 0.3f + white() * 0.012f;
        l += (y - l) * a;
        r += (y - r) * a;
    }
    /* RISER: noise rising with the filters */
    if (FX.rise > 0.001f) {
        float w = white();
        FX.rn_hp += 0.2f * (w - FX.rn_hp);
        w = (w - FX.rn_hp) * 0.09f * FX.rise * FX.rise;
        l += w;
        r += w;
    }
    /* the tape: stopping, spinning up, rewinding, then back to the live sound */
    if (FX.tmode != TP_LIVE) {
        float x, g;
        if (FX.tmode == TP_STOP)
            FX.rate = fm_maxf(0.0f, FX.rate - 1.0f / (0.9f * SR));
        else if (FX.tmode == TP_SPIN) {
            FX.rate += 1.0f / (0.35f * SR);
            if (FX.rate >= 1.0f) {
                FX.rate = 1.0f;
                FX.tmode = TP_BACK;
            }
        } else if (FX.tmode == TP_REW) {
            FX.rate = fm_maxf(-3.0f, FX.rate - 2.4f / (0.45f * SR));
            FX.rw_t -= 1.0f;
            if (FX.rw_t <= 0.0f) {
                FX.rate = 1.0f;
                FX.trd = tape_behind(0.01f);
                FX.tmode = TP_BACK;
            }
        }
        FX.trd += FX.rate * 0.5f;
        if (FX.trd >= (float)TAPE_N)
            FX.trd -= (float)TAPE_N;
        else if (FX.trd < 0.0f)
            FX.trd += (float)TAPE_N;
        x = tape_at(FX.trd);
        FX.tlp += (FX.tmode == TP_REW ? 0.25f : 1.0f) * (x - FX.tlp);
        g = fm_minf(1.0f, fm_fabsf(FX.rate) * 2.5f);
        if (FX.tmode == TP_BACK) {
            FX.tmix -= 1.0f / (0.03f * SR);
            if (FX.tmix <= 0.0f) {
                FX.tmix = 0.0f;
                FX.tmode = TP_LIVE;
            }
        } else
            FX.tmix = fm_minf(1.0f, FX.tmix + 1.0f / (0.004f * SR));
        l += (FX.tlp * g - l) * FX.tmix;
        r += (FX.tlp * g - r) * FX.tmix;
    }
    *pl = l;
    *pr = r;
}

