/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (STRANGER SYNTH) */
/* STRANGER SYNTH's HEART: the heartbeat groove. Included by dsp/engine.c (one unit: the voices, the queue and the effects are in scope). */

/* the HEART: a heartbeat as the groove, at the tempo (or the MIDI clock): lub and dub. Each beat is an analogue kick
 * (a sine falling from high to low, a click of pitch) blended with a real heart's thump (a soft sine, muffled, a puff of
 * low noise): REAL 0 all kick, 1 all heart. SINGLE (lub), DOUBLE (lub-dub), SLOW (lub-dub every two beats), RACING
 * (lub-dub each half beat). It sounds under the sidechain, and with SC RATE at HEART it drives it. */
static volatile float hb_lvl, hb_real, hb_dec;
static volatile uint8_t hb_pat;
static struct {
    float ph;                            /* the beat's phase (own tempo) */
    uint32_t tick;                       /* clock ticks (outside tempo) */
    float f, penv, pdec, amp, att, adec, namp, ndec, sph, n1, n2, lp, vel, env;
    uint8_t dub_done;
} HB = {0};
void eng_heart(float level, int pattern, float real, float decay)
{
    hb_lvl = fm_clampf(level, 0.0f, 1.0f);
    hb_pat = (uint8_t)((uint32_t)pattern < NHB ? pattern : HB_DOUBLE);
    hb_real = fm_clampf(real, 0.0f, 1.0f);
    hb_dec = fm_clampf(decay, 0.0f, 1.0f);
}
float eng_heart_env(void) { return HB.env; }
static void hb_hit(float vel)            /* a beat of the heart: the kick and the thump start */
{
    float re = hb_real;
    if (hb_lvl < 0.001f)
        return;
    HB.vel = vel;
    HB.f = 46.0f + 6.0f * vel;
    HB.penv = 1.0f;
    HB.pdec = fm_expf(-1.0f / ((0.012f + 0.02f * re) * SR));
    HB.att = 0.0f;
    HB.adec = fm_expf(-1.0f / ((0.10f + 0.30f * hb_dec) * SR));
    HB.namp = 1.0f;
    HB.ndec = fm_expf(-1.0f / (0.07f * SR));
    if (sc_rate_v == SC_HEART)
        sc_hit();
}
static const float HB_CYC[NHB] = {1.0f, 1.0f, 2.0f, 0.5f};   /* beats a cycle */
static void hb_block(uint32_t n, int ext)  /* own tempo: the beats due this block */
{
    float cyc, dub;
    if (ext || hb_lvl < 0.001f)
        return;
    cyc = SR * 60.0f / (A.bpm > 1.0f ? A.bpm : 120.0f) * HB_CYC[hb_pat];
    dub = hb_pat == HB_RACE ? 0.42f : 0.30f;   /* the second beat: a third of the way, sooner when racing */
    HB.ph += (float)n / cyc;
    if (HB.ph >= 1.0f) {
        HB.ph -= (float)(int)HB.ph;
        HB.dub_done = 0;
        hb_hit(1.0f);
    } else if (hb_pat != HB_SINGLE && !HB.dub_done && HB.ph >= dub) {
        HB.dub_done = 1;
        hb_hit(0.72f);
    }
}
static void hb_tick(int start)           /* (arp_clock) a MIDI clock tick, or START */
{
    uint32_t cyc = (uint32_t)(24.0f * HB_CYC[hb_pat]), dub = hb_pat == HB_RACE ? 5u : (uint32_t)(0.3f * (float)cyc + 0.5f), t;
    if (start) {
        HB.tick = 0;
        return;
    }
    t = HB.tick % cyc;
    if (t == 0u)
        hb_hit(1.0f);
    else if (t == dub && hb_pat != HB_SINGLE)
        hb_hit(0.72f);
    HB.tick++;
}
static inline float hb_sample(void)      /* the heart's sample (mono) */
{
    float re = hb_real, s, q, w, body, x;
    if (HB.amp < 0.0001f && HB.namp < 0.0001f && HB.att >= 1.0f)
        return 0.0f;
    HB.sph += HB.f * (1.0f + (3.0f - 2.4f * re) * HB.penv) / SR;
    if (HB.sph >= 1.0f)
        HB.sph -= 1.0f;
    HB.penv *= HB.pdec;
    q = HB.sph < 0.5f ? HB.sph : HB.sph - 0.5f;
    s = 16.0f * q * (0.5f - q);
    s = HB.sph < 0.5f ? s : -s;
    if (HB.att < 1.0f) {                 /* the attack: a click (kick) or a soft push (heart) */
        HB.att += 1.0f / ((0.001f + 0.012f * re) * SR);
        HB.amp = fm_minf(1.0f, HB.att);
    } else
        HB.amp *= HB.adec;
    w = white();
    HB.n1 += 0.012f * (w - HB.n1);
    HB.n2 += 0.012f * (HB.n1 - HB.n2);
    HB.namp *= HB.ndec;
    body = HB.n2 * 9.0f * HB.namp;       /* the thump's breath: low noise */
    x = s * HB.amp * (1.0f - 0.35f * re) + body * re;
    HB.lp += (0.35f - 0.3f * re) * (x - HB.lp);   /* the real heart muffled, as through a chest */
    HB.env = HB.amp * HB.vel;
    return HB.lp * HB.vel * hb_lvl * 1.8f;
}
