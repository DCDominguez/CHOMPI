# Forge — developer resume checkpoint

Updated 2026-10-02 (UTC), candidate 0.3 sound-fix checkpoint. Read this first.

## Scope (unchanged, authoritative)

DC wants an **AI-programmable playable instrument**, not effects-only: describe a
sound → AI configures installed modules → play keys/MIDI → adjust → save/recall.
Patch changes need no recompile; new DSP algorithms need firmware work; AI never
generates executable effects. Sampling, looping, sequencing, more engines/effects,
flexible routing and SD presets are future work. ONE consolidated hardware session.
No main merge, flashing or real-key API calls by agents.

## Branch and publishing

Repo DCDominguez/CHOMPI, branch `forge/foundation`, draft PR #1 (still draft;
`main` untouched). The integration checkpoint is on the remote as commit
`15681b8cc10dd76bdff4a6d39e62cc96bc75c7b2` ("Integration review, real browser
tests, live-AI prep, 0.3 bundle tooling"), parent `2b30b1c`, tree
`cf807ad0bcfd0d107cc853294d6eec6ad060fe02`. The SHA differs from the
locally made `558810e` because DC's connector re-created the commit; the tree
SHA is identical, so the content is exactly that checkpoint. Verified by an
agent on 2026-10-02 (UTC). Later handoff commits sit on top of it; use
`git log` for the current head.

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
| Software-tested | 3 native C++ suites (now incl. steal-click, triangle-alias, epoch-recovery tests), 35 Python tests, 3 ASan/UBSan suites, 8 real-Chromium browser tests, ARM build (xPack GCC 10.3.1) — all pass 2026-10-02 UTC after the sound fixes |
| Hardware-verified | **Nothing.** No flash, audio, keybed, MIDI transport, CPU or battery test |
| Live AI | **Not run.** Formats checked against provider docs 2026-10-03; mocks only |

## Done in this checkpoint

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
49.43%, SRAM 17.01%, RAM_D2 68.07%, SDRAM 0.57%. Only vendored-libDaisy warnings.

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

## Next actions (priority order)

1. Done: checkpoint confirmed on the remote; sound fixes committed.
2. Bundle: the firmware changed, so regenerate it from the current clean tree
   (`python3 host/package_candidate.py /abs/path/Forge_0.3_Test_Candidate.zip`).
   Preferred: pinned Arm 10.3-2021.10 (`GCC_PATH=<toolchain>/bin`), which needs
   developer.arm.com reachable; otherwise xPack, labelled
   `built_with_pinned_compiler: false`.
3. DC: run LIVE_AI_TEST.md (CLI preflight, then webapp). Record provider/model.
   If OpenAI/Gemini rejects the schema, relax only the offending keyword.
4. DC: run TEST_SESSION.md once with the new bundle; record in TEST_RESULTS.md.
   Listen specifically at 3.4/3.6 for steal clicks and high-note aliasing.
5. Agent: fix only what the session finds; then ask DC to choose the next engine
   (sampling/looping vs more synthesis/FX).
