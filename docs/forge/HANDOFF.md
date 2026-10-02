# Forge handoff — instrument candidate 0.3

Updated 2026-10-02 (UTC). **Read [CONTINUE.md](CONTINUE.md) first**; it is the
live checkpoint with claim levels, what changed and prioritized next actions.

- Development branch `forge/foundation`, draft PR #1. No merge, no flash.
- Scope: AI-programmable playable instrument (four-voice synth + stereo delay,
  keybed/MIDI, v2 module patches, AI webapp). Never narrow to effects-only.
- Plan: develop roadmap features first (PROJECT.md order), QA each later.
- Firmware changes since the integration checkpoint: sound fixes (`0a605f6`:
  click-free voice reuse, polyBLAMP triangle, epoch-based recovery), then
  sustain pedal + pitch bend (roadmap item 1).
- Software-tested: native, Python, sanitizer, real-Chromium browser and ARM build
  (xPack GCC 10.3.1; the pinned Arm archive was unreachable).
- Last bundle: from `0a605f6` (sha256 `ff9f3386…c299`, xPack). It predates
  sustain/bend, so regenerate before QA. Every earlier 0.2/0.3 ZIP is stale.
- Not verified: anything on hardware; any live OpenAI/Gemini request; device CPU.
- Next for the agent: roadmap item 2 (v3 synth palette). QA for DC later:
  [LIVE_AI_TEST.md](LIVE_AI_TEST.md), then [TEST_SESSION.md](TEST_SESSION.md);
  record results in TEST_RESULTS.md only after running them.
