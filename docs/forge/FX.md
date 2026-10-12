# The FX section (Choowi 0.20, development)

Status: **Phase 1 built and software-tested, not hardware-verified.** Development line only
(branch `forge/fx`); not in any release yet. Design and decisions: [CONTINUE.md](CONTINUE.md).

Four modules, one effect chosen in each, run in a route. Today's sound is the default: with
nothing changed, CHOMPI sounds exactly as 0.19.2 (checked bit for bit in `tests/fx_test.cpp`).

| Module | Choices (key) | Built in Phase 1 | Knob on the FX page |
|---|---|---|---|
| **Space** (white) | echo + room (C3) · cloud (D3) · tape echo (E3) | echo + room, cloud | SW4: amount (as SW3 page 1); hold + turn: delay time and room / cloud size |
| **Colour** (green) | saturation (F3) · fold (G3) · crush (A3) | saturation | SW1: amount (as SW3 page 2) |
| **Texture** (yellow) | cassette (C4) · broken (D4) · squash (E4) | cassette (today's warble) | SW2: amount |
| **Motion** (orange) | chorus (F4) · phaser (G4) · tremolo (A4) | chorus | SW3: amount; hold + turn: rate |

The DJ filter is not a module: it stays first in the chain and on SW3 page 3, as before.

## Getting there

Toggle **down**, hold **CHOMPI** (the menu), then **KEY_21** (C#4) to step harmony → parts →
synth → **FX**; or hold KEY_21 for 4 s from TAPE's page. The CHOMPI light turns **green**.

- **White keys** pick each module's effect: the chosen key is bright in the module's colour, the
  other two dim. Effects not built yet stay very dim and do nothing.
- **A#3** steps the route (light white → green → yellow → orange):
  **Classic** (DJ filter, Colour, Motion, Texture, Space: today's order) → **Wear last** (Texture
  after Space: echoes and wash get worn too) → **Wash first** (Space before Motion: the chorus
  moves the whole wash) → **Side by side** (the reverb hears the dry signal, not the echoes).
- **F#3 / G#3**: effects before / after the looper (the lit one is in use).
- The **knob rings** show each module's amount in its colour.

## Saved with the sound

A sound saved while the FX section is in use (a choice, the route or Motion changed) is saved as
patch v8 and brings its FX settings back when recalled. Older sounds leave the FX settings as they
are (as TAPE's saturation always did). A v8 preset reads as empty on 0.19.2 firmware.

## Costs

Release 240,840 B / development 252,532 B of 288,768 B; 33 KB of idle D2 SRAM for the chorus and
cloud. CPU: the worst case without the new effects is ~60 % peak (sampler + looper + every effect,
RESOURCE_LEDGER); chorus and cloud are to be measured on CHOMPI in the listening session.

## For the listening session

1. Chorus on a pad: amount (SW3) and rate (hold SW3 + turn) — lush or seasick?
2. Cloud on a pad and on plucks (D3, then SW4 up; hold SW4 + turn for size) — wash, ringing, too dark?
3. The four routes with chorus, warble and cloud all up — does each sound distinct and useful?
4. Save a sound with cloud + chorus, recall another, recall it again — do the FX come back?
5. The lights: groups, colours, route key, rings, the green CHOMPI light.
