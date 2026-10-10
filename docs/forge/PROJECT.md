# Forge project brief

## Goal and user decisions

Forge is an **AI-programmable instrument for CHOMPI**: describe a sound, construct
a patch from installed sound-generating/processing modules, play the keys or
MIDI, adjust knobs, save and recall. Effects are one capability, not the whole
project. DC explicitly clarified this on 2026-10-02 after the initial prototype
had been narrowed to stereo delay. Do not repeat that scope mistake.

AI runs on a computer using OpenAI/Gemini with the user's key. CHOMPI runs the
real-time audio. New settings and supported routing do not require recompiling;
new algorithms/drivers still need firmware work. AI does not upload executable
DSP code. Tab5 is optional future hardware, not a prerequisite.

DC wants one consolidated physical test. Continue software development and
validation without per-feature flash requests. A real defect may require retest.
DC also requested continuously updated documentation for other development agents;
read and maintain [CONTINUE.md](CONTINUE.md) at each checkpoint.

## Current milestone: instrument candidate 0.15.1

Everything below "0.4" was the first milestone and is kept as history. Since then:
sampler with recording (0.5), knob pages and v5 patch knobs (0.6), USB card / firmware
loader (0.7), stock power behaviour (0.8), key lights (0.9), TAPE panel parity (0.10),
install power check (0.11), TAPE per-slot settings (0.12), harmony mode and patch v6
(0.13), clock / arp / bass and patch v7 (0.14), event recorder and projects (0.15), and
the full-review fixes (0.15.1). All software-tested, none hardware-verified yet; the
next step is DC's one consolidated hardware session ([TEST_SESSION.md](TEST_SESSION.md)).
Details: [CHANGELOG.md](CHANGELOG.md), [CONTINUE.md](CONTINUE.md).

### First milestone: instrument candidate 0.4

- Synth with up to four voices (1–4 per patch, glide), two oscillators with
  interval/detune, noise, amplitude ADSR, velocity.
- Per-voice resonant low-pass with its own envelope; one LFO to pitch, filter
  and amplitude, optionally under the mod wheel.
- Keybed/MIDI playing, sustain pedal, pitch bend, note-source isolation,
  click-free voice stealing and panic recovery.
- v3 named synth/filter/lfo/delay/reverb/output modules; synth or aux into
  delay into reverb into output. v1 delay and v2 instrument patches still work
  (bit-exact) and convert to v3.
- Webapp authoring (v3 instruments), provider/key/model selection, editing of
  every module, save/send/capture.
- Ten presets, native/sanitizer/browser tests, simulated renderer, ARM build and
  handoff docs.

This is a bounded module format, **not a general-purpose modular graph**. No
sampler, recorder, looper, sequencer, custom modulation graph, FM, onboard
AI, Wi-Fi or Tab5 integration yet (SD preset banks exist as of 0.4). Stock TAPE features are not
available inside Forge. Upstream sources remain separate and unmodified.

## Architecture and acceptance

Real-time code is hardware-independent and allocation-free. Main loop parses
MIDI and queues requests; the audio callback owns mutable DSP state and processes
bounded requests between blocks. No storage, network, model calls or MIDI TX in
the callback. JSON stores physical units; the wire quantizes parameters to 14 bits.

First instrument acceptance: describe sound → generate → review/edit → send →
play from keys/MIDI → adjust → capture/save → recall. Include stereo external
input routing, panic, repeated/overlapping notes, overload recovery and CPU load
in the same session. Software tests cannot establish musical quality, physical
mapping, worst-case CPU time or device recovery. Actual model use is unverified.

## Roadmap (priority order agreed 2026-10-02)

DC decided to develop features first and run hardware/live-AI QA afterwards,
per feature. Each feature lands as its own commit(s) with its own tests and
TEST_SESSION steps so it can be QA'd separately. Known risk: no Forge build (0.3 onward) has
run on hardware yet, so base defects may surface late; device CPU is
unknown, so CPU-heavy work must keep a fallback (fewer voices / lower quality).

1. **Playability basics** — CC64 sustain, pitch bend (done, software-tested).
   Candidates if wanted: octave shift for the keybed (needs a control
   assignment), configurable bend range (wire change), mono note-priority stack.
2. **Richer synth palette (v3 patch)** — second oscillator/detune, noise,
   resonant filter with its own envelope, LFO + mod wheel, voices/glide, reverb
   (done, software-tested; firmware 0.4). Saw/square anti-aliasing beyond
   2-point polyBLEP waits for a device CPU figure (TEST_SESSION 6.2b).
