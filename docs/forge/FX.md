# The FX section (Choowi 0.20, development)

Status: **all twelve effects built and software-tested, not hardware-verified.** Development line only
(branch `forge/fx`); not in any release yet. Design and decisions: [CONTINUE.md](CONTINUE.md).

Four modules, one effect chosen in each, run in a route. Today's sound is the default: with
nothing changed, CHOMPI sounds exactly as 0.19.2 (checked bit for bit in `tests/fx_test.cpp`).

| Module | Choices (key) | Knob on the FX page: turn = amount | Hold + turn = character |
|---|---|---|---|
| **Space** (white) | echo + room (C3) · cloud (D3) · tape echo (E3) | SW4 (as SW3 page 1) | delay time and room / cloud size |
| **Colour** (green) | saturation (F3) · fold (G3) · crush (A3) | SW1 (as SW3 page 2) | fold: bias (even harmonics); crush: lower sample rate |
| **Texture** (yellow) | cassette (C4) · broken (D4) · squash (E4) | SW2 | cassette: hiss and worn highs; broken: longer dropouts, then stutters; squash: slower release |
| **Motion** (orange) | chorus (F4) · phaser (G4) · tremolo (A4) | SW3 | rate (chorus 0.08–2 Hz, phaser 0.05–1.3 Hz, tremolo 0.3–10 Hz) |

- **Tape echo:** the delay's head wobbles slowly and each repeat comes back darker and gently saturated.
- **Fold:** a wavefolder, glassy then metallic as it drives harder. **Crush:** fewer bits (12 down to 3).
- **Broken:** the sound drops out for moments at random, like failing tape. **Squash:** a heavy, pumping compressor.
- **Phaser:** four swept all-pass stages with feedback. **Tremolo:** the level pulses, the sides in opposite phase (at full depth it pans).

The DJ filter is not a module: it stays first in the chain and on SW3 page 3, as before.

## Getting there

Toggle **down**, hold **CHOMPI** (the menu), then **KEY_21** (C#4) to step harmony → parts →
synth → **FX**; or hold KEY_21 for 4 s from TAPE's page. The CHOMPI light turns **green**.

- **White keys** pick each module's effect: the chosen key is bright in the module's colour, the
  other two dim.
- **The order** (the modules run in series, each on the one before's output, so the order changes how
  they affect each other):
  - **tap A#3**: the next preset: **Classic** (orange: Colour, Motion, Texture, Space: today's order) ->
    **Wear last** (blue: Texture after Space, the wash gets worn too) -> **Wash first** (purple: Space before
    Motion, the motion moves the whole wash);
  - **hold A#3 and tap the modules in the order you want** (any key of a group = that module), let go: that
    order (modules you didn't tap follow in their order). A#3 lights **white** for your own order; while it
    is held the groups show the order by brightness, first brightest;
  - **B3**: **side by side** on / off (red): the reverb hears the sound before the delay, not the echoes.
- **F#3 / G#3**: effects before / after the looper (the lit one is in use).
- The **knob rings** show each module's amount, dim (off) to bright; **while you hold a knob** its ring
  shows the character instead, in white.

## Saved with the sound

A sound saved while the FX section is in use (a choice, the route or Motion changed) is saved as
patch v8 and brings its FX settings back when recalled. Older sounds leave the FX settings as they
are (as TAPE's saturation always did). A v8 preset reads as empty on 0.19.2 firmware.

## Costs

Release 245,784 B / development 257,372 B of 288,768 B; 33 KB of idle D2 SRAM for the chorus and
cloud (the other effects need only a few words of state). CPU: the worst case without the new effects
is ~60 % peak (sampler + looper + every effect, RESOURCE_LEDGER); one effect per module runs at a time;
to be measured on CHOMPI in the listening session.

## For the listening session

1. Each module's three effects on a pad and on plucks, amount and character — keep, tune, or swap?
2. Chorus on a pad: amount (SW3) and rate (hold SW3 + turn) — lush or seasick?
3. Cloud on a pad and on plucks (D3, then SW4 up; hold SW4 + turn for size) — wash, ringing, too dark?
4. The four routes with chorus, warble and cloud all up — does each sound distinct and useful?
5. Save a sound with cloud + chorus, recall another, recall it again — do the FX come back?
6. The lights: groups, colours, route key, rings, the green CHOMPI light.
