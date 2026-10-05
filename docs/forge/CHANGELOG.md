# Forge changelog

## 0.13 Harmony — 2026-10-05 (software-tested; not installed yet)

Harmony Phase 1 from DC's brief (HARMONY_BRIEF.md; clean-room, no code from other
projects). DC's choices: a menu toggle stored in presets, the Static map, Real =
the key is the root, output to the internal sound and MIDI out. Nothing here is
hardware-verified.
- **One key plays a chord** (`core/harmony.h`): 9 modes, chord size fifth to 13th,
  Static layout (white keys I–vii twice, black keys secondary dominants, borrowed and
  interchange chords, C5 = Shift) and Real layout (the key is the root), Shift
  transforms, inversions, open spread, voice leading; at most 5 notes and never
  more than the patch's voices; no stuck notes through overlaps, mode changes,
  harmony off or panic (200k random events tested). Synth and chromatic sampler
  patches; kit and v1 delay patches play as before. MANUAL section 8a.
- **Harmony page:** TAPE's menu page, hold KEY_21 1 s (a tap is still effects
  before the looper). Keys set the tonic (lit white, scale blue); SW4 mode / on-off,
  SW1 chord size / Static-Real, SW2 inversion / voice leading, SW3 open spread.
- **Patch v6** = v5 + harmony (PROTOCOL.md): host, webapp *Harmony* group, AI
  schema, conversion of older patches. Status and presets saved on CHOMPI carry the
  live settings. Fixed while testing: the host decoder refused v6 status replies.
- **Inspector page 8:** the harmony state and the last chord, named on the host
  (e.g. *Am7 · I · tonic*); a HARMONY line in the text Inspector.
- **Memory (DC: "implement our memory savings plan", keep the audio buffers):**
  queues in `.bss`, control code `-Os`, start-up construction: release 267,876 →
  215,160 B before the menu/v6 work; 0.13 release **221,712 B**, development
  234,376 B (headroom 67,056 / 54,392 B; 0.12: 27,544 / 11,500 B), including the
  4 KB cubic table.
- **CPU fix A (DC's item 2):** the pitched-down sample read (every voice below 1×:
  SW4 pitch down, kit pads below 1×) is now a 4 KB table of Q14 Hermite weights read
  with the M7's dual 16-bit multiply-accumulate, two taps per instruction; within
  61–89 dB of the exact Hermite (sampler_test). Profiling the emulator also found:
  (1) a glide fix: the slide toward the target stopped on float rounding before it
  arrived (up to ~1.2 cents short per second of glide, never finishing, ~11
  instructions per gliding voice every sample, synth and sampler); glides now end on
  the target (v3_test); (2) restarted-voice declick tails are tracked in a bit mask
  instead of two float tests per voice per sample; (3) smaller per-voice checks.
  Emulator, instructions/sample vs WAVE 2,694.9: pitch .75 **2,669.2** (was 2,800.8),
  kit pads below 1× 2,564.2 (2,755.8), 1× worst case 2,603.5 (2,677.9); both
  pitched-down scenarios are now gated, not informational. The bench now settles
  TAPE's pitch slide before the notes; a new informational row measures it still
  sliding (2,690.5, as while SW4 turns). Hardware 6.2f confirms on CHOMPI.
- New TEST_SESSION 3.65–3.67 (bridge checklist 102 steps).

## 0.12 Per-slot sample settings — 2026-10-05 (software-tested; not installed yet)

