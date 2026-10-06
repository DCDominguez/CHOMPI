# Forge handoff — instrument candidate 0.15.1

Updated 2026-10-06 (UTC). **Read [CONTINUE.md](CONTINUE.md) first**; it is the
live checkpoint with claim levels, what changed and prioritized next actions.

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
