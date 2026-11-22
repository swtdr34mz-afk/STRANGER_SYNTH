# STRANGER SYNTH: the manual

A performance synth for the M-VAVE FM-1 by swtdr34mz. Unofficial: not made or supported by M-VAVE; at your own risk.

## 1. Start

Install the `.fwsc` from the releases like any FM-1 firmware. At power-on the red title appears, lightning strikes,
and the screen opens on preset 1, **THEME SEQUENCE**, ready to play: the low split two octaves down (left hand: the
bass), the right hand on the arpeggiator, UP-DOWN at 84 BPM. Hold a chord with the right hand, a bass note with the
left, and turn **FILTER** (K4).

## 2. The sound

Every preset is a voice of two oscillators (saw, pulse or sine), a sub an octave down, a 24 dB resonant low-pass with
its own envelope, ring modulation, noise, an ensemble chorus, a ping-pong delay and a reverb up to 30 s.

**The four knobs** (MAIN) are macros: each moves several things at once and rests for a few detents on its
**sweetspots**, whose names light up.

| Knob | Macro | Sweetspots |
| --- | --- | --- |
| K1 | SHAPE: the envelopes | PLUCK, STAB, BRASS, SWELL, PAD |
| K2 | DRIFT: detune, analogue drift, chorus, width, tape wow | CLEAN, ANALOG, ENSEMBLE, WIDE, TAPE |
| K3 | SPACE: the room and the delay | ROOM, HALL, CATHEDRAL, CANYON, FREEZE |
| K4 | FILTER: cutoff, resonance, drive | DARK, WARM, OPEN, BRIGHT, SCREAM |

**SELECT** turns TONE (darker / brighter), **ALGORITHM** turns **SIZE** (chorus, width and room together: bigger),
**PRESET** steps through the presets. Each preset keeps its own knobs; HOME held 1 s puts them back as they came.

**The presets**

| # | Preset | |
| --- | --- | --- |
| 1 | THEME SEQUENCE | a wall of detuned saws and the sub; starts in the split, the right hand on the arp |
| 2 | KIDS ON BIKES | saw and pulse with a square sub and chorus, ringing out long |
| 3 | LAZY NIGHT | dark, detuned plucks with a sour tritone beside them |
| 4 | NOIR TAPE | a hollow square synth from a worn tape: wobble, hiss |
| 5 | CHAOS | a grinding minor second, ringing, warped, swelling out of the dark |
| 6 | FIVE BRASS | brass from below, an octave down, a cathedral of reverb |
| 7 | CHORUS STRINGS | one wide pulse and its chorus |
| 8 | MODEL BASS | two saws an octave apart, a fat filter; starts on the arp (UP-DOWN) |
| 9 | OB BRASS | wide, bright brass |
| 10 | SIX PAD | saw and a fifth in pulse, slow and wide |
| 11 | MONO ARP BASS | pulse and sub, squelchy; starts on the arp (UP-DOWN) |
| 12 | SYNC LEAD | a resonant lead an octave up, wide, with a hall |
| 13 | MIRROR CHOIR | hollow pulses from tape, far away |

A preset that starts on the arp or in the split switches them off again when you move to another preset.

## 3. Playing

- **The keys** play with velocity (harder: brighter) and pressure (PRESSURE page). **OCT- / OCT+**: the octave.
- **REC**: HOLD, every note stays (a key with no key down starts a new chord). REC held 1 s: every note off.
- **SPLIT** (SEL): K1 SPLIT (off, 1..3 octaves down), K2 POINT, K3 SCALE, K4 FOLLOW. SEL held + a key: the split there.
  The low split's keys glow softly. **SEQ**: LOW HOLD, the low notes stay, the right hand plays free.
- **FOLLOW**: the right hand follows the low split's note. SNAP: every key to the scale; WHITE: the scale on the white
  keys; CHORD: the chord's notes on the white keys.
