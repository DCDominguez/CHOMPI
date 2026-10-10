# Forge handoff — instrument candidate 0.15.1

Updated 2026-10-09 (UTC). **Read [CONTINUE.md](CONTINUE.md) first**; it is the
live checkpoint with claim levels, what changed and prioritized next actions.

## Handoff: cloud session → DC's local PC (2026-10-09)

DC is moving work from a Claude Code cloud session to a session on DC's own PC
(`claude remote-control` in the repository folder, or the Claude Desktop app). The
conversation does not carry over; this section and CONTINUE do.

**Where things stand**
- Branch `forge/foundation`, last code commit `7d1af64` (0.15.1, pushed). Working
  tree clean. Draft PR #1 untouched; the designated `claude/forge-foundation-review-*`
  branch is unused. GitHub Actions "Forge Bridge (Windows exe)" passed on `7d1af64`.
- Firmware 0.15.1 = 0.15 event recorder and projects + fixes for every verified
  finding of DC's full review (CHANGELOG 0.15.1). Software-tested: `make test`
  16 native + 128 Python, `make sanitize` 15, `make browser-test` 11 + 7, `make bench`
  PASS (pitch .75 2,676.2 vs WAVE 2,694.9); ARM release 235,380 B / development
  249,188 B, layout OK.
- Hardware: nothing from 0.11 on has run on a CHOMPI; whether 0.10 ever booted is
  unknown (CONTINUE, "RESOLVED: CHOMPI dark after the 0.10 install"). DC was last on
  stock TAPE 2.0 with a full battery.

**Release-candidate assessment (2026-10-09): not an RC yet.** Blockers and to-dos,
in the order recommended to DC:
1. *(Fix done 2026-10-10, see CONTINUE; the independent review is still to do.)* *Bug in 0.15.1's own fix (unreviewed code):* `core/parts.h` `keys_[128]` (held-key
   count) is shared across note sources, so holding C4 on the panel while the recorded
   loop or MIDI plays C4 through the arp lets the other source's note-off release the
   panel key (unlatched phrase ends early). Fix: per-source bit per key (4 sources).
   Related, older: the arp set (`Add` / `Remove`) has no reference count. Add tests in
   `tests/parts_test.cpp`, then get an independent review of the whole 0.15.1 diff
   (`1cd73ae..7d1af64`), especially the mailbox hand-over (`core/sequence_store.h`
   `SaveFile`, `PanelController` export after `PresetAction`, `StoreQueued`).
2. *Build identity:* the device reports only minor 15; 0.15.1 cannot be told from 0.15
   on CHOMPI. Report a build id (commit hash or patch number) in status / Inspector and
   show it in Connect CHOMPI / Check setup.
3. *Start-up crash guard:* "stop restarting after repeated start-up crashes" (0.10.1
   hardening list) is still not implemented.
4. *CI:* the workflow runs 4 of 16 native suites, no Python tests, sanitizers or bench,
   and ships only the development build (test hooks). Add the full gate and a release
   build. Builds use xPack GCC 10.3.1, not the pinned Arm 10.3-2021.10 archive.
5. *DC's hardware session* (HOME_CHECKLIST → TEST_SESSION), on charged power. Critical:
   1.x (boots, right version), 4 (panic), 6.2b–g (CPU), 7–8 (install / power), and
   reproducing or ruling out the 0.9 "random shut-off while playing" (still open).
6. Then tag `forge-0.15.x-rc1` from the exact commit tested. No tags or releases exist.

Items 1–4 are software work an agent can do now; 5 is DC's.

**Standing rules (DC)**
- Work on `forge/foundation`; never force-push; never touch `main`; no PRs unless asked.
- No real API keys in chat, source or logs; DC runs `host/forge_ai_check.py` with
  DC's own key.
- Agents never flash hardware or claim hardware results DC did not report. One
  consolidated hardware session, no per-feature flashing requests.
- Keep upstream firmware (chompi-tape / wave / tempo, bootloader, vendored libDaisy)
  untouched.
- Commit messages separate implemented / software-tested / hardware-verified and end
  with the session's `Co-Authored-By` / `Claude-Session` trailers. No model IDs in
  artifacts. Report branch and commit after each push.
- AGENTS.md checks: native tests for audio / protocol, sanitizers for DSP, ARM build for
  firmware; v1 presets keep working; no allocation or blocking I/O in the audio
  callback; update CONTINUE, RESOURCE_LEDGER and STORAGE_STREAMING figures as needed.

**Local setup** (all commands from `firmware/chompi-forge/`)
- Get the code: `git clone -b forge/foundation https://github.com/DCDominguez/CHOMPI.git`
  (or `git fetch origin && git checkout forge/foundation && git pull`).
- Native and Python tests: a C++14 compiler, `make`, Python 3 with
  `pip install -r host/requirements.txt` (bridge: also `host/bridge-requirements.txt`).
  `make test`; sanitizers: `ASAN_OPTIONS=detect_leaks=0 make sanitize`.
