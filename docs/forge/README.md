# Forge documentation

Latest development addition: [Forge Inspector](INSPECTOR.md), a read-only viewer
of running firmware state for the physical test. Current validation and limits
are in [CONTINUE.md](CONTINUE.md); Inspector is development-only.

Forge is a community firmware project for CHOMPI, built on its Daisy Seed audio
hardware. Candidate **0.5** implements a playable synth (up to four voices, two oscillators, noise, resonant
filter with envelope, LFO, glide), a TAPE-compatible sampler with recording, SD-card presets,
stereo delay and reverb, live controls, atomic patch
recall, host-managed JSON presets, and diagnostics. The computer can optionally
author presets through a local OpenAI/Gemini webapp with a user-supplied API
key, or through the optional Ollama CLI.

**Software-tested; not yet hardware-verified.** The candidate has not been
flashed or auditioned on DC's CHOMPI. Actual model use also remains unverified;
the adapters have been tested using mock responses. The next acceptance step is
one consolidated hardware session.

## Documentation map

| Document | Purpose |
| --- | --- |
| [Agent resume checkpoint](CONTINUE.md) | Current implementation, evidence, known limits and exact next tasks |
| [Project brief](PROJECT.md) | Goals, scope, decisions and milestones |
| [Firmware guide](../../firmware/chompi-forge/README.md) | Build commands, audio routing, parameter/CC mapping and defaults |
| [Host guide](../../firmware/chompi-forge/host/README.md) | Launch the webapp, configure providers, edit/save presets and control MIDI |
| [Architecture](ARCHITECTURE.md) | Processing boundaries, ownership, queues, patch lifecycle and source map |
| [Developer guide](DEVELOPMENT.md) | Checkout, dependencies, software validation, packaging and troubleshooting |
| [Protocol](PROTOCOL.md) | Exact SysEx framing, requests, replies, errors and overload semantics |
| [Compatibility](COMPATIBILITY.md) | Comparison with stock TAPE/TEMPO/WAVE: bootloader, memory, MIDI, keybed, CPU benchmark |
| [Looping](LOOPING.md) | Looper (roadmap item 5): TAPE behaviour, keys, memory, CPU budget, saving a loop |
| [Sampling](SAMPLING.md) | Sampler design (roadmap item 4): TAPE behaviour kept, what Forge changes, memory, v4 patch, panel |
| [Test session](TEST_SESSION.md) | One physical acceptance checklist and results template |
| [Handoff](HANDOFF.md) | Latest implementation, evidence, limitations and next action |
| [Changelog](CHANGELOG.md) | Changes by candidate version |

## Choose a starting point

**To try the candidate:** start with the test-session checklist. Prepare the
computer with the host guide before scheduling the single physical session.

**To contribute code:** read the architecture and developer guide, then consult
the handoff for unfinished work. Keep the first candidate bounded until physical
results justify expansion.

**To author a patch:** use a preset in
[`firmware/chompi-forge/presets/`](../../firmware/chompi-forge/presets/) as an
example, and run the host's `validate` command. JSON describes settings for the
installed synth/delay modules; it does not contain executable DSP.

## What is implemented and what is pending

| Area | Implemented | Remaining evidence or scope |
| --- | --- | --- |
| Audio | v3 synth (oscillators, noise, resonant filter, LFO, voices/glide), two source routes, stereo delay and reverb | Physical routing, listening and CPU headroom (worst case: TEST_SESSION 6.2b) |
| Controls | Keybed/MIDI notes, encoders, MIDI CC, panic, USB/TRS patch and status protocol | Actual encoder mapping, USB enumeration and TRS I/O |
| Presets | Host JSON save/capture and atomic recall | Audible transitions and capture/recall on the unit |
| Diagnostics | Callback CPU average/peak, drop/reject counters | Measured device performance under normal use |
| AI authoring | OpenAI/Gemini webapp and Ollama CLI; strict validation and saved JSON | Live model availability, latency and musical interpretation |
| Persistence | JSON files on the computer; device presets on the SD card (panel menu, program change, host) | SD timing, card swap and LED feedback on the unit |
| Expansion | Versioned patch formats (v1–v3) and roadmap in PROJECT.md | Sampler/looper, more effects, general graphs, Tab5 and Wi-Fi are not implemented |

The repository contains source and documentation. Generated test bundles remain
separate; their manifests identify the exact source commit/tree and file hashes.
Later documentation changes do not silently update an already-generated bundle.

## Maintaining these docs

When behavior changes, update the firmware/host guide and changelog. When wire
formats change, update the protocol and its version rules alongside both ends
and their tests. Put new verification results and the next task in the handoff.
Use the results template after the hardware session; never turn an unrun test
into a pass. Avoid duplicating exact parameter mappings across multiple guides.

Live provider procedure: [LIVE_AI_TEST.md](LIVE_AI_TEST.md).
