# Forge looping (roadmap item 5): design

Status: design agreed with DC on 2026-10-03 (section 3). **Implemented and
software-tested** (steps 1–4 of section 5); nothing is hardware-verified.
TEST_SESSION 3E (3.42–3.51) and 6.2d cover it.

Implementation notes:
- Code: `core/looper.h` (engine, gestures, CC buttons), `core/engine.h`
  (signal chain, panic, CC 24/26/27, voice cap), `core/panel_controller.h`
  (keys, SW5 context, menu, LEDs), `core/preset_menu.h` (loop as copy
  source), `core/sample_loader.h` (save from the loop), `src/forge_main.cpp`.
- Saving the loop: Samples page, COPY (KEY_24), **LOOP** as the source (TAPE's
  "slot 16"), a white key, CHOMPI. The loop is locked while the WAV is
  written (no overdub, new take or clear); it keeps playing. Panel only (no
  host command yet).
- **CPU budget:** while the looper records or overdubs, sampler voices are
  capped at 6 (`kLooperVoiceCap`; a voice above the cap is released, not cut).
  `make bench`: 7 sampler voices + overdub at 1.37× = 2,555 instr/sample
  (≤ WAVE 2,695; 2,816 without the cap). Sampler worst case without the
  looper 2,671 (2,628 before). Raise the cap to 7 if TEST_SESSION 6.2d shows
  headroom.