- Firmware: Arm GCC 10.3 (`arm-none-eabi-gcc`) on PATH; `make firmware`,
  `make firmware-dev`, `make bench` (needs the same toolchain). The cloud session's
  xPack 10.3.1 copy does not move with the session.
- Browser tests: `pip install playwright && python3 -m playwright install chromium`,
  then `make browser-test`.
- On Windows: use WSL (or Git Bash with make) for the Makefile; the hardware session
  itself only needs the Forge Bridge exe from the Actions artifact.
- A local session can reach CHOMPI over USB (bridge, `host/forge_host.py`); installing
  firmware stays DC's action.

**First prompt for the local session:** "Read AGENTS.md, docs/forge/CONTINUE.md and
the 'Handoff: cloud session → DC's local PC' section of docs/forge/HANDOFF.md, then
start on item 1."

---

Latest: 0.15.1 fixes every verified finding of DC's full project review (recorder
stuck notes, MIDI stop/start, full loop files, project hand-over, KEY_22 side effect,
presets.json retries, host routes / validation / CLI / AI schema, install-power and
test-session docs). Before it: 0.15 event recorder and projects, 0.14 clock / arp /
bass, 0.13 harmony, 0.12 per-slot sample settings, 0.11 install power check, 0.10
TAPE parity, 0.7–0.9 USB loader / power / key lights. Software-tested only; DC has
not installed anything after the review (see CONTINUE). Paragraphs below are older
milestones kept as history.

Latest: knob pages and patch knobs (KNOBS.md, firmware 0.6) — press a knob to
step its page (patch's knob, then filter / envelope / LFO / space controls),
its light shows the page; v5 patches (and the AI) choose what the four knobs
do on page 1. Software-tested only. See CONTINUE.

Before that: the looper (roadmap item 5, LOOPING.md) — TAPE-style on KEY_27/28,
~83 s, save to a sample slot; software-tested only. See CONTINUE.

Before that: boot fix ("64 MHz bug": boot_info now in backup SRAM; release and
development firmware) and cleanup; kits rebuilt. See CONTINUE.

Before that: plug-and-play bridge. Connect CHOMPI finds the ports itself.
Automatic checks find the audio interface and measure CHOMPI's output. The
Windows development kit includes Python and all packages (double-click
`Start Forge bridge.cmd`). See CONTINUE's current checkpoint and BRIDGE.md.
Software-tested only: 8 native suites, 87 Python and 15 Chromium tests.

Before that: [Forge Inspector](INSPECTOR.md), development-only shared telemetry
and the [browser hardware test bridge](BRIDGE.md), with guided checks, controls,
session exports and a Windows launcher. Latest host validation: 73 Python and
14 real Chromium tests pass. Firmware is unchanged by the bridge increment.
See CONTINUE's prior checkpoint for the 8 native
passes, 61/62 Windows Python result (baseline-reproduced HTTP-body failure), ARM
builds, release byte-identity and pending physical tests. No hardware was tested.

- Development branch `forge/foundation`, draft PR #1. No merge, no flash.
- Scope: AI-programmable playable instrument (synth with up to four voices,
  two oscillators, noise, resonant filter, LFO, glide; TAPE-compatible sampler
  with recording, up to seven voices; stereo delay and reverb; looper; knob
  pages; keybed/MIDI; v5 module patches with v1–v4 still supported; AI webapp).
  Never narrow to effects-only.
- Plan: develop roadmap features first (PROJECT.md order), QA each later.
- Firmware changes since the integration checkpoint: sound fixes (`0a605f6`),
  sustain pedal + pitch bend (roadmap item 1, `0f4290e`), v3 instrument
  palette (roadmap item 2, firmware 0.4: `91c11c2`, `79c5d9f`), device presets
  on the SD card with a TAPE-style key + encoder menu (roadmap item 3), MIDI
  CCs/knob order matching stock, and the sampler (roadmap item 4, firmware 0.5,
  `c7ffc78`…; design in [SAMPLING.md](SAMPLING.md)).
- Software-tested: native, Python, sanitizer, real-Chromium browser and ARM build
  (xPack GCC 10.3.1; the pinned Arm archive was unreachable).
- Stock comparison and CPU benchmark: [COMPATIBILITY.md](COMPATIBILITY.md)
  (`make bench`).
- QA bundle: `Forge-0.5-test-b7a504a` (with the sampler); checksums in CONTINUE.md;
  older ZIPs are stale. DC keeps bundles on his Google Drive.
- Not verified: anything on hardware; any live OpenAI/Gemini request; device CPU.
- Current state and remaining work: CONTINUE.md "Checkpoint summary".
- Next for the agent: roadmap item 5 (looping), design first from TAPE's
  looper; open DC decisions are listed in CONTINUE.md. QA for DC later:
  [LIVE_AI_TEST.md](LIVE_AI_TEST.md), then [TEST_SESSION.md](TEST_SESSION.md);
  record results in TEST_RESULTS.md only after running them.
