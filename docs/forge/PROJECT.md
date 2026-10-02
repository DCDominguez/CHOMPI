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

## Later directions (not implemented or promised in this milestone)

1. Sampling/recording and playable sample maps.
2. Loop capture, overdub and manipulation.
3. More DSP modules, bounded routing/modulation and assignable controls.
4. Preset banks/SD persistence and a fuller performance UI.
5. Optional dedicated/networked controllers.

Choose the next engine after instrument acceptance and DC's priorities. Do not
silently expand hardware dependencies or claim these features already exist.
