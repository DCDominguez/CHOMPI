# Forge — developer resume checkpoint

Updated 2026-10-12 (instrumented battery build, development only, not on CHOMPI); before that 2026-10-11 (late night), checkpoint: **0.19.2 (build 8b51764) on CHOMPI**; guided hands-on test done except the TAPE functions (recording, sampler, looper); open: charging from a power bank while on. Read this first; the owner's next session is
[HOME_CHECKLIST.md](HOME_CHECKLIST.md), then [TEST_SESSION.md](TEST_SESSION.md).

## 2026-10-12 (later): harmony secret modes built (resume here)

- Built, not installed: Chord Bloom, Chord Conversations, Harmony Mischief (`core/secret.h`,
  `tests/secret_test.cpp`; MANUAL 8a "Secret modes"). `make test` passes (147 host tests and every
  C++ suite incl. secret_test); release 256,040 B, development 271,988 B (+3.6 KB each; key-time
  code kept out of line after a first build came out +15 KB from inlining).
- After the release: dream mode on hardware **unplugged** (no dream after ~3 min on the PC; any MIDI
  arriving resets its timer, and the sound may have been a kit); the battery diagnostics through a
  long or low-battery session.

## 2026-10-12: instrumented battery build (development only)

- Built, software-tested, **not run on CHOMPI**: Inspector page 11 in `make firmware-dev`
  (`core/battery_diag.h`, `forge_main.cpp` battery check) and `host/forge_battery_log.py`
  (10 Hz CSV + change lines). Observes the stock reader only; release build unchanged.
- Checks: `make test CXX="g++ -D_USE_MATH_DEFINES"` passed (17 C++ suites; 142 host tests,
  1 skipped: Playwright not installed); `make firmware-dev` layout OK 268,348 B (headroom 20,420 B);
  `make firmware` layout OK 252,416 B (unchanged). Sanitizers not run (no DSP change).
- Next (needs the owner): install the development `FORGE.bin`, run
  `D:\tools\forge-venv\Scripts\python.exe forge_battery_log.py --csv battery_log.csv` from
  `firmware/chompi-forge/host` through the shut-off tests, then read the log against the
  hypotheses in [BATTERY_READINGS.md](BATTERY_READINGS.md) "Instrumented build". Unverified:
  the 0x0C read-back on real hardware (value, bits that may read differently), I2C timing.

## 2026-10-12: battery readings under investigation

- The owner suspects the battery *readings*, not only the battery. Analysis in
  [BATTERY_READINGS.md](BATTERY_READINGS.md): the stock reader starts at "medium" for 30 s after
  every power-on, mixes 3.3 V and 3.0 V comparator readings in one history, likely reads the
  wrong sample for green / yellow, and ignores I2C write errors (threshold could stick at 3.3 V).
  Nothing changed yet; the owner is getting a second opinion. Next: an instrumented development
  build (raw registers, 0x0C read-back, write results), then a Forge-owned reader if confirmed.
- Also since the last checkpoint: the webapp is Choowi's sound workshop (no keys, no hardware,
  chatbot copy and paste, always v7, settings in folds), the Web MIDI page `/send`, and Choowi's
  sample pack, now the default bank purple (bank a); copied to the owner's card as bank d before that move. Sample bank d is red (firmware, not yet installed).

## 2026-10-11 (late night): guided test nearly done, repo refreshed

- On CHOMPI: **0.19.2 build 8b51764** (both power changes). Guided test by the owner's hand: parts
  1-3 passed earlier; save and recall pass; sample start / end on SW1 / SW2 (toggle down) work
  with their lights; the owner reports most other features tried and working. **Not yet: the
  TAPE functions** (recording, sampler, looper / SW5), best on a full battery.
- Open bugs / ideas for the final release: (1) charging from the owner's power bank stops while
  CHOMPI is on (0 W; 9.4 W off): log raw charger registers to the card, compare stock TAPE;
  (2) knob lights dim instead of off in the record position (the owner: later; TAPE turns them
  off); (3) SW6's battery colour is a 3.3 V comparator, not a gauge (documented).
- Stale docs refreshed (README status, PROJECT milestone, HANDOFF, HOME_CHECKLIST, AGENT_GUIDE,
  UX_REVIEW note, RESOURCE_LEDGER 0.19.2 sizes).

## 2026-10-11 (night, later): shut-off caught, fix built

- CHOMPI runs **0.19.1** (build e1bd640). The guided full test stopped in part 4 at the shut-off.
- `D:	ools\chompi-session\power_log.py` (10 readings a second, CSV in `reports/power_log.csv`)
  caught a shut-off at 21:46:13 on the PC port (weak supply, charging): battery high, then
  medium + the below-3.0 V flag, then no answer within 0.6 s. Cause: the stock lockout's weak-supply
  branch (`hardware.h` LowBatteryLockoutCheck: lights off, STOP mode) on 8 low readings (160 ms).
- Fix in 0.19.2: `forge::power::LockoutGuard` (3 s of low readings before the stock protection);
  main loop only. `make test` passed (136 host tests + C++ incl. `LockoutDip`); `make firmware-dev`
  built, layout OK (266,204 B). **Not installed, not hardware-verified.** Next: install (the owner's
  press), repeat the power-log test on the PC port, then the save test video (`save_test.py`),
  then resume the guided test at part 4.
- Second fix built after that: a charger plugged in while CHOMPI is on never started charging
  (power bank 0 W until a restart). `ChargerUsb` now also starts the hand-over when USB power
  appears in the 20 ms reads. Tests pass, firmware builds (266,272 B). Not installed. Hardware
  check: CHOMPI on, unplug and re-plug the power bank, watts should rise within a few seconds.
- **8b51764 installed; the power bank still reads 0 W whenever CHOMPI is on** (9.4 W off).
  The hand-over change is not the fix. Next session: log raw charger registers to the card on
  the bank (no PC there), compare off/on and stock TAPE, before touching input-limit registers.
  Meanwhile: charge CHOMPI switched off.
- Hardware note for the owner: a charged battery sagging under 3.0 V suggests a worn cell or a
  poor battery connection. A front case USB port left CHOMPI unseen by Windows (a white flash of
  every light, likely a brown-out restart); the usual port works.

## 2026-10-11 (end of day): 0.19 staged on the card, stress test done

- CHOMPI runs 0.17.0; **0.19.0 (build 9f0ce1c) is written to the card** and installs at the owner's
  next power cycle (bootloader). First thing next session: confirm the build over USB, then
  TEST_SESSION 3J-3M, the SW5 "after" video (before: `reports/cam/filter_before_0170.mp4`), and the
  heaviest stress steps again on 0.19 (`D:	ools\chompi-session\stress.py`).
