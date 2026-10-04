# Forge knob pages and patch knobs (firmware 0.6)

Status: **implemented and software-tested** on 2026-10-04; nothing is
hardware-verified. TEST_SESSION 3F (3.52–3.56) covers it; the bridge runs
3.52, 3.53 and 3.55 automatically (they also pass in the simulation).

DC's decisions (2026-10-04): both a fixed page set and patch/AI-assigned
knobs; press a knob to change its page and its light shows the page; SW6's
press does nothing for now; build before the hardware test.

## What the player sees

The four knobs are logical knobs 1–4 = **SW4, SW1, SW2, SW3** (stock order,
MIDI CC 20–23). Each has four pages. **Pressing a knob steps its page**
(1 → 2 → 3 → 4 → 1); the light at that knob shows the page:

| Page | Light | Knob 1 (SW4) | Knob 2 (SW1) | Knob 3 (SW2) | Knob 4 (SW3) |
| --- | --- | --- | --- | --- | --- |
| 1 | dim white | the patch's knob 1 | the patch's knob 2 | the patch's knob 3 | the patch's knob 4 |
| 2 | red | filter cutoff | attack | LFO rate | delay mix |
| 3 | green | resonance | decay | LFO filter depth | delay feedback |
| 4 | blue | filter envelope amount | release | osc 2 detune (sampler: loop crossfade) | reverb mix |

Page 1 is what the patch says (v5 `knobs`), or, when it says `default` or the
patch is v1–v4, the old behaviour: delay mix, time, feedback, output level; on
sampler patches TAPE's page 0: pitch, start, end, delay mix.

- Pages are per knob and belong to the panel: they survive preset changes and
  reset at power-up (not stored, like TAPE's encoder pages).
- A control the patch version cannot carry is ignored (v1 has no envelope,
  v1/v2 no resonance/LFO/reverb, v1–v3 no sampler). Turning does nothing; it
  never fails or crashes.
- Knob 1 still selects the bank while the menu is open; presses still step
  pages. SW5 (cutoff/panic, looper transport) and SW6 (level) are unchanged.
  SW6's press does nothing (DC: "nothing for now").
- Each turn step is 1/127 of the control's range, as before.

## Patch knobs (patch version 5)

v5 = v4 + four bytes: what knobs 1–4 do on page 1. JSON:

```json
"knobs": ["filter.cutoff_hz", "lfo.rate_hz", "reverb.size", "reverb.mix"]
```

Each entry is `"default"` or one of 26 `module.key` controls
(`forge_host.KNOB_TARGETS`): delay mix/time/feedback, output level, filter
cutoff/resonance/envelope amount, amplitude attack/decay/sustain/release, LFO
rate/vibrato/filter/tremolo depth, osc 2 level/detune, noise, glide, reverb
mix/size/damping, sampler pitch/start/end/crossfade. Delay bypass is not a
knob target. MIDI CC 20–23 always move the page-1 controls, whatever page the
panel shows. The AI authoring mode now writes v5 and chooses the four knobs
for the sound; the webapp's *Panel knobs* group edits them; *Convert to v5*
upgrades any older patch with all four `default`. Factory preset
`14-knob-pad.json` shows it.

Wire: request 88 bytes (index 83–86 = Parameter id + 1, 0 = default; see
PROTOCOL.md), status 100 bytes (`kMaxReply` 96 → 100). v1–v4 requests and
presets are unchanged and still accepted; device presets store v5 like the
others. Firmware minor 6 (0.6).

## Code

- `core/parameters.h`: Parameter ids for every continuous control, `knobs[4]`,
  `Resolve`, `Field`, `MinVersion`, version-gated `Apply`.
- `core/protocol.h`: v5 layout, sizes. `core/engine.h`: knob resolution,
  reverb reconfiguration for size/damping.
- `core/panel_controller.h`: `KnobPageParameter` (the table above), page per
  knob, `KnobPages()`, `ComposeKnobLeds`. Encoder switches are buttons 0–3
  (ENC_1..4_SW); logical knob n's switch is `kKnobEncoder[n]`.
- `src/forge_main.cpp`: through-hole LEDs 1–4 (TAPE's ENC_4, ENC_1, ENC_2,
  ENC_3 lights), redrawn at ~30 Hz.
- Inspector page 3 gains the knob pages (97 bytes); the bridge shows each
  knob's page and control.
- Host: `forge_host` (schema, codec, upgrade, `knob_control`), `forge_ai`,
  `forge_inspector`, webapp, automatic checks, presets.

## Costs and limits

- Code: release 227,060 B (78.6 %, +3.6 KB), development 242,512 B. RAM:
  D1 SRAM 93,532 B release (4 bytes of knob pages; MIDI frames 4 bytes larger).
- CPU: a knob turn reconfigures the synth (or reverb) once per audio block,
  as SW5's cutoff already did; `make bench` unchanged (worst 2,671 ≤ WAVE 2,695).
- Taken from TAPE 2.0's source, not yet seen on hardware in Forge: the knob
  lights (TAPE `NormalPage.h` `led_map`: ENC_1_SW → 2, ENC_2_SW → 3,
  ENC_3_SW → 4, ENC_4_SW → 1) and the encoder presses (TAPE also uses them as
  page keys). Also unverified: zipper noise on fast turns.
