# Forge — developer resume checkpoint

Updated 2026-10-03 (UTC), checkpoint after roadmap items 1–4 (sampler,
firmware 0.5). Read this first.

## Current checkpoint: looper (roadmap item 5), 2026-10-03

DC: "proceed with the looper build but run the design doc with me first".
Design and decisions: [LOOPING.md](LOOPING.md) (D1 ~83 s stereo with the sample
pool at 32 MiB, D2 SW5 context, D3 TAPE parity + save loop to a slot, D4 LOOP
ends the first take into overdub). Built in four steps, each tested:
1. `core/looper.h` + `tests/looper_test.cpp` (gestures, seam, overdub/feedback,
   fades, varispeed/reverse/scrub, auto-close, panic, limiter, CC buttons,
   random use; 8 mutations caught).
2. Engine/panel/MIDI/firmware wiring; CC 26/27/24; menu feedback and effects
   position; SDRAM; voice cap 6 while writing (CPU gate; 5 mutations caught).
3. Save the loop to a sample slot (menu gesture, locked loop, loader
   `from_loop`); Inspector page 5 looper fields (94 bytes ≤ 96).
4. TEST_SESSION 3E (3.42–3.51) + 6.2d, bridge checks (73, generated from the
   table; consistency test), automatic checks 3.42 (audio) and 3.42s–3.45
   (virtual keys, real time only), docs.

Checks 2026-10-03: `make test` 9 native suites PASS, Python tests OK; `make
sanitize` 9 PASS (detect_leaks=0); `make browser-test` OK; `make bench
--check` PASS (looper worst case 2,555 ≤ WAVE 2,695); ARM release 223,444 B
(77.4 %) and development 239,632 B with the layout check; SDRAM 99.4 %.
Exact figures in the commit messages. Hardware: none. Risks: real CPU of 7
voices + looper (6.2d decides the cap), SDRAM bandwidth, KEY_27/28 and LEDs
7/8 (taken from TAPE's source), loudness of overdub stacking.


DC asked to review the new community CHOMPI firmware forks (sfaber02,
lnetzel, ugrossek, xNeoclox, sthompsonjr; upstream CHOMPI-Club unchanged) and
then to apply the boot fix and cleanup. Findings and the feature ideas list are
in the chat summary; ideas not yet built are listed under "What's left".

**Firmware fix (release and development): the "64 MHz" boot bug.** Found by
sfaber02 (hardware-tested on TAPE/TEMPO/WAVE). CHOMPI's linker scripts, and
Forge's copy, had no BACKUP_SRAM region, so libDaisy's `boot_info` linked into
uninitialised D1 SRAM (Forge: 0x2404df98). DaisySeed::Init() reads the
bootloader version there before clock setup; a 0 skips clock and SDRAM setup
(64 MHz, white LEDs, SD timeouts, no audio), depending on leftover RAM per
unit/build. `src/forge_sram.lds` now has libDaisy's reference BACKUP_SRAM
region and `.backup_sram (NOLOAD)` section: `boot_info` links at 0x38800000.
Its static initialiser does not touch it (checked in the disassembly).
`host/check_firmware_layout.py` runs after every `make firmware`/`firmware-dev`
and fails the build if `boot_info` moves or the image fails the bootloader's /
launcher's checks. `src/Makefile` now relinks when the linker script changes
(before, a script edit was silently ignored).

Cleanup: development probe page 0 retired (no host read it; the Inspector
carries the same state); the three audio-callback atomics only it used are
gone. New `tests/test_consistency.py`: bridge checklist vs TEST_SESSION ids
(found and fixed drift in section 1A), automatic-check ids, Inspector key table
vs firmware, linker script, layout checker. One shared `shared_words` helper.
Stale docs fixed (README 0.4, DEVELOPMENT suites/layout, code headroom).

Checks 2026-10-03: `make test` 8 native suites PASS, 92 Python tests OK;
`make sanitize` 8 PASS (detect_leaks=0); `make browser-test` 11 + 4 OK; ARM
(xPack 10.3.1) release FORGE.bin 213,716 B (74.0 %), sha256 `a7997e4c…ef793`;
development 229,404 B (79.4 %), sha256 `627c8039…f16fd`; layout guard OK for
both. Bench not rerun: DSP engine code unchanged. Hardware: none; the boot fix
is verified on hardware only for the stock firmwares (by sfaber02), not Forge.

Kits built from `1d1618e` (xPack GCC 10.3.1), both `verify_bundle.py` OK and
both FORGE.bin images pass the layout check:
- `Forge-Bridge-dev-1d1618e.zip` (development + bridge + bundled Windows Python),
  26,341,616 B, sha256 `513f797b…85266d`, FORGE.bin `627c8039…f16fd`, 1,290 files.
- `Forge-0.5-test-1d1618e.zip` (release QA bundle), 3,515,836 B, sha256
  `41f9af6b…6544ae3`, FORGE.bin `a7997e4c…ef793`, 65 files.
Sent to DC in chat; DC keeps them on Google Drive. Older kits
(`Forge-Bridge-dev-50cdc4b`, `Forge-0.5-test-b7a504a`) lack the boot fix: do
not flash them.


DC reviewed the bridge and asked for it to be "more plug and play". Built on
DC's (ChatGPT session's) browser bridge at `7fa8624`, which an agent reviewed
first: all firmware changes are `FORGE_TEST_HOOKS`-only; release FORGE.bin is
byte-identical (`f0a18b13…`, reproduced on Linux), development `c2a4fb3d…`
reproduced; 8 native, sanitizer (all 8, first Linux run) and browser suites pass.
An earlier agent-only CLI bridge (local branch, never pushed) was folded in as
`host/forge_audio.py` instead of being pushed separately.

