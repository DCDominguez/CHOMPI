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

## Current milestone: instrument candidate 0.3

- Four-voice oscillator synth with four waveforms, ADSR, velocity and low-pass tone.
- Keybed/MIDI playing, note-source isolation, voice stealing and panic recovery.
- v2 named synth/delay/output modules; synth or aux into delay into output.
- Existing delay and v1 presets retained, with atomic recall/status/capture.
- Webapp instrument authoring, provider/key/model selection, editing and save/send.
- Six presets, native tests, simulated renderer, ARM build and handoff docs.

This is a bounded module format, **not a general-purpose modular graph**. No
sampler, recorder, looper, sequencer, custom modulation graph, FM/reverb, onboard
AI, SD preset bank, Wi-Fi or Tab5 integration yet. Stock TAPE features are not
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
TEST_SESSION steps so it can be QA'd separately. Known risk: the 0.3 base has
not run on hardware yet, so base defects may surface late; device CPU is
unknown, so CPU-heavy work must keep a fallback (fewer voices / lower quality).

1. **Playability basics** — CC64 sustain, pitch bend (done, software-tested).
   Candidates if wanted: mod wheel (needs the LFO from 2), octave shift for
   the keybed (needs a control assignment), configurable bend range (wire change).
2. **Richer synth palette (v3 patch)** — second oscillator/detune, noise,
   resonant filter with its own envelope, LFO, reverb module; saw/square
   anti-aliasing improvements fold in here. Widens what AI can configure.
3. **SD preset banks** — device-side save/recall without a computer.
4. **Sampling** — recording and playable sample maps (SDRAM + SD).
5. **Looping** — capture, overdub, manipulation on the sampling buffers.
6. **Dedicated/networked controllers** (Tab5) — optional, last.

Do not silently expand hardware dependencies or claim these features exist
before they are implemented and tested. AI still only configures installed
modules; each new engine is firmware work.
