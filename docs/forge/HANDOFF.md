# Forge handoff — instrument candidate 0.3

Updated 2026-10-02 (UTC). **Read [CONTINUE.md](CONTINUE.md) first**; it is the
live checkpoint with claim levels, what changed and prioritized next actions.

- Development branch `forge/foundation`, draft PR #1. No merge, no flash.
- Scope: AI-programmable playable instrument (four-voice synth + stereo delay,
  keybed/MIDI, v2 module patches, AI webapp). Never narrow to effects-only.
- Latest firmware change: sound fixes at commit `0a605f6` (click-free voice
  reuse, polyBLAMP triangle, epoch-based stuck-note recovery).
- Software-tested: native, Python, sanitizer, real-Chromium browser and ARM build
  (xPack GCC 10.3.1; the pinned Arm archive was unreachable).
- Current test bundle: built from `0a605f6`, FORGE.bin sha256
  `ff9f3386b9ec418b16718b491f01e6e7913f62e7f0bd9604fa3ea88d414cc299`,
  `built_with_pinned_compiler: false`. Every earlier 0.2/0.3 ZIP and hash is stale.
- Not verified: anything on hardware; any live OpenAI/Gemini request; device CPU.
- Next for DC: [LIVE_AI_TEST.md](LIVE_AI_TEST.md), then the single
  [TEST_SESSION.md](TEST_SESSION.md). Record results in TEST_RESULTS.md only
  after running them.
