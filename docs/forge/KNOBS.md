# Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10)

Status: **implemented and software-tested** on 2026-10-05; nothing is
hardware-verified. TEST_SESSION 3F (3.51–3.57) covers it; the bridge runs 3.52,
3.53, 3.55 and the audio checks 3.57a–f automatically. Earlier layout (0.6–0.9):
see git history of this file.

DC's decisions (2026-10-05, the TAPE parity checklist): keep TAPE's workflow;
TAPE's knob layout, step sizes and gestures, but keep Forge's synth controls on
extra pages; SW5 stays filter cutoff with the loop speed on push-and-turn; panic
moves to SW4 + SW3; a long press resets a knob. TAPE's controls are listed with
source lines in [TAPE_CONTROLS.md](TAPE_CONTROLS.md).

## What the player sees

The four page knobs are **SW4, SW1, SW2, SW3** (TAPE's knobs 0–3, MIDI CC 20–23).
Pages follow TAPE first, then Forge's extra controls, then the patch's own knob
(v5 `knobs`) as one last page when the patch sets it:

| Knob | Sampler patch | Synth patch | Effects-only patch (line in) |
| --- | --- | --- | --- |
| SW4 | 1 pitch · 2 gain · 3 resonance · 4 filter envelope | same | 1 delay mix · 2 output level |
| SW1 | 1 sample start · 2 attack · 3 LFO speed | 1 attack · 2 decay · 3 LFO speed | 1 delay time |
| SW2 | 1 sample end · 2 release · 3 LFO filter · 4 loop crossfade | 1 release · 2 sustain · 3 LFO filter · 4 osc 2 detune | 1 feedback |
| SW3 | 1 reverb + delay · 2 saturation · 3 DJ filter | same | same (on the line input) |
| SW5 | turn: filter cutoff · push and turn: loop speed (scrub when paused) · click: speed 1× | same | same |
| SW6 | turn: volume · short press: input gain page · hold 2 s: battery | same | same |

- **Harmony page** (0.13, menu: hold KEY_21 1 s): the four knobs leave their pages
  and set harmony instead (SW4 mode / on-off, SW1 chord size / layout, SW2 inversion /
  voice leading, SW3 open spread); MANUAL section 8a.
- **Parts page** (0.14, KEY_21 from the harmony page): SW4 tempo (press = tap tempo),
  SW1 arp octaves, SW2 arp gate, SW3 bass rate; MANUAL section 8b.
- **Pitch** (SW4 page 1) is TAPE's: 0.83 = 1×, the centre (0.5) stops, below the
  centre plays backwards (samples); thirds of each side are .01–.5×, .5–1×, 1–2×.
  On oscillators it transposes by the same ratio.
- **Gain** (SW4 page 2): TAPE's voice gain, 2v² + 0.01 (0.704 ≈ 1×).
- **Reverb + delay** (SW3 page 1): one value sets the delay feedback, half of it
  the delay mix and the reverb mix (TAPE drives delay and reverb together).
- **Saturation** (SW3 page 2): TAPE's soft clip with level compensation.
- **DJ filter** (SW3 page 3): TAPE's: left of centre low-pass, right high-pass,
  centre open. The resonance is in the menu (stage 2).
- Pitch, gain, saturation, DJ filter, pan, warble, compressor and input gain are
  device performance state (TAPE's knob positions), kept across preset changes and
  reset at power-up as in TAPE; they are not stored in patches.
- **Sampler patches (0.12):** pitch, gain, pan, start, end, attack and release
  (and the menu's auto-loop/sustain presses) belong to the sample slot, saved as you
  turn into TAPE's `presets.json` (MANUAL section 7). In kit mode they edit the pad
  last played; the shared pitch/gain/pan stay where they were. A long press resets
  them to TAPE's defaults (1×, full sample, no attack/release, centre).

Gestures:

- **Step size (TAPE):** SW4 and SW5 count one step per click, the others three;
  coarse controls move 0.01 per step, sample start/end and pitch 0.003. So SW1–SW3
  and SW6 move 3 % per click (33 clicks end to end), start/end 0.9 %, pitch 0.3 %.
  Forge 0.6–0.9 moved 1/127 per click, which made single clicks inaudible (DC).
- **Page:** changes when the knob is **released** (TAPE). Pages are per knob,
  survive preset changes (a patch with fewer pages sends that knob back to page 1)
  and reset at power-up.
- **Reset:** hold a knob **1.5 s** without turning: its current control goes back
  to the preset's value (performance controls to TAPE's default); the light
  flashes white; the page does not change.
- **Panic:** hold **SW4 and SW3 together 1 s** (also MIDI CC 120/123).
- **Lights:** TAPE's value colours on TAPE's pages (SW4 pitch: blue → green →
  yellow → red away from the centre; gain blue → pink → red; SW1 yellow → orange;
  SW2 orange → red; SW3 teal → blue, yellow → red, purple → pink → white); Forge's
  extra pages red (first) and green (second); the patch page dim to bright white.
  In the record position knob lights 1–4 are off and PLAY/LOOP/SW5 dim to 70 %
  (TAPE). SW5's two lights show the loop speed (LED 6 forward, 5 reverse). SW6:
  dim to bright green for volume, blue → red for input gain.
- In the menu's TAPE page the knobs are TAPE's second layer (MANUAL section 7);
  on Forge's presets page SW4 picks the preset bank.

## Patch knobs (patch version 5)

v5 = v4 + four bytes: what knobs 1–4 control on their last page and through MIDI
CC 20–23. JSON:

```json
"knobs": ["filter.cutoff_hz", "lfo.rate_hz", "reverb.size", "reverb.mix"]
```

Each entry is `"default"` or one of 26 `module.key` controls
(`forge_host.KNOB_TARGETS`). `"default"` adds no page; CC 20–23 then move the
source's default (delay mix/time/feedback/level, or the sampler's pitch, start,
end, delay mix). Performance controls (pitch, gain, saturation…) are not knob
targets. The AI authoring mode writes v5 and chooses the knobs; the webapp's
*Panel knobs* group edits them; `14-knob-pad.json` shows it.

Wire: unchanged from 0.6 (request 88 bytes, index 83–86 = Parameter id + 1,
0 = default). Inspector page 3 reports knob n's page in bits 3n..3n+2 (0.10;
0.6–0.9: 2 bits).

