# Choowi display: CHOMPI's screen on a phone (0.20, development)

Status: **implemented and software-tested; not yet tried on CHOMPI or on a phone.** Branch `forge/fx`, part of
the 0.20 line (the owner's request, 2026-10-13: "I need it for phone", Android). Not published: the forge site
stays as released until 0.20 ships.

CHOMPI has no screen, and its menus have grown deep (presets, TAPE's page, harmony, arp and bass, synth, FX).
The display is a page of the forge site (`docs/workshop/display.html`) that shows, live, over USB:

- **the panel's lights** as they are now: the 25 keys, CHOMPI, PLAY, LOOP, the four knob rings, SW6 and the
  transport pair, drawn in place on a picture of the panel;
- **the page you're on** (play, TAPE record, presets, TAPE page, harmony, arp and bass, synth, FX) in its
  CHOMPI colour, or Dream;
- **what each knob moves** right now (name, page, value). On the FX page the four module amounts; on the
  harmony page mode, chord size, inversion and spread (with harmony on / off, layout and voice leading); on
  the arp and bass page tempo, arp octaves, gate and bass rate (with tap tempo or MIDI clock, the bass octave);
- **the chord** harmony mode just played (name, numeral, notes), **the looper** (state, position, length,
  effects before or after), **the event recorder**, SW6 (volume or input gain) and the **battery**.

**Page context everywhere** (the owner, 2026-10-13: "labels on everything"): every key is labelled with
what it does on the page you're on (note names when playing; chord numerals with harmony on in the static
layout; slots, bank, save / copy / erase on the presets and TAPE pages; the effects on the FX page; waves,
intervals and voices on the synth page; patterns, rates and the bass on the arp and bass page), each knob's
control is written under it, and an **on this page** list below the readout explains the page's keys, combos
and the other controls (CHOMPI, SW5, SW6, PLAY, LOOP). `tests/test_display_js.py` checks the key labels
against the firmware's key handlers, so they can't drift apart.

It only reads: nothing on the page changes CHOMPI.

## Using it

1. Android phone, Chrome. Plug CHOMPI into the phone with a USB-C cable (an OTG cable or adapter); CHOMPI
   may draw power (and charge) from the phone while connected: mind the phone's battery on long sessions
   (not yet measured).
2. Open the forge site's **phone display** page and press **Connect**; allow MIDI devices (Chrome asks once,
   including "system exclusive" messages, which is how the view travels).
3. Turn the phone sideways for a bigger panel: portrait puts the panel above the readout, landscape side by
   side, sized to the screen with no scrolling. The ⤢ button goes full screen; the page keeps the screen
   awake while it's open.

iPhones have no Web MIDI in any browser, so the page can't work there. **Show a demo** draws the layout with
made-up lights and state, without a CHOMPI.

**Before 0.20 is published** the page isn't on the site yet. To try it on the phone from the development
computer: serve `docs/` (the `site` entry in `.claude/launch.json`, port 8800), then either forward the port
over adb (`adb reverse tcp:8800 tcp:8800`, open `http://localhost:8800/workshop/display.html`) or add the
computer's address to `chrome://flags/#unsafely-treat-insecure-origin-as-secure` (Web MIDI needs a secure
origin). With CHOMPI on the phone's only USB port, adb has to go over Wi-Fi (wireless debugging).

## Wire format (opcode 0D, every build)

Request `F0 7D 46 47 01 0D seq_lo seq_hi page checksum F7`, page 0 or 1. The main loop answers at once from
what it already draws for the LEDs (no audio-owner round trip, no SD work). Reply opcode **0x49**:

- **Page 0, the lights** (115 bytes without F0/F7): `00 00`, then 35 lights × 7-bit RGB, as drawn (before the
  LED balance): 0–9 the black keys C#3…A#4, 10–24 the white keys C5 down to C3 (TAPE's order), 25 CHOMPI,
  26 PLAY, 27 LOOP, 28–31 the rings SW4 SW1 SW2 SW3, 32 / 33 the transport pair, 34 SW6.
- **Page 1, the state** (98 bytes): `00 01`, then 16 words of 32 bits, 5 bytes each (7 bits per byte, low
  first), then the chord's 5 notes, then battery level, power flags and charge state (as the status reply):

| Word | Contents |
|---|---|
| menu | `PresetMenu::Packed`: bit 0 open, 1–3 mode (7 harmony page, 6 parts, 5 synth or FX), 21 samples page |
| looper | `PackLooper`: 0–2 state, 3 overdub, 4 effects before, 5 has a loop, 6–15 position ×1023, 16–31 length in 10 ms |
| knob_state | pages (3 bits per knob, SW4 first), 12–15 the sound's own page, 16–19 reset flash, 20 SW6 on input gain, 21 effects-only sound, 24–31 SW6's value |
| knob_values | each knob's value 0–255 (SW4 lowest byte) |
| knob_targets | each knob's control on its page (`Parameter` id, SW4 lowest byte) |
| fx_lights, fx_knobs | the FX page's lights (`FxPack`; bit 31 = the FX page is open) and module amounts |
| synth_lights, synth_knobs | the synth page's lights and knobs |
| harmony_lights, parts_lights, parts_clock, sequence_lights | as drawn on those pages |
| chord | `Engine::ChordView`: root pitch class 0–3, quality 4–6, kind 7–9, degree 10–12, shifted 13, notes 14–16, chord counter 17–23, the 5th note 25–31 |
| flags | 0 record position, 1 recording, 2 safe mode, 3 sample job busy, 4 install armed, 5 SW5 just turned, 8–15 SW5's cutoff 0–255, 16–25 the loop speed as (speed + 2) × 255 (−2…2) |
| dream | the Dream lights (bit 26: Dream is playing) |

The page polls page 0 and page 1 in turn as fast as CHOMPI answers, and the status (opcode 02) every 2 s for the
firmware version. Firmware before 0.20 answers "unknown operation"; the page then says it needs 0.20.

## Code

- Firmware: `core/protocol.h` (`RequestKind::View`, `ViewState`, `EncodeViewLeds` / `EncodeViewState`),
  `src/forge_main.cpp` (`view_leds` filled by `DrawLeds`, `ViewNow`, answered in `HandleFrame`),
  `core/panel_controller.h` (`KnobTargets`, the looper length in `PackLooper`), `core/engine.h` (`ChordView`,
  published by the audio callback only when a new chord sounds). Release +1.26 KB of executable space.
- Host: `forge_host.view_message`, `VIEW_WORDS`, `decode_response` (0x49).
- Site: `docs/workshop/display.html`, `display.css`, `display.js` (drawing, polling, demo),
  `display-model.js` (names: pages, knobs, chord, looper), `forge-midi.js` (ports and one request at a time),
  `forge-wire.js` (`viewMessage`, the 0x49 reply).
- Tests: `tests/protocol_test.cpp` ProtocolView, `tests/harmony_test.cpp` (chord view), `tests/panel_test.cpp`
  (looper length, knob targets), `tests/test_display_js.py` (replies decoded alike in Python and the browser,
  the knob names in `Parameter` order, chord names as the Inspector's, pages from the menu word).

## Not yet

- Tried on CHOMPI and an Android phone (USB OTG, poll rate, battery draw).
- A cheat sheet per page under the readout (the owner's idea, with the illustrated guides).
