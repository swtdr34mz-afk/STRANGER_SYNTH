# STRANGER SYNTH

A performance synth for the M-VAVE FM-1 by **swtdr34mz**, in the spirit of 80s analogue film scores. Based on
[OFFWORLD](https://github.com/swtdr34mz-afk/OFFWORLD), built on [fm1-base](https://github.com/charlesvestal/fm1-base)
and [felucca-bsp](https://github.com/hugelton/felucca-bsp).

![STRANGER SYNTH](site/img/screen-main.png) ![The title](site/img/screen-splash.png) ![The mirror](site/img/screen-mirror.png)

**Work in progress.** Unofficial: not made or supported by M-VAVE; at your own risk.

## Plan

1. Scaffold: name, splash, IDs, hub files (done: v0.0.1, still sounds like OFFWORLD)
2. LOW SPLIT + arpeggiator (synced to MIDI clock) (done: v0.0.2)
3. Presets in the style of classic 80s poly and mono synths (done: v0.0.3)
4. CHORD mode with sidechain, TONE FOLLOW (to the low split or the chords) (done: v0.0.3)
5. Performance FX: tape stop, VHS wobble, tape rewind, gated reverb, tape echo throw, lights out, walkie, riser (done: v0.0.3)
6. The screen: a small town at night, the mirror world (done: v0.1.0)
7. SIX MACHINES sends MIDI clock and its kick as the sidechain trigger (done: v0.2.0, and SIX MACHINES' MIDI out)

## Build

`X0X_FONTSET=terminus python3 fm1-base/tools/build.py . --host`, then `build/host/stranger_synth_host tests/basic.txt build/host`.
GitHub Actions builds the `.fwsc` on every push (Actions > the run > Artifacts) and attaches it to a release on a tag `v*`.

## The arpeggiator (v0.0.2)

- **ARP** (tap): the arpeggiator on / off. With the low split on it plays the left hand (the right hand plays free and
  follows it); with the split off, every key. LOW HOLD keeps its notes running. **ARP held 1 s**: EPIC.
- **LFO**: the ARP page. K1 RATE (1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32), K2 PATTERN (UP, DOWN, UP-DOWN, PLAYED, RANDOM),
  K3 OCTAVES (1..4), K4 GATE (5..100 %). SELECT on this page: the tempo (40..240 BPM).
- **MIDI clock** in (USB or the TRS jack): the arp follows it (START from the first step, STOP silent); the page shows EXT CLOCK.

## Sounds, chords, performance (v0.0.3)

- **Ten presets** in the manner of classic 80s analogues (our own names): NIGHT SEQUENCE, FIVE BRASS, CHORUS STRINGS,
  MODEL BASS, OB BRASS, SIX PAD, MONO ARP BASS, SYNC LEAD, MIRROR CHOIR, THE DARK BELOW.
- **The knobs (MAIN)**: K1 SHAPE (PLUCK, STAB, BRASS, SWELL, PAD), K2 DRIFT (CLEAN, ANALOG, ENSEMBLE, WIDE, TAPE),
  K3 SPACE (ROOM .. FREEZE), K4 FILTER (DARK, WARM, OPEN, BRIGHT, SCREAM). OFFWORLD's rain is gone.
- **CHORD** (ENV): K1 CHORD (OFF, MINOR, MAJOR, MIN7, MAJ7, SUS2, SUS4, POWER, MIN9, ADD9) from one finger (the low split,
  or every key with the split off; with ARP the arp plays the chord), K2 SPREAD (CLOSE, OPEN, WIDE), K3 SIDECHAIN depth,
  K4 SC RATE (1/4, 1/8, 1/2, KICK: a kick on MIDI channel 10, note 36).
- **FOLLOW CHORD** (SEL page, FOLLOW): the right hand's white keys play the chord's notes.
- **PERFORM** (FX tapped; or FX held + a key on any page): the lowest eight white keys switch the effects:
  TAPE STOP, VHS, REWIND (once), GATED reverb, ECHO throw, LIGHTS OUT, WALKIE, RISER. The knobs stay the macros.
- **REC** tapped: TAPE STOP (again: it spins back up). REC held 1 s: every note off.

## The screen (v0.1.0)

- **The intro**: a VCR wakes up, a red storm rises over the trees and houses, a bike crosses, the title buzzes on in red neon.
- **Eight places**, one night (a VCR clock in the corner), cut with a tape tear and colour bars every 18 to 35 s:
  MAPLE LANE, THE WOODS, THE LAB, ROUTE 9, THE BASEMENT, THE SKY, RADIO TOWER, THE ARCADE. Windows, lamps, headlights,
  fireflies, arcade screens and sparks follow the notes; in the red storm the notes bring lightning.
- **The MIRROR**: while PERFORM effects play, the picture turns over into a cold, dark copy of itself, red at the
  brightest, ash floating up, the tracking line rolling. All drawn here; no figures, logos or images from any film or show.

## LINK: two FM-1s (v0.2.0)

**SAVE** switches LINK on: STRANGER SYNTH's USB port plays host. Connect the FM-1 running SIX MACHINES to it (an OTG
cable or adapter; the port gives no power, so the other FM-1 runs on its battery or through a powered hub). The header
shows LNK (green: connected). SIX MACHINES' clock then drives the arp and the sidechain (START, STOP), and with
SC RATE at KICK its kick ducks the sound. SAVE again: back to the computer (no computer while LINK is on).
