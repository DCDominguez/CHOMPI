# Chomposer: architecture (design, 2026-10-14)

Companion to [CHOMPOSER.md](CHOMPOSER.md) (the idea, the conducting research, the control map). This is
how it would be built. **Design only: nothing here is implemented or measured.** Numbers marked ~ are
estimates to check when the code exists.

## 1. Constraints (the workshop's rules, applied)

- **Real time:** the audio callback (48 kHz, 24-frame blocks = 0.5 ms) owns the clock and every
  time-critical decision; bounded work, no allocation, no blocking I/O, no file access. Analysis,
  arranging, files and lights run in the main loop.
- **Fixed memory:** every table has a fixed size known at compile time. No heap after start-up.
- **Deterministic:** the same seed, settings and arranger version give the same piece, so a piece is
  stored as its *recipe* (small), regenerated on load, and host tests can pin it byte for byte.
- **Never a stuck note:** every note-on sent is tracked until its note-off is sent; stop, panic, cutoff,
  a role swap and a power-down path all end what's sounding.
- **Hardware:** STM32H750 (Daisy), 282 KB code area, DTCM 128 KB, D2 SRAM 288 KB, SDRAM 64 MB; TRS MIDI
  (UART, 31,250 baud) and USB MIDI (device); 25 keys, 6 knobs, CHOMPI / PLAY / LOOP, the toggle, LEDs;
  the built-in mic; **no speaker**.

## 2. Overview

```
                 MAIN LOOP (~1 kHz)                                   AUDIO CALLBACK (2 kHz blocks)
 ┌──────────────────────────────────────────────┐       ┌──────────────────────────────────────────────┐
 │ panel input ─► phase state machine ──────────┼──cmd──►  CONDUCTOR                                    │
 │                                              │ queue │   clock (tempo, rubato, fermata)            │
 │ seed buffer ─► ANALYSIS ─► ARRANGER ─► SCORE ┼─swap──►   score reader (4 parts)                    │
 │                   (once per seed / setting)  │ (A/B) │   dynamics stack, articulation              │
 │ hand-mic features ◄──────────────────────────┼─feat──┤   note tracker (per channel)                │
 │ lights, Choowivision view, files (card)      │       │   internal synth voice (optional part)      │
 │ MIDI pump: UART DMA + USB  ◄─────────────────┼─midi──┤   hand-mic DSP (band filters, transients)   │
 └──────────────────────────────────────────────┘ queue └──────────────────────────────────────────────┘
```