What is new (host only; firmware unchanged):
- **Connect CHOMPI** finds the ports itself (`discover`): only ports named
  CHOMPI/Daisy are opened (USB product string "CHOMPI"), confirmed by a Forge
  status reply; clear messages when none is found or Forge does not answer.
  Manual port choice stays available (DIN/UART).
- **Automatic checks** card: Find audio interface (plays a C4, listens on each
  input; then a −18 dBFS 1 kHz beep, interface's own outputs first) and Run
  automatic checks (`host/auto_checks.json`, 19 TEST_SESSION steps: pitch,
  clicks, levels, panic silence, menu/LED state via Inspector, samples, line-in
  recording, CPU). Background jobs on the server with progress, cancel,
  spectrograms on demand, results in the session export and `reports/<time>/`.
  Without numpy/sounddevice or an interface, audio steps are skipped.
- `python host/forge_audio.py run`: the same session from a terminal (for an
  agent on DC's PC).
- **Development kit ships Python** (NuGet `python` 3.12.10, PSF-signed binaries,
  plus mido/python-rtmidi/numpy/sounddevice and Microsoft-signed msvcp140.dll,
  all pinned by SHA-256 in `host/windows-runtime.json`; fetched by
  `host/fetch_windows_runtime.py`, bundled by `package_candidate.py
  --development --windows-runtime`). Reason found during this work:
  python-rtmidi 1.5.8 has **no Windows wheel for Python 3.13/3.14**, so the
  previous launcher fails on a fresh PC with current Python. The source-checkout
  launcher now picks 3.10–3.12 and explains otherwise. `*.cmd` checked out CRLF.

Checks run 2026-10-03 (Linux container): `make test` 8 native suites PASS,
87 Python tests OK (14 new); with numpy hidden 87 OK, 11 skipped; `make
browser-test` 11 + 4 OK (new: automatic checks in simulation); 8 targeted
mutations of the new code all caught. Simulated autorun: 6 pass, 13 skipped
(no audio), 0 fail. DLL imports of every bundled native module resolved
against the kit (found and fixed: msvcp140.dll for rtmidi).

Development kit: `Forge-Bridge-dev-50cdc4b.zip` built from `50cdc4b` (26,340,768
bytes, sha256 `be7eed92…c17ec997`; FORGE.bin = development `c2a4fb3d…`, xPack
GCC 10.3.1; `verify_bundle.py`: 1,290 files OK). Sent to DC in chat; DC keeps it
on Google Drive. Unpacked on Linux, its own host code ran the simulated session
from the kit layout and still verified OK afterwards.

Not verified: anything on Windows (launcher, bundled runtime, WASAPI device
handling, MIDI port names), any real audio interface, detection on real
hardware, measurement thresholds against real CHOMPI output. No hardware test.


### Browser hardware test bridge (supersedes terminal-only delivery below)

Starting head `4aa4d82da7f778d42f6e586b2904e1eb5625fb3e` was confirmed remotely.
The user requested the full test workflow. [BRIDGE.md](BRIDGE.md) describes the
new `/inspector` page and Windows launcher: four live state groups, physical and
logical keys, encoders, LED shadow, voices, events, explicit patch/panel/MIDI/
storage controls, 62 guided hardware checks, notes and JSON/JSONL session export.
One server session owns MIDI for all pages/actions; timeout disconnects without
retry. Override/pedal cleanup is best-effort on disconnect or browser expiry.
Reports retain 1,200 trace records plus all check evidence, and expose omissions.
Simulation uses the same C++ probe and decoder, remains visibly labelled, and
never establishes a physical pass. The same browser tab can recover reports
after reload. The Windows launcher creates an ignored local Python environment
and uses OS certificate trust when installing pinned MIDI dependencies.

Host fixes: correctly parse string routing for sampler knob labels; bounded
draining of rejected HTTP bodies resolves Windows resets without weakening the
existing 400/403 assertions. The original terminal-only checkpoint's HTTP test
failure is now fixed. No firmware/protocol/DSP changes in this increment.

Validation: 73/73 Python tests PASS (11 bridge tests); 3/3 new Chromium bridge
tests and 11/11 existing workshop Chromium tests PASS. Desktop/mobile layouts,
injection versus physical labels, report downloads/reload, MIDI cleanup,
exclusive access, storage writes, timeout and bounded retention exercised with
the real C++ simulator. No live AI or physical CHOMPI test was performed.
Firmware hashes remain unchanged: development `c2a4fb3dc91b6e7046a45be31cb02ddd010ee5c7c903e3ed748c162ca3ee87df`,
release `f0a18b13d95a328f92b060f35d746ae7f4b32781959d25a17e0954d5b7b682e1`.
No ARM rebuild required for host-only work; earlier build evidence remains valid.
Development footprint 229,912 B / D1 SRAM 96,564 B; release 213,952 B / 92,588 B.
Next: try the labelled simulation UI, then run the consolidated physical session
with the development candidate, starting with C3 and SW4, followed by Glass Keys.

### Prior telemetry implementation and validation

This checkpoint supersedes the earlier validation/size figures below.
User scope for this session was observability, with no new musical/DSP features.
Starting remote head was verified as `7e8fd1955872ed91fba9a44b76dbb145b93f677b`.
Work stays on `forge/foundation`; PR #1 remains draft, main untouched, no flash.

Publishing checkpoint: implementation commit `e1723a5d7dfd7b94ebe865c1a1e07ecfa80a3582`
and checkpoint commit `9e3bbf758e3e30a099285356913908731cd586e1` were published
to `forge/foundation` after the user refreshed GitHub authentication. The remote
was rechecked at the supplied `7e8fd195…` head before the normal fast-forward
push; PR #1 was verified open and draft at `9e3bbf7…`. Main remained
`a73d732613da684e4de844619b690776f0f50ccf`. No force push or merge occurred.
The exact development firmware, report and source patch are preserved in this
chat's outputs. Hardware verification remains pending.

- [INSPECTOR.md](INSPECTOR.md) contains the audit, field availability, ownership,
  exact checks, memory measurements and the exact next physical test.
- One reusable development model (`core/inspector.h`) feeds new probe pages
  2 SYSTEM, 3 PANEL, 4 ENGINE, 5 STORAGE, 6 EVENTS and 7 PATCH. Existing pages
  0/1 and all release/status/patch layouts remain unchanged. Schema is explicitly
  versioned; release rejects 0A/0B. Protocol details: PROTOCOL.md.
- Read-only `host/forge_inspector.py` provides a four-group terminal viewer and
  JSONL recording. Both MIDI and offline paths use the same decoder/collector.
  No dashboard UI or Tab5 implementation yet; those should consume this model.
- Audio updates cheap scalar state on a <=20 Hz mailbox request and pushes
  bounded numeric event observations. Main creates/latches snapshots, retains
  the log, serializes and sends. No new audio IO, allocation or waiting.
- Eight native suites PASS (three factory TAPE files checked). Seven new
  Inspector Python tests PASS. Full Python suite 61/62 PASS: the existing
  oversized HTTP-body test fails with Windows WinError 10054, reproduced using
  unmodified HEAD host/tests. Its assertions remain intact. Not an all-green run.
- ARM release/development builds PASS (xPack 10.3.1-2.3 Windows x64, verified
  archive hash in INSPECTOR). Release is byte-identical to original HEAD's
  rebuild: 213,952 bytes, SRAM 92,588; no Inspector symbols. Development
  footprint is recorded in INSPECTOR.md; SDRAM/DTCM/D2 unchanged.
- ASan/UBSan could not link: installed Windows LLVM lacks the MinGW sanitizer
  runtimes. The new suite is included in `make sanitize` for Linux. No sanitizer,
  browser, benchmark, physical hardware or live-AI pass claimed this session.
- Next: user-run TEST_SESSION 1A, starting with physical C3/button 15 and SW4
  encoder index 3 on the dry aux patch, then Glass Keys for voice routing.
  Record JSONL, actual LED mapping and CPU/queue losses with polling on/off.

Still invisible: actual DMA underruns, framing/parser discards, stack/heap peaks,
per-voice sample position/filter modulation and per-kit-slot loading progress.
None of the hardware risks in the earlier checkpoint have been retired.

## Scope (unchanged, authoritative)

DC wants an **AI-programmable playable instrument**, not effects-only: describe a
sound → AI configures installed modules → play keys/MIDI → adjust → save/recall.
Patch changes need no recompile; new DSP algorithms need firmware work; AI never
generates executable effects. Looping, sequencing, more engines/effects,
flexible routing are future work (SD presets since 0.4, sampler since 0.5). ONE consolidated hardware session.
No main merge, flashing or real-key API calls by agents.

## Checkpoint summary (2026-10-03)

**Where we are.** Forge 0.5 on `forge/foundation`, software-tested only:
- Instrument: 4-voice synth (v1/v2 legacy bit-exact; v3 palette with osc2,
  noise, resonant filter + envelope, LFO/mod wheel, voices/glide, reverb),
  TAPE-compatible sampler with recording (v4, up to 7 voices), delay, keybed
  + MIDI (sustain, bend, mod wheel, program change, CCs as stock).
- Patches: versioned SysEx v1–v4, host CLI, AI webapp (OpenAI/Gemini,
  mocked only; may only use samples the device reports), upgrades to v3/v4.
- Device presets: 8 × 15 SD slots, TAPE-style key + encoder menu (Presets
  page); Samples page for TAPE's sample slots, source and save/copy/erase.
- Stock comparison: bootloader layout, keybed, SD setup, CCs and knob order
  match stock; Forge reads/writes TAPE's sample files; sampler worst case
  ~97 % of WAVE's engine (emulated), synth ~55 %; ~73 KB code headroom (TEMPO split).
- QA bundle `Forge-0.5-test-b7a504a` is current (see Next actions).

**What's left.**
- DC decisions made 2026-10-03: CCs match stock (done: CC20+n = encoder n);
  start-up stays dry aux; QA bundles go to DC's Google Drive (DC uploads;
  agents have no Drive access). Still open: configurable MIDI channel.
- Features (PROJECT.md roadmap): 5 looping (TAPE's looper; ~7.6 MB SDRAM
  and ~73 KB code left in release, ~57 KB in the development build), 6 Tab5 controllers (optional).
  Smaller candidates: TAPE per-slot settings, threshold-armed recording, kit
  MIDI note range, preset/sample names, mono note stack, octave shift, bend
  range, cheaper sine for Bell Keys.
- QA (DC, later, per feature): LIVE_AI_TEST.md (own key, own terminal), then
  TEST_SESSION.md in one hardware session; record in TEST_RESULTS.md.
- Hardware-unverified risks: device CPU (above all 7 sampler voices reading
  SDRAM, TEST_SESSION 6.2c), SD load/save speed and main-loop stalls, card
  swap, recording levels/monitoring/jack detect, LED positions/colours,
  UART/USB reply timing (98-byte v4 status), v3 loudness by ear, line-out vs
  headphone level vs stock.

## Branch and publishing

Repo DCDominguez/CHOMPI, branch `forge/foundation`, draft PR #1 (still draft;
`main` untouched). The integration checkpoint is on the remote as commit
`15681b8cc10dd76bdff4a6d39e62cc96bc75c7b2` ("Integration review, real browser
tests, live-AI prep, 0.3 bundle tooling"), parent `2b30b1c`, tree
`cf807ad0bcfd0d107cc853294d6eec6ad060fe02`. The SHA differs from the
locally made `558810e` because DC's connector re-created the commit; the tree
SHA is identical, so the content is exactly that checkpoint. Verified by an
agent on 2026-10-02 (UTC). Later commits on top: `716c6bc` (SHA record),
`e24015c` (mitigation proposal), `0a605f6` (sound fixes, the bundle source),
`3ea7529` (bundle record), `66b72a7` (documentation audit), `0f4290e`
(sustain/bend), `91c11c2` (v3 engine), `79c5d9f` (v3 host/webapp), v3
docs, `4fec6ac` (device presets), `aef8c7c` (bundle record), `cd8d6df`
(stock comparison refresh). Use `git log` for the current head.

### Independent reproduction, 2026-10-02 (UTC), fresh container

All rerun from tree `cf807ad0…`, not copied from earlier notes:
- `make test`: 3 native suites PASS, 35 Python tests OK.
- `ASAN_OPTIONS=detect_leaks=0 make sanitize`: 3 suites PASS.
- `make browser-test`: 8/8 OK (Chromium 141 headless, Playwright 1.56.0).
- ARM: developer.arm.com still 403 from the sandbox, so the pinned Arm
  10.3-2021.10 compiler was not used. xPack 10.3.1-2.3 (sha256 matched the
  published `.sha`) built FORGE.bin 117,432 bytes,
  sha256 `f0cd4efd86f9ef2c30e95d5622ada8562d6b1b259b03810b8789a39ce8b1e415`;
  memory use identical to the figures below. 3 warnings, all vendored libDaisy
  `tim_channel.cpp`. No bundle regenerated (pinned compiler unavailable).

## Claim levels

| Level | What |
| --- | --- |
| Implemented | Everything in README feature table, plus the items below |
| Software-tested | 6 native C++ suites (core, protocol incl. v3, synth, v3, preset, sampler), 55 Python tests, 6 ASan/UBSan suites, 11 real-Chromium browser tests, ARM build (xPack GCC 10.3.1), `make bench --check` (emulated instruction counts vs stock firmware) — all pass 2026-10-03 UTC at firmware 0.5 with the sampler |
| Hardware-verified | **Nothing.** No flash, audio, keybed, MIDI transport, CPU or battery test |
| Live AI | **Not run.** Formats checked against provider docs 2026-10-03; mocks only |

## Done in the integration checkpoint (`15681b8`)

Integration defects found and fixed:
- Webapp refused `http://localhost:8765` (403). Now both loopback spellings are
  accepted; Origin must equal the Host; other hosts still 403 (DNS rebinding).
- Validation errors were a generic message; device rejections (queue busy, ack
  mismatch) were masked as "operation failed". Now 400 names the field; 502 shows
  the device/dependency reason. Messages never contain keys.
- Status banner scrolled out of view next to device buttons (desktop and phone):
  now sticky.
- Cutoff slider was linear (unusable range): now log, same curve as firmware.
- Editor reordered: source/synth group, then delay/output group.
- Host encoding clamps normalized values so float rounding cannot wrap 16384→0.
- Stuck-note recovery moved from `forge_main.cpp` into `RecoveryGate`
  (core/runtime.h) and host-tested; firmware behaviour unchanged.
- PROTOCOL corrected: any dropped ingress frame (not only notes) triggers silence.

Reviewed, no defect found: voice ownership/steal, note-off after steal, panic
O(1) tail flush, route/waveform panic, v1 patch → aux + dormant synth defaults,
v2 decode/encode bounds, capture of edge values, sequence/ack checking.
Known at that checkpoint (three of these were fixed later; see Sound fixes):
post-recovery note drops, retrigger/steal clicks, triangle aliasing. Still true:
held keys must be retriggered after route change; saw/square high-note aliasing.

New tooling:
- `tests/sim_device.py` stateful simulated device (persistent `forge_probe`,
  which now flushes per reply) and `tests/browser_e2e.py` (`make browser-test`).
- `forge_host.py cc` / `note` for the hardware session; notes always released.
- `host/forge_ai_check.py` one real provider request, hidden key prompt,
  key-free JSONL record. Doc: LIVE_AI_TEST.md.
- Provider schemas mirror ranges into descriptions.
- Packager: records actual compiler from FORGE.elf `.comment`, flags
  `built_with_pinned_compiler`, refuses binaries older than firmware sources,
  includes `verify_bundle.py`, LIVE_AI_TEST.md and forge_ai_check.py.
- TEST_SESSION.md rewritten as one ordered session with exact commands.

## Build environment notes

Chromium 141 via Playwright at /opt/pw-browsers worked in the agent sandbox.
Playwright `evaluate`/`wait_for_function` are blocked by the app's CSP; tests use
locator expectations and a separate `bypass_csp` context only for layout
measurement. Playwright from pip must match the installed browser build:
current pip Playwright (1.63) looks for chromium build 1243 and fails; with
the sandbox's `/opt/pw-browsers/chromium-1194` install `playwright==1.56.0`
instead of running `playwright install`. developer.arm.com was blocked (403); xPack
`xpack-arm-none-eabi-gcc-10.3.1-2.3-linux-x64.tar.gz` (sha256 559dcf1c…8719,
matches published .sha) built firmware: FORGE.bin 117,432 bytes, SRAM_EXEC
49.43%, SRAM 17.01%, RAM_D2 68.07%, SDRAM 0.57% (before the sound fixes; see
that section for current figures). Only vendored-libDaisy warnings.

## Sound fixes (implemented 2026-10-02 UTC; software-tested only)

DC chose to implement the measured mitigations. Figures are from simulation
(48 kHz, Blackman-Harris 4096-point DFT, alias = non-harmonic / harmonic
energy below 12 kHz; sine floor < -80 dB). Not listening or hardware results.

1. **Steal/retrigger clicks** (`core/synth.h` `Note`). A reused voice keeps
   `envelope` and `phase`; attack resumes from the current level. Per-voice
   `gain` slews to the new velocity over ~2 ms (only on reuse; a fresh voice
   starts at its velocity, so velocity scaling stays exact). Allocation:
   same source/note → idle → quietest releasing → oldest. Sine 4-voice steal
   worst step: 4.98x → about 1.1x steady state.
2. **Triangle aliasing.** 2-point polyBLAMP at both corners, scale `4*dt`
   (matches the code's polyBLEP convention; 4–4.5 empirically best). Alias at
   C5 -64.1 → -86.8 dB, C7 -46.9 → -78.9, C8 -36.2 → -62.5. Saw/square
   unchanged (about -54 to -60 dB on keybed range, -44 dB at C8); 2x
   oversampling or 4-point BLEP deferred until a hardware CPU figure exists.
3. **Post-recovery dropped notes.** `Request.epoch` (host-side only, not on
   the wire). `forge_main.cpp`: `RaiseEmergency()` increments an atomic count
   (replaces `emergency_silence`); `Queue()` stamps every request. Audio:
   `RecoveryGate::Observe` once per block, `Admit` per request; newer epoch →
   panic once; older note → drop; everything else executes. Wrap-safe int8
   comparison. No drain condition any more.
4. Not done (by choice): panic/route change still cuts voices and the filter
   state abruptly; panic should stay immediate unless the session hears a
   problem.

Tests in `tests/synth_test.cpp`: `ClickFreeStealAndRetrigger`,
`TriangleAliasing`, rewritten `RecoveryAfterLostNotes`. Each new assertion was
run against the previous `synth.h` and failed there (steal ratio, alias at
C7, releasing-voice preference), and passes now.
ARM (xPack 10.3.1): FORGE.bin 118,320 bytes (+888), sha256
`ff9f3386b9ec418b16718b491f01e6e7913f62e7f0bd9604fa3ea88d414cc299`,
SRAM_EXEC 49.80%, SRAM 17.02%, RAM_D2 68.07%; no warnings from Forge sources.
Added per-sample cost: one multiply-add per active voice plus two branches
per triangle voice. Real CPU cost is unmeasured; session step 6.2 records it.
Any 0.3 bundle made before this commit is stale.

## Development plan (DC, 2026-10-02): features first, QA per feature later

Roadmap order is in PROJECT.md. Hardware and live-AI QA are deferred until
features are developed; each feature gets its own commit, tests and
TEST_SESSION steps so it can be QA'd separately. Do not wait for QA to start
the next roadmap item, and never claim a hardware result.

### Roadmap item 1: sustain pedal and pitch bend (implemented; software-tested)

- `core/midi_framer.h`: accepts pitch bend (E0) with running status.
- `core/runtime.h`: `TranslateChannel` (moved out of forge_main) maps
  channel-1 messages to requests and an `Ingress` class: Critical (note,
  CC64, CC121: a full queue raises the stuck-note emergency), Control (CC
  params, bend: drop counted), Emergency (CC120/123). Gate drops stale
  note/pedal/bend/CC121.
- `core/synth.h`: per-source pedal and bend (3 sources). Note-off under pedal
  marks the voice `sustained`; pedal-up releases only those. Bend ±2 semitones,
  `pow` per message, 5 ms one-pole per source, applied to the increment
  (clamped 0.45). Steal: idle → quietest releasing → oldest sustained → oldest.
  `Silence` resets pedal/bend.
- Host: `note --bend N`, `note --sustain`; both always reset in `finally`.
- Tests: `SustainPedal`, `PitchBend`, `ChannelTranslation` (synth_test) and
  `test_bend_and_sustain_are_always_reset` (Python). Mutation check: pedal
  ignored, no steal preference, Silence keeping the pedal, stale pedal
  admitted, no bend smoothing, bend applied across sources → all caught.
- ARM (xPack): FORGE.bin 120,296 bytes, SRAM_EXEC 50.64%, SRAM 17.12%,
  RAM_D2 68.07%; no Forge warnings. Bundle not regenerated for this commit.
- QA steps: TEST_SESSION 3.8 (sustain), 3.9 (bend).
- Wire format: unchanged (SysEx v1/v2). Python sim device unaffected.

### Roadmap item 2: v3 instrument palette (implemented; software-tested)

Firmware minor 4. Commits `91c11c2` (engine/protocol), `79c5d9f` (host,
AI, webapp, presets), then docs.

- Wire: v3 apply 69 bytes, status 81; layout in PROTOCOL.md. One ordered
  table each side: `V3Fields` (core/protocol.h) and `V3_FIELDS`
  (host/forge_host.py) — change both together. Framer accepts 72-byte SysEx;
  firmware reply buffers 83, USB buffer 112, UART timeout = 0.32 ms/byte + 5.
- `core/parameters.h`: v3 fields default neutral, so v1/v2 patches carry no
  v3 behaviour. CC26/27 refused on v1/v2 (status would not report them).
- `core/synth.h`: `legacy_` (version < 3) keeps the old path exactly: shared
  one-pole after the voice sum. v3: osc2 (ratio from semitones+detune), noise
  (xorshift), mix normalized by 1/(1+osc2+noise), per-voice TPT SVF (Q 0.707·16^res,
  gain 1/sqrt(Q/0.707)), filter ADSR (rests at 0 after release), coefficient
  refresh every 16 samples, one shared LFO (S&H draws on wrap), mod wheel
  global (reset by panic/CC121), voices limit (extras released on shrink),
  glide (exponential; from sounding pitch or last note).
- `core/reverb.h`: 4-line FDN, lengths 1601/1949/2311/2741 @48 k (+1 each =
  8606 floats), Hadamard/2, per-line one-pole damping, gains from RT60,
  O(1) Clear via unread counter. Engine skips it while mix target and smoothed
  mix are 0 and clears on restart. Reverb memory (`kReverbCapacity` 8704)
  moved to DTCM on 2026-10-03 after the stock-firmware comparison (see
  COMPATIBILITY.md); a test proves uninitialised DTCM never reaches output. `Engine::Init` 6-arg overload; 4-arg leaves reverb silent.
- Engine: v1/v2 ↔ v3 change panics (architecture differs).
- Bit-exact check (one-off, 2026-10-03): a renderer driving notes, bend,
  pedal and CC cutoff through v1 and v2 patches (all waveforms, 384,000
  stereo samples) produced identical bytes with the previous core from git
  (`git show <old>:firmware/chompi-forge/core/*`) and the new core. Repeat it
  for any change near the legacy path.
- Tests: `tests/v3_test.cpp` (osc2 interval/detune, noise, resonance, cutoff
  tracking, ± filter envelope, LFO pitch/tremolo/shapes, mod wheel gating and
  reset, voices/shrink, glide, reverb size/damping/stereo/panic/stability/
  mix-0 exactness, CC26/27 v3-only, structural panic, 60-trial fuzz);
  protocol_test `ProtocolV3`; Python random v3 round trips (200), atomic
  rejection, strict schema, upgrade + CLI + /api/upgrade; browser v3 editing,
  v2 mapping/convert, v3 send/capture. Mutations: 13/14 caught; reverb always
  running is output-equivalent (x + 0·wet), so not detectable.
- Measured filter facts (sim): saw 110 Hz, cutoff 300 vs 990 Hz → 3 kHz
  power ratio 0.008; resonance 0.9 boosts the 990 Hz harmonic ~140×.
- ARM (xPack): FORGE.bin 142,520 bytes, SRAM_EXEC 59.99%, SRAM 21.20%,
  RAM_D2 68.07%, SDRAM 0.62%; no Forge warnings.
- Host/webapp: `upgrade_patch` (server-side, used by the webapp's Convert
  button and the `upgrade` CLI); webapp control table with per-version paths
  and module-qualified IDs (`synth-attack_ms` vs `filter-attack_ms`).
- QA steps: TEST_SESSION 3.10–3.16 and 6.2b (CPU stress preset 10).
- Known limits: mono mode has no note-priority stack; saw/square still
  2-point polyBLEP; device CPU unmeasured; multi-packet USB replies and the
  27 ms UART reply blocking are hardware-unverified; live AI with the larger
  v3 schema unverified (strict-mode providers may reject a keyword — relax
  only that keyword).
- Loudness (simulated renders, same 0.25 level): v3 presets peak 0.02–0.05
  vs 0.07–0.08 for v2. Causes: resonance gain compensation 1/sqrt(Q/0.707)
  (Acid Bass at resonance 0.8 → ×0.34), reverb crossfades dry→wet, osc2/noise
  normalization. Deliberately conservative (no clipping at max resonance);
  revisit after listening (TEST_SESSION 3.10–3.13), e.g. partial compensation
  or higher preset levels.

## Stock firmware comparison and CPU benchmark (2026-10-03, DC's request)

Full write-up: [COMPATIBILITY.md](COMPATIBILITY.md). Key facts for agents:
- Bootloader (TAPE's Chompi_Bootloader source) loads the first non-hidden
  `*.bin`/`*.BIN` on the card root, checks SP in RAM and entry in D1 SRAM or
  QSPI. FORGE.bin passes, same SP/entry layout as factory TAPE/TEMPO/WAVE.
  Risk: macOS `._FORGE.bin` (not hidden) can be picked first → TEST_SESSION 1.1.
- Keybed note table identical to stock in all three apps (checked by script).
- MIDI: stock maps CC20+n → encoder n (CC24 ignored, CC25 → SW6), channel
  from options.json, CC input optional, notes/CCs sent out. Forge: CC24
  bypass, CC25 cutoff, fixed channel 1, no MIDI out. Open decision for DC.
- Upstream rebuilds (scratch copy, xPack): TAPE needs a `Limiter.h` symlink
  on Linux and then overflows SRAM_EXEC by 3,524 B; TEMPO (built with 10.3, superseded below) builds 4,660 B
  smaller than factory. Repo sources + xPack ≠ factory builds.
- `make bench` (bench/): Forge presets 209–1,428 instructions/sample; TAPE
  FX+output 1,249 and TEMPO FX+output 1,362 (10.3; 1,352 with 13.3) (voices excluded, lower bounds);
  WAVE full 8-voice engine 2,695–2,747. Gate: Forge ≤ WAVE. Emulator notes:
  A-profile "max" core (Unicorn M-profile lacks FPU enable), flush the TB
  cache after adding the counting hook, peripheral range mapped as scratch
  for TEMPO's timer init, link with `-u` roots or gc-sections drops entry points.
- Refresh after device presets (2026-10-03, `4fec6ac`, DC's request):
  - Toolchains: factory TAPE/WAVE and all shipped libdaisy.a/libdaisysp.a
    are Arm 10.3-2021.10; factory TEMPO is Arm 13.x (README: 13.3.rel1).
    The earlier TEMPO rebuild/bench used 10.3 — wrong; now 13.3 via
    `TEMPO_GCC_PATH` (xPack 13.3.1-1.1, sha256 006c8933…81b959).
  - xPack 10.3.1 vs Arm 10.3: libDaisy 188/188 and DaisySP 56/56 objects
    disassemble identically (SDMMC/FatFS/UART included). The pinned-compiler
    risk for Forge's hardware drivers is retired.
  - Rebuilds with shipped libs: TAPE +3,900 B (overflow 3,524), WAVE
    +3,892 B, TEMPO (13.3) +388 B vs factory. The ~3.9 KB is the xPack
    runtime libraries, not source.
  - SD coexistence: same SDMMC (FAST, 4-bit) and ffconf as TAPE/WAVE; no
    stock app reads `FORGE/`; bench unchanged except TEMPO 1,352 (13.3).
  - Forge 4fec6ac: 187,592 B, SRAM_EXEC 79 % (~48 KB headroom).
- Firmware change from this: reverb memory SDRAM → DTCM (`.dtcmram_bss`),
  FORGE.bin unchanged in size, DTCM 26.6 %; test
  `ReverbIgnoresUninitializedMemory`. The 66af4c5 bundle is now stale.

### Roadmap item 3: device presets on the SD card (implemented; software-tested)

DC chose "key and encoder combo similar to TAPE" (2026-10-03). TAPE's menu
was read from chompi-tape NormalPage.h / MenuPage.h / ui.h: toggle + CHOMPI key
opens it; white keys = slots (KeyToSlot); KEY_23/24/25 erase/copy/save;
KEY_16/17 banks; CHOMPI confirms; SMT LED 25 − slot# under white keys,
0–9 under black keys KEY_16..KEY_25.

- `core/preset_store.h`: Storage interface; record `'F''P' fmt len DATA crc16`;
  `PresetStore` (save with read-back, load, erase, copy, occupancy, Rescan).
- `core/preset_menu.h`: `PresetMenu` (audio-owner FSM) + `RenderMenuLeds`.
  Bank persists between openings; knob 1 (hw SW4) clamps (1–8), bank keys wrap; a
  selection keeps its bank if the bank changes before confirming.
- `core/protocol.h`: `EncodePatchData`/`DecodePatchData` shared by SysEx,
  status and records; opcodes 04–07, replies 42 (12 B) / 43 (33 B), errors 7–9.
- `core/midi_framer.h` + `TranslateChannel`: program change → silent recall.
- `core/runtime.h`: Store → Snapshot response; silent Patch (no reply);
  `StoreReply`/`EraseReply`/`ListReply`/`RecallRequest` shared by firmware and
  `host/forge_probe.cpp` (in-memory card).
- Firmware: `src/fatfs_storage.h`, SD mount at boot (as TAPE), 1 s card
  watch, `RunPanelActions`, `DrawLeds` (30 Hz; panel LED 0 flash), `USE_FATFS = 1`
  (comment on its own line: trailing spaces broke `ifeq`). FatFS objects must
  stay out of DTCM (SD DMA) — they are globals in D1 SRAM.
- Host: `preset_message`, decode 42/43, CLI `store|recall|erase|slots`
  (1-based), `/api/preset`, webapp Device presets panel (Erase = 2 presses).
- Tests: `tests/preset_test.cpp`, `tests/test_presets.py`, web + browser
  tests. Mutations caught: releases consumed, no toggle needed, slot map off
  by one, no read-back, CRC ignored, save into the wrong bank, silent recall
  replying.
- ARM: FORGE.bin 187,592 bytes, SRAM_EXEC 78.96 %, SRAM 22.96 %, DTCM
  26.56 %, RAM_D2 72.63 %; no Forge warnings.
- QA: TEST_SESSION 3.17–3.27. Unverified: SD timing (main-loop stall while
  writing), card swap, LED colours/positions, which toggle position TAPE calls
  "menu" (Forge reuses `GetToggleState()` exactly as TAPE does).
- Known limits: no names on the device; boot still starts in dry aux (no
  auto-recall); mono mode note stack still missing.

### Roadmap item 4: sampler (implemented; software-tested; firmware 0.5)

DC: "follow the existing sample functionality on TAPE … if there's anything
cool we can add without overloading". Design and differences: SAMPLING.md.
Commits: `c19522d` design, `c7ffc78` WAV, `054bfab` v4 + voices, `698cda1`
recorder/loader, `89bdfde` menu page, `ed80acf` firmware, `107b8ea` host/AI/
webapp + bench optimisation, then docs.

- Wire: v4 apply 84 bytes, status 96 (`kMaxRequest`/`kMaxReply`); framer 88;
  opcodes 08 (list → 0x44) and 09 (save/erase/copy → 0x45). V3Fields holds
  the v4 entries (index ≥ 68 only for version 4); Python mirrors in
  `V4_SAMPLER`. Firmware minor 5.
- Memory: SDRAM pool 40 MB + recording 16 MB + delay; SDRAM 88 %. Not zeroed;
  reads bounded by `loaded`. Code: SRAM_EXEC 213,164 B (89.7 %) after
  noinline on non-realtime helpers, one `HandleFrame` for both transports and
  no snprintf (saved ~5 KB). Watch this before adding features.
- Concurrency: `SampleHandoff` (request/ack/publish atomics) before the
  loader rewrites slots; recorder save lock is atomic; main reads only the
  recording's atomic `loaded`. Sample jobs from the panel cross on
  `sample_jobs`; host save goes audio (lock) → main (job) → 0x45.
- CPU (`make bench`): 7 voices 2,104–2,236, stress 2,628 vs WAVE 2,695.
  First version was 3,233/4,185; optimisations listed in COMPATIBILITY §6.
  v1–v3 +2–3 % (7-voice array, stereo engine call), still bit-exact.
- Tests: tests/sampler_test.cpp (+ tests/sample_card.h in-memory card) — WAV
  incl. real factory TAPE files, v4 protocol, voices, recorder, loader,
  handoff, jobs, requests; preset_test samples page/LEDs/record gesture;
  test_samples.py (150 v4 round trips, CLI, web, AI guard); browser test
  for sampler controls + Device samples. ~45 targeted mutations caught across
  steps (a few weak tests were strengthened: loop window shorter than the
  1024-frame minimum, temp-file cleanup, file gate).
- Harness: forge_probe has a simulated card (jammi_a1, cubbi_a1/a2) and a 1 s
  recording; kit renders use notes 48/50/52.
- QA: TEST_SESSION 3.28–3.41 and 6.2c. Unverified: everything on hardware,
  especially SDRAM read cost, SD speeds (load time of a kit), record levels,
  jack detection polarity, TAPE reading Forge-saved files.
- Known limits: per-slot settings not stored (TAPE presets.json ignored);
  kit MIDI uses notes 48–72 only; recording lost at power-off unless saved;
  no sample names; looping not implemented.

## Next actions (priority order)

1. Done 2026-10-03 (DC): CCs aligned with stock (COMPATIBILITY.md §4);
   start-up stays dry aux (no boot recall); QA bundles → DC's Google Drive.
   Open, low priority: configurable MIDI input channel (stock: options.json).
2. Done: roadmap item 4, sampling (see "Roadmap item 4" below).
3. Agent: roadmap item 5, looping — design first from TAPE's LooperEngine /
   FileSampler (tape-style overdub, varispeed), within ~7.6 MB SDRAM and the
   remaining code space; confirm scope with DC before building.
4. Bundle: `Forge-0.5-test-b7a504a` built from `b7a504a` (xPack GCC 10.3.1,
   zip sha256 `cb0380da…dfe4e1`, FORGE.bin sha256 `513d295c…af45df`,
   `verify_bundle.py`: 51 files OK). DC stores it on Google Drive. Older ZIPs
   are stale; regenerate if firmware changes again.
5. DC (later, per feature): LIVE_AI_TEST.md, then TEST_SESSION.md; record in
   TEST_RESULTS.md. Agent then fixes only what QA finds.
