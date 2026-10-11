<p align="center"><img src="../../docs/forge/assets/choowi.svg" width="140" alt="Choowi"></p>

# Choowi: the first firmware of the forge

This folder is **Choowi**, the first firmware line in the Forge family of custom CHOMPI
firmware. Current release: **Choowi 0.19.2, release candidate**
([release page](https://github.com/myth-world/Forge/releases/tag/choowi-0.19.2-rc1)).

| Folder | What's in it |
| --- | --- |
| `core/` | The instrument, hardware-independent: synth, sampler, looper, effects, harmony (and its secret modes), arp / bass, event recorder, magic, panel controller, protocol |
| `src/` | The Daisy Seed app: audio callback, MIDI, SD card, LEDs, power, install |
| `host/` | The computer side: `forge_host.py` (CLI), `forge_card.py` (card files and installs over USB), `forge_battery_log.py`, the local workshop (`forge_web.py`, `web/`), Forge Bridge, `make_sample_pack.py`, `build_site.py` |
| `presets/` | Starter sounds (JSON, every patch version) |
| `samples/choowi-pack/` | Choowi's own sounds: chromatic and kit bank **purple** (CC0) |
| `tests/` | Native C++ suites, Python host tests, browser tests |

What it does for players: the [user manual](../../docs/forge/MANUAL.md) and the
[guide](../../docs/forge/GUIDE.md). Everything else: the [docs index](../../docs/forge/README.md).
The folder keeps its old name (`chompi-forge`) so builds and links keep working.

## Build and test

From this folder:

```sh
make test                 # every C++ suite and the Python host tests
make sanitize             # the suites under AddressSanitizer / UBSan (Linux / macOS)
make browser-test         # the workshop and bridge pages in Chromium (needs Playwright)
make firmware             # the release build: src/build/FORGE.bin
make firmware-dev         # the development build (Inspector, battery diagnostics): src/build-dev/FORGE.bin
```

- **Arm compiler:** GNU Arm Embedded **10.3-2021.10** on `PATH` (or `GCC_PATH=…/bin`). Both
  firmware targets end with `layout OK` from `host/check_firmware_layout.py`; sizes are tracked in
  [RESOURCE_LEDGER.md](../../docs/forge/RESOURCE_LEDGER.md).
- **Windows:** MinGW-w64 g++ for the native tests needs `CXX="g++ -D_USE_MATH_DEFINES"`; a
  `python3` on `PATH` is needed by the layout check. Some antivirus programs quarantine the freshly
  built test programs in `build/`.
- The firmware links the vendored WAVE libDaisy (built into `build/libdaisy/`) and never touches
  the bootloader; the Makefile has no flash targets. Installs go through the card or USB
  (`host/forge_card.py install`, which needs a press of CHOMPI's key on the panel).

## A release

1. `make test` and both firmware builds pass; the docs say what changed (CHANGELOG, MANUAL).
2. `python host/make_sample_pack.py` (only when the sounds changed) and
   `python host/build_site.py` (when the workshop controls, the chatbot prompt or the presets
   changed; `tests/test_site.py` / the site checks fail when they are stale).
3. The community card zip: `src/build/FORGE.bin`, `samples/choowi-pack/*.wav`, the illustrated
   manual as `MANUAL.html` and a `README-FIRST.txt` (backup, empty the samples, copy, power on).
4. The GitHub release gets the card zip, `FORGE.bin`, `samples/choowi-samples.zip` and the manual.
   `main` carries the release commit; GitHub Pages serves `docs/` (the landing page and the forge
   page at `/workshop/`).

## Where the details live

- Panel, knobs, presets, samples, harmony, looper, power: [MANUAL.md](../../docs/forge/MANUAL.md),
  [KNOBS.md](../../docs/forge/KNOBS.md), [TAPE_CONTROLS.md](../../docs/forge/TAPE_CONTROLS.md).
- USB / MIDI messages and patch formats (v1-v7): [PROTOCOL.md](../../docs/forge/PROTOCOL.md);
  host tools: [host/README.md](host/README.md).
- How the code is organised (audio owner, queues, no allocation in the callback):
  [ARCHITECTURE.md](../../docs/forge/ARCHITECTURE.md); setting up a checkout:
  [DEVELOPMENT.md](../../docs/forge/DEVELOPMENT.md).
- What is checked on hardware and what is software-tested only:
  [TEST_RESULTS.md](../../docs/forge/TEST_RESULTS.md); the live checkpoint:
  [CONTINUE.md](../../docs/forge/CONTINUE.md).

## Rules that stay

`core/` is hardware-independent and allocation-free; DSP state belongs to the audio callback,
which never blocks or allocates. Patches are validated before they are queued and applied whole.
The host / device wire format and its tests change together. v1 delay presets stay usable.
Upstream firmware, libDaisy and the bootloader are not modified.