TAPE parity A4. DC's choices: save as TAPE does (the moment a control moves), kit
pads keep their own settings, the slot's settings win over a Forge preset's, and
share TAPE's own `presets.json` with a backup. Nothing here is hardware-verified.
- **Per slot (TAPE's nine):** pitch, start, end, attack, release (TAPE: decay),
  auto-loop, sustain, gain, pan, in TAPE's layout and units (`core/slot_settings.h`;
  TAPE's version-1 files read too). The recording keeps a set in memory (TAPE).
- **Chromatic:** turning or pressing those controls saves them to the slot; choosing
  a slot in the menu loads its settings (an unsaved slot: TAPE's defaults);
  recalling a Forge preset on a saved slot loads the slot's, the preset sets the rest;
  a webapp/AI patch keeps its own values (and later turns save them).
- **Kit:** each pad's settings play on its own voices (pitch, gain, pan, window,
  envelope, loop, sustain); the knobs edit the pad last played and their lights show
  it; the shared pitch/gain/pan stay neutral in kit mode.
- **Menu:** saving the recording to a slot copies the recording's settings, copying
  a slot copies its settings, erasing clears them (TAPE).
- **Card:** read at start-up and when a card goes in; written 2 s after the last
  change (TAPE: every 5 s while silent) via `presets_temp.json` and a rename, and
  before an install restart; the card's original goes to `FORGE/presets_backup.json`
  before the first write.
- **CPU (emulator):** gate scenarios unchanged within 7 instructions/sample (worst
  2,677.9 vs WAVE 2,694.9). Found while testing: every voice pitched below 1× (SW4
  pitch down, or kit pads below 1×) costs 2,800.8 / 2,755.8, about 4 % over WAVE;
  true since 0.10's TAPE pitch. Added as informational scenarios and hardware step
  6.2f. DC (2026-10-05): keep the cubic read; accepted if 6.2f stays below 70 % on CHOMPI.
- **Safety extras (DC: "include the extras"):** SW6 blinks yellow twice every 4 s
  while the battery is yellow off a strong charger, fast when a reading is below the
  3.0 V shut-off mark (the stock protection gives no warning on a weak supply);
  Inspector power flags 8 weak supply, 16 low reading, 32 install refused; Check
  setup warns on them. TEST_SESSION 8.5, 8.6.
- Release 261,224 B, development 277,268 B (layout OK). New TEST_SESSION 3.62–3.64, 6.2f, 8.5, 8.6.

## 0.11 Install safety — 2026-10-05 (software-tested; not installed yet)

After DC's 0.10 install left CHOMPI dark (flat battery on a computer's USB port:
the stock bootloader's low-battery wait, not 0.10). Nothing here is
hardware-verified.
- **Install power check:** CHOMPI refuses a firmware install (new error 10,
  before anything on the card is renamed) unless the battery reads green/white with
  no recent low reading, or USB power is on a supply that is neither legacy nor at
  its current limit; checked again at the CHOMPI key press. Every file-transfer
  reply carries flag 16 when an install would be refused, so the bridge and
  `forge_card.py install` stop before uploading and say what to do.
- **Battery lockout log:** just before the stock protection switches CHOMPI off
  (low battery, no USB) or stops it (low battery, weak supply), Forge appends a
  "battery low" line to `FORGE/RESTARTS.TXT`; these leave no reset flag, so the
  earlier "random shut-off" reports had no record.
- **Start-up hardening:** the fault handler's vector table copy is in DTCM
  (uncached; it was in write-back-cached AXI SRAM without cache maintenance); the
  `options.json` read buffer is 32-byte aligned (whole cache lines for the SD DMA).
  Dropped from the plan: stopping restarts after repeated start-up crashes (a
  restart loop still lets the bootloader install another `.bin` from the card).
- Release 254,756 B, development 269,808 B (layout OK). Per-slot sample settings
  (A4) move to 0.12.

## 0.10 TAPE parity — 2026-10-05 (ready to install; per-slot settings moved to 0.12)

Software-tested only. DC chose the recommended answer for every row of the TAPE
parity checklist ([TAPE_CONTROLS.md](TAPE_CONTROLS.md), [KNOBS.md](KNOBS.md)).
- **Knobs as TAPE:** TAPE's pages first (SW4 pitch/gain, SW1 start/attack, SW2
  end/release, SW3 reverb+delay / saturation / DJ filter), Forge's synth controls
  on extra pages, the patch's own knob as a last page; TAPE's step sizes (3 % per
  click on SW1–SW3 and SW6, 1/127 before); page changes on release; hold 1.5 s =
  reset to the preset's value; SW4 + SW3 held 1 s = panic.
- **SW5:** turn = cutoff always; push and turn = loop speed (scrub when paused);
  click = 1×. **SW6:** short press = input gain (75 % at power-on, as TAPE).
- **TAPE's effects ported:** DJ filter, saturation, warble, output compressor
  (`core/tape_fx.h`); pitch with reverse, voice gain, pan as performance state.
- **Recording:** 1.5 s count-in (CHOMPI and the white keys blink red; letting go
  cancels); input monitored in the headphones in the record position; after a take
  pitch/gain/start/end return to default (TAPE); keys play at full velocity.
- **Lights as TAPE:** knob value colours (off in the record position), CHOMPI input
  meter / purple when held, PLAY/LOOP/SW5 dimmed in the record position, SW5 speed
  lights, SW6 page colours; white balance trimmed (green .85, blue .6) for the
  light-blue whites DC saw.
