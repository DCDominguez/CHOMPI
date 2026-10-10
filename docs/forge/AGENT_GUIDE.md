# Forge for other agents (Muse / Cosmo on the Tab5)

Written 2026-10-10 for DC's Muse agent ("Cosmo", the Tab5 Muse gadget) so it can act as a
CHOMPI assistant: understand Forge, draw its menus and patches, design effects and edit
the firmware. The integration itself is DC's separate design. This page is the map;
the linked documents hold the exact details. Firmware here is **0.15.2**, software-tested,
**not yet hardware-verified** (CONTINUE.md is the live state; trust it over this page).

## 1. What Forge is

Community firmware for CHOMPI (Daisy Seed, STM32H750, 48 kHz, 24-frame blocks) that
keeps stock TAPE's workflow and adds a playable instrument:

- **Sources:** a virtual analog synth (two oscillators: sine / triangle / saw / square,
  interval and detune; noise; per-voice resonant low-pass with its own envelope; amp
  ADSR with velocity; LFO sine / triangle / square / S&H to pitch, filter or amp, mod
  wheel; glide; voice count per patch), a TAPE-compatible sampler with recording (up to
  seven voices), or the line input.
- **Chain:** source → stereo delay → FDN reverb → TAPE's saturation, warble, DJ filter
  and compressor → output. Looper (TAPE's) on PLAY / LOOP.
- **Music:** harmony mode (a key plays a chord, 9 modes), arpeggiator + bass on a clock
  (internal, tap, or MIDI clock in / out), an event recorder (notes and knob moves loop
  with overdub), projects (preset + parts + loop) on the SD card.
- **Computer side:** a JSON patch format (versions 1–7), a local webapp and AI authoring
  (OpenAI / Gemini with the user's key, or Ollama), the Forge Bridge (tests over USB).

The computer never uploads DSP code: a patch only sets parameters of modules already
in the firmware. **A new effect is firmware work** (section 6).

## 2. How to talk to CHOMPI

Everything is MIDI. Forge's requests are checksummed SysEx with a 14-bit sequence
number; every request gets one reply (PROTOCOL.md is exact, byte by byte).

| Opcode | What |
| --- | --- |
| 01 | Apply a whole patch (v1–v7); reply = status with the patch echoed |
| 02 | Status: current patch, CPU, drops, version, **build** and **battery** (0.15.2) |
| 03 | Panic |
| 04–07 | Device presets on the SD card: store, recall, erase, list (8 banks × 15 slots) |
| 08–09 | Samples on the card: list; save the recording, erase, copy |
| 0A / 0B | **Development builds only:** inject panel events (virtual keys / knobs) and read the Inspector pages |
| 0C | USB file transfer and firmware install (install needs the CHOMPI key pressed) |

Plus ordinary channel-1 MIDI: notes, CC (knob CCs match stock: MANUAL section 9),
program change (recalls presets), MIDI clock.

Three ways in, from easiest to most direct:

1. **Python host** (`firmware/chompi-forge/host/forge_host.py`): `status`, `send
   <patch.json>`, `capture`, `store`, `recall`, `battery`, `schema` … The reference
   implementation of every message (encode / decode / validate).
2. **The local webapp** (`host/forge_web.py`): HTTP JSON API (`/api/status`, `/api/send`,
   `/api/generate`, `/api/preset`, `/api/samples`, `/api/bridge/*`). **Loopback only by
   design** (origin check + per-session token): a Tab5 on Wi-Fi cannot call it as-is.
   Opening it to the network is a security decision for DC, not a config change.
3. **USB MIDI from the Tab5 directly** (its USB host to CHOMPI's USB port): speak the
   SysEx protocol yourself; port `forge_host.py`'s `message`, `encode_patch`,
   `decode_response`, `decode_identity`, `decode_power`. CHOMPI has one USB port, so
   the Tab5 then replaces the computer (and its charging) on that port.

Rules for any client: one client at a time on a port; wait for the reply (or a timeout)
before the next request; after a timeout, query status before resending (a patch may
have applied); never send a patch the host validator rejects.

## 3. Patches

A patch is JSON. Get the exact schema with `python host/forge_host.py schema
--instrument` (v7). Versions add modules, never break old ones (v1 delay-only presets
still load):

| Version | Adds |
| --- | --- |
| v1 | stereo delay on the line input (`engine: stereo_delay`) |
| v2 | simple synth (waveform, cutoff, envelope) |
| v3 | modules: `synth` (2 osc, noise, voices, glide), `filter` (+ envelope), `lfo`, `delay`, `reverb`, `output` |
| v4 | `sampler` (TAPE slots, mode, bank, slot, loop, reverse …) |
| v5 | `knobs`: what the four knobs do on their extra page |
| v6 | `harmony` (mode, tonic, extension, layout, voicing) |
| v7 | `parts` (arp pattern / rate / octaves / gate / latch, bass mode / rate / octave, tempo, seed, clock out) |

`forge_host.upgrade_patch` converts old to new; `validate_patch` is strict (keys, types,
ranges). The 14 starter presets are in `firmware/chompi-forge/presets/` (6 are still
v1/v2; upgrading them is on the backlog, PROJECT.md).

## 4. The panel, for visualizations

CHOMPI's controls: 25 keys (MIDI 48–72), CHOMPI key, PLAY / LOOP, a toggle, six
encoders with push (SW1–SW6), RGB lights on all of them.

- **Key map:** `core/panel_controller.h` `panel::kKeyNotes` (button index → note),
  `SlotLed` / `BlackLed` (key → light index). 26 lights: 0–24 keys, 25 CHOMPI.
- **What each knob page does:** KNOBS.md (table per patch type) and
  `core/knob_layout.h` (`Pages`, `Target`, step sizes, page colours `KnobColour`).
  Pages step on knob release; a 1.5 s hold resets; SW4 + SW3 held 1 s = panic.
- **Menus** (MANUAL sections 6–8c): toggle down + hold CHOMPI opens TAPE's menu page;
  hold KEY_22 1 s = Forge presets page; hold KEY_21 1 s = harmony page, KEY_21 again =
  parts page (arp / bass / tempo / event recorder).
- **How the lights are computed:** pure functions in `core/panel_controller.h`
  (`LedView` → `ComposeLeds`, `RenderPlayLeds`, `ComposeKnobLeds`,
  `ComposeMenuKnobLeds`, `ComposeHarmonyKnobLeds`, `ComposePartsKnobLeds`). The same
  code runs on the device, in the tests and in the simulator, so a visualization can
  mirror it exactly.
- **Live state to draw from** (Inspector, development builds, opcode 0B; PROTOCOL.md
  "Inspector pages"): page 1 the 26 light colours as commanded; 2 system (version, build,
  CPU, battery, restarts); 3 panel (keys held, toggle, packed menu word, knob pages);
  4 engine (seven voices, envelopes, cutoff, LFO); 5 storage; 6 recent events; 7 the
  live patch; 8 harmony (last chord); 9 parts (arp set, tempo, clock); 10 event recorder.
  `host/forge_inspector.py` decodes all of them; `host/web/inspector.html` is a working
  browser view to borrow from.
- Release builds have no Inspector: status (02) still gives patch, build and battery.

Good visualizations to start with: a keybed showing held keys and chord / arp notes
(pages 1, 4, 8, 9), the four knob rings with their page names and values (3, 7 +
KNOBS.md), the patch as a signal chain (7), CPU and battery (2).

## 5. Where things live (`firmware/chompi-forge/`)

| Path | What |
| --- | --- |
| `src/forge_main.cpp` | Boot, audio callback, transports, SD card, lights, safe mode |
| `core/engine.h` | The audio path and whole-patch application |
| `core/synth.h` | Voices: oscillators, sampler voices, envelopes, filter, LFO, glide |
| `core/reverb.h`, `core/tape_fx.h` | FDN reverb; TAPE's saturation, warble, DJ filter, compressor |
| `core/parameters.h` | Parameter ids, ranges, validation, CC map |
| `core/protocol.h` | Wire format: requests, patch field tables, status, identity, power |
| `core/panel_controller.h`, `core/knob_layout.h`, `core/preset_menu.h` | Panel, knob pages, menus, lights |
| `core/harmony.h`, `core/parts.h`, `core/sequencer.h` | Harmony, clock / arp / bass, event recorder |
| `core/looper.h`, `core/recorder.h`, `core/sample_*.h` | Looper, recording, sample loading |
| `core/inspector.h` | Development telemetry pages |
| `host/` | Python host, webapp, AI adapters, bridge, Inspector, probe (simulated CHOMPI) |
| `tests/` | 16 native C++ suites + Python suites |
| `bench/` | CPU benchmark against stock firmware (emulated ARM) |

A code knowledge graph of all this is in `graphify-out/` (`graphify query "…"`,
`graphify explain "<name>"`; AGENTS.md).

## 6. How to make an effect

The checklist the existing modules followed (the v7 parts change touched ~25 files):

1. **DSP** in a new `core/<effect>.h`: header-only, allocation-free, caller-owned memory,
   bounded loops, no I/O. A bypass that costs nothing when off. Control-rate code
   `FORGE_COLD`; per-sample code tight.
2. **Memory:** SDRAM is full (387 KB free). Short delay lines (chorus / flanger) in
   internal SRAM / DTCM; anything long needs memory found first (RESOURCE_LEDGER.md).
3. **Parameters:** `core/parameters.h` (id, range, default, validation, CC if any).
4. **Engine:** call it at its place in `core/engine.h`'s chain.
5. **Patch format:** a new version vN: `core/protocol.h` field table and sizes
   (`kMaxRequest` / `kMaxReply`), `host/forge_host.py` field table, schema, validator,
   `upgrade_patch`; `host/forge_ai.py` provider schema; `host/web/app.js` editor;
   presets. Old versions keep loading unchanged.
6. **Panel:** a knob page (`core/knob_layout.h`) or a menu page; the lights.
7. **Tests:** native (extend `synth_test` / `v3_test`, `knob_audio_test`, `protocol_test`),
   `tests/test_host.py` round trip, sanitizers. `make test` must stay green.
8. **CPU:** add the worst case to `bench/forge_bench.cpp`; `make bench` gates against
   stock WAVE's figure. The device figure is still unmeasured, so keep a cheap mode.
9. **Docs:** PROTOCOL, MANUAL, KNOBS, CHANGELOG, CONTINUE, RESOURCE_LEDGER, TEST_SESSION
   (+ `host/bridge_checks.json`; `test_consistency.py` checks they agree).

The planned effects (chorus / flanger, phaser, lo-fi, tremolo / auto-pan, fuller delay
and reverb) are in PROJECT.md "Backlog".

## 7. How to edit the firmware

- **Build:** from `firmware/chompi-forge/`: `make test` (native + Python), `make
  sanitize`, `make firmware` (release) and `make firmware-dev` (development, with test
  hooks), `make bench`. Arm GCC **10.3-2021.10** (pinned; `GCC_PATH=…/bin`). On DC's PC:
  `D:\tools\arm-gcc-10.3`, MinGW in `D:\tools\mingw-16.2.0`, Python venv
  `D:\tools\forge-venv` (keep everything off C:). CI runs the whole gate on every push.
- **Rules** (AGENTS.md, DC's): work on `forge/foundation`, never touch `main`, never
  force-push; leave upstream firmware (TAPE / WAVE / TEMPO, bootloader, vendored
  libDaisy) untouched; no allocation or blocking I/O in the audio callback; old presets
  keep working; update CONTINUE at each checkpoint; commit messages separate implemented
  / software-tested / hardware-verified; never claim a hardware result DC did not see.
- **Installing:** agents do not install firmware. DC installs: first time by SD card
  (`FORGE.bin` on the card root, every other `.bin` renamed), then over USB (Forge Bridge
  or `forge_card.py install`, confirmed with the CHOMPI key). Battery green or white
  first (`forge_host.py battery`, or hold SW6 2 s).
- **Safety nets:** the build id tells builds apart; safe mode starts without the card's
  settings after three quick crashes; `FORGE/RESTARTS.TXT` logs every start and crash.

## 8. Read next

[CONTINUE.md](CONTINUE.md) (live state) · [PROJECT.md](PROJECT.md) (scope, backlog) ·
[MANUAL.md](MANUAL.md) (player's view) · [PROTOCOL.md](PROTOCOL.md) (wire) ·
[KNOBS.md](KNOBS.md) · [ARCHITECTURE.md](ARCHITECTURE.md) (written at 0.5; the source map
above is current) · [HARMONY_BRIEF.md](HARMONY_BRIEF.md) · [TAPE_CONTROLS.md](TAPE_CONTROLS.md).