3. **SD preset banks** — device-side save/recall without a computer (done,
   software-tested): 8 × 15 slots, TAPE-style key + encoder menu, program
   change, host CLI and webapp. Candidates: names on the device, recall the
   last preset at boot, preset import from TAPE/WAVE cards.
4. **Sampling** — TAPE-compatible sampler (design: [SAMPLING.md](SAMPLING.md)):
   JAMMI/CUBBI slots from TAPE-named WAVs, recording (mic/line/resample),
   v4 patches, samples menu page (done, software-tested; firmware 0.5).
   Candidates: TAPE's per-slot settings (`presets.json`), threshold-armed
   recording, wider MIDI note range in kit mode, sample names.
5. **Looping** — capture, overdub, manipulation on the sampling buffers.
6. **Dedicated/networked controllers** (Tab5) — optional, last.

**Backlog (DC, 2026-10-10), after the hardware session:**
- **Synth edit page** (DC: "add to the list"): full sound design on CHOMPI, in TAPE's
  menu style. Keys choose oscillator 1/2 waveform, osc 2 interval, LFO shape and target,
  voice count; knobs set noise, glide, delay time and the other patch fields the knob
  pages do not reach (KNOBS.md). Saves through the existing presets page.
- **Round out the effects** (DC). Picked: chorus / flanger, phaser, lo-fi / bitcrusher,
  tremolo / auto-pan. Also wanted: fuller **delay** (today one stereo delay: add tempo sync
  to the clock, ping-pong, TAPE-style filtered / wobbly repeats) and fuller **reverb**
  (today one FDN: add size / damping / pre-delay, room-plate-hall characters, maybe
  shimmer). Constraints: CPU is unmeasured on CHOMPI (emulator worst case just under
  WAVE), so each effect needs a bypass that costs nothing and a cheap mode; SDRAM is full,
  so short delay lines (chorus, flanger) go in internal RAM and long ones need memory found
  first (or SD streaming, v2). Order after DC's hardware session and a real CPU figure.
- **Webapp redesign** (DC: "too clunky, need something more UX friendly"). The sound
  designer (`host/web/index.html`, `app.js`) and possibly the bridge pages: friendlier
  layout and flow for describe → generate → tweak → send → save. Host-only, so it does not
  wait for the hardware session; keep the existing API, validation and browser tests.
  Mockup v1 (2026-10-11, agent, awaiting DC's sign-off on look and live editing):
  https://claude.ai/artifact/UtZTBzg5xoWY9xEY41uxFM (private to DC). DC adds a virtual CHOMPI screen.
- **Tab5 Muse / Cosmo as a CHOMPI assistant** (DC designs the integration separately;
  roadmap item 6). Cosmo's map of Forge: [AGENT_GUIDE.md](AGENT_GUIDE.md). Open choices
  for DC: USB MIDI from the Tab5 directly, or through the computer (the webapp API is
  loopback-only by design).
- **Design questions from 2026-10-11 (DC):** (1) should recalling a preset without harmony /
  parts (v5 or older) switch them off? Today they keep running, by design; (2) should a new
  first loop take start at 1× speed? (3) SW5 cutoff on the loop: loop-only filter or a master
  filter after the looper? (4) menu page shown by the CHOMPI key's colour (teal TAPE, blue
  presets, purple harmony, orange parts)? (5) Saw Bass's v2 → v5 upgrade is ~half as bright.
- **DC's answers (2026-10-11, later):** (2) yes, a new first take starts at 1×; (3) SW5 filters
  the loop; (4) yes, a colour per page, **and** a colour per menu item while turning knobs;
  (5) yes, apply the waveform-aware cutoff upgrade (sine ×1, triangle ×1.6, saw / square ×2.5).
  (1) still open. New: SW5 should light during scrub (today its lights are off unless a loop
  plays); the webapp gets a **virtual CHOMPI screen** (patch, page, control being turned,
  chord, BPM, looper / recorder state), reused on the Tab5. DC's bar: every function rated
  4 / 5 or better for ease of use: [UX_REVIEW.md](UX_REVIEW.md).
- Candidates raised the same day: upgrade starter presets 01–06 (v1/v2) to v3+ with the
  same sound so every knob page works; add starter presets that show harmony, arp / bass
  and the event recorder (useful for DC's listening session).

**Forge v2 candidates** (not v1 work): storage streaming / SDRAM reclamation —
stream samples and recordings from the SD card to free ~36–42 MiB of SDRAM
(estimate). Design and limits: [STORAGE_STREAMING.md](STORAGE_STREAMING.md);
revalidate it against v1 changes before starting.

Do not silently expand hardware dependencies or claim these features exist
before they are implemented and tested. AI still only configures installed
modules; each new engine is firmware work.
