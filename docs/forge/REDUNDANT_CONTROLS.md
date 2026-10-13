# Redundant controls: review for 0.20 (proposal, nothing changed yet)

The owner asked (2026-10-13) for a pass on controls that do the same thing in two places now that the FX
page exists. Found from the firmware's own tables (`core/knob_layout.h` LayoutTarget, `PanelController`
FxAmount / SynthTarget / FX keys, `core/preset_menu.h`). Each item: what overlaps, why it matters, a
recommendation. **Decisions are the owner's**; nothing below is implemented.

## Same control, two places

| # | Control | Where | Recommendation |
| --- | --- | --- | --- |
| 1 | **Space** amount (TAPE's reverb + delay) | SW3 page 1 when playing · SW4 on the FX page | Keep both (SW3 page 1 is TAPE's muscle memory). Rename SW3 page 1 to "Space" in docs and the display so it reads as the same thing. |
| 2 | **Colour** amount | SW3 page 2 ("saturation") · SW1 on the FX page | **Fix the name**: with fold or crush chosen, SW3 page 2 turns *that* effect, but it's still called saturation. Call it "Colour" (and the display: "Colour: fold"). |
| 3 | Effects **before / after the looper** | TAPE's page KEY_21 / KEY_22 taps · FX page F#3 / G#3 | Keep both (TAPE parity), or drop F#3 / G#3 to free two FX keys for later. |
| 4 | **Delay time** | Synth page SW3 page 2 · FX page hold SW4 + turn (with room size) · SW1 on effects-only sounds | Synth page's SW3 page 2 looks out of place (a sound-design page turning an effect). Suggest: synth page SW3 page 2 → LFO to filter, or osc 2 interval fine-tune. |
| 5 | **LFO rate** | SW1 page 3 when playing · synth page SW2 page 2 | Fine (the synth page gathers the synth's controls); no change. |
| 6 | **Osc 2 detune** | SW2 page 4 when playing (synth sounds) · synth page SW1 page 1 | Same as 5; no change. |
| 7 | **Harmony on / off** | SW1 + SW2 combo · press SW4 on the harmony page | A shortcut and a setting; no change. |

## Same name, different controls (confusing)

| # | Names | What they are | Recommendation |
| --- | --- | --- | --- |
| 8 | **Tremolo** | Synth page SW1 page 2 (the LFO's amplitude depth, per voice) · FX page A4 (the Motion module's tremolo on the whole mix) | Rename the synth page one "LFO → volume" (display and docs). |
| 9 | **Warble** | TAPE's Texture amount (parameter Warble) also drives broken and squash on the FX page | Call it "Texture" in docs and the display, as the FX page does. |
| 10 | **Resonance** | SW4 page 3 · SW5 also raises resonance as it closes (0.17.1) | Fine; the manual says so. No change. |

## Smallest useful change set (if the owner agrees)

Names only, no behaviour change, so no hardware retest beyond a look at the display: 2 (Colour), 8 (LFO →
volume), 9 (Texture), 1 (Space naming). Optional: 4 (synth page SW3 page 2), 3 (free F#3 / G#3).
