# Forge changelog

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