- Code space +9.5 KB (release 223,444 B, 77.4 %; development 239,632 B); SDRAM 99.4 % (~390 KB spare).
- Panic now also pauses the loop (kept); patch changes no longer call it (they
  use the engine's internal Silence(), as before minus the looper).
- Simulator: 20 s looper; automatic-check looper steps that need real time
  are skipped there. Source analysis: TAPE 2.0 `LooperEngine.h`, `Sampler.h`
(`FileSampler`), `RamBuffer.h`, `NormalPage.h`, `MenuPage.h`, `ui.h`,
`DSPEngine.h` (read only); community notes and forks listed at the end.

## 1. What TAPE does (the behaviour to keep)

- **Two keys.** KEY_28 = LOOP (record / overdub), KEY_27 = PLAY.
  - Empty looper: LOOP starts the first recording; LOOP again ends it and the
    loop plays, *going straight into overdub* (TAPE habit); PLAY instead ends
    the first take and just plays.
  - Playing: LOOP toggles overdub; PLAY pauses / resumes.
  - Hold PLAY ≥ 2 s while paused: jump to the start.
  - Hold PLAY + LOOP ≥ 2 s: clear the looper (fades out first).
  - Hold PLAY + LOOP while empty, then play a key: "record armed", recording
    starts with the first note.
  - The first take ends itself when the buffer is full.
- **Tape behaviour.** Varispeed (−2…+2×, negative = reverse) with linear
  interpolation and an optional slow "tape slew"; scrubbing by turning the
  transport encoder while paused; ~5 ms fades at the loop seam and on
  reverse; overdub feedback ("dub gain", 0–100 %, default 100 %) with a hard
  limiter on the written signal so stacking overdubs cannot clip.
- **Transport encoder (SW5).** While playing: looper pitch (optionally
  quantised); while paused: scrub; press: pitch back to 1×.
- **Signal chain.** Everything the instrument outputs (voices, monitoring) goes
  into the looper; the effects (delay/reverb) sit before or after the looper
  (menu KEY_21 / KEY_22). The loop plays back mixed with the live sound.
- **Menu.** PLAY / LOOP keys: dub gain −/+ 10 %. "Slot 16" lets the loop be
  saved to a sample slot, or a sample copied into the looper.
- **MIDI.** CC 26 = PLAY, CC 27 = LOOP (edge-triggered with a dead zone:
  ≥ 85 press, ≤ 41 release); CC 24 = transport encoder. TAPE also ignores MIDI
  for ~1.5 s after boot (Forge does not need to).
- **LEDs.** PLAY: off empty, teal while recording/playing (brightness follows
  the position), white when paused, white when armed. LOOP: red first take,
  yellow overdub (position), red blink armed.
- **Memory.** 31.7 MB loop buffer (~165 s stereo, 16-bit) — TAPE streams
  samples from SD, so it has the room.

## 2. Forge plan

**Engine (`core/looper.h`, audio owner, no allocation):** TAPE's state machine
and tape mechanics, rewritten cleanly and tested natively:
- 16-bit stereo buffer in SDRAM (as TAPE), linear interpolation (as TAPE),
  writes at the read head while overdubbing (`old × feedback + input`, hard
  limit), seam/reverse/clear fades, auto-close when full.
- Varispeed −2…+2 with optional tape slew; scrub while paused.
- Bounded per-sample work: one read, at most one write per output sample
  (≤ 2 at 2× speed); no per-sample divisions.
- **Panic** pauses the looper and silences it at once, keeps the loop.
  Clear only by the TAPE gesture (or host command).

**Panel.** KEY_27 PLAY / KEY_28 LOOP with TAPE's gestures and LED colours.
Menu (Presets page, toggle up + CHOMPI): PLAY/LOOP = dub gain −/+ 10 %;
KEY_21 = effects before the loop, KEY_20 = effects after (TAPE uses 21/22, but
Forge's KEY_22 already switches menu pages).

**Signal chain.** voices → filter → [delay, reverb] → **looper** → level
(default "effects before"; the loop records what you hear). "Effects after":
voices → **looper** → delay/reverb → level.

**MIDI.** CC 26 / 27 as TAPE (edge + dead zone). CC 24 follows SW5 (see D2).

**Not in patches.** Like TAPE, the loop and its transport are performance
state, not part of a patch: no wire/patch version change. Dub gain and
effects position are panel/menu settings (kept until power-off).

**Development/test hooks.** Inspector page 5 gains looper fields (state,
length, position, speed, dub gain). Automatic checks: record a line-in tone
loop, check it repeats at the loop period without seam clicks, overdub,
pause/resume, clear, panic. TEST_SESSION steps 3.42–3.5x. `make bench` adds a
"7 sampler voices + looper overdubbing" scenario.

## 3. Decisions (DC, 2026-10-03)

- **D1 Loop length:** the sample pool shrinks 40 → 32 MiB; the loop gets
  4,000,000 stereo 16-bit frames (16 MB, **~83 s**), leaving ~390 KB of SDRAM
  spare. A factory kit bank (~100 s) still fits the pool (~174 s stereo).
- **D2 SW5:** context. While a loop exists, SW5 is the looper transport (pitch
  while playing, scrub while paused, press = 1×) and CC 24 follows it;
  otherwise cutoff / panic as now. Panic stays on CC 120/123 and the webapp.
- **D3 Scope:** TAPE parity + save the loop into a sample slot. Decay Memory's
  AGE and a 1× detent come later.
- **D4 First take:** as TAPE: LOOP ends the first take and goes straight into
  overdub; PLAY ends it with plain playback.

## 4. Risks

- **Improvement over TAPE:** the first take gets 5 ms fades written into the
  loop itself, so the seam needs no playback dip.
- **CPU.** The emulated sampler worst case is 2,628 of WAVE's 2,695
  instructions/sample; a looper adds roughly 60–120. With all seven sampler
  voices *and* overdubbing it may pass WAVE's figure in the emulator; the
  hardware number (TEST_SESSION 6.2c, tomorrow) decides whether a combined
  limit is needed (e.g. 6 sampler voices while overdubbing). Built so that an
  empty or paused looper costs ~nothing.
- **SDRAM bandwidth** with 7 voices + loop read + write: unknown until hardware.
- **Panel keys 27/28** are not yet verified on hardware in Forge (TAPE uses
  them, so the wiring is known).

## 5. Build order (each step tested before the next)

1. `core/looper.h` + native tests + sanitizers.
2. Panel/menu gestures, LEDs, CC 26/27/24, panic; preset_test/panel_test.
3. Firmware wiring (SDRAM buffer, signal chain), ARM build, layout check,
   bench scenario.
4. Inspector fields, simulator, automatic checks, TEST_SESSION, docs, kits.

Community sources: lnetzel's TAPE MIDI notes (AI-written; confirmed against
`ui.h` for CC 26/27); xNeoclox Decay Memory (AGE, varispeed detent ideas).
