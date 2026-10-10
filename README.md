# Forge for CHOMPI

**An AI-programmable instrument: describe a sound, play it, shape it, save it.**
Forge runs audio on CHOMPI's Daisy Seed and uses a computer webapp for AI patch
authoring, editing and MIDI control. OpenAI and Gemini use your own API key.

**Development firmware 0.7 runs on CHOMPI.** First hardware sessions (2026-10-04,
[TEST_RESULTS](docs/forge/TEST_RESULTS.md)): every key, knob, switch and light
confirmed by the panel walk; pitch, keybed, synth, menu, looper states, knob pages
and panic pass the automatic checks; worst-case synth CPU 37 %. Still unverified on
hardware: line-in paths, the sampler with real kit files, the USB card/firmware
loader and live AI provider requests. This is experimental community firmware, not
an official or hardware-approved release.

**Try it:** download *Forge Bridge.exe* (Windows) from the latest green
[Forge Bridge (Windows exe)](https://github.com/DCDominguez/CHOMPI/actions/workflows/forge-bridge-exe.yml)
run → *Artifacts*; it includes `FORGE.bin` and
[HOME_CHECKLIST.md](docs/forge/HOME_CHECKLIST.md).

**[User manual](docs/forge/MANUAL.md)** · [Developer resume checkpoint](docs/forge/CONTINUE.md) · [Documentation](docs/forge/README.md)
· [Project scope](docs/forge/PROJECT.md) · [Draft PR #1](https://github.com/DCDominguez/CHOMPI/pull/1)
· [Development branch](https://github.com/DCDominguez/CHOMPI/tree/forge/foundation)

## Project and implemented features

The goal includes sound generation and manipulation, not only external effects.
AI chooses settings and supported connections among installed modules. New
patches require no recompile; new DSP algorithms still require firmware work.
The original upstream TAPE/WAVE/TEMPO and bootloader sources remain separate.

| Area | Implemented (0.7) |
| --- | --- |
| Looper | TAPE-style: LOOP/PLAY keys, first take, overdub with feedback, pause/resume, clear, varispeed/reverse/scrub on SW5, save the loop to a sample slot ([LOOPING.md](docs/forge/LOOPING.md)) |
| Knob pages | Press a knob to step its page (light shows it): patch knobs, filter/envelope, LFO/delay, reverb; v5 patches choose what page 1 controls ([KNOBS.md](docs/forge/KNOBS.md)) |
| USB card loader | Copy TAPE samples or new firmware to the SD card over USB; firmware installs after a CHOMPI key press (no card removal) ([PROTOCOL.md](docs/forge/PROTOCOL.md)) |
| Test bridge | *Forge Bridge.exe*: setup check, guided panel walk, automatic audio checks with per-step CPU, card and firmware loader ([BRIDGE.md](docs/forge/BRIDGE.md)) |
| Sampler (v4) | TAPE-compatible: plays TAPE's `jammi_`/`cubbi_` WAV files from the SD card (chromatic or kit, banks a–e, 14 slots) and a recording; pitch, start/end, loop with crossfade, hold or one-shot, reverse; up to 7 stereo voices through the filter, LFO, delay and reverb; reads 16/24-bit/float, mono/stereo, any rate |
| Recording | Toggle down + hold CHOMPI (as TAPE) from mic, line in or resample, ~87 s; edge fades and normalisation; save/copy/erase TAPE-format files from the panel menu (Samples page), CLI or webapp |
| Synth | Up to four voices (1–4 per patch, mono with glide); two oscillators (sine, polyBLAMP triangle, polyBLEP saw/square) with interval and detune, plus white noise |
| Articulation | Attack/decay/sustain/release envelope, MIDI velocity; stealing prefers a releasing voice, then the oldest, and continues from the current level and phase |
| Tone | v3: per-voice resonant low-pass (40–16000 Hz) with its own ADSR and ±6-octave amount; v1/v2 keep the original one-pole low-pass |
| Modulation | LFO (sine/triangle/square/sample-and-hold, 0.05–20 Hz) to pitch, filter and amplitude; mod wheel (CC1) can control its depth |
| Playing | CHOMPI's 25 keys mapped to MIDI 48–72, fixed velocity 100; incoming channel-1 notes 0–127, sustain pedal (CC64) and ±2-semitone pitch bend per MIDI source |
| Note ownership | Keybed, USB and UART tracked separately; velocity-zero note-on releases |
| Audio routes | Synth, sampler (v4) or stereo aux → delay → reverb (v3) → output; mono synth duplicated to stereo |
| Delay | Stereo buffers, 10–1000 ms, mix, feedback capped at 85%, output level, wet bypass |
| Reverb | v3: stereo 4-line feedback-delay-network reverb, mix, size (0.2–10 s decay), damping |
| Audio configuration | 48 kHz, 24-frame blocks; headphone/main output mirroring; microphone used for recording only |
| Parameter handling | Delay/output smoothing; pitch glide when delay time changes; finite bounded output |
| Live controls | Encoders (knobs 1–3 become pitch/start/end on sampler patches, as TAPE), MIDI CC20–25 as stock (CC20+n = knob n), CC71 resonance / CC91 reverb mix (v3), CC74 cutoff, CC85 bypass, CC1 mod wheel, CC64 sustain, CC121 reset controllers, pitch bend, and atomic whole-patch changes between blocks |
| Recovery | SW5 press, CC120/123, host panic; silence voices and delay/reverb tails; note-overflow recovery |
| Patch format | v5 = v4 named synth/filter/lfo/sampler/delay/reverb/output modules with three routes, plus the four page-1 knob assignments; v1–v4 files still work and convert |
| Presets | Fourteen: Dry, Slap, Long Echo (v1); Glass Keys, Soft Pad, Saw Bass (v2); Warm Pad, Acid Bass, Bell Keys, CPU Stress test (v3); Recorded Keys, TAPE Kit A, Sampler Stress test (v4); Knob Pad (v5) |
| Computer persistence | Save/import/export JSON, capture device targets and recall |
| Device presets | 8 banks × 15 slots on the SD card; TAPE-style panel menu (toggle + CHOMPI key, white keys, bank keys/knob 1, save/copy/erase), MIDI program change, host CLI and webapp |
| Webapp | Instrument/effect authoring selector, provider/model/key input, editor for every module (greys out what an older preset lacks), convert-to-v4, device presets and device samples panels |
| AI providers | OpenAI and Gemini structured output (v4 instruments using only samples the device reports, v1 delay) plus independent validation; Ollama CLI for v1 delay only |
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
| Delay mix | Knob 1 (hw SW4) / CC20 | 0–100% |
| Delay time | Knob 2 (hw SW1) / CC21 | 10–1000 ms |
| Feedback | Knob 3 (hw SW2) / CC22 | 0–85% |
| Output level | Knob 4 (hw SW3) and volume SW6 / CC23, CC25 | 0–1 |
| Wet bypass | CC85 | >=64 on; dry still obeys output level |
| Synth cutoff | SW5 turn / CC24, CC74 | 40–16000 Hz, logarithmic |
| Filter resonance | CC71 (v3) | 0–1 (Q 0.7–11) |
| Reverb mix | CC91 (v3) | 0–1 |
| LFO depth | CC1 mod wheel (when the patch enables it) | 0–full |
| Panic | SW5 press / CC120 or CC123 / webapp | Stop all sources and old delay/reverb tails |
| Waveforms | Preset or web editor | Sine / triangle / saw / square (LFO: also sample-and-hold) |
| Amp and filter attack / decay | Preset or web editor | 1–2000 ms each |
| Sustain | Preset or web editor | 0–1 |
| Release | Preset or web editor | 5–5000 ms |
| Osc 2 interval / detune | v3 preset or web editor | ±24 semitones / ±50 cents |
| Voices / glide | v3 preset or web editor | 1–4 / 0–2000 ms |
| LFO rate | v3 preset or web editor | 0.05–20 Hz |

With knob pages ([KNOBS.md](docs/forge/KNOBS.md)) the table is page 1 of a
default patch. Encoder IDs follow hardware source; the panel walk confirmed them
on hardware. Boot
uses dry aux with time 257.5 ms, feedback 21.25%, level fading toward 0.25. Route
or waveform changes silence current voices/tails; release/retrigger held keys.
Steal clicks and triangle aliasing were reduced in simulation; saw/square can still
alias on high notes. Musical quality and v3 CPU cost need listening and measurement
on hardware; lower the voice count if CPU is short.
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

CLI remains available for ports, validate, encode, send, status, capture, panic,
store/recall/erase/slots (device presets)
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

| Check | Recorded evidence (details: [CONTINUE](docs/forge/CONTINUE.md), [TEST_RESULTS](docs/forge/TEST_RESULTS.md)) |
| --- | --- |
| Native suites | Ten C++ suites pass (DSP/queue, protocol, synth, v3, presets/menu, sampler, panel, Inspector, looper, USB file transfer incl. 200,000 fuzzed requests), and again under ASan/UBSan |
| Python / web | 111 Python tests; 11 + 6 real-Chromium tests (webapp, bridge) |
| ARM | Release 235,980 B / development 251,684 B of 282 KB executable SRAM, layout checks OK (xPack GCC 10.3.1); GitHub Actions builds the development firmware and the Windows exe on every change |
| Hardware | Two sessions by DC on 2026-10-04: panel walk all pass; automatic checks 21 of 22 on 0.7 (the miss: no kit file on the card); line-in steps not yet run |
| AI providers | No live provider request performed yet |

Firmware SHA-256 depends on the compiler; use the value printed by the test
bundle's `verify_bundle.py`, which also names the compiler that built it.
Memory allocation is not a worst-case stack or CPU measurement.

## Package and consolidated test

From a clean, committed tree after building firmware and native tests:

```sh
cd firmware/chompi-forge
python3 host/package_candidate.py /absolute/output/Forge-test.zip          # release kit
python3 host/package_candidate.py /absolute/output/Forge-Bridge-dev.zip --development   # test bridge kit
```

The generator includes firmware, webapp/CLI, the JSON schemas (v1 delay, v2,
v3, v4 and v5 instrument), fourteen presets, simulated references, docs, licenses and
hashes/source identity. Binaries and ZIPs are generated artifacts, not tracked
source. Earlier 0.2 and 0.3 ZIPs are obsolete and have not been silently updated.

DC's workflow is **one consolidated hardware session**; follow
[TEST_SESSION.md](docs/forge/TEST_SESSION.md). A defect may require a focused
retest. Do not substitute software passes for actual device acceptance.

## Not implemented yet

- Sequencing, TAPE's per-slot settings (`presets.json`) and its other performance functions.
- Further effects (chorus, distortion, EQ), FM synthesis, multiple LFOs/envelopes, or arbitrary routing/modulation graphs.
- Aftertouch/MPE, clock sync, arpeggiator, octave controls, configurable bend range, mono note-priority stack or note output.
- Preset names on the device, automatic recall after reboot, or importing TAPE/WAVE presets.
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
| [PROTOCOL.md](docs/forge/PROTOCOL.md) | Exact v1–v4 wire layout, sample requests and recovery semantics |
| [SAMPLING.md](docs/forge/SAMPLING.md) | Sampler design: TAPE behaviour kept, what Forge changes, memory, panel |
| [COMPATIBILITY.md](docs/forge/COMPATIBILITY.md) | Forge vs stock TAPE/TEMPO/WAVE: bootloader, memory, MIDI, keybed and CPU benchmark |
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