- **Restart record:** reset cause and crash location (backup SRAM + Forge's own
  fault handler, which restarts instead of freezing) in Inspector page 2 (98 bytes),
  Check setup's *Last start* line and `FORGE/RESTARTS.TXT`.
- **Toggle wording fixed everywhere:** down = menu, up = record (firmware unchanged).
- Bridge: starter presets are upgraded to v5 before storing; knob model, walk and
  checks updated; automatic audio checks 3.57a–f and CPU check 6.2e; Check setup
  *Last start*. Tests: `tests/knob_audio_test.cpp` (127 knob pages audible).
- CPU (emulated): every preset scenario cheaper than 0.9 (worst 2,664.9 vs 2,671);
  every TAPE effect on at once 3,052.9, informational, measured on hardware in 6.2e.
- Firmware minor 10. Release 252,420 B (+14,328), development 268,720 B.

Stage 2 so far (same version; software-tested only):
- **TAPE's menu first:** the menu opens on TAPE's page with TAPE's keys (KEY_21 /
  KEY_22 tap = effects before / after the looper); hold KEY_22 1 s for Forge's
  presets page (the menu remembers the last page).
- **TAPE's menu knob layer:** SW4 quantised pitch (fifths/octaves) / pan, SW1+SW2
  move the start-end window (synth: attack and release together), SW3 delay time /
  warble / DJ resonance, SW5 loop speed, SW6 compressor; presses: pitch or gain+pan
  reset, auto-loop and sustain on/off (lights), all effects reset, monitor position.
- **TAPE's monitor positions:** headphones (default), both, send/return.
- **TAPE's options.json** read at start-up (never written): record latch, MIDI in/out
  channel, tape slew, monitor position, which pitch mode is quantised (also the loop
  speed), split delay.
