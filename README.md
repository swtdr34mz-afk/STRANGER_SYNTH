# STRANGER SYNTH

A performance synth for the M-VAVE FM-1 by **swtdr34mz**, in the spirit of 80s analogue film scores. Based on
[OFFWORLD](https://github.com/swtdr34mz-afk/OFFWORLD), built on [fm1-base](https://github.com/charlesvestal/fm1-base)
and [felucca-bsp](https://github.com/hugelton/felucca-bsp).

**Work in progress.** Unofficial: not made or supported by M-VAVE; at your own risk.

## Plan

1. Scaffold: name, splash, IDs, hub files (done: v0.0.1, still sounds like OFFWORLD)
2. LOW SPLIT + arpeggiator (synced to MIDI clock) (done: v0.0.2)
3. Presets in the style of classic 80s poly and mono synths
4. CHORD mode with sidechain, TONE FOLLOW (to the low split or the chords)
5. Performance FX: tape stop, VHS wobble, tape rewind, gated reverb, tape echo throw, lights out, walkie, riser
6. The screen: a small town at night, the mirror world
7. SIX MACHINES sends MIDI clock and its kick as the sidechain trigger

## Build

`X0X_FONTSET=terminus python3 fm1-base/tools/build.py . --host`, then `build/host/stranger_synth_host tests/basic.txt build/host`.
GitHub Actions builds the `.fwsc` on every push (Actions > the run > Artifacts) and attaches it to a release on a tag `v*`.

## The arpeggiator (v0.0.2)

- **ARP** (tap): the arpeggiator on / off. With the low split on it plays the left hand (the right hand plays free and
  follows it); with the split off, every key. LOW HOLD keeps its notes running. **ARP held 1 s**: EPIC.
- **LFO**: the ARP page. K1 RATE (1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32), K2 PATTERN (UP, DOWN, UP-DOWN, PLAYED, RANDOM),
  K3 OCTAVES (1..4), K4 GATE (5..100 %). SELECT on this page: the tempo (40..240 BPM).
- **MIDI clock** in (USB or the TRS jack): the arp follows it (START from the first step, STOP silent); the page shows EXT CLOCK.