- **PRESSURE** (GLO): what pressing harder does (FILTER, SWELL, POWER, VIBRATO), how much, TONE, VIBRATO.
- **GLIDE** (EDIT twice): LOW GLIDE and LOW MODE for the split, GLIDE and MODE for the right hand (POLY, MONO, LEGATO).

## 4. The arpeggiator

**ARP** switches it on and off; held 1 s: EPIC (fifth and octave on every note). With the split off it plays every key;
with the split on, one hand: on the ARP page, ARP tapped switches LEFT / RIGHT (shown at the bottom).
**LFO** opens the ARP page: K1 RATE (1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32), K2 PATTERN (UP, DOWN, UP-DOWN, PLAYED, RANDOM),
K3 OCTAVES (1..4), K4 GATE. **SELECT** there: the tempo, 40..240 BPM. With MIDI clock coming in (USB, the TRS jack, LINK)
the arp follows it (START from the first step) and the page shows EXT CLOCK.

## 5. Chords and the sidechain

**ENV** opens the CHORD page. K1 CHORD (OFF, MINOR, MAJOR, MIN7, MAJ7, SUS2, SUS4, POWER, MIN9, ADD9): one finger plays
it (on the low split, or every key with the split off; with the arp, the arp plays the chord). K2 SPREAD (CLOSE, OPEN,
WIDE). K3 SIDECHAIN: how deep the sound ducks. K4 SC RATE: 1/4, 1/8, 1/2 of the tempo, KICK (a kick from outside: MIDI
channel 10, note 36), HEART (every heartbeat).

## 6. The HEART

**EDIT** opens the HEART page: a heartbeat as the groove, at the tempo or the clock. K1 HEART: its level (left: off).
K2 PATTERN: SINGLE, DOUBLE (lub-dub), SLOW (every two beats), RACING. K3 REAL: from an analogue kick (a sine falling
from high to low, a click) to a real heart's muffled thump. K4 DECAY. The heart is not ducked by the sidechain; with SC
RATE at HEART it drives it. The picture pulses red with it.

## 7. Performance

- **PLAY**: TAPE STOP, the tape slows to a stop; again: it spins back up.
- **FX held + one of the lowest eight white keys** switches an effect; while FX is held, the keys of the effects that
  are on light up. FX tapped shows the PERFORM page (the keys play as always there; the knobs are the macros):
  TAPE STOP · VHS (wow, flutter, the highs gone, hiss) · REWIND (once) · GATED reverb · ECHO throw · LIGHTS OUT (all
  under a low-pass) · WALKIE (a walkie-talkie) · RISER (the filters open over 8 s, noise rising).
- While effects play, the screen turns over into the MIRROR: cold, dark, red at the brightest, ash floating up.

## 8. LINK: a second FM-1

**SAVE** switches LINK on: the USB port plays host (no computer meanwhile). Connect the FM-1 running SIX MACHINES (with
MIDI out) through an OTG cable or adapter; the port gives no power, so the other FM-1 runs on its battery or through a
powered hub. LNK in the header: dim while looking, green when connected. SIX MACHINES' clock then drives the arp, the
heart and the sidechain; with SC RATE at KICK its kick ducks the sound. SAVE again: back to the computer. A USB-MIDI
controller works the same way.

## 9. MIDI

MIDI in from USB, the TRS jack or LINK plays the synth on any channel (channel 10 is left to the drums: its note 36 is
the KICK for the sidechain). MIDI clock, START, STOP and CONTINUE drive the arp, the heart and the sidechain. Polyphonic
pressure works as the keys' pressure.

## 10. The screen

Eight places of a small town at night (MAPLE LANE, THE WOODS, THE LAB, ROUTE 9, THE BASEMENT, THE SKY, RADIO TOWER, THE
ARCADE), panning sideways, cut every 18 to 35 s with a tape tear and colour bars. Windows, lamps, headlights, fireflies,
arcade screens and sparks follow the notes; in the red storm the notes bring lightning. HOME twice draws all the
controls on the screen; any button closes them.