Three single-producer / single-consumer queues (as Choowi's `SpscQueue`): **commands** main → audio
(start, stop, cue, cut, solo, swap, jump, mood, tempo...), **MIDI events** audio → main (time-stamped,
with priority), **features** audio → main (the mic's hand readings, the beat for the lights). The **score**
is double-buffered: the arranger writes the spare copy, then one atomic index flip hands it to the
conductor at a section boundary.

## 3. Data model

| Struct | Holds | Size (~) |
|---|---|---|
| `Seed` | up to 128 notes: pitch, velocity, start and length in ticks (96 PPQN); bars, meter, quantize | 1 KB |
| `Analysis` | key (tonic, mode), confidence, meter, one chord per half bar (degree, quality, inversion), the motif (first cell: pitches as scale steps + rhythm), phrase length | 0.2 KB |
| `Casting` | 4 instruments: output (TRS / USB / internal), MIDI channel, role, program, register (lowest / highest note), on / off | 64 B |
| `Prep` | key and mode override, chord richness, tempo, feel (straight / swing %), form length, mood, per-instrument level, brightness, reverb send, mood follow | 64 B |
| `Score` (x2) | ~6 sections x 4 parts x up to 8 bars, each part a list of events: step, pitch, velocity, length, accent flag | 2 x 12 KB |
| `Performance` | live: tempo target and current, pace, mood (current and target), per-instrument live level, character, cue / cut / solo state, queued jump, fermata | 0.2 KB |
| `NoteTracker` | per channel (16) x 128 notes: sounding count + which instrument | 2 KB |
| `HandFeatures` | openness 0-1, breath 0-1, tap event + time, height 0-1, speed (signed) | 32 B |

`Score` and `NoteTracker` live in D2 SRAM; the rest in DTCM. Total under 32 KB.

## 4. From seed to analysis

**Capture.** Toggle up: keys go to CHOMPI's synth (so the seed is heard) and into `Seed` with their tick
times. The take starts on the first note; it ends at the set length (2 / 4 / 8 bars) or on LOOP. Quantize
moves starts to the grid (1/8, 1/16, triplets) with a strength (default 80 %, to keep some feel).

**Key and mode.** Krumhansl-Schmuckler: a duration-weighted pitch-class histogram correlated with the 24
major / minor key profiles; the best match wins, the margin is the confidence. Below a threshold, or when
the owner set it on the key page, the prep's key is used. Modes beyond major / minor (dorian, lydian...)
come from the prep (Choowi's harmony knows all nine), not from detection.

**Meter.** The prep's meter (4/4, 3/4, 6/8); the downbeat is the first note unless an accent pattern
clearly says otherwise (later).

**Chords.** For every half bar, each diatonic chord of the key (Choowi's `harmony` table) is scored by how
many of that half bar's weighted notes it contains (downbeat notes and long notes weigh more); a small
bonus for common progressions (I-IV, IV-V, V-I, ii-V, vi-IV) and for keeping the previous chord. The
best path is chosen with a tiny dynamic program (7 chords x 16 half bars). The last bar is pulled towards
a cadence (V-I, or IV-I for gentle moods).

**Motif.** The seed's first rhythmic cell: notes up to the first long note or rest, at most 2 beats,
stored as scale steps from the chord root plus the rhythm, so it can move to any chord and key.

Cost: microseconds to a few ms, main loop, once per seed or setting change.

## 5. The arranger

**Form templates** (by the form length in the prep; bars per section scale with the seed length L):

| Form | Sections |
|---|---|
| Short (~1 min) | Intro (L/2) · A (L) · A′ (L) · Ending (1 bar) |
| Song (~2-3 min) | Intro · A · B · A′ · Outro |
| Long | Intro · A · B · A′ · Bridge (C) · A″ · Outro |

**What each section does to the material:**

| Section | Melody | Texture | Harmony |
|---|---|---|---|
| Intro | fragment of the motif, or none | 1-2 instruments (pad + rhythm), soft | the progression, maybe just I and V |
| A | the seed as played | everyone, the prep's balance | the analysed chords |
| B | **development**: sequence (the motif moved up a step per bar), then fragmentation | thinner; the counter-melody gets space | moves to the relative key or IV, comes back via V |
| A′ | the seed again, ornamented (passing notes, octave up in the last half) | fuller: everyone, louder | the analysed chords, a stronger cadence |
| Bridge (C) | inversion or augmentation of the motif | a new combination (melody to another instrument) | a contrasting area (vi or bVI) |
| Outro / Ending | the motif slowing (augmentation), last note held | thinning out one by one | I, final cadence, ritardando built in |

**Generators by role** (each writes one part for one section, inside the instrument's register):

- **Melody:** the section's melodic material (above); notes snapped into the chord or the scale; the
  phrase's last note on a chord tone.
- **Counter-melody:** answers the melody in its gaps (imitation of the motif a bar later, a third or sixth
  away), else moves in contrary motion; rests where the melody is busy; one register away from it.
- **Harmony / pad:** the chords, voiced with Choowi's voice leading (smooth inner voices), sustained, or
  re-struck on beats for brighter moods.
- **Bass:** roots on chord changes; fills by mood: root-fifth (march), walking steps between roots
  (moving moods), a pedal on the tonic (suspense, intros); the octave in the register's bottom.
- **Rhythm / ostinato:** the seed's rhythm (its onsets) as a repeating figure on chord tones, or an
  arpeggio pattern (Choowi's arp patterns) at the mood's rate. On channel 10, a General MIDI drum groove
  built from the same onsets (kick on strong onsets, snare on 2 and 4, hats on the grid) (open question).
- **Drone:** tonic and fifth, held through sections, re-struck at section starts.

Randomness only from a seeded generator (xorshift32 of the piece's seed number): "re-roll" changes the
number, nothing else. Cost: a few ms per piece in the main loop, done while the previous score plays.

## 6. Moods (cue bundles)

From the emotion research (mode strongest, then tempo, register, loudness, articulation, timbre; additive).
Values relative to the prep; morphing moves every cue together over a phrase.

| Mood | Mode | Tempo | Register | Loudness | Articulation (gate) | Brightness (CC 74) | Texture |
|---|---|---|---|---|---|---|---|
| Tender | major (lydian) | x0.85 | mid, narrow | soft (pp-mp) | legato 100 % | low | sparse, pad-led |
| Joyful | major | x1.15 | high | mf-f | light staccato 60 % | high | busy rhythm, bright |
| Solemn | minor (aeolian) | x0.75 | low | mp, steady | legato 95 % | low | chorale, drone |
| Heroic | major (mixolydian) | x1.0 | wide | f-ff | marcato 80 % + accents | high | everyone, bass octaves |
| Mysterious | minor (dorian / phrygian) | x0.9 | wide, sparse | pp-mp | mixed | mid | pedal bass, rests |
| Melancholy | minor | x0.85 | mid-low | p-mp | legato 90 % | low-mid | counter-melody forward |
| Stormy | harmonic minor | x1.25 | low + high | f-ff, swells | short 50 % | high | fast ostinato, tremolo |

A mood change of mode is applied at the next phrase (never mid-chord); the others glide. The hand's
openness and speed (CHOMPOSER.md) move a continuous point between these.

## 7. The conductor (real time)

**Clock.** 96 PPQN internally (fine enough for swing and rubato; MIDI clock out at 24 PPQN as Choowi's
`parts::Clock` sends it). `samples_per_tick = 60 * 48000 / (96 * tempo)`.

**Tempo model.** `tempo_now` glides to `tempo_target` (SW5, tap tempo, mood scale) at a set rate.
Accelerando / ritardando keys ramp the target. **Rubato:** within a marked phrase the tick rate follows a
gentle arch (a little faster into the peak, slower into the cadence) whose depth the prep sets; the phrase
still lands on time (the curve's integral is 1). **Fermata:** LOOP held stops tick advance after the
current note-ons; sounding notes keep sounding; release resumes from the same tick. **Tap tempo:** the
CHOMPI key (or a mic tap) sets the target from the median of the last 4 intervals; a tap far off the
beat is a new downbeat only when stopped.

**Scheduling.** Each block, for every part, events whose tick falls inside the block are emitted with the
sample offset (0.5 ms resolution). Commands are **quantized to musical boundaries**: cue in at the next
bar (the part enters with a one-beat pickup if its material has one), cutoff at the end of the part's
current phrase (or next beat with the knob held), role swap and section jump at the next phrase.

**Dynamics stack** (per note):
`velocity = clamp(score_velocity x mood_loudness x prep_level x live_level x shape_arc(phrase_pos) x master)`.
Sustained parts also get **CC 11** (expression) following the same stack, rate-limited to one message per
20 ms per instrument and only when it changes by 2 or more. Crescendo / diminuendo keys ramp `shape`.

**Articulation.** Gate = note length x mood gate x live character (push + turn); marcato adds +12
velocity on beats; legato overlaps the next note by a tick for instruments that glide.

**Note tracker.** Every note-on increments `tracker[channel][note]` and records the instrument; note-offs
decrement. Stop / panic / cutoff / swap send note-offs for exactly what's tracked, then CC 123 (all notes
off) as a backstop. The internal synth has its own voices and the same rule.

## 8. MIDI out

**Today in Choowi** (`SendPanelMidi`): up to 8 messages per main-loop pass, each sent with a **blocking**
3-byte UART transmit (~1 ms at 31,250 baud), and USB waiting up to 20 ms for the endpoint. Fine for an arp;
too slow and jittery for an ensemble.

**Chomposer's output layer:**
- **UART by DMA** from a ring buffer (no blocking), with **running status** (repeated note messages on one
  channel drop the status byte: 2 bytes = 0.64 ms instead of 0.96 ms).
- **Priority on a crowded beat:** note-offs first (they free voices and never smear audibly), then bass
  and melody note-ons, then harmony, rhythm, then CC. A full downbeat (4 instruments x up to 5 notes)
  costs ~13-19 ms on one TRS line: spread across the beat by priority it stays musical, but **the score
  generator keeps chords on TRS to 3-4 notes** and offsets pad re-strikes by a tick.
- **Bandwidth:** TRS carries ~1,000-1,500 messages/s; a dense piece is ~60-150/s plus expression: plenty,
  outside the bursts.
- **USB** in parallel (fast; no burst problem) for a computer, a DAW, or a USB host box.
- **The internal synth** is an output like the others (instrument "CHOMPI").
- **Per instrument:** program change and bank select (CC 0 / 32) at casting and at load; CC 7 for the prep
  level once; CC 11 live.
- **MIDI in:** optional clock follow (an external drum machine leads the tempo; `parts::Clock` already
  follows MIDI clock); start / stop / continue; nothing else is needed from the instruments.

## 9. Panel and phases

A small state machine: **Seed** (toggle up) → **Casting / Prep** (toggle down, menu open, KEY_21 steps
pages) → **Performance** (menu closed). Leaving Seed with a new take re-runs analysis and arranging in
the background; the old score keeps playing until the new one is ready (swapped at a section boundary).
The control map and lights are in CHOMPOSER.md; the panel code reuses Choowi's `PanelController`
patterns (hold / tap / combo detection, knob pages and rings, menu pages, flashes).

## 10. Hand-mic module

Mic channel 0 (only when nothing is in line in). Per block in the audio callback: 4 band energies
(Goertzel or one-pole band-passes: 100-300 Hz, 300 Hz-1 kHz, 1-4 kHz, 4-12 kHz), a peak / RMS transient
detector (taps), slow noise-floor tracking. Every ~20 ms (main loop, from a 1,024-sample ring): the
**seashell theremin's** cepstrum (a 1,024-point real FFT, log magnitude, inverse FFT; the peak between 0.1
and 1.5 ms is the hand's echo delay → height). Outputs `HandFeatures`: **openness** (high-band / low-band
ratio against the room's calibrated level), **breath** (low-band turbulence above floor), **tap** events,
**height** and **speed**. Mapping (main loop): openness → brightness and softening, breath → swell,
taps → tap tempo, height / speed → dynamics and the mood point. A calibration at start (2 s of room).
Cost: ~2-5 % CPU for the bands, the FFT every 20 ms ~1-2 % (to measure); ~6 KB RAM.

## 11. Files on the card

Chomposer keeps its files in **its own folder** (`/CHOMPOSR/`, launcher-friendly, as the community
convention asks). A **piece** is a small JSON file, versioned like Choowi's patches:

```json
{"chomposer_piece": 1, "arranger": 1, "seed_number": 48213,
 "seed": {"bars": 4, "meter": "4/4", "notes": [[60, 96, 0, 48], [64, 80, 48, 48]]},
 "casting": [{"output": "trs", "channel": 1, "role": "melody", "program": 41, "low": 55, "high": 84}],
 "prep": {"key": "D", "mode": "dorian", "tempo": 92, "swing": 0, "form": "song", "mood": "melancholy",
          "levels": [100, 80, 70, 90], "brightness": [64, 64, 50, 40]}}
```

The score is regenerated from it (deterministic), so files stay tiny and a newer arranger can re-arrange
an old seed (the `arranger` field keeps the old one reproducible when kept in the code). Slots like
Choowi's presets: banks x 15 on the panel's keys.

## 12. Choowivision and the AI page

- **View** (as Choowi's opcode 0D, a new page): phase, form and current section / bar, the 4 instruments
  (role, level, playing / resting / cued), tempo, mood, the hand features. Choowivision draws it as a
  conductor's score: sections across, instruments as lanes.
- **AI page:** the piece JSON above is the contract. "A slow, hopeful piece for strings, flute and bass"
  → the chatbot writes casting and prep (and optionally a seed); the page validates it and sends it over
  MIDI SysEx, as Choowi's forge page does with sounds.

## 13. Code layout

```
firmware/
  chompi-forge/            Choowi (unchanged)
  chomposer/               the new line
    core/   seed.h analysis.h arranger.h moods.h conductor.h midi_out.h note_tracker.h hand_mic.h piece.h
    src/    chomposer_main.cpp, Makefile, linker script (copy of Forge's, BACKUP_SRAM fix included)
    tests/  analysis_test arranger_test conductor_test midi_out_test hand_mic_test piece_test
```

**Shared code:** reuse Choowi's `harmony.h`, `parts.h` (clock, arp patterns), `midi_framer.h`,
`protocol.h` framing, `usb_packets.h`, the panel helpers and the synth, by including them from
`chompi-forge/core` at first; move them into a shared `forge-core/` only when both lines need changes, so
Choowi stays untouched (one change at a time, Choowi's tests guarding it).

## 14. Testing

- **Analysis:** known seeds → expected key, chords, motif (golden tables); ambiguous seeds fall back to the
  prep's key.
- **Arranger:** a fixed recipe → a byte-identical score (golden hash per form and mood); every note inside
  the instrument's register; no two parts on the same note in the same register at once (unless unison
  is intended); every section ends on a chord tone.
- **Conductor:** simulated blocks: tempo glides and rubato land phrases on time (± 1 tick); fermata holds
  exactly; cues enter on the bar; **no stuck notes** after any sequence of commands (a fuzz test of random
  commands, then stop: the tracker is empty).
- **MIDI out:** byte stream with running status; priority order on bursts; worst-case downbeat time.
- **Hand-mic:** recorded mic clips (cup, breath, taps, a hand rising) → feature curves within bounds.
- **Sanitizers** (UBSan as Choowi's DSP); the ARM build with a layout check; one consolidated hardware
  session per milestone (instruments on TRS and USB, the internal synth, the mic).

## 15. Resource estimates

| Part | Code (~) | RAM (~) | CPU (~) |
|---|---|---|---|
| Seed + analysis | 6 KB | 1.5 KB | main loop only |
| Arranger + moods | 15-20 KB | 24 KB (two scores) | main loop only |
| Conductor + note tracker | 8 KB | 2.5 KB | < 2 % |
| MIDI out (DMA, priority, USB) | 4 KB | 2 KB | < 1 % |
| Hand-mic | 6 KB | 6 KB | 3-7 % |
| Panel, phases, lights, files, view | 20-25 KB | 2 KB | small |
| Internal synth (reused) | as Choowi | as Choowi | as Choowi (fewer voices) |
| **Chomposer-specific total** | **~60-70 KB** | **~40 KB** | **~10 %** plus the synth |

With the sampler, looper and most of Choowi's FX left out, the line fits the 282 KB code area with room
to spare.

## 16. Milestone 1 (first playable), acceptance

1. Toggle up, play 4 bars: CHOMPI hears it; LOOP or bar 4 closes the take; PLAY plays it back.
2. Analysis finds the key of a plain C-major or A-minor seed (host test) and a sensible chord per half bar.
3. The arranger writes Intro · A · A′ · Ending for melody, harmony, bass and rhythm (host golden test).
4. Casting by the panel: instrument 1 = CHOMPI's synth; instruments 2-4 on TRS channels 2-4.
5. PLAY: a one-bar count (CHOMPI light pulsing), then the piece; SW5 tempo; PLAY again stops at the phrase
   end; nothing stuck afterwards (host fuzz test and on hardware).
6. SW1-SW4 turn: per-instrument dynamics, heard on hardware.

Everything else (cues, moods, the mic, Choowivision, the AI page) builds on this.

## 17. Risks

- **TRS bursts** on dense downbeats (section 8): mitigated by priority, running status and thinner chords;
  measure with a logic analyser or a MIDI monitor.
- **Arranger taste:** rules can sound mechanical; mood bundles and the owner's ear decide, so the rules
  stay tables that are easy to tune.
- **External instruments differ** (velocity curves, CC support, polyphony): casting stores a register and
  a polyphony limit; CC 74 / 11 are optional per instrument.
- **The mic:** room noise varies; calibration and an off switch; the theremin needs proving on hardware.
- **Scope:** a new firmware line is large; milestone 1 must be small and playable.
