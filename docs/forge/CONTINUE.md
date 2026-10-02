# Forge — developer resume checkpoint

Updated 2026-10-03, candidate 0.3 integration/browser checkpoint. Read this first.

## Scope (unchanged, authoritative)

DC wants an **AI-programmable playable instrument**, not effects-only: describe a
sound → AI configures installed modules → play keys/MIDI → adjust → save/recall.
Patch changes need no recompile; new DSP algorithms need firmware work; AI never
generates executable effects. Sampling, looping, sequencing, more engines/effects,
flexible routing and SD presets are future work. ONE consolidated hardware session.
No main merge, flashing or real-key API calls by agents.

## Branch and publishing

Repo DCDominguez/CHOMPI, branch `forge/foundation`, draft PR #1. Previous remote
head `2b30b1c`. This checkpoint was committed locally on top of it by an agent
WITHOUT push access; DC applied it (patch/bundle) — compare `git log` with the
commit message "Integration review, real browser tests, live-AI prep, 0.3 bundle
tooling". If the remote head is still `2b30b1c`, this work has not been applied.

## Claim levels

| Level | What |
| --- | --- |
| Implemented | Everything in README feature table, plus the items below |
| Software-tested | 3 native C++ suites, 35 Python tests, 3 ASan/UBSan suites, 8 real-Chromium browser tests, ARM build (xPack GCC 10.3.1) — all pass 2026-10-03 |
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
Known, documented, not changed: post-recovery notes arriving before the request
queue drains are dropped (safe, rare); retrigger/steal clicks; triangle/high-note
aliasing; held keys must be retriggered after route change.

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
measurement. developer.arm.com was blocked (403); xPack
`xpack-arm-none-eabi-gcc-10.3.1-2.3-linux-x64.tar.gz` (sha256 559dcf1c…8719,
matches published .sha) built firmware: FORGE.bin 117,432 bytes, SRAM_EXEC
49.43%, SRAM 17.01%, RAM_D2 68.07%, SDRAM 0.57%. Only vendored-libDaisy warnings.

## Next actions (priority order)

1. If DC's remote does not contain this checkpoint, get it applied first.
2. DC: run LIVE_AI_TEST.md (CLI preflight, then webapp). Record provider/model.
   If OpenAI/Gemini rejects the schema, relax only the offending keyword.
3. DC (optional): rebuild firmware with the pinned Arm 10.3-2021.10 archive and
   regenerate the bundle; otherwise test the xPack-built bundle as labelled.
4. DC: run TEST_SESSION.md once; record in TEST_RESULTS.md.
5. Agent: fix only what the session finds; then ask DC to choose the next engine
   (sampling/looping vs more synthesis/FX).
