# Forge project brief

Status: foundation implementation, 2026-10-01. Community firmware for CHOMPI;
not an official CHOMPI Club release.

## Goal

Turn CHOMPI into a playable, externally controllable audio-effects instrument.
Keep its real-time audio engine on the Daisy; allow a future computer, phone,
or optional Tab5 controller to describe and adjust sounds without rebuilding
firmware for each parameter change.

The user's existing direction is Forge firmware first, expansions later, with
project continuity in this repository. The earlier detailed handoff was not
present in the repository at the start of this work. The implementation choices
below establish a provisional first milestone, not a claim that every detail
was previously agreed.

## Architecture

- Audio: stereo aux input -> compiled DSP -> headphone and main stereo outputs.
- Controls: physical encoders and USB/TRS MIDI -> validated normalized commands.
- Ownership: main loop produces MIDI commands; audio callback consumes at most
  16 commands per block and exclusively owns parameter/DSP state. Encoders are
  polled and applied in that callback after queued MIDI commands.
- Real-time boundary: no allocation, storage operations, network, or AI calls
  in the Forge audio core. Fixed-size buffers and bounded command consumption.
- Future AI: translates intent into supported parameters/presets on an external
  host. It is not currently implemented, and arbitrary generated DSP cannot be
  loaded by this foundation.

“No recompile” applies to controls exposed by the installed engine. New DSP
algorithms, routing capabilities, or hardware drivers still require builds.
Patch files and graph changes will need a separately designed schema, validation,
and atomic application mechanism; none exist in M0.

## Milestones and acceptance

| Milestone | Deliverable | Acceptance |
| --- | --- | --- |
| M0 foundation | Stereo delay, live parameters, source build, host tests | Build and host tests pass; physical bring-up still required |
| M1 hardware validation | Confirm routing, encoders, USB/TRS MIDI, battery behavior | Logged tests on DC's hardware, CPU load measurement, stock restore verified |
| M2 patch model | Versioned validated presets and atomic recall | Bad patches leave the active patch intact; audible transition testing |
| M3 external authoring | Text-to-supported-patch host prototype | Host edits parameters live, device rejects unsupported values |
| Later | More DSP blocks, controller or network expansion | Choose after M1/M2 evidence |

## M0 choices

Use WAVE's hardware abstraction, encoder driver, linker script, and modified
libDaisy in place. Add `firmware/chompi-forge/` without changing upstream
TAPE/WAVE/TEMPO. Rebuild libDaisy into Forge's ignored build directory.

The first DSP block is a stereo delay with 10–1000 ms time, feedback capped at
0.85, wet/dry mix, level, and wet bypass. Changes use 20 ms one-pole smoothing;
delay-time changes glide in pitch. Buffers are initialized before audio starts.
Aux input is the only active source; microphone, sampler, looper, SD presets,
sequencer, and stock performance UI are outside this first milestone.

No additional hardware is required by this architecture. Tab5, Wi-Fi, and AI
integration remain later work. This is not a replacement for the stock sampler's
complete feature set.

## Source baseline

Fork: `DCDominguez/CHOMPI`, base `a73d732613da684e4de844619b690776f0f50ccf`.
Primary references in this repo: `firmware/README.md`, WAVE's README,
`code/src/hardware.h`, `code/src/chompi_main.cpp`, `code/src/MidiManager.h`,
and `code/src/chompi_sram.lds` under `firmware/chompi-wave/`.

Retain the root MIT license and third-party notices. Treat CHOMPI's names,
artwork, and marks according to `TRADEMARKS.md`.
