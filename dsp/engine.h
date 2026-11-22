/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (OFFWORLD) */
/* OFFWORLD's sound engine: one instrument, an 80s brass synth for drones.
 *
 *   voice     two saws (or saw and pulse) a little apart, a sine sub an octave down, a 24 dB low-pass with its own
 *             brass envelope (it opens slowly, like a horn section swelling), an amp envelope, a delayed vibrato,
 *             and a slow random drift of its pitch (an old analogue's)
 *   effects   an ensemble chorus (the 80s width), a dark ping-pong delay, a reverb of eight lines (up to 30 s, FREEZE)
 *   rain      hiss, drops, the city's rumble, and thunder far off (at STORM and FLOOD)
 *
 * Every preset is a TIMBRE (the oscillators, the filter) plus four MACROS, one per knob, 0..127. A macro moves several
 * parameters at once, each along its own curve, and has SWEETSPOT ZONES: positions where everything it moves lands
 * on a value that sounds right together (their names are shown). The app makes a knob rest on a zone for a few
 * detents; the engine turns the four values into the sound.
 *
 * The main loop talks to the engine through eng_* calls (notes go through a small queue); eng_render runs in the
 * audio interrupt. */
#pragma once
#include <stdint.h>

#define ENG_SR 44100.0f
#define ENG_NV 8                         /* voices: notes at once */

/* a macro: its name and its sweetspot zones (centre 0..127, name), left to right */
#define MACRO_ZONES 5
typedef struct {
    const char *name;
    uint8_t nzones;
    uint8_t centre[MACRO_ZONES];
    const char *zone[MACRO_ZONES];
} macro_t;
extern const macro_t MACROS[4];          /* K1 SHAPE, K2 DRIFT, K3 SPACE, K4 FILTER */

/* a preset: its name, its timbre, and the macros it starts at */
typedef struct {
    const char *name;
    float o2_semi;                       /* the second oscillator, semitones from the first (0, 7, 12, -12 ...) */
    uint8_t o2_pulse;                    /* 1: the second is a pulse (hollow, reedy); 2: a sine */
    float pw;                            /* its width 0.5 .. 0.9 */
    float o2_lvl, sub;                   /* the second's level, the sub's */
    float cut_hz;                        /* the filter closed */
    float env_oct;                       /* how far the brass envelope opens it, in octaves */
    float res;                           /* resonance 0 .. 0.9 */
    float bright;                        /* velocity's share in the opening, 0 .. 1 */
    uint8_t m[4];                        /* the macros it starts at */
    float ring, noise, pluck;            /* RING 0..1, NOISE 0..1, PLUCK 0..1 */
    float sine;                          /* SINE: the first oscillator from a saw toward a sine, 0..1 */
    int8_t arp;                          /* the arp it starts with (ARP_*), -1: as it is */
    uint8_t deep;                        /* 1: with the arp, a deep root held an octave under its lowest note */
    float wob;                           /* WOBBLE: a fast vibrato from the start, cents */
    uint8_t split;                       /* the LOW SPLIT it starts with, octaves down (0: as it is) */
    uint8_t arp_hi;                      /* 1: its arp plays the right hand (with the split on) */
} preset_t;
#define NPRESETS 13
extern const preset_t PRESETS[NPRESETS];

void eng_init(void);
void eng_size(float s);                  /* SIZE 0..1: chorus, width and room together (then eng_sound) */
/* what pressing a key harder does */
enum { PRESS_OFF, PRESS_FILTER, PRESS_SWELL, PRESS_POWER, PRESS_VIBRATO, NPRESS };
/* the sound: the preset's timbre, the four macros, TONE -1..1 (the filter darker / brighter), VIBRATO 0..1,
 * PRESSURE (PRESS_*) and its amount 0..1 */