## Code

- `core/knob_layout.h`: the table above (`LayoutTarget`, `Pages`, `Target`), step
  sizes (`Step`), TAPE's colours (`KnobColour`, `VolumeColour`).
- `core/parameters.h`: performance controls (`Performance`, ids after
  `SampleXfade`, never on the wire), `TapeSpeedRatio`.
- `core/engine.h`: `Value`, `ResetControl` (the patch's value is kept as
  `patch_`), performance and the Space macro; `core/tape_fx.h`: TAPE's DJ filter,
  saturation, warble and compressor; `core/synth.h`: speed (with reverse), gain, pan.
- `core/panel_controller.h`: presses on release, hold reset, SW4 + SW3 panic,
  SW5 push-and-turn, SW6 pages, `PublishKnobs` (values and state for the lights),
  `ComposeKnobLeds`, `ComposeTransportLeds`.
- Host: `forge_host.KNOB_LAYOUT`, `knob_pages`, `knob_control`; the inspector,
  walk and automatic checks use them.

## Tests

- `tests/panel_test.cpp` `KnobPages`: every page and step size, release paging,
  hold reset, patch page, page clamp on patch change, lights.
- `tests/knob_audio_test.cpp`: every page of every starter preset (as stored by
  the bridge: v5) must change the rendered sound by at least 5 % after 10 clicks
  (spectrum, envelope, loudness or balance). 2026-10-05: 127 pages audible; 24
  need another control first and are listed with the reason (LFO speed without an
  LFO depth; LFO on the mod wheel, which CHOMPI's panel lacks — Acid Bass, Bell
  Keys; detune without oscillator 2; crossfade with loop off; delay time/feedback
  with delay mix 0; filter pages on a pure, wide-open sine; decay with sustain ≥ 0.7;
  sample end beyond a short note).
- Bridge 3.57a–f: the same comparison on CHOMPI's real output.
