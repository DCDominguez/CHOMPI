# The step sequencer (Choowi rc4, design 2026-10-14)

Status: **design, not built.** The owner's request: a 4-track sequencer for rc4, with a **drum track that
plays inside CHOMPI**, core features first. Inspired by BaudGirl's FM-1+VA firmware for the M-VAVE FM-1
(lanes, step and live recording that layers, chords per step, per-step ratchet / chance / gate / tie,
parameter locks, pattern chaining); the core version below takes the first half of that list.

## What it is

- **4 tracks x 16 steps** (sixteenth notes, one bar), on the parts clock: Choowi's tempo, swing and MIDI
  clock in / out, so the sequencer, the arp and the bass stay in time.
- **Tracks 1-3: CHOMPI's sound** (the current synth patch). They share its voices (up to 7), so a track can
  hold a chord, a bass line, a melody.
- **Track 4: drums inside CHOMPI.** A second sampler voice set plays a **kit bank** (default: Choowi's
  purple drums) **alongside the synth**: new for CHOMPI, which so far plays the synth *or* the sampler.
  Works when the main sound is a synth patch (the sample memory then holds the kit); with a sampler patch
  as the main sound, track 4 is silent (the memory holds that patch's bank) and its light says so.
- **Per step:** on / off, up to 4 notes (a chord; drums: any of the 15 pads), **gate** (25-100 % of the
  step, or **tie** into the next), **chance** (100 / 75 / 50 / 25 %), **ratchet** (1-4 hits in the step).
- **Live recording that layers:** arm, close the menu, play: notes land on the nearest sixteenth of the
  selected track, added to what's there (harmony chords record as their chord notes; drum pads from the
  white keys as in kit mode).
- **Saved with the preset** (`FORGE/BbSss.FST` beside the `.FPR`), like the event recorder's projects.
- **Choowivision** shows the grid (4 lanes x 16 steps, the playhead, the selected step).

Later (not in rc4's core): 64 steps (pages), parameter locks (hold a step + turn a knob), copy / move
steps, pattern chaining with repeats, a MIDI channel per track (the road to Chomposer), drums with a
sampler patch as the main sound.

## The sequencer page (proposal)

Open the menu (toggle down, hold CHOMPI), KEY_21 steps harmony → parts → synth → FX → **sequencer**; the
CHOMPI light turns **yellow** (the one panel colour no page uses yet).

| Control | Does |
|---|---|
| **The first 16 keys** (C3 ... D#4, in panel order) | the 16 steps of the selected track: tap = step on / off (a synth track takes the last notes played on it; drums take the selected pad); **hold a step + turn a knob** = edit that step |
| **E4 · F4 · F#4 · G4** | select track 1-4 (lit in the track's colour) |
| **G#4** | record arm (live recording happens with the menu closed) |
| **A4** | play / stop (starts on the next bar when the arp or the looper is running) |
| **A#4** | clear the selected track (tap twice within 2 s) |
| **B4** | drums: next pad to place (cycles the 15 pads; the pad plays as you pick it) |
| **C5** | hold: audition the selected track alone |
| SW1 (holding a step) | gate: 25 / 50 / 75 / 100 % / tie |
| SW2 (holding a step) | chance: 100 / 75 / 50 / 25 % |
| SW3 (holding a step) | ratchet: 1-4 |
| SW4 (holding a step) | transpose the step's notes in scale steps (drums: the pad) |
| SW1-SW4 (no step held) | the four tracks' levels |

**Lights:** a step that's on glows in the track colour, dim; the playhead runs bright white across the
16 keys; the held step blinks; track keys show their colour (the selected one bright); G#4 red while armed;
A4 green while playing. Track colours from CHOMPI's nine: track 1 pink, 2 teal, 3 blue, 4 (drums) orange.

## How it's built

- `core/step_sequencer.h` (new): the pattern (4 x 16 steps, ~0.6 KB), the step clock on the parts clock,
  chance from Choowi's seeded random, ratchets, gates and ties, a note tracker so stop / clear / panic end
  every note; audio callback only, bounded per block; host tests.
- **Engine:** a fifth note source (the sequencer) beside wired MIDI, USB, the keys and the event recorder
  (`Synth::kSources` 4 → 5); tracks 1-3 play into the main synth; track 4 into a **second `Synth` set up as
  a kit sampler** (same code, its own ~4 voices, mixed after the main one). The sample loader asks for the
  drum kit when the main patch isn't a sampler.
- **Panel:** `MenuPage::Seq` after `Fx` in the KEY_21 cycle, its keys / knobs / lights as above; the view
  (Choowivision) gains a sequencer word or page.
- **Files:** `FORGE/BbSss.FST` with a version byte; old presets load with an empty pattern.
- **Budget (to measure):** code ~8-12 KB of the 34 KB release headroom (the development build has 22 KB);
  RAM ~2 KB; CPU: the drum voices are the cost (a kit hit is short; capped at 4 voices) on top of the
  synth: measure with sampler + synth + FX on CHOMPI before calling it done.
- **Tests:** step timing on the clock (straight and swung), gates / ties / ratchets / chance (seeded),
  layering live recording, the note tracker (no stuck notes after stop, clear, panic, tempo change), the
  drum sampler alongside the synth, file round trip, the page's keys and lights.

## Open for the owner

- The page layout above (keys for steps, tracks, transport).
- Track 4's kit: Choowi's purple drums by default, or the kit bank chosen on TAPE's page?
- Should the sequencer's play also start the looper / the event recorder in time (one transport)?
