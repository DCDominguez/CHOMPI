# Forge for CHOMPI

**An AI-programmable instrument: describe a sound, play it, shape it, save it.**
Forge runs audio on CHOMPI's Daisy Seed and uses a computer webapp for AI patch
authoring, editing and MIDI control. OpenAI and Gemini use your own API key.

**Candidate 0.3 is software-tested and builds for ARM.** Real CHOMPI operation,
live provider requests and browser interaction/layout remain unverified. This
is experimental community firmware, not an official or hardware-approved release.

[Developer resume checkpoint](docs/forge/CONTINUE.md) · [Documentation](docs/forge/README.md)
· [Project scope](docs/forge/PROJECT.md) · [Draft PR #1](https://github.com/DCDominguez/CHOMPI/pull/1)
· [Development branch](https://github.com/DCDominguez/CHOMPI/tree/forge/foundation)

## Project and implemented features

The goal includes sound generation and manipulation, not only external effects.
AI chooses settings and supported connections among installed modules. New
patches require no recompile; new DSP algorithms still require firmware work.
The original upstream TAPE/WAVE/TEMPO and bootloader sources remain separate.

| Area | Implemented in 0.3 |
| --- | --- |
| Synth | Four fixed voices; sine, triangle, polyBLEP saw and square oscillators |
| Articulation | Attack/decay/sustain/release envelope, MIDI velocity, oldest-voice stealing |
| Tone | One-pole low-pass, 40–16000 Hz, smoothed cutoff coefficient |
| Playing | CHOMPI's 25 keys mapped to MIDI 48–72, fixed velocity 100; incoming channel-1 notes 0–127 |
| Note ownership | Keybed, USB and UART tracked separately; velocity-zero note-on releases |
| Audio routes | Synth→delay→output or stereo aux→delay→output; mono synth duplicated to stereo |
| Delay | Stereo buffers, 10–1000 ms, mix, feedback capped at 85%, output level, wet bypass |
| Audio configuration | 48 kHz, 24-frame blocks; headphone/main output mirroring; microphone unused |
| Parameter handling | Delay/output smoothing; pitch glide when delay time changes; finite bounded output |
| Live controls | Encoders, MIDI CC20–25 and atomic whole-patch changes between blocks |
| Recovery | SW5 press, CC120/123, host panic; silence voices/old tail; note-overflow recovery |
| Patch format | v2 named synth/delay/output modules with two supported routes; v1 delay files retained |
| Presets | Six examples: Dry, Slap, Long Echo, Glass Keys, Soft Pad, Saw Bass |
| Computer persistence | Save/import/export JSON, capture device targets and recall; no SD-card writes |
| Webapp | Instrument/effect authoring selector, provider/model/key input, parameter editor, route/waveform/ADSR/tone controls |
| AI providers | OpenAI and Gemini structured output plus independent validation; Ollama CLI for v1 delay only |
| Key handling | Ephemeral page/request memory; no keys in presets, browser storage, source or logs |
| Device bridge | Explicit MIDI port selection, status/capture/send/panic with sequence/checksum/value verification |
| Diagnostics | Average/peak audio callback load, drop/rejection counters; hardware measurements pending |
| Real-time boundaries | Fixed memory, bounded queues and per-block requests; no network/storage/MIDI TX in audio callback |
| Developer tools | Shared C++ runtime/probe, simulated WAV renderer, test/sanitizer targets, checksummed bundle generator |
| Continuity | AGENTS.md plus live handoff, architecture, wire protocol, build guide and one consolidated test checklist |

Generation and editing **never send automatically**. Review a patch, explicitly
send it, then play. The device holds one volatile patch and resets to dry aux
mode on reboot. Load an instrument preset to enable synthesis.

## Controls and ranges

| Control | Hardware / MIDI channel 1 | Range |
| --- | --- | --- |
| Delay mix | SW1 / CC20 | 0–100% |
| Delay time | SW2 / CC21 | 10–1000 ms |
| Feedback | SW3 / CC22 | 0–85% |
| Output level | SW4 and SW6 / CC23 | 0–1 |
| Wet bypass | CC24 | >=64 on; dry still obeys output level |
| Synth cutoff | SW5 turn / CC25 | 40–16000 Hz, logarithmic |
| Panic | SW5 press / CC120 or CC123 / webapp | Stop all sources and old delay tail |
| Synth waveform | v2 preset or web editor | Sine / triangle / saw / square |
| Attack / decay | v2 preset or web editor | 1–2000 ms each |
| Sustain | v2 preset or web editor | 0–1 |
| Release | v2 preset or web editor | 5–5000 ms |

Encoder IDs follow hardware source; printed-panel mapping is unverified. Boot
uses dry aux with time 257.5 ms, feedback 21.25%, level fading toward 0.25. Route
or waveform changes silence current voices/tails; release/retrigger held keys.
Voice stealing may click and high notes may alias; musical quality needs listening.
The fixed two-route format is not a general patch graph or generated executable DSP.

## Run the webapp

Python 3.10+ on the computer connected to CHOMPI:

```sh
python3 -m pip install -r firmware/chompi-forge/host/requirements.txt
python3 firmware/chompi-forge/host/forge_web.py
```

Open **http://127.0.0.1:8765** on that computer. Choose an instrument preset or
select an AI provider, enter your key and a structured-output-capable model ID,
then generate. Edit and save without hardware, or select MIDI ports and send to
CHOMPI. Play its keys or a MIDI keyboard. No Node build is required. The local
Python bridge is required; this is not a publicly hosted or phone/LAN app.

CLI remains available for ports, validate, encode, send, status, capture, panic
and Ollama authoring. See [host guide](firmware/chompi-forge/host/README.md) for
commands, credentials, compatibility, privacy and provider references.

## Build and verification

```sh
make -C firmware/chompi-forge test
ASAN_OPTIONS=detect_leaks=0 make -C firmware/chompi-forge sanitize
make -C firmware/chompi-forge firmware GCC_PATH=/path/to/gcc-arm-none-eabi-10.3-2021.10/bin
```

Native tests need GNU Make, a C++14 compiler, pthreads and Python. Firmware uses
GNU Arm Embedded **10.3-2021.10** and vendored WAVE hardware/libDaisy. See the
[firmware guide](firmware/chompi-forge/README.md) for the toolchain download and
verified archive hash. No upstream sources or bootloader were changed.

| Check | Recorded evidence |
| --- | --- |
| Native suites | DSP/queue, MIDI/protocol and synth suites pass |
| Python | 28 tests pass; includes 250 v1 plus 200 v2 randomized protocol round trips and mocked providers |
| Sanitizers | Three C++ suites pass ASan/UBSan; LeakSanitizer disabled for environment limitations |
| Web | JavaScript syntax and HTTP/session/assets checked; actual browser smoke test pending |
| ARM | BOOT_SRAM build succeeds; FORGE.bin 113,408 bytes |
| Link allocations | SRAM_EXEC 47.74%; SRAM 17.01%; RAM_D2 68.07%; SDRAM 0.57% |
| Hardware / AI | No flash, listening, physical I/O, actual CPU measurement, or live provider request performed |

Firmware SHA-256:
`02ecbbb6eed11a20bcfe6774376d95cf8b6188482ab3d15ed17a1c4a22228bff`.
Memory allocation is not a worst-case stack or CPU measurement.

## Package and consolidated test

From a clean, committed tree after building firmware and native tests:

```sh
cd firmware/chompi-forge
python3 host/package_candidate.py /absolute/output/Forge_0.3_Test_Candidate.zip
```

The generator includes firmware, webapp/CLI, both JSON schemas, six presets,
simulated aux/synth references, docs, licenses and hashes/source identity.
Binaries and ZIPs are generated artifacts, not tracked source. The earlier 0.2
ZIP is obsolete for the instrument milestone and has not been silently updated.

DC's workflow is **one consolidated hardware session**; follow
[TEST_SESSION.md](docs/forge/TEST_SESSION.md). A defect may require a focused
retest. Do not substitute software passes for actual device acceptance.

## Not implemented yet

- Sampling, recording, looping, sequencing or stock TAPE performance functionality.
- Additional effects such as reverb, FM synthesis or arbitrary routing/modulation graphs.
- Sustain pedal, pitch bend, aftertouch/MPE, clock sync, arpeggiator, octave controls or note output.
- Device-side preset banks, SD saves or automatic recall after reboot.
- Onboard AI, generated DSP code, plugins or runtime executable loading.
- Tab5 integration, Wi-Fi, public web hosting or phone remote control.
- Auto MIDI retries, unsolicited parameter streaming or protocol authentication.

## Repository map

| Path | Purpose |
| --- | --- |
| [AGENTS.md](AGENTS.md) | Instructions for continuing development |
| [CONTINUE.md](docs/forge/CONTINUE.md) | Live checkpoint and exact remaining work |
| [PROJECT.md](docs/forge/PROJECT.md) | Corrected goal, scope and later directions |
| [ARCHITECTURE.md](docs/forge/ARCHITECTURE.md) | Ownership and processing boundaries |
| [PROTOCOL.md](docs/forge/PROTOCOL.md) | Exact v1/v2 wire layout and recovery semantics |
| [DEVELOPMENT.md](docs/forge/DEVELOPMENT.md) | Build, test and packaging workflow |
| [HANDOFF.md](docs/forge/HANDOFF.md) | Current milestone summary and evidence |
| [CHANGELOG.md](docs/forge/CHANGELOG.md) | Development history |
| [firmware/chompi-forge/](firmware/chompi-forge/) | DSP/protocol, hardware entry point, host/webapp, presets and tests |

## Original CHOMPI release


**CHOMPI** is a quirky chromatic sampler and tape-music instrument by
[CHOMPI Club](https://www.chompiclub.com).

This repo contains all of the production files, both hardware and firmware, that make up the CHOMPI Sampler.

---

## What's here

| | |
|---|---|
| [**Firmware — Start Here**](firmware/README.md) | Quick instructions for setting up your development environment, building the firmware, and loading it onto your CHOMPI. |
| [`firmware/chompi-wave`](firmware/chompi-wave/) | **WAVE 1.0**, a wavetable synth firmware that doubles as a starting point for anyone writing their own firmware. |
| [`firmware/chompi-tempo`](firmware/chompi-tempo/) | **TEMPO 1.0**, a pattern generator firmware — the counterpart to TAPE. |
| [`firmware/chompi-tape`](firmware/chompi-tape/) | **TAPE 2.0**, the sampler firmware every CHOMPI ships with. |
| [`firmware/chompi-bootloader-v6.4-beta`](firmware/chompi-bootloader-v6.4-beta/) | This bootloader never shipped on units, but was created to improve stability of the Daisy Seed's integration with CHOMPI's hardware as well as repair edge-case issues related to bugs inherited from older versions of the Electrosmith bootloader.  |
| [`firmware/card-profiles`](firmware/card-profiles/) | The factory microSD card contents for TAPE, TEMPO and WAVE — firmware, samples and settings. |
| [`hardware/hardware-pcb`](hardware/hardware-pcb/) | Schematic, BOM, EAGLE PCB files, and the full fabrication package. |
| [`hardware/hardware-enclosure`](hardware/hardware-enclosure/) | The six pcb panel enclosure files, as well as laser cutting files for diy panels. |

Each folder contains its own README, so check those out for more details.

## What's not here

**The panel artwork.** The graphic set and CHOMPI logos have all been removed for copyright purposes. If you choose to create your own hardware, we ask that you name it something else to avoid trademark infringement.

## Support Guidelines

The following describes the upstream CHOMPI release. Forge development and its
current limitations are documented separately in the guides linked above.

This is a discontinuation open-source release. As such, this repo is intended to be a permanent source for files and documentation, and will likely not be receiving updates in the future. If you wish to customize your own project, we recommend cloning this repo into your own GitHub.

## Community

Even though this version of CHOMPI is now discontinued, the CLUB is expanding. If you want to discuss this project, share your creations, see what other users have made on their CHOMPI, feel free to check out the CHOMPI Open Source channel on the Chase Bliss Discord.

## License

Everything here is **MIT** — see [`LICENSE`](LICENSE). [`THIRD_PARTY.md`](THIRD_PARTY.md) lists
the work this builds on and the notices that come with it. The CHOMPI name, logo and artwork are
not covered by the license — see [`TRADEMARKS.md`](TRADEMARKS.md).

## HAPPY CHOMPIN'

---
