# STRANGER SYNTH: das Handbuch

Ein Performance-Synth für das M-VAVE FM-1 von swtdr34mz. Inoffiziell: nicht von M-VAVE, ohne Support, auf eigene Gefahr.

## 1. Start

Die `.fwsc` aus den Releases wie jede FM-1-Firmware installieren. Beim Einschalten erscheint der rote Titel, ein Blitz
schlägt ein, dann öffnet sich Preset 1, **THEME SEQUENCE**, spielbereit: Low Split zwei Oktaven tiefer (linke Hand: der
Bass), die rechte Hand auf dem Arpeggiator, UP-DOWN mit 84 BPM. Rechts einen Akkord halten, links einen Basston, und
**FILTER** (K4) aufdrehen.

## 2. Der Klang

Jedes Preset ist eine Stimme aus zwei Oszillatoren (Sägezahn, Puls oder Sinus), einem Sub eine Oktave tiefer, einem
resonanten 24-dB-Tiefpass mit eigener Hüllkurve, Ringmodulation, Rauschen, Ensemble-Chorus, Ping-Pong-Delay und einem
Hall bis 30 s.

**Die vier Regler** (MAIN) sind Makros: Jeder bewegt mehreres auf einmal und rastet ein paar Klicks auf seinen
**Sweetspots** ein, deren Namen aufleuchten.

| Regler | Makro | Sweetspots |
| --- | --- | --- |
| K1 | SHAPE: die Hüllkurven | PLUCK, STAB, BRASS, SWELL, PAD |
| K2 | DRIFT: Verstimmung, Analog-Drift, Chorus, Breite, Tape-Leiern | CLEAN, ANALOG, ENSEMBLE, WIDE, TAPE |
| K3 | SPACE: Raum und Delay | ROOM, HALL, CATHEDRAL, CANYON, FREEZE |
| K4 | FILTER: Cutoff, Resonanz, Verzerrung | DARK, WARM, OPEN, BRIGHT, SCREAM |

**SELECT** dreht TONE (dunkler / heller), **ALGORITHM** dreht **SIZE** (Chorus, Breite und Raum zusammen: größer),
**PRESET** wechselt das Preset. Jedes Preset merkt sich seine Regler; HOME 1 s halten setzt sie zurück.

**Die Presets**

| # | Preset | |
| --- | --- | --- |
| 1 | THEME SEQUENCE | eine Wand aus verstimmten Sägezähnen mit Sub; startet im Split, rechte Hand auf dem Arp |
| 2 | KIDS ON BIKES | Sägezahn und Puls mit Rechteck-Sub und Chorus, lang ausklingend |
| 3 | LAZY NIGHT | dunkle, verstimmte Plucks mit schrägem Tritonus daneben |
| 4 | NOIR TAPE | ein hohler Rechteck-Synth vom abgenudelten Band: Leiern, Rauschen |
| 5 | CHAOS | eine reibende kleine Sekunde, Ringmod, verzogen, schwillt aus dem Dunkeln |
| 6 | FIVE BRASS | Bläser von unten, eine Oktave tiefer, Kathedralen-Hall |
| 7 | CHORUS STRINGS | ein breiter Puls und sein Chorus |
| 8 | MODEL BASS | zwei Sägezähne eine Oktave auseinander, fetter Filter; startet mit Arp (UP-DOWN) |
| 9 | OB BRASS | breite, helle Bläser |
| 10 | SIX PAD | Sägezahn und Quinte als Puls, langsam und breit |
| 11 | MONO ARP BASS | Puls und Sub, schmatzend; startet mit Arp (UP-DOWN) |
| 12 | SYNC LEAD | resonantes Lead eine Oktave höher, breit, mit Hall |
| 13 | MIRROR CHOIR | hohle Pulse vom Band, weit weg |

Ein Preset, das Arp oder Split einschaltet, schaltet sie beim Wechsel zu einem anderen Preset wieder aus.

## 3. Spielen

- **Die Tasten** spielen mit Anschlag (fester: heller) und Druck (Seite PRESSURE). **OCT- / OCT+**: die Oktave.
- **REC**: HOLD, alle Noten bleiben stehen (eine Taste ohne gedrückte Taste startet einen neuen Akkord). REC 1 s
  halten: alle Noten aus.
- **SPLIT** (SEL): K1 SPLIT (aus, 1..3 Oktaven tiefer), K2 POINT, K3 SCALE, K4 FOLLOW. SEL halten + Taste: der Split
  dort. Die Tasten des Low Splits leuchten leicht. **SEQ**: LOW HOLD, die tiefen Noten bleiben, rechts spielt frei.
- **FOLLOW**: Die rechte Hand folgt dem Ton im Low Split. SNAP: jede Taste in die Tonleiter; WHITE: die Tonleiter auf
  den weißen Tasten; CHORD: die Akkordtöne auf den weißen Tasten.
