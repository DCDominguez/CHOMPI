# Forge for CHOMPI

> **Instrument milestone in progress:** the current work extends Forge beyond effects
> with a playable synth. See [developer resume checkpoint](docs/forge/CONTINUE.md)
> for current changes, test results and outstanding work. The 0.2 inventory below
> describes the previous completed software milestone.

**Forge turns CHOMPI into a programmable, externally controlled audio-effects
instrument.** The firmware handles real-time audio on the Daisy Seed; a computer
handles preset files, live patch control, and optional AI-assisted sound authoring.

This fork preserves the original CHOMPI hardware/firmware release and adds Forge
as a separate application. **Candidate 0.2 currently implements one stereo-delay
engine.** Broader multi-effects capabilities are a future direction.

**Status:** software tests and the ARM build pass. Physical CHOMPI acceptance and
actual model sessions remain pending. This is experimental community
firmware, not an official CHOMPI Club release or a hardware-approved release.

Development: [`forge/foundation`](https://github.com/DCDominguez/CHOMPI/tree/forge/foundation)
· [Draft PR #1](https://github.com/DCDominguez/CHOMPI/pull/1)
· [Documentation index](docs/forge/README.md)
· [Current handoff](docs/forge/HANDOFF.md)

## Project summary

The goal is to install a stable audio engine once, then change supported sounds
through knobs, MIDI or complete presets without recompiling each time. An
optional external model can translate a description into a validated preset.
New DSP algorithms, hardware drivers or routing graphs still require development
and a firmware build; Forge does not execute model-generated code.

The first candidate is deliberately bounded: stereo audio, one usable effect,
reliable control, preset recall and diagnostics. Hardware validation is grouped
into **one consolidated test session**, with a focused retest only if a defect
requires one.

## Implemented features

### Audio and physical controls

| Feature | Behavior in candidate 0.2 |
| --- | --- |
| Stereo input | Auxiliary left/right input; microphone is not mixed in |
| Stereo output | Effect output mirrored to headphone and main stereo outputs |
| Audio configuration | 48 kHz, 24-frame blocks using CHOMPI's existing hardware support |
| Stereo delay | Separate channel buffers; 10–1000 ms delay time |
| Wet/dry mix | Continuous blend from dry input to delayed signal |
| Feedback | Adjustable repeats, capped at 85% |
| Output level | 0–1 gain; starts with a fade from silence toward 25% |
| Wet bypass | Smoothly removes the wet contribution while preserving output level and delay state |
| Parameter smoothing | 20 ms one-pole smoothing; delay-time changes glide in pitch |
| Numeric bounds | Non-finite input is silenced; input/output clipped to [-1, 1]. This is not a mastering limiter |
| Encoders | Local mix, time, feedback and level control; the physical volume encoder also controls level |
| Startup handling | Delay memory cleared before audio; cyan initialization indicator, red engine-init failure indicator |
| Battery handling | Reuses WAVE's battery checks and low-battery behavior; physical validation pending |

### MIDI and live patch control

| Feature | Behavior in candidate 0.2 |
| --- | --- |
| USB and TRS MIDI | Both transports accept individual controls and whole-patch requests |
| MIDI CC | Channel 1, CC20–24 for mix, time, feedback, level and bypass |
| Complete patch transfer | Versioned SysEx messages with a sequence number and checksum |
| Atomic recall | A valid patch updates all parameter targets together between audio blocks |
| Patch rejection | Invalid versions, lengths, checksums or values are rejected without partial application |
| Acknowledgements | Replies confirm the accepted target values; host checks sequence and returned patch |
| Status queries | Read current targets, firmware minor version and diagnostic counters |
| MIDI framing | Handles CC running status and interleaved real-time bytes; discards oversized SysEx |
| Bounded work | Fixed-capacity queues, response backpressure and at most 16 requests consumed per audio block |
| Transport handling | Replies sent from the main loop; USB buffers retained until completion; UART reply timeout accounts for wire duration |

### Host tools, presets and optional AI

| Feature | Behavior in candidate 0.2 |
| --- | --- |
| Python command-line controller | List ports, validate/encode/send patches, query status and capture targets |
| Human-readable presets | Versioned JSON using physical units and a patch name |
| Strict validation | Checks keys, types, supported engine/version, finite numbers and ranges; rejects duplicate JSON keys |
| Save and capture | Capture knob-adjusted device targets to a new computer file; existing files are not overwritten by default |
| Preset recall | Send any validated saved patch without recompiling firmware |
| Included presets | Dry routing check, short slap and long echoes |
| Local webapp | Browser-based authoring with OpenAI or Gemini, using your own API key and model ID |
| Patch editor | Five editable controls, three presets, JSON import/export and device capture |
| Temporary credentials | Keys remain in page/request memory; not saved in presets, browser storage or logs |
| Explicit device actions | Select MIDI ports, read status, capture or send with acknowledgement |
| Optional Ollama adapter | Ask a configured local model for schema-constrained delay settings |
| Validated model output | Generated JSON must pass host validation before saving; sending is a separate explicit action |
| Offline use | JSON validation, schema output and SysEx encoding require no hardware or MIDI dependencies |

Presets are saved **on the computer**. CHOMPI holds one volatile active patch and
returns to defaults after reboot. Patch names remain on the host. There are no
on-device SD preset writes in this candidate.

### Diagnostics and developer tooling

| Feature | Behavior in candidate 0.2 |
| --- | --- |
| CPU reporting | Smoothed average and peak audio-callback load since boot; actual hardware readings pending |
| Control diagnostics | Dropped ingress/control/reply counts and rejected recognized-request counts |
| Shared offline harness | Exercises the same patch decoder, runtime and DSP used by firmware |
| Audio simulations | Renders synthetic stereo plucks through presets; these are not recordings from CHOMPI |
| Automated checks | DSP, queue/concurrency, protocol, framing, USB packetization and Python integration tests |
| Sanitizer checks | AddressSanitizer and UndefinedBehaviorSanitizer targets |
| Source build | Rebuilds vendored libDaisy into Forge's own build directory and links a BOOT_SRAM application |
| Test bundle generation | Packages firmware, controller, presets, audio references, docs and licenses with source identity and file hashes |
| Project continuity | Architecture, protocol, development guide, changelog, handoff and one hardware-test checklist |

## Controls and startup defaults

| Parameter | MIDI CC, channel 1 | Hardware encoder ID | Range | Startup target |
| --- | --- | --- | --- | --- |
| Wet/dry mix | 20 | SW1 | 0–100% | 0% / dry |
| Delay time | 21 | SW2 | 10–1000 ms | 257.5 ms |
| Feedback | 22 | SW3 | 0–85% | 21.25% |
| Output level | 23 | SW4 and SW6 | 0–1 gain | 0.25, faded in |
| Wet bypass | 24 | MIDI only | 0–63 off; 64–127 on | Off |

Encoder IDs follow the hardware source; printed-panel correspondence still needs
checking on the unit. SW5 and keybed actions are unassigned. CC changes use
7-bit values; complete patches use 14-bit normalized parameter words. SysEx is
channel-independent. There is no MIDI clock output or note processing.

Bypass preserves output gain and circulating delay state. Acknowledgement means
targets were accepted; it does not certify the audible result. If a request times
out, the patch may already be active—query status before retrying.

## Build summary and verification

Candidate 0.2 uses the Daisy Seed / STM32H750 and WAVE's hardware abstraction,
encoder driver, SRAM linker script and modified libDaisy. Upstream TAPE, TEMPO,
WAVE and bootloader source files are preserved separately.

| Build item | Recorded result |
| --- | --- |
| Toolchain | GNU Arm Embedded 10.3-2021.10 |
| Application type | BOOT_SRAM, loaded using the installed CHOMPI bootloader |
| Firmware binary | `FORGE.bin`, 100,592 bytes for the tested candidate |
| Link allocations | SRAM_EXEC 42.34%; SRAM 15.55%; RAM_D2 68.07%; SDRAM 0.57% |
| C++ verification | DSP/queue and protocol suites pass, including 100,000 concurrent transfers and 100,000 fuzz bytes |
| Python integration | 24 tests pass, including 250 random patch round trips through the actual C++ runtime |
| Sanitizers | ASan/UBSan pass; LeakSanitizer disabled due to the execution environment's `/proc` restriction |
| Host MIDI dependencies | Pinned dependency imports and message construction checked; physical ports untested |
| Model adapter | Request/response behavior tested with mocks; no actual model session yet |

Memory figures are linker allocations, not CPU measurements or worst-case stack
usage. No device flash, audio audition, USB enumeration, TRS I/O, measured load,
battery validation or stock restoration has been performed for this candidate.
See the [handoff](docs/forge/HANDOFF.md) for detailed evidence and limitations.

## Build from source

Required: Git, GNU Make, a C++14 compiler with pthreads, Python 3.10+, and GNU Arm
Embedded **10.3-2021.10**. Vendored libraries are already included. The native
build/tests were exercised on Linux; consult the [developer guide](docs/forge/DEVELOPMENT.md)
for platform notes and the [firmware guide](firmware/chompi-forge/README.md) for
compiler setup/checksum information.

```sh
git clone --branch forge/foundation https://github.com/DCDominguez/CHOMPI.git
cd CHOMPI
make -C firmware/chompi-forge test
make -C firmware/chompi-forge sanitize
make -C firmware/chompi-forge firmware GCC_PATH=/absolute/toolchain/path/bin
```

If the correct ARM toolchain is already on PATH, omit `GCC_PATH`. In containers
that prevent LeakSanitizer inspection, use
`ASAN_OPTIONS=detect_leaks=0 make -C firmware/chompi-forge sanitize` and record
that leak checking was disabled.

Outputs are in `firmware/chompi-forge/src/build/`: `FORGE.bin`, `FORGE.elf`,
`FORGE.hex` and `FORGE.map`. Forge's Makefile does not flash the unit; it rejects
`program*` and `flash*` targets at this development stage.

## Use the host controller

For the webapp, run this from the repository root:

```sh
python3 -m pip install -r firmware/chompi-forge/host/requirements.txt
python3 firmware/chompi-forge/host/forge_web.py
```

Open **http://127.0.0.1:8765** on the same computer. Choose OpenAI or Gemini,
enter your provider API key and a model ID that supports structured JSON, then
generate and review your patch. You can edit controls, import/export presets,
select MIDI ports, capture settings, and explicitly send to CHOMPI. Keys are
not saved by Forge. The Python bridge runs locally; no Node build is required.
Live provider and hardware tests are still pending.

The command-line controller remains available:

Run these commands from the repository root:

```sh
python3 -m pip install -r firmware/chompi-forge/host/requirements.txt
python3 firmware/chompi-forge/host/forge_host.py ports
python3 firmware/chompi-forge/host/forge_host.py validate firmware/chompi-forge/presets/03-long-echo.json
python3 firmware/chompi-forge/host/forge_host.py status --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
python3 firmware/chompi-forge/host/forge_host.py send firmware/chompi-forge/presets/03-long-echo.json --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
python3 firmware/chompi-forge/host/forge_host.py capture my-patch.json --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
```

Replace the port placeholders with the exact names from `ports`. USB supports
both directions; TRS requires a bidirectional connection through a MIDI interface
for acknowledgements. Use one host with one outstanding request at a time.
JSON-only commands do not require installing the MIDI packages.

The optional CLI authoring path uses a running Ollama server and an installed model:

```sh
python3 firmware/chompi-forge/host/forge_host.py ai "Long echoes with gentle repeats, output at 25 percent" --model YOUR_INSTALLED_MODEL --out my-ai-patch.json
```

Inspect the generated preset, then send it using the `send` command. The model
runs on the external host; it authors settings for the existing delay. The host
never installs models or automatically sends generated patches. See the
[host guide](firmware/chompi-forge/host/README.md) for all commands and configuration.

## Package and test the candidate

After building/testing and committing the source, generate a bundle outside the
repository from a clean checkout:

```sh
cd firmware/chompi-forge
python3 host/package_candidate.py /absolute/output/Forge_0.2_Test_Candidate.zip
```

The ZIP includes firmware, host tools, three presets, simulated reference audio,
docs and license notices. Its manifest identifies the exact source commit/tree
and file checksums. The packager requires existing firmware and harness builds;
it does not rerun the tests or establish hardware readiness.

Source, presets, tests and documentation are tracked in git. Binaries, generated
WAVs, build directories and test ZIPs are separate generated artifacts.

Before flashing, use the [single hardware-test checklist](docs/forge/TEST_SESSION.md):
back up the working card, retain a known-good stock restore path, use a separate
test card and the installed bootloader, and begin with low monitoring levels.
The repository's beta bootloader is not part of the Forge test procedure.

## Not implemented in 0.2

- Additional DSP algorithms, multi-effect chains or runtime routing graphs.
- The stock sampler/looper, sequencer, microphone processing or full performance UI.
- Device-side preset storage, automatic preset restoration after reboot, or a preset bank.
- Onboard AI, arbitrary generated DSP, scripts or hot-loaded executable code.
- Tab5 integration, Wi-Fi, networking on CHOMPI, or public web hosting.
- Automatic MIDI retries, unsolicited parameter streaming or protocol authentication.

The next step is the consolidated physical session. Results will guide further
effects and controller work; the broader roadmap is in the [project brief](docs/forge/PROJECT.md).

## Documentation and repository layout

| Path / guide | Contents |
| --- | --- |
| [`firmware/chompi-forge/`](firmware/chompi-forge/) | Forge firmware, core DSP/protocol, host tools, presets and tests |
| [Documentation index](docs/forge/README.md) | All Forge guides and current implementation status |
| [Architecture](docs/forge/ARCHITECTURE.md) | Audio path, state ownership, queues, patch lifecycle and source map |
| [Developer guide](docs/forge/DEVELOPMENT.md) | Build/test/package workflow and troubleshooting |
| [Protocol](docs/forge/PROTOCOL.md) | Exact requests, replies, error codes and overload behavior |
| [Test session](docs/forge/TEST_SESSION.md) | One-session checklist and results template |
| [Changelog](docs/forge/CHANGELOG.md) | Development milestones |
| [Handoff](docs/forge/HANDOFF.md) | Verified state, limitations and next action |

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