- Stress test (TEST_RESULTS): CPU peak 67 %, flood 5,594 msg/s with 0 dropped, bank 8 40/40.
- Brand: Spark; mascot Choowi (or Choowee), releases are his tools (PROJECT.md); manual updated.
- Repo no longer names the owner (memory `repo-anonymity`); history unchanged (force push only
  with the owner's explicit OK).

## 2026-10-11 (late night): 0.19.0 synth page built

- Built, not installed: 0.17.1 (SW5 sweep), 0.18.0 (magic 4-6), 0.19.0 (synth edit page, menu
  page 5, pink). One install covers all three (the owner's press). Webapp: Spark mockup v2 published
  (virtual screen, dice, characters). Next: install + TEST_SESSION 3J-3M + the filter "after"
  video; then the stress test; then the virtual screen protocol and the Spark webapp build.
- Development headroom 22,876 B (RESOURCE_LEDGER): plan a code move before effects.

## 2026-10-11 (night): 0.17.1 + 0.18.0 built, Spark chosen

- On CHOMPI: 0.17.0 (e7b5e12), magic 1-3 checked (TEST_RESULTS). Built, NOT installed (the owner out;
  Claude Code's permission check refused a virtual CHOMPI press twice, even with the owner's chat OK, so
  installs wait for the owner's physical press): 0.17.1 SW5 sweep (80 Hz..8 kHz + resonance) and 0.18.0
  (chomp rolls, dream mode, roll sparkle, loop ripple). Before / after video of SW5: the before is
  `reports/cam/filter_before_0170.mp4`; record the after once installed.
- Brand: Spark (PROJECT.md). Next per the owner: magic + Spark branding (webapp mockup in Spark with the
  virtual screen, dice, characters), then the synth edit page; then the stress test.
- Development build headroom 26,580 B (RESOURCE_LEDGER): plan before big additions.

## 2026-10-11 (latest): 0.17.0 magic items 1-3 built

- 0.16.0 is on CHOMPI (e7e3f0a; TEST_RESULTS: 12/13, the fail is plan timing). The owner: magic batch
  before the stress test. Built (CHANGELOG 0.17.0): last preset + start-up chime, surprise me
  (SW1 + SW3), mutate (SW2 + SW3); `core/magic.h`, `tests/magic_test.cpp`, panel `Magic017`.
- Next: install 0.17.0 (the owner's press), TEST_SESSION 3K + the owner's ear; then the stress test to failure
  (bank 8 allowed); then magic items 4-7, virtual screen, webapp.

## 2026-10-11 (later): 0.16.0 Batch 1 + 2 built

- Implemented (CHANGELOG 0.16.0): step colour rule, knob page flash, harmony / parts setting
  colours while turning, CHOMPI page colour, SW5 lights (cutoff, scrub), SW5 filters the
  loop (`Looper::SetTone`), new take at 1×, v2 → v3 cutoff by waveform (host), chord colours
  on the keys, SW1 + SW2 harmony on/off, CHOMPI tap = recorder F#4, KEY_21 held 2 s = parts.
- Checks: 16 native suites + 135 Python pass (MinGW 16.2; `parts_test` built as
  `parts_suite.exe`, AVG blocks the old name); release 235,628 B / development 249,352 B,
  layout OK. Not run locally: sanitizers, browser tests, bench (CI).
- Not on CHOMPI: TEST_SESSION 3J (3.77-3.85). The owner approved install + stress test to failure
  (2026-10-11): installs need the owner's physical CHOMPI press; no card writes outside an agreed
  bank (the owner, 2026-10-11: bank 8 may be used for save / load stress; the rest of the card stays untouched).

## 2026-10-11: agent hardware checks, soak, UX review

- On CHOMPI (agent over USB + UMC + webcam, TEST_RESULTS 2026-10-11): all 34 automatic steps;
  harmony chords, arp / bass / latch, event recorder 5/5; 20-min rapid-key soak (24,137
  presses, no USB loss, 0 dropped, CPU peak 62.6 %). Videos in `reports/cam/` (gitignored).
- Fixed in code, software-tested, **not installed**: harmony Shift stuck after release
  (`core/harmony.h`); arp / bass phrase start sees a half-built chord (15 ms gathering,
  `core/parts.h`); starter-preset loader restores a neutral v7 first; demo presets 15-17;
  test-plan robustness (3.42 SW5 first, steady-tone silence rule).
- The owner's decisions and the per-function ease-of-use ratings (bar: 4/5 each):
  [UX_REVIEW.md](UX_REVIEW.md), PROJECT.md backlog.
- **Next (the owner approved 2026-10-11): Batch 1** = new first loop take at 1×; SW5 lights during
  scrub and cutoff turns; SW5 cutoff filters the loop; CHOMPI key colour per menu page;
  knob colour per menu-item value while turning + page flash; waveform-aware v2 → v3 cutoff
  upgrade (host). **Batch 2** = harmony keys lit by chord quality, Shift lit, quick harmony
  on/off; event recorder close / play / stop without the menu; direct parts-page entry.
  Then the owner allows install + stress test to failure (safety rules in the 2026-10-11
  conversation: physical CHOMPI press for installs, no card writes outside an agreed test
  bank, stop on crash / safe mode / power fault).
- Still open: SW3 DJ filter silencing, sound loss while modulating, unexplained looper
  take, menu opening during the knob test, 0.9 shut-off on battery, design question (1)
  (older preset switches harmony / parts off?).

## Late session 2026-10-10: start-up stall fixed, guided pass started (resume here)

- Fixed and verified on CHOMPI: the 30–60 s start-up stall and failing card writes (FatFs
  `FIL` objects on the DTCM stack; CHANGELOG 0.15.2). The owner's card repaired (chkdsk, empty
  leftovers deleted; byte-identical to the afternoon backup otherwise). Boot: 3 s.
- Installed on CHOMPI: the development build from the current tree (still reports build
  c93357b, "uncommitted changes": nothing committed since c93357b).
- Open, in order: (1) knob ring colour while turning (camera test, repeat in sync);
  (2) looper take nobody may have pressed; (3) menu opening / sampler slot during knob test;
  (4) SW3 DJ filter silencing; (5) misleading `toggle_up` name (true = physically down);
  (6) `forge_card.py install` traceback when CHOMPI restarts; (7) commit + push, then a
  clean build onto CHOMPI; then continue the guided pass from step 2 (knobs).

## Hardware: 0.15.2 on CHOMPI, unattended run (2026-10-10 evening)

The owner installed the development build (c93357b, dirty) from the SD card. It boots and
reports itself; over USB the agent ran the whole automatic plan: 34 of 34 pass after a
host fix and reruns (TEST_RESULTS 2026-10-10 has every figure). First device CPU: synth
stress peak ~44 %, sampler 55 %, sampler + TAPE effects 61 %. Open: start-up USB settling
(descriptor failure, late replies for ~1 min), one-off clicks in 6.2b (not reproduced),
brown-out among the start flags. Host fix: `forge_audio.detect` prefers a stereo input.
Next: the owner's listening session (sound and feel; the owner does not hunt failures), then the
person-only TEST_SESSION steps; re-run 6.2b at each session; a clean committed build.

## Checkpoint: firmware 0.15.2, HANDOFF items 2-4 (2026-10-10, local PC)

Owner request; for the crash guard the owner chose safe mode over "stop restarting".
- Implemented (CHANGELOG 0.15.2): build identity (status reply and Inspector page 2 end in
  patch, flags, 28-bit commit; `src/Makefile` writes `forge_build_id.h` from git; host
  `decode_identity` / `describe_build` read both layouts; Connect CHOMPI, webapp, Inspector
  and Check setup show it; check 1.3 expects 0.15.2). Safe mode (`core/restart.h`
  `StartupGuard`, `forge_main.cpp` `card` / `install_card` split, magenta lights, SAFE MODE
  line in RESTARTS.TXT, flag 4). CI: full gate + release firmware + bench; exe after tests.
- Checks (Windows, MinGW GCC 16.2 for host code, pinned Arm GCC 10.3-2021.10 for firmware):
  16 native + 130 Python PASS (card tests ran against this development build); release
  232,580 B / development 246,256 B, layout OK; `startup_guard` in backup SRAM (0x3880002c).
- Battery checker (the owner): power bytes in every status reply; `forge_host.py battery`; the
  automatic run reads the battery first and stops if an install would be refused. Tried on
  the owner's CHOMPI (stock TAPE): ports found, no Forge reply, SW6 advice printed. TEST_SESSION
  "Unattended over USB" is the owner's no-one-at-CHOMPI method (after the one install).
- Not run locally: sanitizers, browser tests, bench (CI runs them; first run unchecked).
  Not verified: anything on hardware; safe mode cannot be triggered on purpose.
- The MinGW `-O2` `forge_probe --render` crash came and went during these runs (passed in
  the last full run); still open, see the item 1 checkpoint below.
- Next: check the first CI run; the independent review (item 1's second half); then the owner's
  hardware session (charge first; TEST_SESSION 1.3 now checks the build). The owner tests sound
  and feel; agents run the failure testing over USB. After it: PROJECT.md "Backlog" (synth
  edit page on CHOMPI, starter preset upgrades / demos).

## Checkpoint: per-source held keys (HANDOFF item 1, 2026-10-10, local PC)

- Implemented: `Parts::keys_` is a bit per source (UART 0, USB 1, keybed 2, recorder 3);
  `KeyUp` takes the source and ignores a key that source does not hold; the arp set keeps
  per-note source bits (`set_sources_`) so a note leaves only when its last source lets go
  (also the missing set reference count). `Engine::PartsKey` passes the source.
- Tests (`tests/parts_test.cpp`): panel C4 held while source 1 / 3 plays and ends C4
  (unlatched), a stray key-up from another source, chords on two sources sharing a note, a
  fresh phrase after release, engine with the loop's source 3 and with harmony + MIDI. Two
  older cases released a key never pressed (one key carrying two notes); they now press two
  keys. The new cases fail on the previous `parts.h` (checked).
- Checks on Windows (MinGW-w64 GCC 16.2, `-D_USE_MATH_DEFINES` for `M_PI` under strict
  C++14): 16 native suites PASS; Python 125 of 128 PASS, 1 skipped, 2 errors, both also on
  the previous code: `test_card.test_low_power_refuses_the_install` needs
  `src/build-dev/FORGE.bin` (no ARM toolchain here; the test lacks the `skipUnless` guard
  its neighbour has), and `test_host.test_offline_audio_render`: `forge_probe --render`
  crashes (access violation) when built `-O2` with MinGW, also from the unchanged commit;
  `-O0` passes. Linux runs reported it passing; cause not found yet (possible latent UB).
- Not run: `make sanitize` (no ASan/UBSan runtime in MinGW), ARM builds, bench, browser
  tests. Not verified: anything on hardware.
- Next: an independent review of the 0.15.1 diff (`1cd73ae..7d1af64`) plus this change,
  then HANDOFF items 2-4; the `-O2` probe crash needs its own look (Linux / sanitizers).

## Moving to the owner's local PC (2026-10-09)

Work continues in a session on the owner's own PC. The release-candidate assessment (not an
RC yet), the ordered next tasks, standing rules and local setup are in
[HANDOFF.md](HANDOFF.md), section "Handoff: cloud session → the owner's local PC". First task:
the shared held-key count in `core/parts.h` (a bug in 0.15.1's own fix).

## Current checkpoint: firmware 0.15.1 review fixes (2026-10-06)

Owner request,
then "read the review. go for the fixes dude". The review (four parallel reviewers, each
finding verified against the code) found defects in 0.13–0.15 code, the host and the
docs. All verified findings are fixed; each code fix has a regression test.
- Implemented and software-tested (details in CHANGELOG 0.15.1):
  - Core: recorder note-off overflow (pending, not dropped), clock stop / MIDI Start end
    the recorder's notes (`Sequencer::Advance` / `Restart`), patch changes keep the loop
    (`Engine::Silence` → `Sequencer::Silenced`; `Engine::Panic` still stops it), MIDI Stop
    stays stopped (`Clock::Advance` timeout only while running), first MIDI Start seeds the
    tick length, per-key held count in `Parts` (`KeyDown/KeyUp` take the key),
    `Parts::Owns(note, source)`, `SelectSample` keeps v5–v7.
  - Storage / panel: `seq::kFileBuffer` (Storage::Read refuses a file that fills its
    buffer; mocks now do too), export after queueing (`PanelController` exports after a
    successful `PresetAction`; host stores via `StoreQueued` after the reply is pushed),
    `Mailbox::owner` (Panel / Host) with a claiming `SaveFile` that removes a stale `.FSQ`
    when a save went without its loop, host recall queues `SequenceLoad` after the patch
    (`RecallRequest` now carries bank/slot), KEY_22 / KEY_21 page-leave side effect
    (`began_on_tape`), `SlotSettingsSchedule` retry + `presets_old.json` step-aside.
  - Host: webapp ROUTES[7], start/end compared as 14-bit words (`unit_word`), CLI
    `upgrade --to 3..7`, `/api/session` `preset_errors`, `forge_ai.provider_schema`,
    `forge_host.FIRMWARE_VERSION` + `test_version_strings_agree`, stale labels.
  - Docs: install power everywhere (one USB port: charge first, then a computer USB-C
    port with C-to-C), HOME_CHECKLIST rewritten, TEST_SESSION corrections + new steps
    (bridge checklist 122), MANUAL / PROTOCOL / HANDOFF / PROJECT / README refreshed.
- Checks (this checkpoint): `make test` 16 native + 128 Python PASS;
  `ASAN_OPTIONS=detect_leaks=0 make sanitize` 15 PASS; `make browser-test` 11 + 7 PASS;
  `make bench` PASS (unchanged: pitch .75 2,676.2, 1× worst 2,610.5 vs WAVE 2,694.9);
  ARM release 235,380 B / development 249,188 B, layout OK (headroom 53,388 / 39,580 B).
- Not verified: everything on hardware (the owner has not installed 0.11 or later); the
  `presets_old.json` step-aside and `.FSQ` writes on real FatFS; a live AI request with
  the reduced schema (`host/forge_ai_check.py` is the owner's to run, with the owner's own key).
- Firmware still reports minor 15 on the wire (no patch number); docs and bundle say
  0.15.1. Next: the owner's hardware session, or the owner's next request.

## Resource/QA review, 2026-10-04 (UTC)

[RESOURCE_LEDGER.md](RESOURCE_LEDGER.md) is the current resource baseline and
update procedure. Reviewed head `fec2416` (firmware source `131776b`); independently
rebuilt it and handoff `b7d098b` with xPack GCC 10.3.1. Current release/development
227,060 / 242,512 B; executable headroom 61,708 / 46,256 B; SDRAM 66,715,664 B
reserved, 393,200 B free. Current hashes match the recorded kits. Exact deltas,
all memory regions and evidence are linked from the ledger.

Independent checks: 9 native + 95 Python PASS; 9 ASan/UBSan PASS with
`detect_leaks=0` after a container LeakSanitizer failure; 11 + 4 browser tests
PASS; both firmware layout checks PASS; emulated CPU gate PASS (worst 2,671.0
instructions/sample vs WAVE 2,694.9). No hardware, Windows kit or live AI run.
No firmware changes. Next architecture investigation: initialized queue image
cost (25,360 B footprint, savings unproven), then compact shared state/scheduler;
SD streaming remains gated on physical SD measurements. Older checkpoint and
PROJECT/HANDOFF/README summary paragraphs below may describe earlier milestones;
use the latest implementation checkpoint and ledger for present capabilities/budgets.

## Checkpoint: firmware 0.15 event recorder (2026-10-05; 0.14 parts and 0.13 harmony below)

Owner request = (1) finish harmony 0.13, (2) CPU fix A (table-based
cubic read), (3) clock + arp + bass, (4) event recorder + projects on SD. Item 1 is
this checkpoint. The owner is still on stock TAPE (battery charging); 0.11–0.13 are not
installed. One consolidated hardware session covers them all (TEST_SESSION).
- Implemented and software-tested: `core/harmony.h` (`6b4d1b8`) and engine/panel
  wiring; memory savings (queues `.bss`, `-Os` control code, start-up construction,
  `77fb5a7`); the harmony menu page (hold KEY_21 1 s; keys = tonic; SW4/SW1/SW2/SW3);
  Inspector page 8 + host chord names + a HARMONY display line; patch v6 (firmware,
  host, webapp, AI, presets saved on CHOMPI via `Engine::Snapshot`); version 0.13.
  Fixed while testing: the host refused v6 status replies (length/version list).
- Checks: `make test` 14 native + 121 Python PASS; `make sanitize` 13 PASS
  (`detect_leaks=0`); `make bench` PASS (worst 2,677.9 vs WAVE 2,694.9; pitch-below-1×
  2,800.8 informational); `make browser-test` 11 + 7 PASS; ARM release 217,104 B /
  development 229,768 B, layout OK (headroom 71,664 / 59,000 B).
- Docs: MANUAL 8a, PROTOCOL v6 + page 8, CHANGELOG 0.13, TEST_SESSION 3.65–3.67
  (bridge checklist 102), HARMONY_BRIEF "Phase 1 as built", RESOURCE_LEDGER snapshot,
  STORAGE_STREAMING figures.
- Not verified: everything on hardware, including the harmony page lights, chord
  sound and MIDI out (3.65–3.67), and CPU peaks of the `-Os` control paths (6.2).
- Item 2, CPU fix A (after `2180da2`): table cubic read with the M7's dual multiply-
  accumulate, plus a glide fix (glides stalled short of the target and never ended),
  declick tails in a bit mask, fewer per-voice checks. Emulator: pitch .75 2,669.2,
  kit below 1× 2,564.2, 1× worst 2,603.5 vs WAVE 2,694.9 (pitched-down rows now
  gated). Checks: make test 14 native + 121 Python, sanitize 13, browser 11 + 7 PASS;
  release 221,712 / development 234,376 B layout OK. Not verified on hardware (6.2f).
- Item 3, clock + arp + bass = firmware 0.14 (after `dd06649`). The owner's answers (2026-10-05):
  internal tempo + follow MIDI clock + send it, tempo settable on CHOMPI (SW4, tap C5); parts
  page with latch; bass internal + MIDI ch 2; ROOT/FIFTH/ALTERNATE/OCTAVE on the clock.
  Implemented and software-tested: `core/parts.h`, engine wiring, parts page (KEY_21 from
  the harmony page), MIDI real-time in / clock out, patch v7 (firmware, host, webapp, AI),
  Inspector page 9. Fixed on the way: the AI instrument mode asked for v5 while requiring
  harmony (0.13); TEST_SESSION 4.1 / PROTOCOL named SW5 as the panic.
  Checks: make test 15 native + 123 Python, sanitize 14, browser 11 + 7 PASS; bench
  unchanged PASS; release 229,168 / development 242,896 B layout OK.
  Not verified: everything on hardware (3.65–3.72), arp timing feel, MIDI clock with a DAW.
- Item 4, event recorder + projects = firmware 0.15 (after `0b09d45`). The owner's answers
  (2026-10-05): notes + knobs looped with overdub; parts-page keys; project = preset +
  parts + loop; MIDI-in notes recorded. Implemented and software-tested: `core/sequencer.h`,
  `core/sequence_store.h`, engine / panel / firmware / probe wiring, Inspector page 10.
  Checks: make test 16 native + 124 Python, sanitize 15, browser 11 + 7 PASS; bench PASS
  (pitch .75 2,676.2); release 234,448 / development 248,256 B layout OK.
  Not verified: everything on hardware (3.65–3.76), FatFS writes of `.FSQ` on the owner's card.
- The owner's items 1-4 are all done in software. Next: the owner's hardware session (install 0.15 once
  the battery is full, then TEST_SESSION), or the owner's next request.

## Checkpoint: firmware 0.12 per-slot sample settings (2026-10-05)

**Queued next (the owner, 2026-10-05):** the Harmony / Intent Engine brief,
[HARMONY_BRIEF.md](HARMONY_BRIEF.md), to start after the safety additions are done.
Begin with its Phase 0 / first deliverable (architecture and proposal) before code.

A4 built on top of 0.11, per the owner's four answers (save as TAPE, per-pad kit settings,
slot wins over a Forge preset, share TAPE's presets.json with a backup). The owner is still
on stock TAPE while the battery charges; neither 0.11 nor 0.12 is installed.
- Implemented and software-tested: `core/slot_settings.h` (TAPE layout/units, v1
  read, 8 KB bound, menu save/copy/erase); Synth per-voice settings (window, loop,
  gate, envelope pointer, pad pitch/gain/pan) and kit pads; Engine slot ownership
  (`SlotPolicy` Patch/Recall/Select, focus pad, save on turn, TAPE-default reset);
  firmware load at start-up and card insert, write 2 s after the last change and
  before an install restart, backup first; probe and knob audio test use it.
- Checks: `make test` 13 native (new slot_settings_test) + 119 Python PASS;
  `make sanitize` 12 PASS; ARM release 261,072 B / development 277,028 B layout OK;
  `make bench` PASS (worst 2,677.9 vs WAVE 2,694.9); browser 11 + 7 PASS.
- Decided (owner request): every voice below 1× (SW4 pitch down, or kit
  pads) is ~4 % over WAVE in the emulator (2,800.8), since 0.10; the cubic read stays,
  accepted if hardware 6.2f stays < 70 %. Revisit only if 6.2f fails.
- Safety extras (owner request): SW6 low-battery warning blink, Inspector
  power flags 8/16/32 and Check setup warning, TEST_SESSION 8.5/8.6. Checks: make
  test 13 native + 119 Python, sanitize 12, browser 11 + 7 PASS; release 261,224 B /
  development 277,268 B layout OK; no DSP change (bench as above).
- Not verified: everything on hardware; TAPE reading Forge's file (3.64); FatFS
  rename/unlink of presets.json on the owner's card; the warning blink's readings on a real
  weak supply (8.5).
- Next: Harmony Phase 1 (0.13), decisions and plan in HARMONY_BRIEF.md "Phase 0 decisions".
- Correction to the 0.11 entry below: its `make test` claim was premature; the
  consistency test failed at `cfcae21` (7.3p missing from the bridge checklist),
  fixed in `fd08b3e`.

## Checkpoint: firmware 0.11 install safety (2026-10-05)

The owner chose both, safety first: 0.11 install safety, then A4 (per-slot sample
settings, now 0.12). The owner is on stock TAPE 2.0 while the battery charges (yellow at
last report); no Forge firmware is installed now.
- Implemented and software-tested: install power check (`core/power.h`
  `InstallPowerOk`, error 10, file-reply flag 16, re-check at the press; host
  `Card.check_power`, bridge refuses before uploading); stock lockout detection and
  "battery low" lines in `FORGE/RESTARTS.TXT`; vector table copy in DTCM; aligned
  options buffer; version 0.11. Probe: `FORGE_PROBE_POWER_LOW=1` simulates it.
- Checks: `make test` 12 native + 119 Python PASS; `make sanitize` PASS; ARM release
  254,756 B / development 269,808 B layout OK (xPack 10.3.1); `make bench` PASS
  (worst 2,670.9 vs WAVE 2,694.9; unchanged, no DSP change); browser 11 + 7 PASS.
- Not verified: anything on hardware; the real charger flag behaviour (legacy,
  IINDPM) on the owner's supplies; whether the units' v6.2 bootloader waits like the
  v6.4-beta source. Next hardware step: install 0.11 from the card (TAPE → Forge
  needs the SD route) on a USB-C charger with the battery green/white, then Check
  setup; TEST_SESSION 7.3p optional.
- Next: A4 (0.12).

## RESOLVED: CHOMPI dark after the 0.10 install (the owner, 2026-10-05)

The owner installed 0.10 through the bridge (USB install), then could not connect; later
CHOMPI "won't turn on". The red charging LED is on with USB plugged in. Nothing
here is hardware-verified; causes are not established.
- Not a brick: the SD/USB install writes QSPI only; the bootloader lives in internal
  flash and still installs any `.bin` on the card root at power-up.
- Leading hypothesis, power: a flat battery. The bootloader's
  `LowBatteryLockoutCheck` spins with no lights while the battery is low on a
  low-current source (PC port / legacy cable), and goes to shipping mode if low and
  unplugged; the app shows 15 s amber flashes, then shipping mode. Would also explain
  the earlier "random shut-off while playing".
- Alternative, 0.10 at boot: rainbow lights followed by dark or repeating rainbow
  would point at 0.10 (new fault handler resets instead of hanging; VTOR copy;
  boot-time SD reads for options/restart log; USB MIDI out).
- Sent the owner: charge from a wall charger with the switch on; if still dark, put only
  one `.bin` on the card root, either Forge 0.9 (dev build of `1e0e35a`, xPack
  10.3.1, 253,696 B, sha256 8a9b4785…ac9100) or `card-profiles/tape-2.0`. Waiting
  for the owner's light description and `FORGE/RESTARTS.TXT`.
- The owner, later: the bridge lost CHOMPI during the install (expected: the install
  restarts it); with the switch on there are no lights at all at power-on, red
  charge light only. The bootloader's battery check runs before its first light and
  before it reads the card, so no light at all means no firmware (0.9 or 0.10) has
  run yet. That supports the battery hypothesis; the owner is charging from a wall charger.
- Hardware (Rev4 schematic sheets 1-2, BOM): both red LEDs (LED_STAT1 charge,
  LED_PG1 input power good) are driven by the MP2722 charger itself, so they say
  nothing about the Daisy. Power: USB-C -> SW7 (battery bypass) -> MP2722 SYS ->
  S1 main switch -> VSYS -> LMR62421 boost (VBOOST ~9.6 V) -> Daisy Seed2 DFM VIN; the
  35 RGB LEDs only light when the Daisy sends data. USB data passes a USB3740 switch
  whose select (USB_SW, PC3) is pulled low = charger side until firmware drives it,
  and the schematic brings no BOOT/RESET out: STM32 ROM DFU through CHOMPI's USB-C
  is therefore not expected to work (earlier advice corrected). Test points: TP15
  BATT_P (battery voltage), TP14 VSYS_BMC.
- Bootloader detail (v6.4-beta source, `boot_hardware.h` LowBatteryLockoutCheck): the
  wait `while(batt_low && (legacy_cable || iindpm_stat)) {}` never re-reads the
  charger, so once entered it lasts until power is cycled, however long it charges.
  The owner's power bank showed 2.5 W (0.5 A default USB) on USB-A, 9.4 W on USB-C. Told the owner
  to switch off and on with USB-C attached. (Assumes units' v6.2 does the same;
  only its binary is in the repo.)
- Outcome (the owner, photos): after switching off and on with USB-C (9.4 W), the bootloader
  ran (gradient lights across the keys) and CHOMPI started; the knob lights then
  match 0.10's sampler-page colours (SW4 green, SW1 yellow, SW2 red, SW3 teal).
  Correction: the owner then said they loaded TAPE, so those lights may be TAPE's own
  (0.10 copies TAPE's colours); whether 0.10 ever ran on hardware is unknown. The owner is
  on stock TAPE 2.0 for now. Cause of the dark unit: the bootloader's low-battery
  wait (flat battery + 0.5 A USB-A supply), not established to be 0.10. Still unconfirmed: firmware version and Last start via the bridge (Check
  setup). For hardware sessions: power CHOMPI from USB-C to USB-C. Pending, low
  priority: 0.10.1 hardening (clean D-cache after the vector-table copy or place it
  in DTCM; 32-byte-align the options.json buffer; stop restarting after repeated
  start-up crashes).

## Checkpoint: firmware 0.10 TAPE parity, ready for the owner's install (2026-10-05)

Everything the owner chose in the parity checklist is built and software-tested except A4
(TAPE presets.json per-slot settings), which the owner moved after 0.10 (now 0.12, after 0.11 install safety).
See CHANGELOG 0.10 (stage 1, 2a, 2b), KNOBS.md, MANUAL sections 4–11, TAPE_CONTROLS.md.
Checks at afc64be: make test (12 native + 116 Python), make sanitize, browser tests
(11 + 7), make bench PASS (2,670.9 ≤ WAVE 2,694.9; all TAPE effects 3,077.9
informational → 6.2e), ARM release 253,636 B / development 268,728 B layout OK.
Nothing hardware-verified. On install, run: Check setup (Last start line), the
panel walk (new light answers), automatic checks (3.57a–f knob audio, 6.2e CPU),
then TEST_SESSION 3.51–3.61 by hand.
Unverified/risks: Daisy bootloader may clear RCC_RSR; white-balance factors guessed;
Saw Bass v5 conversion sounds ~20 % different in spectrum; Acid Bass/Bell Keys LFO
is on the mod wheel (panel can't reach it); MIDI out on USB unverified; FORGE_COLD
(-Os) panel code — check no audio glitches when turning knobs.
Next (0.11): A4 per-slot settings (read/write TAPE presets.json, back it up first);
The owner's hardware findings from 0.10.

## Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05

The owner reported no lights on key presses. Added TAPE's NormalPage key lights with
the menu closed (`RenderPlayLeds` in core/panel_controller.h): held keys white,
sampler kit slots dim in the bank colour (recording slot pink), chromatic C3/C4/C5
markers. Held keys reach the main loop as a 32-bit atomic (`KeysDown()`; note keys
are switches 0–31; a 64-bit atomic does not link on the M7). Checks: `make test`
(11 native + 112 Python), `make sanitize`, `make browser-test`, ARM release
238,092 B / development 253,696 B, layout OK. Not on hardware. Storage-streaming
figures unaffected (no SDRAM, sampler path or loader change; code +1 KB).

**Hardware feedback on 0.9 (the owner, 2026-10-05):** recording works. Open: random
shut-off while playing keys (not reproduced; details to capture and candidates in
TEST_RESULTS 2026-10-05); toggle labels backwards everywhere (menu = toggle
DOWN, record = UP; firmware matches TAPE, naming only); requested record
count-in (~1.5 s, CHOMPI + white keys blink red 3×, release cancels). The owner asked
to wait for the rest of the hardware findings before any firmware change, then
one install. **Parity checklist (2026-10-05):** the owner asked to keep TAPE's workflow. TAPE's controls
are catalogued in [TAPE_CONTROLS.md](TAPE_CONTROLS.md); every difference is a row in
the checklist artifact (claude.ai/artifact/ErLnHhu2KkYKJ2qBm4K8SG) with a recommended
answer. Waiting for the owner's answers; then one firmware update. Biggest finding: TAPE
knobs move .03 per click (Forge 1/127), which explains why SW4 seemed to do nothing. The owner's
answers so far: SW5 push-and-turn for loop speed with panic moved elsewhere; count-in
1.5 s; convert old presets.

**The owner's parity answers (2026-10-05):** A1 TAPE steps; A2 page on release; A3 hold a
knob **1.5 s** to reset; A4 open (question sent); B0 recommended (TAPE layout,
synth equivalents); B1 TAPE pitch/gain; B2–B4 TAPE layout **but keep Forge's synth
controls** (LFO, filter env, detune…) on extra pages, audibility via A1 (layout
proposal sent); B5 turn = cutoff, push-and-turn = loop speed/scrub, click = 1×;
B6 SW6 press = input gain; B7 TAPE; C1/C2 TAPE menu shift layer; D1 vel 127; D2
MIDI out; E1 relabel; E2 count-in 1.5 s; E3 TAPE monitoring + 3 modes; E4 TAPE
CHOMPI light; E5 record latch; E6 TAPE after-recording; F1 TAPE menu first, Forge
presets on a second page; G1 TAPE's saturation/warble/DJ filter/compressor **plus**
Forge's filter; H1/H2 TAPE; I1 panic = SW4 + SW3 held 1 s; J1 read options.json;
K1 keep Forge (no lockout); L1–L5 recommended. Also: "only one key plays" — Acid
Bass is 1 voice by design (others 4, samplers 7). Shut-off: the owner saw it only while
playing keys rapidly (not with many keys held, knobs, looper or recording).
Native ASan/UBSan stress (14 presets × ~37k random fast presses, chords, knob
turns, LED composition): no fault — points away from a core memory bug; restart
reason logging (L2) is the next step.

Later feedback: knob "white" looks light blue (LED balance; TAPE is the same);
SW4 on Glass Keys inaudible (1/127 per click; pages 2–4 dead on v1/v2 patches);
wants long-press knob reset (later). Starter presets: bridge job `presets`
(no firmware change), done. Idea for the shut-off: record the STM32 reset cause (RCC_RSR) and a
fault marker in backup SRAM, show them in Check setup.

## Previous checkpoint: firmware 0.8, power as stock, 2026-10-05

The owner's battery ran flat on 0.7 and the owner asked for power/charging "and any other thing
we missed". Review: COMPATIBILITY §7 (stock TAPE/WAVE/TEMPO main loops and
TAPE's NormalPage). Found and fixed (software-tested, not on hardware): no
shipping/storage mode (stock: hold CHOMPI + PLAY + LOOP at start-up; CHOMPI's
hardware switch S1 is the normal on/off and was never affected); no battery display (TAPE: SW6 held 2 s); no runtime
USB/charger hand-over (TAPE only). Added battery/charger state to Inspector page 2
and Check setup. Also fixed the bridge's line-in hint that contradicted the
headphone-out advice. Not ported: the factory test page (SW6 at start-up) — use
stock firmware for it. Checks: 11 native suites (new `power_test`, SW6 hold in
`panel_test`) + sanitizers, Python and browser suites, ARM release 237,068 B /
development 252,760 B, layout OK. Open: how CHOMPI is switched back on after
shipping mode (the owner to confirm), TEST_SESSION 8.1–8.4.

## Design checkpoint: storage streaming / SDRAM reclamation, 2026-10-04 (parked for v2)

The owner asked for a feasibility study and design only, stopping for architecture
review before any memory allocation changes: [STORAGE_STREAMING.md](STORAGE_STREAMING.md).
Verified baseline (head `753078b`): SDRAM 66,715,664 B used, 393,200 B free
(32 MiB pool, 16 MiB recorder, 16,000,000 B looper, 384,016 B delay). Key
finding: stock TAPE already streams all 7 voices from SD with ~85 ms FIFOs and
`_double.wav` for pitch-up. Estimates: ~36 MiB reclaimable (conservative) to
~42 MiB (balanced) for ~7–12 KiB of code. The owner (2026-10-05): **move to Forge v2**;
keep it as the reference for limits and possibilities. When v1 touches SDRAM,
the sampler read path, loader/recorder/looper, USB file replacement, FatFS or
code headroom, update the study (its "Revalidate" list). No code changed.

## Current checkpoint: loader stress review, per-step CPU, home checklist, 2026-10-04

The owner (away from the hardware) asked for items 1-3: stress-test the USB loader,
per-step CPU, tidy up for the first session home. Software-tested only;
0.7 has still never run on CHOMPI.
- **Bug found and fixed:** the CHOMPI bootloader (v6.4 `SearchBin`) flashes the
  first visible root file whose name *contains* `.bin`/`.BIN` (`strstr`), so
  0.7's `TAPE.bin` → `TAPE.bin.old` set-aside still left a file it could pick
  before FORGE.bin. Now `.bin` becomes `_bin` (+ `.old`), anything the
  bootloader would match is renamed (no overwrite, batches until none are
  left), and Install fails if one cannot be renamed. Same rule in the probe.
- Device-side guards: FORGE.bin must pass the bootloader's image test (stack
  in DTCM/D1, Thumb entry inside the image) at End; it is read back from the
  card (size + CRC-32, 4 KB aligned buffer in D1 SRAM) before it replaces the
  old FORGE.bin. Repeated End for the file just written succeeds (lost reply);
  Install abandons an upload idle 5 s. `static_assert` that Data chunks stay
  below one sector (f_write never DMAs from the DTCM stack).
- Host: Begin/End retried on a lost reply; a failed upload sends Abort.
- Tests: bootloader names/set-aside, image guards, card read-back corruption,
  lossy link (40 uploads with drops, duplicates and late requests both ways),
  200,000 fuzzed requests (4,695 complete uploads, busy-loader refusals) under
  ASan/UBSan; Python: lost End reply, dead link leaves nothing open.
- Per-step CPU: status (02) takes an optional flags byte, 1 = reset the peak
  after the reply (audio callback owns the meter). The runner resets at the
  start of every step that reads status; reports mark "since boot" on 0.6.
- `HOME_CHECKLIST.md` (in the exe download and the development kit); the exe
  artifact now holds `Forge Bridge.exe`, `FORGE.bin` and the checklist.
- ARM (xPack 10.3.1): release 235,980 B (SRAM_EXEC 81.7 %), development
  251,684 B (87.2 %); SRAM 108,196 / 112,260 B; layout checks OK.
- Kit `Forge-Bridge-dev-b36ba99.zip` (26,910,887 B, sha256 `4b8277f0…565647`,
  FORGE.bin sha256 `7953ab0c…6890c9b39`; `verify_bundle.py`: 1301 files OK)
  sent to the owner. CI run 37195690217 on `b36ba99` green (firmware, native tests,
  exe build and smoke test; artifact = exe + FORGE.bin + checklist).

## Previous checkpoint: firmware 0.7 USB loader and Forge Bridge.exe, 2026-10-04

Owner request. Built (software-tested; nothing hardware-verified):
- Firmware 0.7 (`0454ae8`): opcode 0C file transfer (FORGE.bin and TAPE
  samples, staged + CRC-32, resume), install with a CHOMPI key press and a
  restart into the bootloader. PROTOCOL "USB file transfer", TEST_SESSION 7.
  Release 233,748 B, development 249,580 B. The owner needs one last SD-card flash
  to get 0.7 onto CHOMPI; after that, updates go over USB.
- Host: `forge_card.py`; bridge *Card & firmware* section; kit `card` folder.
- `Forge Bridge.exe` (`d3c8531`): GitHub Actions builds the development
  firmware on Linux and a one-file PyInstaller program on Windows with it
  inside, then smoke-tests it. FORGE_HOST_DIR relocates the host's files.
- CI run 37193971640 on `dac79f7` is green (first run failed on a broken
  Makefile rule, fixed in `dac79f7`): firmware job built the development
  FORGE.bin, passed the layout check and native tests; the Windows job built
  `Forge Bridge.exe` and its smoke test passed (server, pages, presets,
  firmware inside, card tool). Artifact `Forge-Bridge-exe` (23.6 MB zip,
  expires 2027-01-02): Actions → Forge Bridge (Windows exe) → latest run.
  Not yet run on the owner's PC.
Untested on hardware: USB throughput, FatFS on the real card, the bootloader
taking the new FORGE.bin after the restart, the exe on the owner's PC.

## Previous checkpoint: first hardware session and bridge test tooling, 2026-10-04

The owner flashed the `131776b` development kit and ran automatic checks four times
(results: [TEST_RESULTS.md](TEST_RESULTS.md)). Firmware behaved as designed
wherever measured (pitch, keybed, looper states, knob pages, v5 knobs, CPU peak
≤ 37.6 % since boot in the third run); most audio failures were the rig (left
output only, 144 Hz hum on IN 1, interface clipping, no line in, no samples).
Bridge work since (host only, firmware unchanged; commits `7f04996`, `fbd98c1`,
`2b0ec66` and the setup/walk commit): starting-state `ensure` actions, line-jack
aware detection, setup check, guided panel walk with light questions and camera
evidence, re-run failed steps, `CLAUDE.md` for an agent on the test PC. 103
Python + 16 Chromium tests pass; the walk's physical part is untested on hardware.
The owner also got the TAPE bank-a sample zips and the Panel Map / QA sheet artifacts.

Next: the owner fixes the rig (right output to IN 2, IN 1 gain −10 dB, a USB port or
hub without the hum, interface OUT 1/2 → CHOMPI line in, samples at the card
root), runs Check setup, the panel walk and the automatic checks, then the
QA-sheet hand checks. Open: per-step CPU (firmware: reset peak), raise the
looper voice cap to 7 if 6.2d agrees.

## Previous checkpoint: knob pages and patch knobs (firmware 0.6), 2026-10-04

Owner request
Decisions (AskUserQuestion): **both** fixed pages and patch/AI-assigned knobs,
press a knob to change its page and its light shows the page; SW6 press does
nothing for now; build **now**, before the hardware test, and send a new kit.
Design, map and costs: [KNOBS.md](KNOBS.md). Not run past the owner as a separate
design doc (they answered the design questions directly).

Built and software-tested (nothing hardware-verified):
- Panel: SW4/SW1/SW2/SW3 presses step each knob's page; page 1 = the patch's
  knob, pages 2–4 = filter / envelope / LFO-osc / space; knob lights
  (through-hole LEDs 1–4, TAPE's map) dim white, red, green, blue.
- Patch v5 (`knobs`, request 88, status 100, firmware minor 6); v1–v4
  unchanged. Every continuous control has a Parameter id; version-gated.
- Host/AI/webapp v5, preset `14-knob-pad.json`, Inspector page 3 (97 bytes)
  with knob pages, MIDI framer 92 bytes, TEST_SESSION 3F (3.52–3.56), 78
  bridge checks, automatic checks 3.52/3.53/3.55 (pass in simulation).

Checks 2026-10-04: `make test` 9 native suites PASS + 95 Python tests OK; `make sanitize` 9 PASS; `make browser-test` 11 + 4
OK; `make bench --check` PASS (unchanged: worst 2,671 ≤ WAVE 2,695); ARM
(xPack GCC 10.3.1) release 227,060 B (78.6 %), development 242,512 B, both
layout OK. Hardware: none.

Kits from `131776b` (xPack GCC 10.3.1), both `verify_bundle.py` OK, both
images pass the bootloader/layout image check. **Use these for the hardware
session** (they include the looper and the boot fix; older kits are stale):
- `Forge-Bridge-dev-131776b.zip` 26,869,464 B, sha256 `0e31e669…38fc47e6`,
  FORGE.bin (development) `b663e4d9…4798d0c`, 1,292 files.
- `Forge-0.6-test-131776b.zip` 4,045,406 B, sha256 `9f588c32…39af80f5`,
  FORGE.bin (release) `bce7d1f6…937602c`, 67 files.

Risks: knob light positions and encoder-press reads (from TAPE's source),
zipper noise on fast turns of filter/envelope controls, the 102-byte v5
status over USB/UART (two USB packets, as v3/v4).

## Previous checkpoint: looper (roadmap item 5), 2026-10-03

Owner request.
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
Exact figures in the commit messages. Hardware: none.

Kits from `b7d098b` (xPack GCC 10.3.1), both `verify_bundle.py` OK and both
images pass the layout check. Superseded by the `131776b` kits above (they
add the knob pages):
- `Forge-Bridge-dev-b7d098b.zip` 26,351,953 B, sha256 `e7044fc0…bbd42ad41`,
  FORGE.bin (development) `8f680063…361e29`, 1,290 files.
- `Forge-0.5-test-b7d098b.zip` 3,526,677 B, sha256 `12ed81f4…a31c268e`,
  FORGE.bin (release) `2181994c…b3f23a`, 65 files.

Risks: real CPU of 7 voices + looper (6.2d decides the cap), SDRAM
bandwidth, KEY_27/28 and LEDs 7/8 (taken from TAPE's source), loudness of
overdub stacking.

## Earlier checkpoint: boot fix and cleanup, 2026-10-03

The owner asked to review the new community CHOMPI firmware forks (sfaber02,
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
Sent to the owner in chat; the owner keeps them on Google Drive. Older kits
(`Forge-Bridge-dev-50cdc4b`, `Forge-0.5-test-b7a504a`) lack the boot fix: do
not flash them.


The owner reviewed the bridge and asked for it to be simpler to set up. Built on
The owner's (ChatGPT session's) browser bridge at `7fa8624`, which an agent reviewed
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
  agent on the owner's PC).
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
GCC 10.3.1; `verify_bundle.py`: 1,290 files OK). Sent to the owner in chat; the owner keeps it
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

The owner wants an **AI-programmable playable instrument**, not effects-only: describe a
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
- The owner decisions made 2026-10-03: CCs match stock (done: CC20+n = encoder n);
  start-up stays dry aux; QA bundles go to the owner's Google Drive (the owner uploads;
  agents have no Drive access). Still open: configurable MIDI channel.
- Features (PROJECT.md roadmap): 5 looping (TAPE's looper; ~7.6 MB SDRAM
  and ~73 KB code left in release, ~57 KB in the development build), 6 Tab5 controllers (optional).
  Smaller candidates: TAPE per-slot settings, threshold-armed recording, kit
  MIDI note range, preset/sample names, mono note stack, octave shift, bend
  range, cheaper sine for Bell Keys.
- QA (the owner, later, per feature): LIVE_AI_TEST.md (own key, own terminal), then
  TEST_SESSION.md in one hardware session; record in TEST_RESULTS.md.
- Hardware-unverified risks: device CPU (above all 7 sampler voices reading
  SDRAM, TEST_SESSION 6.2c), SD load/save speed and main-loop stalls, card
  swap, recording levels/monitoring/jack detect, LED positions/colours,
  UART/USB reply timing (98-byte v4 status), v3 loudness by ear, line-out vs
  headphone level vs stock.

## Branch and publishing

Repo the Forge repository, branch `forge/foundation`, draft PR #1 (still draft;
`main` untouched). The integration checkpoint is on the remote as commit
`15681b8cc10dd76bdff4a6d39e62cc96bc75c7b2` ("Integration review, real browser
tests, live-AI prep, 0.3 bundle tooling"), parent `2b30b1c`, tree
`cf807ad0bcfd0d107cc853294d6eec6ad060fe02`. The SHA differs from the
locally made `558810e` because the owner's connector re-created the commit; the tree
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

The owner chose to implement the measured mitigations. Figures are from simulation
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

## Development plan (the owner, 2026-10-02): features first, QA per feature later

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

## Stock firmware comparison and CPU benchmark (2026-10-03, the owner's request)

Full write-up: [COMPATIBILITY.md](COMPATIBILITY.md). Key facts for agents:
- Bootloader (TAPE's Chompi_Bootloader source) loads the first non-hidden
  `*.bin`/`*.BIN` on the card root, checks SP in RAM and entry in D1 SRAM or
  QSPI. FORGE.bin passes, same SP/entry layout as factory TAPE/TEMPO/WAVE.
  Risk: macOS `._FORGE.bin` (not hidden) can be picked first → TEST_SESSION 1.1.
- Keybed note table identical to stock in all three apps (checked by script).
- MIDI: stock maps CC20+n → encoder n (CC24 ignored, CC25 → SW6), channel
  from options.json, CC input optional, notes/CCs sent out. Forge: CC24
  bypass, CC25 cutoff, fixed channel 1, no MIDI out. Open decision for the owner.
- Upstream rebuilds (scratch copy, xPack): TAPE needs a `Limiter.h` symlink
  on Linux and then overflows SRAM_EXEC by 3,524 B; TEMPO (built with 10.3, superseded below) builds 4,660 B
  smaller than factory. Repo sources + xPack ≠ factory builds.
- `make bench` (bench/): Forge presets 209–1,428 instructions/sample; TAPE
  FX+output 1,249 and TEMPO FX+output 1,362 (10.3; 1,352 with 13.3) (voices excluded, lower bounds);
  WAVE full 8-voice engine 2,695–2,747. Gate: Forge ≤ WAVE. Emulator notes:
  A-profile "max" core (Unicorn M-profile lacks FPU enable), flush the TB
  cache after adding the counting hook, peripheral range mapped as scratch
  for TEMPO's timer init, link with `-u` roots or gc-sections drops entry points.
- Refresh after device presets (2026-10-03, `4fec6ac`, the owner's request):
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

The owner chose a key and encoder combination like TAPE's (2026-10-03). TAPE's menu
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

Owner request. Design and differences: SAMPLING.md.
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

0. Done 2026-10-05: 0.8 installed over USB on hardware (first card-free update);
   every automatic check passes on 0.8; power 8.1–8.3 pass (8.4 optional, not run).
   Next for the owner: 6.2d (sampler + looper CPU → looper voice cap), QA-sheet listening.
1. The owner, at home: [HOME_CHECKLIST.md](HOME_CHECKLIST.md): one last card flash of
   0.7 (only FORGE.bin may contain ".bin" in the card root), rig fixes, Check
   setup, panel walk, automatic checks, one USB install and one sample upload;
   send the reports folder.
2. Agent, after that session: fix only what it finds; record it in
   TEST_RESULTS.md. Then decide the looper voice cap (7 if 6.2d agrees) from
   the per-step CPU figures.
3. Storage streaming: parked for Forge v2 (STORAGE_STREAMING.md); keep its
   "Revalidate" list in sync with v1 changes.
4. Open, needs the owner's choice: next instrument features (arpeggiator/sequencer,
   more effects, configurable MIDI input channel, Tab5 controller).
5. Unverified on hardware: everything in 0.7 (USB throughput, FatFS writes and
   read-back on the real card, the restart into the bootloader, the exe on
   the owner's PC), the knob lights and encoder presses, zipper noise.

Roadmap items 1–5 are built (PROJECT.md); bundles before 0.7 are stale.