- **PRESSURE** (GLO): was fester Drücken macht (FILTER, SWELL, POWER, VIBRATO), wie viel, TONE, VIBRATO.
- **GLIDE** (EDIT zweimal): LOW GLIDE und LOW MODE für den Split, GLIDE und MODE für rechts (POLY, MONO, LEGATO).

## 4. Der Arpeggiator

**ARP** schaltet ihn an und aus; 1 s halten: EPIC (Quinte und Oktave auf jede Note). Ohne Split spielt er alle Tasten,
mit Split eine Hand: Auf der ARP-Seite wechselt ARP antippen zwischen LEFT und RIGHT (unten angezeigt).
**LFO** öffnet die ARP-Seite: K1 RATE (1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32), K2 PATTERN (UP, DOWN, UP-DOWN, PLAYED, RANDOM),
K3 OCTAVES (1..4), K4 GATE. **SELECT** dort: das Tempo, 40..240 BPM. Kommt MIDI Clock rein (USB, TRS-Buchse, LINK),
folgt der Arp ihr (START ab dem ersten Schritt), und die Seite zeigt EXT CLOCK.

## 5. Akkorde und Sidechain

**ENV** öffnet die CHORD-Seite. K1 CHORD (OFF, MINOR, MAJOR, MIN7, MAJ7, SUS2, SUS4, POWER, MIN9, ADD9): ein Finger spielt
den Akkord (im Low Split, oder auf allen Tasten ohne Split; mit Arp spielt der Arp den Akkord). K2 SPREAD (CLOSE, OPEN,
WIDE). K3 SIDECHAIN: wie tief der Sound duckt. K4 SC RATE: 1/4, 1/8, 1/2 des Tempos, KICK (eine Kick von außen: MIDI-Kanal
10, Note 36), HEART (jeder Herzschlag).

## 6. Das HEART

**EDIT** öffnet die HEART-Seite: ein Herzschlag als Groove, im Tempo oder zur Clock. K1 HEART: Lautstärke (links: aus).
K2 PATTERN: SINGLE, DOUBLE (Doppelschlag), SLOW (alle zwei Beats), RACING. K3 REAL: von einer analogen Kick (Sinus, der
von hoch nach tief fällt, mit Klick) bis zum dumpfen Schlag eines echten Herzens. K4 DECAY. Das Herz wird vom Sidechain
nicht geduckt; mit SC RATE auf HEART steuert es ihn. Das Bild pulsiert rot mit.

## 7. Performance

- **PLAY**: TAPE STOP, das Band bremst bis zum Stillstand; nochmal: es läuft wieder an.
- **FX halten + eine der acht untersten weißen Tasten** schaltet einen Effekt; solange FX gehalten wird, leuchten die
  Tasten der aktiven Effekte. FX antippen zeigt die PERFORM-Seite (die Tasten spielen dort ganz normal, die Regler sind
  die Makros): TAPE STOP · VHS (Leiern, Flattern, Höhen weg, Rauschen) · REWIND (einmal) · GATED Hall · ECHO-Wurf ·
  LIGHTS OUT (alles unter einem Tiefpass) · WALKIE (Funkgerät) · RISER (die Filter öffnen sich über 8 s, Rauschen steigt).
- Während Effekte laufen, kippt das Bild in den SPIEGEL: kalt, dunkel, das Hellste rot, Asche steigt auf.

## 8. LINK: ein zweites FM-1

**SAVE** schaltet LINK ein: Der USB-Anschluss wird Host (solange kein Computer). Das FM-1 mit SIX MACHINES (mit MIDI-Out)
über ein OTG-Kabel oder einen Adapter anschließen; der Anschluss liefert keinen Strom, das andere FM-1 läuft also auf
Akku oder über einen Hub mit Netzteil. LNK oben: gedimmt beim Suchen, grün wenn verbunden. Die Clock von SIX MACHINES
steuert dann Arp, Herz und Sidechain; mit SC RATE auf KICK duckt seine Kick den Sound. SAVE nochmal: zurück zum
Computer. Ein USB-MIDI-Controller geht genauso.

## 9. MIDI

MIDI über USB, die TRS-Buchse oder LINK spielt den Synth auf jedem Kanal (Kanal 10 bleibt den Drums: seine Note 36 ist
die KICK für den Sidechain). MIDI Clock, START, STOP und CONTINUE steuern Arp, Herz und Sidechain. Polyphoner Druck
wirkt wie der Tastendruck.

## 10. Der Bildschirm

Acht Orte einer Kleinstadt bei Nacht (MAPLE LANE, THE WOODS, THE LAB, ROUTE 9, THE BASEMENT, THE SKY, RADIO TOWER, THE
ARCADE), seitlich schwenkend, alle 18 bis 35 s geschnitten mit Bandriss und Farbbalken. Fenster, Lampen, Scheinwerfer,
Glühwürmchen, Automaten-Bildschirme und Funken folgen den Noten; im roten Sturm bringen die Noten Blitze. HOME zweimal
zeichnet alle Bedienelemente auf den Bildschirm; jede Taste schließt sie.
