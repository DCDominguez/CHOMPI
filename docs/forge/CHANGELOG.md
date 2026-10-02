# Forge changelog

## 0.3 playability: sustain pedal and pitch bend — 2026-10-02

- CC64 sustain and 14-bit pitch bend (±2 semitones, 5 ms smoothing), both per
  MIDI source; CC121 resets them; panic/route change clears them. Steal order
  adds pedal-sustained voices before held ones.
- Channel-message decoding moved from forge_main into host-tested
  `TranslateChannel`; lost pedal/CC121 raises the stuck-note emergency, a lost
  bend only counts a drop.
- `forge_host.py note --bend/--sustain` for the hardware session (always reset).
- Tests: sustain, bend pitch/isolation/smoothing, translation and gate cases;
  6 code mutations each caught. Hardware still unverified.

## 0.3 sound fixes — 2026-10-02

- Voice steal/retrigger keeps level, phase and (slewed) velocity gain; steals a
  releasing voice before a held one. Simulated steal step 4.98x → 1.12x.
- Triangle corners band-limited with polyBLAMP: alias below 12 kHz at C7
  -46.9 → -78.9 dB (simulated).
- Recovery uses emergency epochs: only notes queued before an emergency are
  dropped; later notes play even under continuous traffic.
- New native tests for all three (each fails on the previous code). Firmware
  binary changed: older 0.3 bundles are stale. Hardware still unverified.

## 0.3 integration review and browser testing — 2026-10-03

- First real browser run (Chromium 141): 8 end-to-end tests against a stateful
  simulated device using the C++ runtime; mocked providers; phone/tablet layout.
- Fixed: localhost URL refused; field/device errors hidden behind generic text;
  status banner out of view; linear cutoff slider; 14-bit wrap on float rounding.
- Stuck-note recovery factored into host-tested `RecoveryGate`; firmware logic
  unchanged. PROTOCOL recovery description corrected.
- Added `cc`/`note` test commands, `forge_ai_check.py` live preflight, provider
  range descriptions, bundle `verify_bundle.py`, compiler identity in manifest.
- Rewrote the consolidated hardware checklist with exact commands.
- 35 Python, 3 native, 3 sanitizer, 8 browser tests and ARM build pass.
  Hardware and live providers still unverified.

## 0.3 instrument software candidate — 2026-10-02

- Corrected project scope to AI-programmable instrument, not effects-only.
- Added four-voice synth, ADSR/tone/velocity, keybed and MIDI notes, source ownership
  and panic; retained external stereo delay and v1 patch compatibility.
- Added v2 synth/delay/output modules with two routes, host encoding/capture,
  OpenAI/Gemini instrument authoring, web controls and three instrument presets.
- Added synth native/sanitizer coverage and 200 v2 integration round trips.
  Three C++ suites, 28 Python tests, sanitizers and ARM build pass.
- Added AGENTS.md and live CONTINUE.md for ongoing agent handoffs; updated docs,
  wire protocol, package generator and single physical acceptance checklist.
- Browser, live-provider and actual hardware acceptance remain pending.

## Local AI webapp — 2026-10-02

- Added a local browser interface with OpenAI and Gemini using user-provided API
  keys and configurable model IDs; the Ollama CLI remains available.
- Added validated patch generation, parameter editing, preset import/export,
  MIDI port selection, status, capture and explicit acknowledged send.
- Kept credentials out of disk/browser storage; bounded local requests, checked
  origin/session, disabled cross-origin access and refused provider redirects.
- Added twelve provider/server tests; all 24 Python tests and both C++ suites pass.
- Updated packaging to include the webapp. Existing ZIPs are unchanged.
- Firmware remains 0.2; live provider and physical hardware tests are pending.
- Browser smoke testing is pending: the development Chromium download failed;
  static asset serving and JavaScript syntax checks pass.

## Documentation follow-up — 2026-10-02

- Added a documentation index, architecture and developer guides.
- Added prominent navigation and current Forge status to the repository README.
- Clarified tracked source versus generated test artifacts and verification limits.
- Expanded the root README with the project/build summary, full candidate feature
  inventory, controls/defaults, usage commands and explicitly unimplemented scope.
- No firmware behavior or candidate binary changed.

## 0.2 software candidate — 2026-10-02

- Added versioned host JSON patches, strict validation, save/capture and atomic recall.
- Added acknowledged USB/TRS SysEx control and status with sequence/checksum checks.
- Added framing tolerant of interleaved MIDI real-time bytes, complete USB SysEx
  packetization, response backpressure and transport-lifetime handling.
- Added average/peak callback load and drop/rejection reporting.
- Added Python control tools, optional Ollama authoring, presets and offline rendering.
- Added protocol/integration tests, a bundle generator and one hardware-test checklist.
- Software tests and ARM build pass; device and actual-model acceptance are pending.

## Initial foundation — 2026-10-01

- Added a standalone Forge BOOT_SRAM application using WAVE's hardware support.
- Implemented stereo delay, smoothed parameters, encoder/MIDI CC controls and host tests.
- Established the project brief and repository handoff.
- Updated the plan to batch physical validation into one consolidated session.

These are development milestones, not hardware-approved releases. The initial
foundation did not publish a formal semantic version or GitHub release.