void eng_sound(int preset, const uint8_t m[4], float tone, float vib, int press, float pamt);
/* a note: id names it (the same id again strikes it again), pitch in MIDI semitones, velocity 0..1 */
void eng_note_on(int id, float pitch, float vel);
/* GLIDE: a new note that slides from pitch from to pitch in about glide_s seconds */
void eng_note_glide(int id, float pitch, float vel, float from, float glide_s);
/* legato: the voice of old_id becomes id and slides to pitch (retrig: its envelopes start again); none: a new note */
void eng_note_move(int old_id, int id, float pitch, float vel, float glide_s, int retrig);
void eng_note_off(int id);
/* the world: the scene on the screen (its sound: WORLD_*), its level 0..1; things happening in it (SFX_*: a, b below) */
enum { NWORLD = 8 };
enum { SFX_FLYBY, SFX_ZAP, SFX_FLAME, SFX_SIREN, SFX_CLINK };   /* FLYBY: a = direction -1 / 1, b = seconds; SIREN: a = pan */
void eng_world(int scene, float level);
void eng_sfx(int type, float a, float b);
void eng_memory(int on);                 /* MEMORY: 1 the last 3 s frozen into a cloud; 0 it fades (and records again) */
void eng_press(int id, float p);        /* a key's pressure 0..1 */
void eng_all_off(void);                  /* every note let go (they ring out) */
void eng_panic(void);                    /* silence now */
void eng_shed(void);                     /* the render ran late: a voice let go quickly (audio interrupt) */
void eng_render(int32_t *lr, uint32_t n, float gain);

/* the ARPEGGIATOR (STRANGER SYNTH): it runs in the render, so its steps land on time (a block, 5.8 ms, at most late,
 * never drifting). The app gives it the notes held for it, in the order they were played; it plays them one at a
 * time, RATE a step, in PATTERN, over OCTAVES, each sounding GATE of its step. With MIDI clock coming in (24 a quarter)
 * it follows the clock (START from the first step, STOP silent); else its own tempo, BPM. */
enum { ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_PLAYED, ARP_RANDOM, NARPPAT };
enum { RATE_4, RATE_8, RATE_8T, RATE_16, RATE_16T, RATE_32, NRATE };
#define ARP_MAX 16
void eng_arp(int on, int rate, int pat, int octs, float gate, float bpm);
void eng_arp_notes(const float *pitch, const float *vel, int n);   /* the held notes, as played (n 0: none) */
enum { CLK_TICK, CLK_START, CLK_STOP, CLK_CONT };
void eng_clock(int what);                /* MIDI real time in: a clock tick (0xF8), START, STOP, CONTINUE */
int eng_clock_ext(void);                 /* 1: the clock comes from outside (ticks within the last half second) */
uint32_t eng_arp_steps(void);            /* steps played since the start (the screen, the tests) */
float eng_arp_pitch(void);               /* the arp's last note (0: none yet) */

/* PERFORM (STRANGER SYNTH): the live effects, on the whole sound (bit i of the mask: effect i on; REWIND runs once when
 * its bit rises, TAPE STOP spins back up when it falls) */
enum { FX_TAPESTOP, FX_VHS, FX_REWIND, FX_GATED, FX_ECHO, FX_LIGHTS, FX_WALKIE, FX_RISER, NFX };
void eng_fx(uint32_t mask);
uint32_t eng_fx_busy(void);              /* the tape still away from the live sound (stopped, spinning, rewinding) */
float eng_fx_amt(int fx);                /* how much of an effect sounds now, 0..1 (the screen) */
/* the SIDECHAIN: everything ducked on each beat (1/4, 1/8, 1/2: the tempo or the MIDI clock), or on KICK (eng_sc_hit,
 * a kick from outside), DEPTH 0..1 */
enum { SC_4, SC_8, SC_2, SC_KICK, SC_HEART, NSC };
void eng_sidechain(float depth, int rate);
void eng_sc_hit(void);
float eng_sc_gain(void);                 /* the duck now, 0..1 (1: none) */
/* the HEART: a heartbeat as the groove (an analogue kick and a real heart's thump), at the tempo or the clock */
enum { HB_SINGLE, HB_DOUBLE, HB_SLOW, HB_RACE, NHB };
void eng_heart(float level, int pattern, float real, float decay);   /* level 0: off */
float eng_heart_env(void);               /* the beat now, 0..1 (the screen) */

/* for the screen and the tests */
/* a write to the flash stops the sound for a moment: the app fades the output out first (and back in after) */
void eng_duck(int on);
int eng_ducked(void);
int eng_voices(void);                    /* voices sounding */
float eng_level(void);                   /* the output, 0..1, smoothed */
float eng_limit(void);                   /* the limiter's deepest cut since the last call (1: none) */
uint32_t eng_thunder(void);              /* thunders since the start (the screen flashes) */
uint32_t eng_notes(void);                /* notes started since the start */
