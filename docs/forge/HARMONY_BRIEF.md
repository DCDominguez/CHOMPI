# Harmony / Intent Engine — brief

Status (2026-10-05): **Phase 1 (0.13) and Phase 2 (0.14: clock, arp, bass) implemented and
software-tested** (not yet on hardware: TEST_SESSION 3.65–3.72). See "Phase 2 as built".
The paragraph below is the original filing note.

Original status: **queued, not started.** DC brought this brief over from a ChatGPT
brainstorm on 2026-10-05, to run **after the safety additions are finished**. Nothing
below is implemented. When it starts, Phase 0 (architecture and the first deliverable
at the end) comes before any large code change.

Notes added when the brief was filed (2026-10-05, branch head `9ea9fcd`):
- The brief's resource figures are out of date. Measured at 0.12: SRAM_EXEC 282 KiB
  (288,768 B); release 261,072 B (headroom 27,696 B), development 277,028 B
  (headroom 11,740 B); SDRAM 393,200 B free. Re-measure before starting.
- Panel mapping must respect what 0.10–0.12 gave the controls (KNOBS.md,
  TAPE_CONTROLS.md, MANUAL sections 4–8), including the per-slot settings in 0.12.
- Clean-room: O'PIAN is AGPL-3.0, so use it only as a behavioural reference and copy
  no code. Name any source used beyond general music theory in the report.

Review of the brief against the code (2026-10-05; details in Phase 0):
- **No clock exists.** Forge ignores MIDI real-time bytes (`midi_framer.h`) and has no
  tempo; the looper times itself by loop length. The arpeggiator and the "tempo /
  clock phase" part of MusicalState need a small new clock (internal BPM, later MIDI
  clock in). The brief's "reuse existing timing" can't apply.
- **One instrument.** Forge plays one patch with 4 oscillator voices (v1–v3) or up to 7
  (v4). Internal roles (chord + bass + arp) share that sound and those voices; separate
  sounds per role only exist on MIDI out (per-channel). 9th–13th chords need the
  note-dropping rules before they reach 4 voices.
- **Note ownership.** Each held key must remember exactly the notes it started, so
  key-up, voice-led chord changes, mode change and panic can't leave notes stuck.
  Phase 1 needs this, not Phase 4.
- **The keybed fits Static mode.** The 15 white keys (C3–C5) can map to degrees I–vii
  over two octaves, and the black keys to the chromatic functions (V/ii, V/iii, V/V,
  V/vi…). Kit mode keeps its pads (harmony off).
- **Phase 1 needs some panel access**, or the one consolidated hardware test can only
  reach it over MIDI/the host. Suggest a harmony on/off mode and tonic/mode selection in
  the menu in Phase 1, with the full mapping in Phase 5.
- **Presets:** storing harmony settings means patch v6 (wire, host, webapp, AI, starter
  presets); v1–v5 stay valid. **Inspector:** one new compact page (≤ 100 bytes).
- **Real mode** needs one definition from DC: the key pressed is the chord root and the
  scale decides the chord quality (a key outside the scale → a chromatic function).
- **Size:** at 11,740 B of development headroom, Phases 1–3 should fit (a few KB of
  integer code, tables tiny); bass/arp/clock and protocol may need another round of
  `FORGE_COLD` trimming. Measure after each phase.
- Avoid "Nopia" in user-facing text (trademark); "harmony mode" instead.

## Phase 0 decisions (DC, 2026-10-05)

Status: Phase 1 started after the 0.12 safety extras (`715b1e3`); done in 0.13
(see "Phase 1 as built" after the plan).
- **Entry:** harmony on/off from a menu page, and presets can store it (patch v6).
  Off = the keybed plays notes exactly as before.
- **Static layout:** white keys C3–B4 = degrees I–vii twice (lower and upper
  register); black keys = chromatic chords (secondary dominants V/ii, V/iii, V/V,
  V/vi and borrowed chords); C5 = Shift (hold).
- **Real layout:** the key pressed is the chord's root; the key/mode decides the
  quality; a root outside the scale gives a chromatic chord.