- **Quantised pitch and loop speed** in fifths/octaves (TAPE: the menu by default).
- **MIDI out as TAPE:** keys, knob CCs (TAPE's cc_map, physical turns only), PLAY/LOOP
  CC 26/27, CHOMPI CC 21 in the record position; UART and USB.
- Code size: rarely-run functions (panel, menu, LEDs, start-up, MIDI frames) compiled
  for size (`FORGE_COLD`); release 253,636 B, development 268,728 B.

## Bridge: starter presets — 2026-10-05

No firmware change. *Card & firmware → Load starter presets* writes 12 of the
factory patches into slots 1–12 of a chosen bank through the device's own store
request (send patch, store), keeps occupied slots, and sends the sound that was
playing back afterwards. Test patches (CPU/sampler stress) are left out.

## 0.9 Key lights while playing — 2026-10-05

Software-tested only. DC: "we dont have indicator lights on key presses". As
TAPE's NormalPage, with the menu closed:
- A held note key lights white.
- Sampler kit mode: the keys that hold a sample glow dim in the bank's colour
  (TAPE's bank colours); the recording key (white key 15) glows dim pink when a
  recording exists.
- Sampler chromatic mode: C3, C4 and C5 (white keys 1, 8, 15) glow dim in the
  bank's colour, pink while playing the recording.
- Firmware minor 9. Release 238,092 B (+1,024), development 253,696 B.

## 0.8 Power as stock — 2026-10-05

Software-tested only. After DC's battery ran flat on 0.7, a review against
TAPE/WAVE/TEMPO (COMPATIBILITY §7). CHOMPI's hardware switch S1 always switched it
off; what Forge lacked is listed here.
- Start-up storage gesture as all stock firmwares: hold CHOMPI + PLAY + LOOP
  while CHOMPI starts → charger shipping mode (battery disconnected; USB power
  wakes it). The 0.5 s start-up key scan also clears shift-register junk, as stock.
- TAPE's battery light: hold SW6 (volume) 2 s, its light shows white (charged),
  green, or yellow (below ~3.3 V) while held.
- TAPE's USB/charger hand-over when power is plugged in or changes while running
  (charger interrupt; I2C wait bounded at 50 ms).
- Battery, USB power, charge state and charger faults on Inspector page 2
  (91 bytes; the bridge still reads 0.7's 88) and in Check setup's Power line.
- Bridge: Check setup's line-in hint allows the headphone out (monitor knob on
  playback); TEST_SESSION section 8 (power); bridge checks 86.
- Firmware minor 8. Release 237,068 B (+1,088), development 252,760 B.

## 0.7 USB card and firmware loader — 2026-10-04

Software-tested only; needs one last card flash to get it onto CHOMPI.
- Opcode 0C: write FORGE.bin or TAPE samples to the SD card over USB MIDI,
  staged in FORGE/UPLOAD.TMP and replacing the target only after the CRC-32
  matches; resume after a lost reply; samples are rescanned and reloaded.
- Firmware install: every other root file the bootloader would flash (any
  name containing ".bin") is renamed (TAPE.bin -> TAPE_bin.old), CHOMPI's
  key blinks white and a press (within 15 s) restarts CHOMPI so the bootloader
  flashes the new FORGE.bin. The press is kept from the menu and recorder.
- MIDI SysEx up to 288 bytes. Firmware minor 7.
- Hardening before the first flash (stress review): the bootloader matches
  ".bin" anywhere in a name, so 0.7's first `.bin.old` names would still have
  been flashed; FORGE.bin is now checked like the bootloader checks it and read
  back from the card before it replaces the old one; a repeated End (lost
  reply) succeeds; Install takes over an upload idle for 5 s; the host retries
  a lost Begin/End and aborts a failed upload. Tests: bootloader names,
  firmware guards, a lossy link (drops, duplicates, late requests) and 200,000
  fuzzed requests under ASan/UBSan.
- Status (opcode 02) takes an optional flags byte: 1 starts a new CPU peak.
  The automatic checks reset it at the start of each step that reads status,
  so CPU figures are per step (0.6 firmware: still since boot, and the report
  says so).
- Host: `forge_card.py` (upload / sync / install); bridge *Card & firmware*
  section with the kit's `card` folder and an install button.

## Bridge: setup check, panel walk, re-runs — 2026-10-04

Host only (firmware unchanged). From the first hardware session.
- Automatic checks set their own starting state (knob pages, menu page,
  looper); line-in detection needs the line jack and a clear level; tone steps
  skip without a plug; 3.29 needs the kit file; 3.1k starts from a known panel.
- Setup check (cables, both outputs, gain, hum with its likely source, line in).
- Guided panel walk with automatic confirmation of every physical control and
  light questions (optional camera photos); re-run failed steps; CLI
  `--setup-only` / `--only`; `CLAUDE.md` in the development kit for an agent.

## 0.6 knob pages and patch knobs — 2026-10-04

Design and map: KNOBS.md. Software-tested only.
- Press a knob (SW4, SW1, SW2, SW3) to step its page; its light shows the page
  (dim white, red, green, blue). Page 1 = the patch's knob; pages 2–4 fixed:
  SW4 cutoff / resonance / filter envelope, SW1 attack / decay / release, SW2
  LFO rate / LFO filter depth / osc 2 detune (sampler: loop crossfade), SW3
  delay mix / feedback / reverb mix. Pages are per knob, kept across presets.
- Patch v5 = v4 + `knobs`: what the four knobs (and CC 20–23) control on page 1,
  from 26 controls or `default`. 88-byte request, 100-byte status; v1–v4
  unchanged. Firmware minor 6.
- Every continuous control can now be set by Parameter id (engine/panel);
  controls a patch version cannot carry are refused, as before.
- Host: schema/codec/upgrade to v5, `knob_control`; AI authoring writes v5 and
  picks the knobs; webapp *Panel knobs* group, Convert to v5; preset
  `14-knob-pad.json`; Inspector page 3 carries the knob pages; MIDI framer
  holds 92-byte SysEx. TEST_SESSION 3F (3.52–3.56), automatic checks 3.52,
  3.53, 3.55; bridge checks 78.

## 0.5 looper (roadmap item 5) — 2026-10-03

TAPE's looper on KEY_28 LOOP / KEY_27 PLAY (design: LOOPING.md). Software-tested only.
- First take, overdub with feedback (soft limited), pause/resume, hold PLAY
  2 s = start, hold both 2 s = clear, armed recording; varispeed −2…+2 with
  reverse and tape slew, scrub while paused; seamless 5 ms seam fades; ~83 s
  stereo (sample pool 40 → 32 MiB).
- SW5 is the looper transport while a loop exists (else cutoff/panic); menu:
  PLAY/LOOP set the feedback, KEY_21/KEY_20 put the effects before/after the
  loop; Samples page COPY → LOOP → slot → CHOMPI saves the loop as a sample.
- MIDI CC 26 PLAY / 27 LOOP (as TAPE); CC 24 follows SW5.
- Panic pauses the loop (kept); patch changes leave it playing.
- While recording/overdubbing, sampler voices are capped at 6 (CPU budget).
- Inspector page 5 carries the looper state; TEST_SESSION 3E + 6.2d; bridge
  checks and automatic checks.

## 0.5 boot fix and cleanup — 2026-10-03

- Fixed (release and development firmware): libDaisy's `boot_info` now links
  into backup SRAM (0x38800000), where the bootloader writes its version.
  Before, it sat in uninitialised RAM; a 0 there made the firmware skip clock
  and SDRAM setup on some units ("64 MHz bug", found by sfaber02). A layout
  check now fails the build if it moves; the firmware relinks when the linker
  script changes.
- Development probe page 0 retired (Inspector pages cover it); consistency
  tests for the duplicated checklist, key table and step ids; docs refreshed.

## 0.5 plug-and-play bridge — 2026-10-03

DC: "more plug and play". Host only; firmware unchanged. Software-tested only.
- Connect CHOMPI: automatic port discovery (CHOMPI/Daisy-named ports only,
  confirmed by a Forge reply).
- Automatic checks: finds the audio interface, then runs 19 measurable
  TEST_SESSION steps (pitch, clicks, levels, panic, menu/LEDs, samples,
  recording, CPU) with spectrograms and reports; `forge_audio.py run` for
  terminals. Audio steps skip without an interface or numpy.
- Windows development kit bundles Python 3.12 and its packages (pinned,
  signed sources); python-rtmidi has no Windows build for Python 3.13+.
- verify_bundle ignores the kit's `reports/` folder; `*.cmd` use CRLF.

## 0.5 development Inspector — 2026-10-03

- Shared versioned SYSTEM/PANEL/ENGINE/STORAGE telemetry and retained cursor log,
  extending development probe 0B with pages 2–7. Original pages 0/1 and status
  layouts unchanged; release still rejects 0A/0B.
- Physical versus injected keys/encoders, indexed voices/modulation/resolved
  parameters, transport/queue counts, load/record/job state and storage errors.
- Read-only terminal viewer and JSONL collector reuse the same model for MIDI
  and the labelled simulation. Audio publishes bounded scalar/edge data; main
  owns snapshots, formatting and transport. No musical/DSP changes.
- New native/Python coverage and consolidated hardware checklist. Validation
  includes a preserved Windows baseline HTTP test failure; details and limits
  in INSPECTOR.md. Hardware remains unverified.

## 0.5 sampler (roadmap item 4) — 2026-10-03

Following TAPE's sampling (DC's request), with cheap extras. Design and
differences: SAMPLING.md. Software-tested only; nothing on hardware.
- Patch v4: sampler module (chromatic/kit, banks a–e, slots 1–14 + the
  recording, pitch, start/end, loop with crossfade, hold/trigger, reverse),
  7 voices, sampler route; wire 84/96 bytes; firmware minor 5. v1–v3 output
  bit-exact.
- Plays TAPE's `jammi_`/`cubbi_` files from the card; reads 8/16/24-bit,
  float, mono/stereo, 8–96 kHz; 40 MB SDRAM pool loaded in 16 KB main-loop
  steps behind a lock-free handoff; notes can start while loading.
- Recording as TAPE (toggle down + hold CHOMPI) from mic/line/resample, ~87 s,
  5 ms fades, normalisation, monitoring; becomes chromatic slot 15.
- Samples menu page (KEY_22): TAPE's shift-menu keys for mode/bank, source,
  save/copy/erase; record gesture; LEDs in TAPE's bank colours.
- Sample voices through the resonant filter, filter envelope, LFO, glide,
  delay, reverb; Hermite when pitched down; declicked restarts.
- Host: v4 schema/codec/upgrade, `samples`/`sample-save|erase|copy`,
  opcodes 08/09 (replies 44/45); webapp sampler controls and Device samples
  panel; AI authors v4 and may only use samples the device reported.
- Presets 11 Recorded Keys, 12 TAPE Kit A, 13 Sampler Stress; `make bench`
  sampler scenarios (worst 2,628 vs WAVE 2,695 instructions/sample).
- Fixed in passing: v3→v4 upgrade shared module dicts with its input.

## 0.4 MIDI CCs match stock — 2026-10-03

- DC's decision: CC20+n sets encoder n (SW1–SW6) as on TAPE/TEMPO/WAVE:
  CC24 = cutoff (SW5), CC25 = output level (SW6). Forge-only controls moved
  to General MIDI numbers stock leaves free: CC71 resonance, CC74 cutoff,
  CC85 wet bypass, CC91 reverb mix. Stock virtual-key CCs (14, 15, 26–33)
  are ignored. Wire format (SysEx) unchanged.
- Knob order follows stock `encoder_map = {1, 2, 3, 0, 4, 5}`: logical knob
  1–4 = hardware SW4, SW1, SW2, SW3, so CC20–23 move the same physical knobs
  as stock. Panel functions moved with them (mix is now on hardware SW4).

## 0.4 stock comparison refresh — 2026-10-03

- COMPATIBILITY.md refreshed for `4fec6ac`: SD card coexistence section,
  toolchain provenance (TEMPO is a GCC 13 build), proof that xPack 10.3.1
  generates the same libDaisy/DaisySP code as Arm 10.3-2021.10, corrected
  factory rebuild deltas, current Forge size and headroom.
- `make bench`: `TEMPO_GCC_PATH` builds TEMPO with its own compiler;
  TEMPO FX+output 1,352 instructions/sample with GCC 13.3.

## 0.4 device presets on the SD card — 2026-10-03

- 8 banks × 15 slots in `FORGE/B<bank>S<slot>.FPR` (CRC-checked wire DATA,
  temp + rename, read-back). Never `.bin`, so the bootloader ignores them.
- TAPE-style panel menu: toggle + CHOMPI key; white keys recall; KEY_16/17
  and encoder 1 select banks; KEY_25 save, KEY_24 copy, KEY_23 erase,
  confirmed with CHOMPI; key LEDs show occupancy, selection and mode.
- MIDI program change 0–119 recalls; SysEx opcodes 04–07 (store, recall,
  erase, list) with replies 42/43 and errors 7–9; `forge_host.py
  store|recall|erase|slots`; webapp Device presets panel.
- Shared patch DATA codec (`EncodePatchData`/`DecodePatchData`) used by SysEx,
  status replies and SD records.
- Tests: new native preset suite (records, card faults, menu, LEDs, protocol,
  runtime), 6 Python and 1 browser test; 7 code mutations each caught.
  SD card, LEDs and timing are hardware-unverified.

## 0.4 stock-firmware comparison and CPU benchmark — 2026-10-03

- New docs/forge/COMPATIBILITY.md: bootloader acceptance (FORGE.bin layout
  matches factory TAPE/TEMPO/WAVE), memory maps, audio config, identical
  keybed map, MIDI differences (CC24/25, fixed channel), upstream rebuild
  findings (TAPE case-sensitive include, xPack overflow; TEMPO size delta).
- `make bench`: ARM instruction counts in an emulator for Forge presets vs
  TAPE/TEMPO FX stages and WAVE's 8-voice engine; gate: Forge <= WAVE.
- Reverb memory moved from SDRAM to DTCM (as the stock apps do); a test
  proves uninitialised DTCM never reaches the output.
- TEST_SESSION: stock loudness reference before flashing, single-.bin and
  macOS `._` file rule at 1.1, restore from firmware/card-profiles.

## 0.4 v3 instrument: richer synth and reverb — 2026-10-03

- Patch v3 (firmware minor 4): second oscillator (waveform, level, ±24
  semitones, ±50 cents), noise, per-voice resonant SVF low-pass with its own
  ADSR and ±6-octave amount, LFO (4 shapes, 0.05–20 Hz) to pitch/filter/amp
  with mod-wheel gating, voices 1–4 (CPU fallback), glide, FDN reverb.
- CC1 mod wheel, CC26 resonance, CC27 reverb mix (v3 only).
- v1/v2 output bit-exact against 0.3 (simulation, 384,000 stereo samples).
- Transport buffers sized for 69-byte requests / 83-byte replies; UART
  timeout computed per reply. Multi-packet USB replies unverified on device.
- Host: strict v3 schema/validation, table-driven codec, `upgrade` command,
  AI instrument mode authors v3. Webapp edits every module; v1/v2 fields
  greyed; Convert to v3. Presets: Warm Pad, Acid Bass, Bell Keys, CPU Stress.
- Tests: new v3 native suite, v3 protocol tests, 200 random v3 round trips,
  upgrade/endpoint tests, 9 browser tests; 13/14 mutations caught (14th is
  output-equivalent). Hardware and live AI still unverified.

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
