# Forge sampling (roadmap item 4): design

Status: design agreed in scope with DC on 2026-10-03 ("follow TAPE's sampling,
which works; add cool things that don't overload"). **Implemented and
software-tested in firmware 0.5** (sampler steps 1–7, see CHANGELOG). Nothing
here is hardware-verified; TEST_SESSION 3D and 6.2c cover it.

Implementation notes (where it differs from or refines the plan below):
- Interpolation: 4-point Hermite only when pitched down, linear at or above
  the original speed (TAPE uses linear everywhere). Chosen after the CPU
  benchmark; see COMPATIBILITY.md §6.
- Code: `core/wav.h`, `core/sample_table.h`, `core/recorder.h`,
  `core/sample_loader.h`, `core/sampler_runtime.h`, sample voices in
  `core/synth.h`, the Samples page in `core/preset_menu.h`,
  `src/fatfs_storage.h` (FatFsSampleFiles), wiring in `src/forge_main.cpp`.
- A note in kit mode maps white keys only (MIDI 48–72); TAPE's MIDI input
  maps a wider range of notes onto the keys. Candidate for later.
- CPU (emulated, 7 voices): 2,104–2,236 instructions/sample; worst case
  2,628 vs WAVE 2,695. Code space 89.7 % used (~24 KB left).

Source analysis: TAPE 2.0 (`firmware/chompi-tape/code/src`, read only):
`DSPEngine.h`, `SampleReader.h`, `FileStreamingManager.*`, `FileCopier.h`,
`RamBuffer.h`, `NormalPage.h`, `MenuPage.h`, `ui.h`, and the factory card in
`firmware/card-profiles/tape-2.0`.

## 1. What TAPE does (the behaviour to keep)

- **Slots.** Two modes, JAMMI (one sample played chromatically on all keys)
  and CUBBI (kit: each white key plays its own sample). 5 banks (a–e) × 14
  slots per mode, plus slot 15 = the RAM recording ("CHOMPI" slot).
- **Files.** SD root, `jammi_a1.wav` … `cubbi_e14.wav` (`<mode>_<bank><slot>`),
  16-bit stereo 48 kHz PCM with a 44-byte header, plus a `_double.wav`
  (every second frame) used above 1.5× speed. TAPE regenerates missing
  `_double` files at boot.
- **Keys.** JAMMI: KEY_8 (MIDI 60) plays at unity, ±12 semitones across the
  keybed. CUBBI: white keys KEY_1…KEY_15 → slots 1…15; black keys silent.
- **Sound controls.** Pitch (continuous; negative = reverse), start, end,
  attack, release, loop on/off ("autoloop"), sustain (gate) or trigger
  (one-shot), gain, pan. Up to 7 voices.
- **Recording.** Toggle down + hold the CHOMPI key records into RAM (165 s max)
  from mic, line in (auto-selected by jack detect) or resample (the whole
  output). On release the recording becomes JAMMI slot 15, immediately
  playable. Save to an SD slot from the menu (KEY_25 + slot + CHOMPI).
- **Menu (toggle up + CHOMPI).** KEY_16 / KEY_17 select JAMMI / CUBBI and
  cycle its bank; white keys select a slot; KEY_18/19/20 pick mic / line /
  resample; KEY_23 erase, KEY_24 copy, KEY_25 save, each confirmed with CHOMPI.
- **Normal-page knobs.** Knob 1 pitch, knob 2 start, knob 3 end, knob 4
  effects (TAPE page 0; logical stock knob order, see COMPATIBILITY.md §4).

## 2. What Forge changes, and why

| TAPE | Forge | Reason |
| --- | --- | --- |
| Streams every voice from SD (FatFS inside a 1 kHz timer ISR, shared non-atomic FIFOs) | Loads the selected sample (JAMMI) or bank (CUBBI) into SDRAM, in chunks, from the main loop | Removes the ISR races TAPE's own comments hint at; no SD bandwidth limit per voice; no `_double` files needed |
| 16-bit stereo 48 kHz only; other formats play as noise | Reads 16/24-bit PCM and 32-bit float, mono or stereo, any rate 8–96 kHz (pitch-corrected); mono stays mono in RAM | Common WAVs just work; mono halves memory |
| Linear interpolation | 4-point Hermite when pitched down, linear otherwise | Cleaner pitched-down sound where it is audible, at TAPE's cost elsewhere |
| Loop wrap = fade out/in dip | Real crossfade (length is a patch field) | Smooth pads/loops |
| Minimum attack ≈ 0.2 s | Forge ADSR (1 ms minimum) | Drums and plucks |
| Samples bypass any filter | Samples go through Forge's per-voice resonant filter + filter envelope, LFO, glide, delay and reverb | Makes the sampler an instrument, the "cool" extra that costs little |
| Recording: no fades, no normalisation | 5 ms fade-in/out; peak-normalised on stop (non-destructive gain, applied when saving) | No clicks; consistent levels |

Kept compatible: the same file names, so a TAPE card's samples play in Forge
and samples Forge saves play in TAPE (Forge writes TAPE's exact format and
lets TAPE generate `_double` at its next boot, as TAPE already does for
missing ones). Forge never writes `.bin` files or touches `presets.json`.

## 3. Memory

SDRAM (64 MB, `DSY_SDRAM_BSS`, not zeroed at boot; reads are bounded by
"loaded" counters, so nothing uninitialised is ever played):

| Region | Size | Holds |
| --- | --- | --- |
| Delay lines (existing) | 384 KB | — |
| Sample pool | 40 MB | The JAMMI sample or the CUBBI bank: ~218 s stereo / ~436 s mono |
| Record buffer (slot 15) | 16 MB | ~87 s stereo at 48 kHz |
| Free | ~7.6 MB | Reserved for looping (roadmap item 5) |

A factory TAPE bank is ≈ 100 s of audio, so a whole CUBBI bank fits. A file
that does not fit is loaded up to the pool's end and reported as partial.

## 4. Patch v4 (wire and JSON)

v4 = all v3 fields + a sampler block. Defaults keep v3 behaviour.

| Field | Wire | Meaning |
| --- | --- | --- |
| `source` | byte 0/1 | 0 = oscillators (v3 synth), 1 = sampler |
| `sample_mode` | byte 0/1 | 0 = chromatic (JAMMI), 1 = kit (CUBBI) |
| `sample_bank` | byte 0–4 | Bank a–e |
| `sample_slot` | byte 0–14 | Chromatic slot 1–14, or 15 = RAM recording (kit mode ignores it) |
| `sample_pitch` | 14-bit | ±24 semitones continuous, 0.5 = unity |
| `sample_start`, `sample_end` | 14-bit | Fractions of the sample; end > start (min 1024 frames) |
| `sample_loop` | byte 0/1 | Loop start↔end (else one-shot) |
| `sample_gate` | byte 0/1 | 1 = hold while key down (TAPE "sustain"); 0 = trigger (TAPE: gate drops when the attack ends) |
| `sample_reverse` | byte 0/1 | Play end → start |
| `sample_xfade` | 14-bit | Loop crossfade 0–250 ms |

`voices` becomes 1–7 on v4 patches (TAPE's 7); v1–v3 stay as they were.
Request 84 bytes, status reply 96 bytes; buffers derive from the constants.

## 5. Ownership and data flow

- **Audio owner**: voices, recording into the record buffer, the
  "sampler wants (mode, bank, slot)" word it publishes when the patch changes.
- **Main loop**: SD only. Scans the root once per mount (`f_readdir`, file
  names → occupancy bits), loads samples in 16 KB chunks (one per loop pass,
  so MIDI/SysEx stay responsive), saves/copies/erases WAV files with
  temp-file + rename.
- **Pool handoff** (no locks): to reload, main asks the audio owner to detach;
  audio fades out pool voices (2 ms) and acknowledges; main then rewrites the
  slot table and streams data, publishing each slot's `loaded` frame count
  with release ordering. Voices read only below `loaded` (acquire), so a note
  can start while the rest of the file is still loading.
- **Saving the recording**: a request to the audio owner snapshots the
  record buffer's length and gain and locks it against new recordings until
  main finishes writing the WAV; then it unlocks.

## 6. Panel

Normal play (toggle down):
- Keys play the sampler when the patch's source is the sampler (chromatic
  pitch from MIDI note − 60; kit: white keys → slots 1–15, black keys silent).
- **Hold CHOMPI = record** (TAPE gesture); release stops. The recording
  becomes chromatic slot 15 at once (the live patch switches to the sampler,
  as TAPE does). CHOMPI LED red while recording.
- **Knobs follow the source** (TAPE's page 0 when sampling): knob 1 pitch,
  knob 2 start, knob 3 end, knob 4 delay mix; SW5 cutoff (press = panic); SW6
  level. With the oscillator source, knobs stay mix / time / feedback / level.
  CC20–25 follow the knobs, as on stock.

Menu (toggle up + CHOMPI, existing): **KEY_22 switches page**,
Presets ↔ Samples. The presets page is unchanged. Samples page (TAPE's
shift menu):
- KEY_16 chromatic (JAMMI) / KEY_17 kit (CUBBI): select the mode, press again
  to cycle bank a–e (bank colour on the key).
- White key: chromatic: select and load that slot (plays from then on);
  kit: choose a slot for save/copy/erase.
- KEY_18 mic, KEY_19 line in, KEY_20 resample (lit = current source; jack
  insertion selects line in, removal mic, as TAPE).
- KEY_25 save the recording, KEY_24 copy (source, then destination), KEY_23
  erase; pick a white key, confirm with CHOMPI (same gestures as presets).
- Occupied slots lit, current slot white, missing card red.

## 7. Host, AI and webapp

- Opcode 08 lists samples: reply 0x44 with 2 × 5 bank bitmaps, whether a
  recording exists and its length.
- `forge_host.py samples`; v4 schema; `upgrade` v3 → v4.
- AI instrument mode gets the sampler module and the device's sample list in
  its prompt. It may only select existing slots and set the documented
  fields; it cannot create audio.
- Webapp: sampler controls and a slot picker.

## 8. Verification plan

Native tests: WAV parsing/conversion (formats, odd chunks, truncation),
Hermite/loop/crossfade/reverse/one-shot/gate behaviour, kit/chromatic key
mapping, recorder fades/normalisation/limits, pool handoff (no read beyond
`loaded`, detach before rewrite), loader against an in-memory card, save
round trip (byte-exact TAPE header), menu samples page. Sanitizers. v1–v3
bit-exact check. `make bench` with 7 sampler voices (gate: ≤ WAVE). ARM build
and memory map. Python/browser tests for the host side. Hardware steps go to
TEST_SESSION (one consolidated session).

## 9. Not in this item

Looping (item 5), per-slot settings like TAPE's `presets.json` (candidate),
threshold-armed recording, sample names, time-stretch, slicing.