- **Output:** internal sound plus the same notes on MIDI out; per-part routing comes
  with bass/arp.

Plan (Phase 1 = 0.13, one hardware test):
- `core/harmony.h`: 9 mode interval tables (incl. Locrian), chords by stacked thirds
  (fifth, triad, 7, 9, 11, 13; drop order root, 3rd, 7th, top extension, 5th, rest),
  Static/Real mapping, chromatic table, context-dependent Shift, inversion/spread,
  greedy voice leading (≤ 5 inversions × 3 octaves), MIDI 0–127 clamp; integer only.
- Engine: resolves on note-on/off only; each held key keeps its notes (≤ 5) and a
  per-pitch count, so overlapping chords, key-up, mode change, harmony off and panic
  never leave notes stuck; panel MIDI out sends the chord notes.
- Panel: menu harmony page (hold KEY_21 1 s, as KEY_22's hold opens presets): keys
  show the scale (tonic bright, scale dim); SW4 tonic, SW1 mode, SW2 extension, SW3
  inversion; presses SW4 on/off, SW1 Static/Real, SW2 voice leading, SW3 spread.
- Inspector: one compact page with the state and the last voiced chord.
- Patch v6 (presets, host, webapp, AI) right after the core, in the same 0.13.
- Phase 2: clock, bass, arp, per-part routing, seed use.

Phase 1 as built (0.13, software-tested only):
- One change from the plan: on the harmony page the **keys** set the tonic (a
  direct pick instead of 12 clicks), so the knobs are SW4 mode, SW1 chord size,
  SW2 inversion (3 clicks per step); presses as planned. The key lights show the
  tonic white and the scale blue. MANUAL section 8a.
- Patch v6 = v5 + a 17-bit harmony word (request bytes 87–89; PROTOCOL.md). Status
  replies and presets saved on CHOMPI carry the live settings (a v3–v5 patch with
  harmony on is reported as v6). Host, webapp *Harmony* group and AI schema use it.
- Inspector page 8 (38 bytes): state, the last chord (root, degree, kind, quality,
  Shift, notes), notes sounding, chords played; the host names the chord (e.g.
  *Am7 · I · tonic*).
- Resources: release 217,104 B (+1,944 over the post-savings `77fb5a7`; +6.7 KB
  harmony core before the savings passes), development 229,768 B; SRAM_EXEC
  headroom 71,664 / 59,000 B; no SDRAM or audio-buffer change; bench unchanged
  (harmony runs at note on/off on the main loop, not per sample). Tests:
  `harmony_test` (identity over 12 × 9 × 7 × 6, layouts, Shift, voicing, 200k
  random ownership events, engine/panel/MIDI, the harmony page), v6 protocol and
  host round trips, Inspector page 8, webapp controls; sanitizers.

Phase 2 decisions (DC, 2026-10-05): tempo internal + follow MIDI clock + send it, and set
on CHOMPI ("can we also set the tempo on the chompi"); a parts page next to the harmony page
with an arp latch; bass on the internal sound and MIDI channel 2; ROOT, FIFTH, ALTERNATE,
OCTAVE on the clock.

Phase 2 as built (0.14, software-tested only):
- `core/parts.h`: `Clock` (24 PPQN; internal BPM, tap, MIDI clock follow with start /
  continue / stop and a 0.5 s timeout), `Parts` (note set from keys or the harmony chord,
  arp patterns up / down / up-down / order / random with a seeded xorshift, rates 1/4–1/32
  with triplets, octaves 1–4, gate, latch; bass root / root+fifth (the chord's own fifth) /
  alternate / octave per chord change or 1/2, 1/4, 1/8 in C1–C3). One owner per pitch;
  MIDI out per part (arp = out channel, bass = +1) and clock out. Audio owner, fixed
  arrays, no allocation; Advance once per block (outside the sample loop).
- The "FIFTH" mode of the brief is played as root + fifth together (a power-chord bass);
  ALTERNATE is root then fifth. Rhythmic bass patterns can be added as more modes.
- Engine: the arp takes the keys (or the voiced chord); with only the bass on, the chord
  sounds as before. Parts page (KEY_21 from the harmony page), patch v7, Inspector page 9.
- Resources: release 229,168 B (+7,456), development 242,896 B (+8,520); 700 B of state;
  CPU gate unchanged. Tests: `parts_test` (packing, clock/tap/MIDI, every pattern, seeded
  random, gate, latch, ownership, bass, MIDI/clock out, engine with harmony, panel page),
  v7 protocol and host round trips, Inspector page 9, webapp controls; sanitizers.
- Not yet: event recording of the parts (item 4: event recorder), per-part sounds.

---

DC's brief, as given:

You are implementing the next major musical-capability upgrade for CHOMPI Forge.

Repository:

* DCDominguez/CHOMPI
* Active branch: forge/foundation
* Do not touch main
* Verify the current remote branch head before making changes.
* Never force-push.
* Preserve the existing architecture and test discipline.

## Goal

Build a small, deterministic Harmony / Intent Engine inspired by the musical workflow
of Nopia-style harmonic instruments.

This is NOT a Nopia firmware port. This must be an original Forge implementation based
on general musical concepts and publicly observable behavior. Do not copy source code
from O'PIAN. O'PIAN is AGPL-3.0 and should be treated only as a behavioral/architectural
research reference.

The objective is: represent musical relationships first, then deterministically
generate notes/events. This should give Forge dramatically more musical capability with
very little SDRAM and modest executable-code cost.

## Core architecture

Introduce a symbolic musical layer above the existing synth/sampler/MIDI engines:

```
Physical controls / MIDI / Host / AI
                ↓
          Musical Intent
                ↓
        Harmony Resolver
                ↓
         Voice / Part Logic
                ↓
   ┌────────────┼─────────────┐
   ↓            ↓             ↓
Synth        Sampler        MIDI
```

The Harmony Engine must NOT become a DSP subsystem. Its outputs should be ordinary
note/events that existing Forge destinations consume.

## Primary state

Design a compact state representation approximately equivalent to:

```
HarmonyState
tonic
scale/mode
degree
extension
variation/color
inversion
spread/register
voicing style
layout mode
seed
```

Avoid dynamic allocation. Prefer small enums, integer pitch classes, fixed-size arrays
and deterministic transforms. The exact representation is yours to design after
reviewing existing Forge structures.

## Required first-generation features

1. **Tonic.** Support all 12 chromatic tonal centers, internally pitch class 0–11 where
   practical.
2. **Scale / mode.** Initial target: Major, Natural minor, Harmonic minor, Melodic
   minor, Dorian, Phrygian, Lydian, Mixolydian. Locrian and additional modes may be
   added if cost is negligible. Prefer tiny constexpr interval tables (Major: 0 2 4 5
   7 9 11; Natural minor: 0 2 3 5 7 8 10). Do not store giant chord databases if chords
   can be calculated cheaply.
3. **Scale-degree chord resolution.** degree → scale degree root → stacked thirds →
   chord intervals → notes. Example: C major, degree IV, 7th → F A C E. Implement this
   generically from scale structure wherever practical.
4. **Extensions.** power/fifth, triad, 7th, 9th, 11th, 13th. Do not assume every
   extension must retain every chord tone if voice-count limits require musical
   reduction. Define predictable note-dropping rules where necessary.

## Static vs Real layout

One of the highest-value interaction concepts.

- **Static mode:** physical locations represent harmonic functions. Changing the tonic
  does NOT change the physical gesture used for a progression (the same gesture plays I
  → vi → IV → V in C major and in E major). The player develops functional muscle
  memory.
- **Real mode:** physical key geography behaves relative to conventional note/piano
  layout.

Implement both as mapping layers before chord resolution. Do not entangle layout logic
with synthesis.

## Chromatic keys

Do not make non-diatonic inputs useless. Where musically sensible, resolve chromatic
positions into useful functions such as secondary dominants, modal interchange
candidates, altered dominant behavior and substitutions. Start conservatively; a minimal
first implementation could recognize V/ii, V/iii, V/V, V/vi. Do not build an enormous
harmonic expert system yet. The goal is musically useful behavior per byte.

## Shift / alternate-function behavior

Provide one alternate transformation layer analogous to a SHIFT state. Examples: sus4,
dominant conversion, lowered seventh, secondary dominant, tritone substitution. The
transform should depend on harmonic context rather than simply selecting a second
arbitrary chord table. Keep it deterministic.

## Voicing engine

Chord identity and chord voicing are separate stages (chord identity → voicing engine
→ MIDI pitches). Support inversion, register, spread and note-range constraints. Use
fixed-size note arrays. Avoid unnecessary heap use.

## Voice-leading

Important. Add a lightweight voice-leading algorithm that attempts to minimize movement
from the previous chord. It does not need to solve globally optimal classical voice
leading; a deterministic greedy/local solver is acceptable. Possible objective: minimize
Σ |new_voice − previous_voice| subject to pitch range, inversion choices, octave
placement and maximum supported voices. Prefer maintaining common tones where
reasonable. Chord changes should feel intentionally voiced rather than like independent
block chords.

## Musical parts

One Harmony resolve should eventually produce several coordinated roles: Chord/Keys,
Bass, Arp, Pad/additional role. The first implementation need not enable all roles,
but the state/output architecture should not assume only one destination.

## Bass generator

A tiny deterministic bass-role generator. Initial options: ROOT, FIFTH, ALTERNATE,
OCTAVE. Future rhythmic bass patterns should be possible without redesign. Bass
register independent of chord voicing register.

## Arpeggiator

Implement or prepare for a note-set transformer supporting at least UP, DOWN, UPDOWN,
ORDER, RANDOM (future: OUTSIDE_IN, INSIDE_OUT, ROTATE). Parameters should eventually
include rate, octave count, gate, rotation, probability. Do not duplicate timing
infrastructure if existing Forge clock/event infrastructure can support it.

## Deterministic randomness

Any random behavior MUST be reproducible, from an explicit seed (e.g. 0x12345678). The
same patch + event sequence + seed must produce the same result. No uncontrolled
platform RNG for musical decisions (reproducibility, preset recall, debugging, QA,
future AI-generated arrangements).

## Shared MusicalState

Evaluate a compact shared musical state for future generators (tempo, clock phase,
bar, section, tonic, scale, degree/chord, density, tension, energy, seed). Do not add
fields merely because they sound interesting; only what current work needs plus clearly
justified near-term extension. Strategic goal: future bass, arp, melody, drums and FX
behavior derive from one coordinated musical state.

## Event-first architecture

Prefer symbolic events over rendered audio (e.g. `tick 0 degree I, tick 96 degree vi,
tick 192 degree IV, tick 288 degree V` is far cheaper than PCM). Design Harmony outputs
so they can eventually be recorded by an event/automation looper. Do NOT implement a
giant sequencer unless required for this milestone.

## Destination abstraction

Harmony generation should not know or care whether notes go to the internal synth,
sampler, UART MIDI, USB MIDI or future external parts. Boundary: Harmony resolver →
Note/Event Set → Destination router.

## AI compatibility

Do NOT put AI into the real-time firmware. Make the engine controllable through compact
deterministic parameters, so host/AI instructions can say e.g. `tonic = A#, mode =
natural minor, progression = i, VI, III, VII, extension = 7, voicing = close,
voice_leading = on, tension = medium` and Forge produces deterministic output. AI
chooses intent; firmware executes the music.

## Physical CHOMPI controls

Inspect the existing panel mapping before deciding final interaction; do not destroy or
overload existing workflows casually. Propose a usable mapping for tonal center,
degree/function selection, extension, Shift/alternate, Static/Real and
inversion/voicing. Prefer modal interaction that remains playable without a screen.
Tab5 visualization/control may exist later; CHOMPI must remain musically functional
without it.

## Tab5 / Forge Scope compatibility

Expose enough state through Inspector/telemetry for a future Tab5 visualizer to show
tonic, mode, degree, chord (e.g. D#m9), voicing (D#3 A#3 C#4 F4), function
(subdominant) and layout (static). Prefer compact encoded numeric state and let the host
render labels.

## Performance constraints

Extremely cheap compared with audio DSP. No filesystem access, dynamic allocation,
blocking calls, JSON parsing, host interaction, complex graph traversal or unbounded
searches inside the audio callback. Resolve outside the per-sample path; harmony
calculation happens on musical events/state changes, not per sample.

## Memory philosophy

Optimize for musical capability per byte: effectively negligible SDRAM; constexpr scale
tables, compact pitch classes, fixed arrays, integer math, shared algorithms rather than
hundreds of hardcoded chord tables. Track actual binary-size impact.

## Current resource guardrails

Verify actual current numbers from the branch before coding (the brief's earlier
figures — release ~223 KB, headroom ~64 KiB — are stale; see the note at the top).
Report actual before/after measurements.

## Clean-room requirement

References may include publicly documented Nopia behavior, O'PIAN, Harmonia and standard
music theory, but do not copy O'PIAN source (AGPL-3.0). Implement our own algorithms and
structures; identify any external source used beyond general concepts and verify license
implications.

## Suggested stages

- **Phase 0 — Architecture:** inspect Forge; choose the insertion point; event
  ownership/threading; estimate executable cost; propose physical mapping; define state,
  note-output structure and tests. No large implementation before this is coherent.
- **Phase 1 — Core harmony:** tonic, major/minor, degree resolution, triad, 7th,
  Static/Real, fixed-size note output; tests first or alongside.
- **Phase 2 — Expanded harmony:** more modes, 9/11/13, Shift alternatives, chromatic
  functional inputs, inversions, register/spread.
- **Phase 3 — Voice leading:** deterministic previous-chord-aware voicing; test movement
  cost and edge cases.
- **Phase 4 — Part generation:** bass role, arp note-set output, destination-ready role
  representation; no unnecessary DSP changes.
- **Phase 5 — Panel + protocol + Inspector:** physical controls, host protocol where
  appropriate, presets, Inspector, telemetry; backwards compatible unless documented.

## Tests

Native tests for: harmony correctness (all 12 tonics, every scale, all degrees, triads,
extensions, inversions); Static layout (changing tonic preserves function for the same
gesture); Real layout (note-relative mapping); chromatic behavior; voice leading
(common tones, bounded register, determinism, no unintended duplicates, sensible
inversions); randomness (same seed → identical, different seed → controlled variation);
note lifecycle (no stuck notes on chord/mode change, role disable, panic, preset
change); boundary safety (MIDI 0–127, empty/invalid inputs, maximum extensions, range
clipping, duplicate pitch classes, voice-count limits). Run the full existing regression
suite too.

## Resource report (after each phase)

commit; release size and delta; development size and delta; remaining SRAM_EXEC; SDRAM
delta; internal SRAM/DTCM delta; CPU benchmark delta; native tests; sanitizer;
browser/integration tests; ARM builds; hardware verification status. Judge by musical
capability gained ÷ firmware bytes consumed.

## Non-goals

No neural generation, onboard LLM, giant scale/chord databases, complex jazz
reharmonization engine, arbitrary modular routing, large new audio DSP, additional PCM
buffers, multitrack sequencer, full arranger or Tab5 dependency.

## Desired result

From `key → note → synth` toward `gesture → musical function → harmonic state → voiced
notes → musical roles → internal/external destinations`, while preserving ordinary note
playing. A reusable primitive for generative sequencing, event looping, bass, arps,
melody, MIDI orchestration, AI composition and Tab5 visualization/control.

## First deliverable

Before a large code change: (1) inspect the branch; (2) identify the clean integration
point; (3) propose exact HarmonyState / request / result structures; (4) propose the
physical control mapping; (5) estimate code-size impact; (6) identify real-time
ownership concerns; (7) define Phase 1 tests; (8) recommend the smallest implementation
that proves the concept; (9) then implement Phase 1 if there are no architectural
blockers. Keep the design deliberately small. Success criterion: a compact musical
intelligence layer that dramatically multiplies what the existing hardware can do.
