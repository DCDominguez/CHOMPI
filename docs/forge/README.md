<p align="center"><img src="assets/choowi.svg" width="140" alt="Choowi"></p>

# the forge docs

Everything about **Forge**, the workshop of custom CHOMPI firmware, and **Choowi**, its first
firmware line, written down. Current release: **Choowi 0.20.0, release candidate 3 (community testing)**
([download](https://github.com/myth-world/Forge/releases/tag/choowi-0.20.0-rc3): the card zip has
the firmware, Choowi's own sounds and the illustrated manual).

- **Playing it:** the [guide](GUIDE.md) ("how do I…?"), the [illustrated manual](ILLUSTRATED_MANUAL.html)
  (open in a browser; also `MANUAL.html` in the card zip) and the full [user manual](MANUAL.md).
- **Making sounds:** [Choowi's forge](https://myth-world.github.io/Forge/workshop/): describe a sound,
  any chatbot writes it, hear it live on CHOMPI, save it to CHOMPI.
- **Installing:** the card zip's README-FIRST, or [MANUAL section 2](MANUAL.md#2-installing-and-updating).
- **What's checked on a real CHOMPI and what's software-tested only:** [TEST_RESULTS.md](TEST_RESULTS.md).
- **What changed:** [CHANGELOG.md](CHANGELOG.md). **Where the work is now:** [CONTINUE.md](CONTINUE.md).
- **The family** (names, mascots, tools, colours, voice): [FORGE_FAMILY.md](FORGE_FAMILY.md).

## Documentation map

| Document | Purpose |
| --- | --- |
| [Guide](GUIDE.md) | Task by task: play, save, presets, samples, harmony, arp, looper, recorder, trouble |
| [User manual](MANUAL.md) | The reference: install, power, panel, knob pages, presets, samples (banks by colour), harmony, parts, looper, recorder, MIDI, the forge page, troubleshooting |
| [Illustrated manual](ILLUSTRATED_MANUAL.html) | The storybook tutorial and panel diagrams |
| [Knobs](KNOBS.md) | Every knob page and the v5 patch knob choices |
| [TAPE controls](TAPE_CONTROLS.md) | What every stock TAPE control does (parity reference, with source lines) |
| [Sampling](SAMPLING.md) · [Looping](LOOPING.md) · [Harmony brief](HARMONY_BRIEF.md) | Design notes for the sampler, the looper and harmony |
| [Battery readings](BATTERY_READINGS.md) | How CHOMPI reads its battery, why it can mislead, the instrumented build and its log |
| [Protocol](PROTOCOL.md) | USB / MIDI SysEx: framing, requests, replies, patch formats v1-v7, errors |
| [Architecture](ARCHITECTURE.md) · [Developer guide](DEVELOPMENT.md) | How the code is organised; setting up, building, testing, packaging |
| [Firmware README](../../firmware/chompi-forge/README.md) · [Host guide](../../firmware/chompi-forge/host/README.md) | Build targets and the release steps; the computer-side tools |
| [Resource ledger](RESOURCE_LEDGER.md) | Firmware sizes, memory and CPU, per release |
| [Compatibility](COMPATIBILITY.md) | Compared with stock TAPE / TEMPO / WAVE: bootloader, memory, MIDI, CPU |
| [Inspector](INSPECTOR.md) · [Bridge](BRIDGE.md) | Development tools: the live device viewer and the Windows test bridge |
| [Guide for other agents](AGENT_GUIDE.md) | Outside agents (Muse / Cosmo on the Tab5): what Forge is and how to talk to it |
| [Project brief](PROJECT.md) · [UX review](UX_REVIEW.md) | Goals, decisions, backlog; ease-of-use ratings |
| [Storage streaming](STORAGE_STREAMING.md) | Forge v2 study (parked): streaming samples from the SD card |
| [Changelog](CHANGELOG.md) · [Test results](TEST_RESULTS.md) · [CONTINUE](CONTINUE.md) · [Handoff](HANDOFF.md) | History, evidence, the live checkpoint |

Older, kept for the record: [TEST_SESSION.md](TEST_SESSION.md) and [HOME_CHECKLIST.md](HOME_CHECKLIST.md)
(the 0.15-era hardware session; the 0.19 hands-on checks are in TEST_RESULTS), and
[LIVE_AI_TEST.md](LIVE_AI_TEST.md) (API-key provider checks; the workshop no longer uses keys).

## Where Choowi stands (0.20.0 release candidate 3)

| Area | On the owner's CHOMPI | Software-tested only / open |
| --- | --- | --- |
| Playing | Synth, sampler, recording, looper, presets, harmony, arp / bass, recorder, page colours, wake-up chime, surprise me, mutate, the synth page, save and recall, sample start / end | The TAPE functions' final check; SW5 sweep, chomp, ripples by hand |
| Hidden harmony modes (kept out of the player manuals; CHANGELOG 0.19.2) | Installed (build b462bea) | Not yet judged by ear |
| Dream mode | | Not seen on a computer-connected CHOMPI (any MIDI keeps it awake): test unplugged |
| Choowi's sounds | On the card (bank purple from the card zip) | Judged by ear |
| The forge page | Live on GitHub Pages | Tested against the firmware's decoder with a Web MIDI stand-in; first real-CHOMPI run pending |
| Power | Battery-dip guard installed; diagnostics log works | A shut-off not yet caught by the log; power-bank charging while on; SW6 colour is rough |

## Maintaining these docs

When behaviour changes, update the manual, the guide (and the illustrated manual when it shows
it) and the changelog. When the wire format changes, update the protocol, both ends and their
tests together (and `docs/workshop/forge-wire.js`). Hardware results go in TEST_RESULTS only after
they were run; never turn an unrun check into a pass. In the repo the user is "the owner"; public
credit is DeeCee.
