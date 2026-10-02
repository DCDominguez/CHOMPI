# Forge handoff — instrument candidate 0.3

Updated 2026-10-02. **Read [CONTINUE.md](CONTINUE.md) first** for the live agent
checkpoint. Development branch: `forge/foundation`, draft PR #1. No merge or
hardware flash has occurred. AGENTS.md establishes ongoing documentation rules.

## Goal corrected

DC wants an AI-programmable instrument, not an effects-only device. This milestone
adds playable synthesis to the previous delay/control/web foundation. Sampling,
looping and general routing are still later work; never describe them as present.
Physical acceptance remains one consolidated session per DC's instruction.

## Implemented

Four-voice synth (four waveforms, ADSR, velocity, one-pole tone), keybed/MIDI notes,
source ownership, bounded voice stealing, panic and note-overflow recovery.
v2 named synth/delay/output modules support synth or aux into delay into output.
v1 delay files still work. Six presets and simulated aux/synth WAV rendering.

Webapp supports instrument/effect generation, OpenAI/Gemini with the user's key,
editable v2 controls/routing, v1 import/export, explicit MIDI send/capture/status
and panic. AI never sends automatically. Credentials remain ephemeral; no live
provider call made. Ollama CLI continues to author v1 delay patches only.

## Evidence

- Three native C++ suites pass (DSP/queue, protocol/framing and synth).
- 28 Python tests pass, including 250 v1 and 200 v2 seeded round trips through
  the actual C++ runtime, malformed-patch atomicity and provider mocks.
- All three C++ suites pass ASan/UBSan; LeakSanitizer disabled due to environment.
- ARM GNU 10.3-2021.10 build passes: binary 113,408 bytes, SHA-256
  `02ecbbb6eed11a20bcfe6774376d95cf8b6188482ab3d15ed17a1c4a22228bff`.
- Link usage: SRAM_EXEC 47.74%, SRAM 17.01%, RAM_D2 68.07%, SDRAM 0.57%.
- JS syntax and HTTP/session serving checked. Browser smoke test remains pending;
  prior Chromium downloads returned truncated archives.

No physical audio, keybed, MIDI, CPU/stack headroom, battery or restore test.
No actual model latency/musical quality/provider compatibility test. Synth voice
stealing may click; triangle is not band-limited; high-note aliasing is possible.
No unverified feature is a hardware pass. Startup remains dry aux until a v2
instrument patch is sent. Route/waveform changes stop notes/tails; retrigger keys.

## Next actions

Follow the prioritized continuation list in CONTINUE.md and consolidated
TEST_SESSION.md. Record actual results in TEST_RESULTS.md only after running
them. Use source branch or regenerate a 0.3 bundle; previous 0.2 ZIPs are stale.
Generated binaries/bundles are ignored; their manifest must match their own
source and file hashes. Keep builds and test evidence current after changes.
