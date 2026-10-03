# Forge handoff — instrument candidate 0.4

Updated 2026-10-03 (UTC). **Read [CONTINUE.md](CONTINUE.md) first**; it is the
live checkpoint with claim levels, what changed and prioritized next actions.

- Development branch `forge/foundation`, draft PR #1. No merge, no flash.
- Scope: AI-programmable playable instrument (synth with up to four voices,
  two oscillators, noise, resonant filter, LFO, glide; stereo delay and reverb;
  keybed/MIDI; v3 module patches with v1/v2 still supported; AI webapp).
  Never narrow to effects-only.
- Plan: develop roadmap features first (PROJECT.md order), QA each later.
- Firmware changes since the integration checkpoint: sound fixes (`0a605f6`),
  sustain pedal + pitch bend (roadmap item 1, `0f4290e`), v3 instrument
  palette (roadmap item 2, firmware 0.4: `91c11c2`, `79c5d9f`).
- Software-tested: native, Python, sanitizer, real-Chromium browser and ARM build
  (xPack GCC 10.3.1; the pinned Arm archive was unreachable).
- Bundles: see CONTINUE.md "Next actions"; any ZIP older than the current
  firmware commit (including the `0a605f6` one) is stale.
- Not verified: anything on hardware; any live OpenAI/Gemini request; device CPU.
- Next for the agent: roadmap item 3 (SD preset banks), design first. QA for DC later:
  [LIVE_AI_TEST.md](LIVE_AI_TEST.md), then [TEST_SESSION.md](TEST_SESSION.md);
  record results in TEST_RESULTS.md only after running them.
