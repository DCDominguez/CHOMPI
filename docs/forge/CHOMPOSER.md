# Chomposer: design study (Forge line #2, planned)

Status: **design only, 2026-10-14.** The build-level architecture (data model, arranger rules, mood
numbers, the conductor's timing, MIDI output, files, tests, milestone 1): [CHOMPOSER_ARCHITECTURE.md](CHOMPOSER_ARCHITECTURE.md). Nothing built. The owner's brief, then a proposed architecture,
a control map for CHOMPI's panel, and the conducting research it rests on. Everything below is a
proposal to settle with the owner; decisions are marked **(owner)** when they are his.

## The brief (owner)

- CHOMPI is an **orchestra conductor**. Instruments are connected over **MIDI**; CHOMPI talks to each.
- **Toggle up and play on the keys: the musical seed.** The seed is interpreted as a musical section,
  divided into **song parts**, for each instrument.
- **Up to 4 instruments**, assigned by **MIDI channel**.
- **Preparation** (after the seed and the casting): set **tone, tempo, mood and balance**.
- **Performance**: control **tempo, pace, shape, emotion** and more, live.
- Every knob, key and button conducts the orchestra.

## What a conductor does (research summary)

The conductor's job is to unify many separate parts into one piece, deciding what stands out and what
stays behind, without playing a note. Two halves, which map straight onto the brief's prep and
performance:

**Before: score study and rehearsal.** Learn the piece completely (form, where the melody passes between
sections, how the accompaniment supports it), decide balance, phrasing and breathing, and mark where
tempo changes. This is Chomposer's **seed → casting → preparation**.

**During: the gestures.**

| Conductor technique | What it does |
|---|---|
| **Ictus** (the beat point) and the **preparatory beat** | The exact moment a beat lands; one prep beat before the first, in the new tempo and style |
| **Beat pattern, tempo** | Tempo is the spacing between ictuses; accelerando / ritardando by changing it gradually |
| **Gesture size** | Dynamics: big = loud, small = soft; palm up = crescendo, palm down = diminuendo |
| **Ictus character** | Articulation: sharp = staccato, flowing = legato, tense = marcato |
| **Left hand** | Independent of the beat: cues, crescendo / diminuendo, balance, holds, cutoffs |
| **Cue** | Eye contact + a prep gesture to a section + a clear ictus on its entrance |
| **Cutoff** | A tempo-matched release; the only clear ictus lands on the note's end |
| **Fermata** | Time stops on a held note, then the conductor releases or continues |
| **Rubato** | Tempo bends within a phrase and comes back |
| **Phrasing / shape** | The arc of a phrase: where it breathes, where it peaks |
| **Balance / blend** | Which part leads at each moment; the rest supports |
| **Tutti / grand pause** | Everyone together; everyone silent |

**Emotion in music** (Juslin and others): listeners read emotion mostly from **mode** (strongest), then
**tempo, register, loudness, articulation and timbre**, and the cues add up rather than interact. High
loudness and pitch read as high-arousal emotions (joy, anger); slow tempo, legato and soft timbre as
low-arousal ones (sadness, calm). That gives Chomposer a concrete recipe: **a mood is a bundle of cue
settings**, and moving between moods is moving those settings together.

**Orchestration roles** (what each instrument can be): **melody** (the tune), **counter-melody** (a second
line that fits, contrasting in register, rhythm or articulation), **harmony / pad** (sustained chords),
**bass** (the lowest line; roots, walking, pedal), **rhythm / ostinato** (a short repeated figure,
arpeggio or groove), **drone / pedal** (long held notes). A melody stays clear when the accompaniment is
lower, simpler, softer or resting.

**Motif development** (how one seed becomes a piece): repetition, **sequence** (the motif moved step by
step), **inversion** (intervals flipped), **retrograde** (backwards), **augmentation / diminution**
(longer / shorter notes), **fragmentation** (a piece of it), **imitation** (another voice answers it).

## Architecture

```
 keys ──► SEED CAPTURE ──► ANALYSIS ──► ARRANGER ──────► SCORE (in RAM, on card) ──► CONDUCTOR ──► MIDI OUT (TRS + USB)
 (toggle up)  notes, timing   key/mode,    form + 4 parts    sections x parts x steps     clock, tempo,        ch 1-16, up to 4
                              meter,       by role, from     (event lists)                rubato, cues,        instruments, plus
                              chords,      the motif                                       dynamics, mood       CHOMPI's own synth
                              motif                              ▲                         ▲
                                                                 │                         │
                                                CASTING + PREP (menu pages)         PERFORMANCE (panel, live)
```

1. **Seed capture** (toggle up). The keys record a short phrase: notes, velocities, timing, quantized to a
   grid. A set length (2 / 4 / 8 bars) or a LOOP tap closes it, as Choowi's looper does. PLAY auditions it.
2. **Analysis** (main loop, once). Key and mode (a key-profile match, or the key the owner sets), meter, a
   chord progression under the seed (Choowi's harmony engine already voices chords in a key), and the
   **motif** (the seed's first rhythmic-melodic cell).
3. **Arranger** (main loop). Builds a **form** from the seed: for example **Intro · A (statement) ·
   B (development: sequence, inversion, fragmentation) · A′ (return, fuller) · Outro**, each a number of
   bars. For every section it writes each instrument's part **from its role**: the melody plays the seed and
   its variations; the counter-melody imitates or moves in contrary motion; the harmony part voices the
   chords; the bass plays roots, a walking line or a pedal; the rhythm part turns the seed's rhythm into an
   ostinato or arpeggio. Deterministic from a seed number, so a piece can be recalled and re-rolled.
4. **Score.** Compact event lists: 4 parts x ~6 sections x 8 bars x 16 steps is about 3,000 steps, ~12 KB.
   Saved on the card with the casting and prep as a **piece** (like a preset).
5. **Conductor** (the real-time part). A clock with tempo, rubato and fermata; for each part it reads the
   score and renders MIDI: notes with velocity and gate length (articulation), CC 7 / 11 (level and
   expression), CC 1 and 74 (intensity and brightness, where the instrument supports them), program
   changes at casting, all-notes-off on cutoffs. **Mood** sets the cue bundle (mode, register, tempo scale,
   loudness, articulation, brightness); **shape** sets the phrase arc and how many layers play.
6. **MIDI out.** CHOMPI already has **TRS MIDI** (UART) and **USB MIDI**, both driven by Choowi's MIDI out
   today. Four instruments on four channels share one TRS output through a MIDI thru chain or a splitter;
   USB reaches a computer / DAW. **CHOMPI's own synth** can be one of the four (an internal channel), so it
   works with fewer instruments, or none. CHOMPI's USB port is a device port, so USB-only synths would
   need a computer or a USB MIDI host box in between (to confirm on hardware, along with the TRS type,
   A or B).

**Real-time rules** (as Choowi): analysis and arranging happen in the main loop, never in the audio
callback; the conductor's clock and MIDI rendering are bounded per block, with no allocation and no
blocking I/O; the score is fixed-size memory.

**What it reuses from Choowi:** the harmony engine (keys, modes, chord voicing), the parts clock and arp
(tempo, swing, patterns), MIDI out on TRS and USB, the synth as the internal instrument, presets on the
card, the panel framework (knob pages, menu pages, lights), Choowivision's view over USB, and Dream (an
idle Chomposer could keep composing variations of the last seed by itself).

**Resources:** a separate firmware gets the whole 282 KB code area, not Choowi's 34 KB of headroom. It
doesn't need the sampler, the looper or most of the FX section, which frees a large share; the score fits
in fast RAM.

## The phases

| Phase | Where | What happens |
|---|---|---|
| **1. Seed** | toggle **up** | Play the idea on the keys; it's captured and analysed |
| **2. Casting** | toggle down, menu: casting page | Up to 4 instruments: MIDI channel, role, register, program |
| **3. Preparation** | menu: tone / tempo / mood / balance pages | The rehearsal: how the piece should sound |
| **4. Performance** | toggle down, menu closed | Conduct it live |

## Control map (proposal)

CHOMPI's panel: 25 keys (C3-C5, 15 white and 10 black), the CHOMPI key, PLAY and LOOP, the toggle,
knobs SW1-SW4 (turn, push, push + turn, with rings), SW5 (turn, push) and SW6 (volume, push).

**The idea that ties it together: the four knobs are the four instruments.** Each knob's ring takes its
instrument's colour; in every phase, SW1-SW4 speak to instruments 1-4. That's the conductor's left hand.

### 1. Seed (toggle up)

| Control | Does |
|---|---|
| Keys | Play the seed (heard on CHOMPI's synth while you play) |
| LOOP | Close the seed now (otherwise it closes at the set length) |
| PLAY | Hear the seed back |
| Hold LOOP 2 s | Clear the seed |
| SW1 | Length: 2 / 4 / 8 bars |
| SW2 | Quantize: off, 1/8, 1/16, triplets |
| SW3 | Meter: 4/4, 3/4, 6/8 |
| SW4 | Key: found from the seed, or set by hand |

### 2. Casting (menu, first page)

| Control | Does |
|---|---|
| C3 · D3 · E3 · F3 | Pick instrument 1-4 (its knob ring lights) |
| Upper white keys | That instrument's **role**: melody, counter-melody, harmony / pad, bass, rhythm / ostinato, drone |
| SW1-SW4 (on the picked instrument) | MIDI channel (1-16, or "CHOMPI's own synth"), program number, register (octave), on / off |
| PLAY | Audition the picked instrument's part alone |

### 3. Preparation (menu pages, KEY_21 steps through them, as in Choowi)

| Page | SW1-SW4 | Keys |
|---|---|---|
| **Tone** | per instrument: brightness (CC 74), attack feel, reverb send (CC 91) | key and mode (the 9 modes), chord richness (triads to 9ths) |
| **Tempo** | — | (SW5) BPM; keys: feel (straight, swing), and the form's length |
| **Mood** | per instrument: how far it follows the mood | the mood palette: tender, joyful, solemn, heroic, mysterious, melancholy, stormy |
| **Balance** | **level of each instrument** (CC 7) | which instrument leads in each section |

### 4. Performance (toggle down, menu closed)

| Control | Conducts | Conductor equivalent |
|---|---|---|
| **PLAY** | Start: a one-bar count (lights pulse), then the downbeat; again: stop at the end of the phrase | the preparatory beat, the final cutoff |
| **CHOMPI key, tapped in time** | **The beat**: tempo follows your taps | the ictus |
| **SW5** | Tempo; **push + turn: pace** (half time, normal, double time feel) | beat spacing; subdivision |
| **SW1-SW4, turn** | That instrument's dynamics | the left hand on a section |
| **SW1-SW4, push** | **Cue** it in (enters at the next bar, after a pickup) or **cut it off** | cue / cutoff |
| **SW1-SW4, hold** | Solo it (the others drop back) | balance: bring a line forward |
| **SW6** | The whole orchestra's dynamics | gesture size |
| **White keys C3-B3** | **Jump to a song part** (intro, A, B, A′, bridge, outro, ending), queued for the end of the phrase | the score's road map |
| **White keys C4-B4** | **Emotion**: the 7 moods, morphing over a phrase | the character of the gesture |
| **Black keys (lower)** | **Shape**: legato, staccato, marcato, accent; held: crescendo / diminuendo | ictus character; palm up / down |
| **Black keys (upper)** | Held: accelerando / ritardando; tap: rubato on the next phrase | tempo bending |
| **LOOP, held** | **Fermata**: time holds on the current chord; release to go on | fermata |
| **C5** | Tutti (everyone in); held: grand pause | tutti / G.P. |

### The knobs in detail (owner: "how about the knobs")

**SW1-SW4 are instruments 1-4, always.** One gesture vocabulary in performance:

| Gesture | Does | Conductor equivalent |
|---|---|---|
| **Turn** | that instrument's dynamics (smooth, no steps) | the left hand towards a section |
| **Push + turn** | its **character**: legato ↔ staccato, and brightness with it (CC 74) | the shape of the gesture to that section |
| **Tap** | **cue** it in at the next bar (after a pickup), or **cut it off** at the end of its phrase | cue / cutoff |
| **Hold 1 s** | **solo**: it leads, the others drop back (released: back to the balance) | bringing a line forward |
| **Push two knobs together** | **swap their roles**: the melody passes from one instrument to the other at the next phrase | the melody passing between sections |
| **SW5 turn / push + turn / tap** | tempo / pace (half, normal, double time feel) / back to the prepared tempo | beat spacing, subdivision |
| **SW6** | the whole orchestra's dynamics (held 2 s: the battery, as Choowi) | gesture size |

**What the rings show (feedback):**
- **Colour = the instrument's role**, from CHOMPI's nine: melody pink, counter-melody teal, harmony purple,
  bass blue, rhythm orange, drone green. A role swap visibly moves the colours between knobs.
- **Brightness = its level**, and it **flickers with every note it plays**, so you see who is playing
  and how busy they are, at a glance.
- **Resting (tacet):** a slow, dim breath. **Cue waiting:** it blinks in time until its entrance lands.
  **Cut off:** a short white flash, then the rest breath. **Solo:** it brightens, the other three dim.
- **Character (push + turn):** the ring turns white while held, bright = legato, pulsing short = staccato.
- **SW5's two lights tick the beat** left / right like a metronome; faster when you push the tempo.
- In the prep pages the rings show the page's value (level, brightness, mood follow) the way Choowi's
  knob pages do; in the seed phase they show the step (length, quantize, meter, key) in the step colours.

**Lights:** the CHOMPI light pulses on every beat (the ictus); knob rings show each instrument's colour
and level, dim when it rests; the lower white keys show the form (the current part bright, the queued one
blinking); the mood keys glow in their colour; PLAY / LOOP as the transport and the fermata.

**Choowivision as the score:** the phone shows the form across the top, the four instruments as lanes
with their roles, the current bar, tempo and mood: what a conductor reads on the stand.

## The microphone: conducting with your hand (owner: "something unconventional")

CHOMPI has a built-in mic and **no speaker** (the owner). Chomposer can still turn the mic into a sensor: **the hand near the mic is the conductor's left
hand**, with no new hardware. Four things the mic can tell apart, cheapest first:

| Hand | What the mic hears | Conducts | Conductor equivalent |
|---|---|---|---|
| **Cupping the mic** | the room's highs drop first; how closed = how much | the orchestra darkens and softens, like a mute; open the hand and it blooms | a closing hand: softer, hold back |
| **Breathing on it** | low, rough turbulence | a swell: every part crescendos together (as a breath controller, CC 2) | the breath before a phrase, a big gesture |
| **Tapping the case** | sharp clicks | **the beat**: tempo follows the taps | the ictus |
| **Hovering / waving over it** (the seashell theremin, below) | the room noise's comb pattern: notch spacing = distance, its change = speed | **gesture size and direction**: towards = louder / forwards, away = softer / held; a fast flutter = agitato (tremolo, tighter articulation) | beat size, expressive shaping |

**The seashell theremin (the unconventional part; owner: "I do like the theremin idea").** Hold a shell to
your ear and you hear the room's own faint noise, coloured by the shell. A hand over the mic does the same:
the room noise bouncing off the palm reaches the mic a fraction of a millisecond late, and the direct and
reflected sound together cut **evenly spaced notches** into the noise (a comb filter). The spacing is the
speed of sound over twice the distance: about 3.4 kHz with the hand 5 cm away, 860 Hz at 20 cm. Measuring
that spacing (a cepstrum: the FFT of the spectrum's log, which turns evenly spaced notches into one peak at
the echo delay) gives **the hand's height over the mic**, and its change over time gives the speed: a
theremin with no antenna and no speaker. It needs some room noise to work with; in a silent room a soft
"shhh", rubbing fingers, or the orchestra itself through headphones / monitors gives it something to hear.
**To prove first:** record 10 s of a hand rising and falling over CHOMPI's mic (CHOMPI's line out into the
audio interface while recording) and analyse it offline; if the delay peak tracks the hand, build it.

**Emotion from the hand:** the motion's **speed** is arousal (still = calm, fast flutter = stormy) and its
**openness** is brightness and warmth (cupped = dark, intimate; open = bright, bold). Together they move
the mood the way the mood keys do, but continuously: slow open-and-close is tender, a closed still hand is
suspense (held like a fermata), an open fast wave is triumphant.

**Lights (the feedback that you're being heard):**
- **Keys: a radar.** The closer the hand, the more keys light outward from the mic's end of the keyboard,
  in the current mood's colour; a wave sends a ripple across them.
- **Rings: breath and openness.** A breath swells all four rings together; cupping dims them as if the hand
  were closing over the light.
- **CHOMPI light: the beat.** A flash on every tap the mic counts, so you know the ictus landed.
- **Listening the room:** at the start the keys sweep slowly while CHOMPI measures the room's quiet level,
  so it still works in a noisy room; a red flicker if it's too loud to hear the hand.
- **Choowivision:** a live "hand" meter (distance, openness, speed) beside the score.

**How it's built:** a few band filters on the mic (Goertzel filters on 3-4 bands, a transient detector, and
a 1,024-point FFT cepstrum about every 20 ms for the theremin): modest CPU (to measure), a few KB of state. Bounded in the audio
callback; the mapping to mood and dynamics in the main loop. The mic only works with nothing plugged into
line in (as today), and the hand control has an on / off (a long press, so it never surprises).

**Could come to Choowi first** as a secret: cupping the mic as a filter / mute on the synth, a breath as a
swell, taps as tap tempo for the arp.

## Building it (milestones)

1. **One seed, four parts, fixed form**: seed capture, analysis, a simple arranger (melody, harmony, bass,
   rhythm), MIDI out on 4 channels, start / stop, tempo. The internal synth as instrument 1, so it plays
   with nothing connected.
2. **Live conducting**: per-instrument dynamics, cues and cutoffs, tap tempo, fermata, SW6 dynamics.
3. **Song parts and development**: the form keys, sequence / inversion / fragmentation in the B section.
4. **Moods and shape**: the cue bundles, articulation, crescendo / accelerando keys.
5. **Preparation pages and pieces on the card.**
6. **Choowivision score view; the AI page** (describe a piece, "a sad waltz for strings", and the chatbot
   writes the casting and prep, as Choowi's forge page writes sounds).

Each milestone is playable on its own.

## Open questions for the owner

- **The mascot**, and the first tool.
- **Up to 4 external instruments, plus CHOMPI's synth as a fifth?** Or is CHOMPI's synth one of the four?
- **How much the arranger writes vs. how literal the seed stays**: a faithful orchestration of the seed,
  or freer development?
- **Drums**: is "rhythm" a melodic ostinato, or a drum part on channel 10 (General MIDI drums)?
- **Fixed form or growing form**: does the piece have an end, or does it keep developing until you stop?
- **MIDI in**: should CHOMPI follow an external clock (a drum machine's tempo), or always lead?
- **Choowi first?** Some of this (the knob = instrument idea, cues over MIDI) could be prototyped as a
  Choowi secret before the new line exists.

## Sources (research, 2026-10-14)

- Conducting technique: [Conducting study guide (RemNote)](https://www.remnote.com/learn/arts-and-humanities/performing-arts-and-media/music/conducting/conducting-study-guide);
  [Expression, tempo changes and cueing (Praxis study guide)](https://open-exam-prep.com/study-guides/praxis-music/performance-conducting/conducting-expression-cues);
  [Conducting from the flutist's chair (The Instrumentalist)](https://theinstrumentalist.com/?p=6071);
  [Conducting self-evaluation (SJSU)](https://phones.sjsu.edu/people/jeffrey.benson/courses/conducting/s0/Self-Evaluation-Form-1.pdf).
- Score study and rehearsal: [Score study handout (SJSU)](https://pdp.sjsu.edu/people/jeffrey.benson/courses/conducting/s0/Score-Study-handout.pdf);
  [Music ensembles course (UNC, Coursera)](https://www.coursera.org/learn/music-ensembles);
  [Fundamentals of Instrumental and Choral Conducting (Routledge)](https://www.routledge.com/Fundamentals-of-Instrumental-and-Choral-Conducting/Giray-Hanawalt/p/book/9781032268798).
- Emotion cues: [Eerola, Friberg and Bresin 2013, Frontiers in Psychology](https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2013.00487/full)
  (re-tests Juslin and Lindström 2010: mode first, then tempo, register, dynamics, articulation, timbre; additive).
- Texture and development: [Developmental techniques (Ithaca College)](https://musictech.ithaca.edu/MusicTech/Form/Handouts/DevTech.html);
  [Motivic manipulations (Ithaca College)](https://musictech.ithaca.edu/MusicTech/ICTheory/OnlineText/Form/Unit%20I/MelManipulation/MelManipulation.html);
  [Texture (Tiverton High School)](https://tiverton.devon.sch.uk/curriculum/knowledge_organisers/year_10/music/Musical%20Elements/Texture-2.pdf).
  The owner's pick: [What do orchestra conductors ACTUALLY do? (Mixed Signals, YouTube)](https://www.youtube.com/watch?v=3Osx3JsCS9s).
  Standard texts worth reading in full: Max Rudolf, *The Grammar of Conducting*; Adler, *The Study of Orchestration*.
