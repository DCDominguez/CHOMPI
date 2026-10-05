# Stock TAPE: what every control does (reference for Forge parity)

Read from `firmware/chompi-tape/code/src` (2026-10-05, read-only). NP = NormalPage.h,
MP = MenuPage.h, DSP = DSPEngine.h, LE = LooperEngine.h, HW = hardware.h,
MAIN = chompi_main.cpp. Decisions on what Forge copies: DC's answers to the parity
checklist (TEST_RESULTS / CONTINUE). "TRUE" toggle = the position where CHOMPI opens
the menu (physically **down** on DC's unit).

## Framework
- Events go to the top page first; a page passes them down by returning false.
  Press = `numberOfPresses == 1`, release = 0 (NP:680, MP:632).
- Normal page ignores buttons/encoders for 1.5 s after init and while FileCopier runs (NP:229-236, 677, 916).
- Encoder map hardware→logical `{1, 2, 3, 0, 4, 5}` (ui.h:30, 333). Logical knobs 0 and 4: ±1 per detent;
  others ±3 (ui.h:335-338). Steps: coarse .01 (so .03/detent), fine .003 (start/end, free pitch .009/detent),
  values clamped 0..1 (NP:102-104, 930-950).
- Hardware → logical: SW1→1 (start/attack, LED 2), SW2→2 (end/decay, LED 3), SW3→3 ("magic" FX, LED 4),
  SW4→0 (pitch/gain, LED 1), SW5→4 (transport, LEDs 5/6), SW6→5 (volume, LED 9). CHOMPI LED 0, PLAY 7, LOOP 8.
- `enc_defaults` (ui.h:16-20): page 1 `{.83, 0, 1, 0, .75, .84}`, page 2 `{.704, 0, 0, 0, 0, .75}`,
  page 3 `{0, 0, 0, .5, 0, 0}`. `knob_num_pages = {2, 2, 2, 3, 1, 2}` (NP:122).
- `cc_map` out (NP:10-13): `{20..25}`, `{28, 29, 30, 31, 0, 32}`, `{0, 0, 0, 33, 0, 0}`.
- Pitch quantisation: normal page `!ps_quant`, menu `ps_quant`; options.json "Pitch Quantize In Shift Menu"
  default true → normal page free, menu quantised (NP:143, MP:37).

## Knobs, normal play
Turns send `cc_map` (physical turns only, NP:1003). Knobs 0-2 write the slot's preset (`DumpValuePresets`);
knobs 3-5 reset at boot. Page advances on knob **release** (NP:693-704); no long press.
- **SW4 (0)**: p1 sample pitch, default .83 = 1× (free: .5 centre, below = reverse, piecewise up to 2×;
  quantised: fifths/octaves every 4 detents, ±2×) (DSP:946-1037). p2 voice gain .704 (applied only with toggle TRUE).
- **SW1 (1)**: p1 start (fine; start+.01 < end) (NP:979-991). p2 attack `(v³+.01)×20+.001` s.
- **SW2 (2)**: p1 end (fine). p2 "decay" = release `(v³+.01)×4+.001` s (sustain off: gate drops at decay).
- **SW3 (3)**: p1 reverb + delay feedback from one value (split-delay option: <.5 delay, >.5 reverb, default .5);
  p2 saturation `log(1.7v+1)×13+1` with gain comp; p3 DJ filter (LP <.5, HP >.5, default .5) (NP:365-403).
- **SW5 (4)**: transport only, default .75 = 1×. Playing: speed `v×4−2` (free) or fifths/octaves (quantised);
  not playing: tape scrub (turn count ×.2, slew per "Tape Slew On"). Press: release resets to .75 + CC 24;
  both edges `SetLooperPitch(1)`. Reset after looper clear (NP:223-227, 414-473, 723-734, 961-972).
- **SW6 (5)**: p1 main gain .84 (light = output VU × value); p2 input gain .75 (mic/line monitor + record;
  resample level) (light blue→red). Short press (< 2 s) changes page on release; hold > 2 s = battery
  (white/green/yellow/red) (NP:475-520, 707-720).
- Toggle FALSE (record position): knob LEDs 1-4 off; PLAY/LOOP/transport LEDs ×0.7.

## Keys
- Chromatic MIDI 48-72 (KEY_1 = 48 … KEY_15 = 72; black 49, 51, 54, 56, 58, 61, 63, 66, 68, 70); transpose
  = note − 60; velocity 127; no octave shift; 7 voices with stealing (DSP:28, 652-767; NP:816-851).
- Armed looper: any key starts looper recording (NP:836-837).
- Chromatic: every key plays the selected slot pitched; sustain/auto-loop per slot (default on).
- Kit: white keys 1-15 = slots 1-15 (15 = RAM recording), black keys silent and send no MIDI.
- Key LEDs: white while sounding; kit: occupied slots 25 % bank colour (purple, orange, teal, dark orange,
  yellow-green), KEY_15 pink; chromatic: KEY_1/8/15 dim (pink when slot 15 selected); hidden when toggle
  FALSE and source MIC (NP:238-271).

## CHOMPI, PLAY, LOOP
- **CHOMPI, toggle FALSE**: CC 21 out; press starts recording into the RAM buffer (stops looper overdub first),
  release stops ("Record Latch": second press stops). On stop: knob pages 0/1 of knobs 0-2 reset, all voices
  stop, chromatic slot 15 selected. Auto-stop when full (~165 s if stereo-interleaved; unverified). Input
  monitored in this position. Toggle → TRUE stops recording. LOOP ignored while recording. LED: pink blink
  while copying files, red recording, else input VU (NP:786-810, 1035-1055, DSP:592-631).
- **CHOMPI, toggle TRUE**: opens the menu (ui.h:306-318); closes on release unless an action is pending.
  Side effect: MIDI out channel = 2 while held, 1 after (looks like a leftover). LED purple while held.
- **PLAY** (CC 26): tap while recording/overdub → play (first take closes without overdub); tap playing →
  pause (on press after 10 ms); tap paused → resume (on release); hold 2 s not playing → jump to start (LE:89-180).
- **LOOP** (CC 27): empty → record → loop closes into overdub → play → overdub… Auto-close when full.
- **PLAY + LOOP** on empty: arm (next key or MIDI note starts); both held 2 s: clear.
- LEDs: PLAY off/white armed/teal first take/teal×(1−pos) playing/white×(1−pos) paused; LOOP off/red blink
  armed/red first take/yellow×pos overdub/white×pos.

## Toggle
`GetToggleState() = tog_state < 100` (HW:377). TRUE: CHOMPI opens menu, monitor off (except monitor BOTH),
knob LEDs on. FALSE: CHOMPI records, input monitored per monitor mode, knob LEDs off, looper LEDs dimmed,
SW4 p2 gain not applied; moving to FALSE closes the menu unless an action runs.

## Menu (shift layer while CHOMPI held, toggle TRUE)
- Closes when ≥ 1 s since the last action and CHOMPI released with nothing pending / action finished /
  toggle went FALSE. Incoming MIDI CCs ignored while open.
- **Knob turns** (MP:409-546): SW4 p1 pitch in the other quantise mode, p2 pan (.01/detent); SW1+SW2 p1 move the
  start-end window (.03/detent), p2 attack and decay together; SW3 p1 delay time (+ reverb time, .05-.97),
  p2 warble (wow/flutter), p3 filter resonance (not persisted); SW5 looper pitch; SW6 output compressor (0-1, default 0).
- **Knob presses**: SW4 reset pitch (p1) or gain .704 + pan .5 (p2); SW1 auto-loop on/off; SW2 sustain on/off;
  SW3 reset all FX; SW6 monitor mode HP → BOTH → SEND_RET (LED orange/blue/yellow); SW5 = looper pitch reset.
- **Black keys**: KEY_16 chromatic (again: next bank a-e); KEY_17 kit (again: next bank); KEY_18/19/20 mic /
  line / resample (jack plug auto-selects line/mic); KEY_21/22 FX before / after the looper; KEY_23 erase,
  KEY_24 copy, KEY_25 save (select modes).
- **White keys**: chromatic, nothing pending: select slot (load its preset); kit: nothing. PLAY/LOOP: overdub
  feedback −/+ 0.1 on press **and** release (≈ ±0.2 per tap; quirk).
- **Save/copy/erase**: target white key 1-14 or PLAY/LOOP (= looper, slot 16), then CHOMPI. Save copies the RAM
  buffer to `<jammi|cubbi>_<a-e><n>.wav` + slot-15 preset values; copy can change bank/mode between source and
  destination; erase deletes `.wav` and `_double.wav` and invalidates the preset.
- LEDs: bank key in bank colour, source key pink, FX key yellow, erase red / copy green / save blue; white keys
  bank colour if a file exists, KEY_15 pink, selected white, blinking during selection; all red without SD.
  CHOMPI blinks red while a target is selected, white while executing.

## Effects and looper
Chain: voices → monitor → [FX if pre] → looper → [FX if post] → main gain (HP/line) → compressor. FX block: DC
block, DJ filter (LP/HP + resonance), saturation, warble, mono feedback delay (max 96,256 entries), reverb.
No bitcrush, no dedicated reverse/half-speed button. Looper buffers in SDRAM, not persisted unless saved.

## MIDI, panic, files
- In: only `midi_ch_in`; notes 24-72 (48-72 map to keys); CC 20-25 = knobs (absolute, CC 24 only while
  playing); CC 26/27 = PLAY/LOOP (> 84 press, < 42 release). Out: notes (vel 127), knob CCs, PLAY 26, LOOP 27,
  CHOMPI 21 (toggle FALSE).
- **No panic** of any kind (stuck-note hack commented out, ui.h:242-262).
- `options.json` (rewritten at boot): Record Latch (false), Midi In/Out Channel (1), Tape Slew On (true),
  Monitor Position (1 HP, 2 BOTH, 3 SEND_RET), Pitch Quantize In Shift Menu (true), Split Delay (false).
- `presets.json`: mode × 5 banks × 14 slots × {pitch, start, end, attack, decay, autoloop, sustain, gain, pan,
  valid}; written every 5 s via `presets_temp.json` → rename, only while silent; slot 15 in RAM only.
- Samples `jammi_a1.wav` … `cubbi_e14.wav` + `_double`; boot rewrites old/incomplete files.

## Boot and power
SW6 held during the 0.5 s scan → factory test; CHOMPI + PLAY + LOOP → shipping mode; boot animation, file
conversion, rainbow (ignores input), 1.5 s lockout. Low battery: yellow flashing 15 s then shipping mode; weak
charger: STOP mode. No SD: 3 s animation, chromatic slot 15, menu bank/save/copy/erase disabled.
