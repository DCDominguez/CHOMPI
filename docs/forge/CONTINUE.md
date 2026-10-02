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
measurement. Playwright from pip must match the installed browser build:
current pip Playwright (1.63) looks for chromium build 1243 and fails; with
the sandbox's `/opt/pw-browsers/chromium-1194` install `playwright==1.56.0`
instead of running `playwright install`. developer.arm.com was blocked (403); xPack
`xpack-arm-none-eabi-gcc-10.3.1-2.3-linux-x64.tar.gz` (sha256 559dcf1c…8719,
matches published .sha) built firmware: FORGE.bin 117,432 bytes, SRAM_EXEC
49.43%, SRAM 17.01%, RAM_D2 68.07%, SDRAM 0.57%. Only vendored-libDaisy warnings.

## Proposed software-only mitigations (not implemented; awaiting DC's go)

Measured 2026-10-02 with a scratch harness around `core/synth.h` (48 kHz,
sustain 1, cutoff max, Blackman-Harris 4096-point DFT; alias = non-harmonic /
harmonic energy; floor measured on sine: -88 dB). Simulation only; nothing
here is a listening or hardware result. No engine change is proposed.

1. **Steal/retrigger clicks.** Cause: `Note()` does `*selected = Voice{}`,
   which zeroes envelope and phase of a sounding voice. Sine, 4 held notes +
   a 5th: worst sample step at the steal = 4.98x the steady-state worst step.
   Fix A (≈3 lines): keep the stolen/retriggered voice's `envelope` and
   `phase`; attack resumes from the current level. Prototype: 1.12x.
   Fix B (optional): steal a releasing voice (lowest envelope) before the
   oldest held one. Behaviour change: PROTOCOL "oldest is stolen" becomes
   "releasing voices first, then oldest"; TEST_SESSION 3.4 still holds.
   Tests: synth_test asserts steal and same-note retrigger worst step <= 1.5x
   steady (sine), and that the envelope never drops at a steal.
2. **Triangle aliasing.** Add 2-point polyBLAMP at both corners:
   `sample += 4*dt*(Blamp(t,dt) - Blamp(t±0.5,dt))` (4·dt matches this code's
   polyBLEP convention; empirically optimal 4–4.5). Alias below 12 kHz:
   C5 -64.1 → -86.8 dB, C7 -46.9 → -78.9, C8 -36.2 → -62.5 dB. Cost: two
   branches per triangle voice. Test: DFT alias at note 96 <= -70 dB (<12 kHz).
   Saw/square (existing 2-point polyBLEP) on the keybed range measure about
   -54 to -60 dB below 12 kHz, -44 dB at C8. Further reduction (2x
   oversampling or 4-point BLEP) costs CPU that is unmeasured on hardware;
   defer until the session gives a real CPU figure.
3. **Post-recovery dropped notes.** Cause: `RecoveryGate` skips *every* MIDI
   note until the request queue is empty once, so under sustained traffic
   notes sent after the emergency are lost too. (Keybed notes bypass the gate.)
   Fix: epoch stamps. Main loop keeps a `uint8_t epoch`, increments it where
   it now sets `emergency_silence`, and stamps each queued Request. Audio:
   a request newer than the gate's epoch → `Begin` (panic) first, adopt it,
   then execute; an older note → skip; equal → play. Use wrap-safe
   `int8_t(a-b)` comparison. Drops exactly the notes queued before the
   emergency; no drain condition. Tests: RecoveryGate unit tests (old notes
   skipped, new notes play with queue non-empty, wraparound, patch/status
   still reply) plus a forge_probe/sim_device flood test.
4. Optional: panic/route change hard-zeroes voices and the filter, so a
   click is possible. A ~2 ms output ramp would remove it, but panic is
   an emergency action, so keep it immediate unless the session hears a
   problem.

Fixes 1A, 2 and 3 are one firmware change for the same consolidated session
(they alter what TEST_SESSION 3.4/3.6 should sound like). DSP change → run
`make test`, `make sanitize`, ARM build, and rebuild the bundle.

## Next actions (priority order)

1. Done: checkpoint confirmed on the remote (see Branch and publishing).
2. DC: run LIVE_AI_TEST.md (CLI preflight, then webapp). Record provider/model.
   If OpenAI/Gemini rejects the schema, relax only the offending keyword.
3. DC (optional): rebuild firmware with the pinned Arm 10.3-2021.10 archive and
   regenerate the bundle; otherwise test the xPack-built bundle as labelled.
4. DC: run TEST_SESSION.md once; record in TEST_RESULTS.md.
5. Agent: fix only what the session finds; then ask DC to choose the next engine
   (sampling/looping vs more synthesis/FX).
