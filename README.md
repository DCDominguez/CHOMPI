# CHOMPI + Forge

This fork preserves CHOMPI's open-source release and develops **Forge**, an
experimental, externally controlled audio-effects firmware for CHOMPI.

## Forge — start here

**Current status: candidate 0.2. Software tests and the ARM build pass;
physical hardware acceptance is pending.** Forge currently provides one stereo
delay engine, live encoder/MIDI control, host-managed presets, diagnostics,
and optional local-model patch authoring. It does not include TAPE's sampler UI.

Development is on [`forge/foundation`](https://github.com/DCDominguez/CHOMPI/tree/forge/foundation)
in [draft PR #1](https://github.com/DCDominguez/CHOMPI/pull/1), pending merge.

| I want to… | Read |
| --- | --- |
| Understand the project and find all docs | [Forge documentation](docs/forge/README.md) |
| Use the controller, presets, or optional AI adapter | [Host guide](firmware/chompi-forge/host/README.md) |
| Build or develop the firmware | [Developer guide](docs/forge/DEVELOPMENT.md) |
| Understand audio, MIDI, and state ownership | [Architecture](docs/forge/ARCHITECTURE.md) |
| Run the planned single hardware test | [Consolidated test checklist](docs/forge/TEST_SESSION.md) |
| Resume work from the latest verified state | [Handoff](docs/forge/HANDOFF.md) |

Source, presets, tests and documentation are tracked here. Firmware binaries,
audio simulations and test ZIPs are generated artifacts; the repository includes
the [packaging script](firmware/chompi-forge/host/package_candidate.py) to recreate
them. See the developer guide for prerequisites and commands.

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
