/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (STRANGER SYNTH) */
/* STRANGER SYNTH's arpeggiator, in the render (engine.h). Included by dsp/engine.c (one unit: the voices, the queue and the effects are in scope). */
/* ------------------------------------------------------------------ the arp --- */
/* The arpeggiator (engine.h). The main loop writes its settings and notes into A_next (a_seq odd while it writes);
 * the render takes a whole copy at the start of a block, steps it, and starts its notes itself (ids ARP_ID, ARP_ID+1
 * in turn: a step's note may still ring while the next begins). */
#define ARP_ID 700
typedef struct {
    uint8_t on, rate, pat, octs, n;
    float gate, bpm;
    float p[ARP_MAX], v[ARP_MAX];
} arp_t;
static arp_t A_next, A;
static volatile uint32_t a_seq, a_took;
static const uint8_t RATE_SPQ[NRATE] = {1, 2, 3, 4, 6, 8};   /* steps a quarter */
static struct {
    float phase;                         /* the step, 0..1 (own tempo) */
    uint32_t idx;                        /* steps into the pattern */
    int cur;                             /* the id sounding (0: none) */
    float gate_left;                     /* samples until it lets go */
    uint8_t had;                         /* notes held at the last block */
    uint32_t ext_age;                    /* samples since the last clock tick */
    uint32_t tick;                       /* clock ticks since START */
    uint8_t run;                         /* the outside clock runs */
    uint32_t steps;
    float last;
    uint32_t rnd;
} AR = {0.0f, 0, 0, 0.0f, 0, 0xFFFFFFFu, 0, 1, 0, 0.0f, 0x9E3779B9u};

void eng_arp(int on, int rate, int pat, int octs, float gate, float bpm)
{
    a_seq++;
    A_next.on = (uint8_t)(on != 0);
    A_next.rate = (uint8_t)((uint32_t)rate < NRATE ? rate : RATE_16);
    A_next.pat = (uint8_t)((uint32_t)pat < NARPPAT ? pat : ARP_UP);
    A_next.octs = (uint8_t)(octs < 1 ? 1 : octs > 4 ? 4 : octs);
    A_next.gate = fm_clampf(gate, 0.05f, 1.0f);
    A_next.bpm = fm_clampf(bpm, 40.0f, 240.0f);
    a_seq++;
}
void eng_arp_notes(const float *pitch, const float *vel, int n)
{
    int i;
    if (n > ARP_MAX)
        n = ARP_MAX;
    a_seq++;
    for (i = 0; i < n; i++) {
        A_next.p[i] = pitch[i];
        A_next.v[i] = vel[i];
    }
    A_next.n = (uint8_t)(n < 0 ? 0 : n);
    a_seq++;
}
void eng_clock(int what)
{
    push(EV_CLK, what, 0.0f, 0.0f);
    commit();
}
int eng_clock_ext(void) { return AR.ext_age < (uint32_t)(SR * 0.5f); }
uint32_t eng_arp_steps(void) { return AR.steps; }
float eng_arp_pitch(void) { return AR.last; }

static void arp_take(void)
{
    uint32_t s0 = a_seq;
    arp_t c;
    if (s0 == a_took || (s0 & 1u))
        return;
    c = A_next;
    if (a_seq != s0)
        return;
    A = c;
    a_took = s0;
}

static void voice_off(int id)
{
    int i;
    for (i = 0; i < ENG_NV; i++)
        if (V[i].on && V[i].id == id) {
            if (V[i].steal) {
                V[i].steal = 0;
                V[i].kill = 1;
            }
            V[i].rel = 1;
        }
}

static void arp_release(void)
{
    if (AR.cur) {
        voice_off(AR.cur);
        AR.cur = 0;
    }
}

/* the step's note: the pattern over the notes (sorted low to high, or as played) and the octaves */
static void arp_step(void)
{
    uint8_t ord[ARP_MAX];
    uint32_t n = A.n, L, i, j, k, pos;
    float p, v, len;
    int id;
    if (!n)
        return;
    for (i = 0; i < n; i++)
        ord[i] = (uint8_t)i;
    if (A.pat != ARP_PLAYED)             /* low to high (a few notes: an insertion sort) */
        for (i = 1; i < n; i++)
            for (j = i; j > 0 && A.p[ord[j]] < A.p[ord[j - 1]]; j--) {
                uint8_t t = ord[j];
                ord[j] = ord[j - 1];
                ord[j - 1] = t;
            }
    L = n * A.octs;
    if (A.pat == ARP_DOWN)
        pos = L - 1u - AR.idx % L;
    else if (A.pat == ARP_UPDOWN && L > 1u) {
        uint32_t per = 2u * L - 2u, q = AR.idx % per;
        pos = q < L ? q : per - q;
    } else if (A.pat == ARP_RANDOM) {
        AR.rnd = AR.rnd * 1664525u + 1013904223u;
        pos = (AR.rnd >> 8) % L;
    } else
        pos = AR.idx % L;
    k = ord[pos % n];
    p = A.p[k] + 12.0f * (float)(pos / n);
    v = A.v[k];
    AR.idx++;
    AR.steps++;
    arp_release();
    id = ARP_ID + (int)(AR.steps & 1u);
    start(id, p, v, p, 0.0f);
    AR.cur = id;
    AR.last = p;
    len = SR * 60.0f / (A.bpm * (float)RATE_SPQ[A.rate]);   /* (with the outside clock: arp_clock sets it) */
    AR.gate_left = len * A.gate;
}

static uint32_t clk_period = 0;          /* samples between the last two ticks */
static void arp_clock(int what)
{
    if (what == CLK_TICK) {
        uint32_t tps = 24u / RATE_SPQ[A.on ? A.rate : RATE_16];
        if (AR.ext_age < (uint32_t)SR)
            clk_period = AR.ext_age;
        AR.ext_age = 0;
        sc_tick(0);
        if (!AR.run)
            return;
        if (A.on && A.n && AR.tick % tps == 0u) {
            arp_step();
            if (clk_period)              /* the gate in real time, from the clock's spacing */
                AR.gate_left = (float)(clk_period * tps) * A.gate;
        }
        AR.tick++;
    } else if (what == CLK_START) {
        sc_tick(1);
        AR.tick = 0;
        AR.idx = 0;
        AR.run = 1;
    } else if (what == CLK_CONT)
        AR.run = 1;
    else {
        AR.run = 0;
        arp_release();
    }
}

/* once a block, before the voices: the settings and notes taken, the steps due */
static void arp_block(uint32_t n)
{
    int ext;
    arp_take();
    if (AR.ext_age < 0x7FFFFFFFu)
        AR.ext_age += n;
    ext = eng_clock_ext();
    if (!ext)
        AR.run = 1;                      /* (the outside clock gone: own tempo, running) */
    if (AR.cur) {
        AR.gate_left -= (float)n;
        if (AR.gate_left <= 0.0f)
            arp_release();
    }
    if (!A.on || !A.n) {
        if (AR.had)
            arp_release();
        AR.had = 0;
        return;
    }
    if (!AR.had) {                       /* the first notes: from the pattern's start */
        AR.idx = 0;
        if (!ext) {                      /* own tempo: the first step now */
            AR.phase = 0.0f;
            arp_step();
        }
        AR.had = 1;
        return;
    }
    if (!ext) {
        float step = SR * 60.0f / (A.bpm * (float)RATE_SPQ[A.rate]);
        AR.phase += (float)n / step;
        if (AR.phase >= 1.0f) {
            AR.phase -= 1.0f;
            if (AR.phase >= 1.0f)
                AR.phase = 0.0f;
            arp_step();
        }
    }
}


