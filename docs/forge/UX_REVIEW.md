# Forge UX review (2026-10-11)

Ease of use per major function, 1–5 (5 = obvious without the manual). DC's bar: **every
function at 4 or more**. "Today" is the panel as built (0.15.2, from the manual, the code and
the 2026-10-10/11 hardware sessions). "Target" assumes the listed fixes. The panel must reach
4 on its own: the virtual screen (webapp / Tab5) helps, but CHOMPI is often played without a
computer.

## Cross-cutting fixes (they lift most rows)

1. **Page colour on the CHOMPI key** (DC approved). While the menu is open, the CHOMPI key
   shows the page: teal TAPE, blue presets, purple harmony, orange parts.
2. **Colour per menu item while turning** (DC approved). Each stepped setting (mode, chord
   size, arp octaves, bass rate…) has its own knob colour per value, and a page change
   flashes the new page's colour on all four rings, so you see where you are.
3. **Virtual CHOMPI screen** (DC approved): webapp panel and Tab5 view showing the patch,
   menu page, the control being turned (name + value), chord name, BPM, looper and recorder
   state. Needs a release-build "panel state" message from CHOMPI (today only the
   development Inspector sends events).

## Ratings

| Function | Today | Main problems | Fixes | Target |
| --- | --- | --- | --- | --- |
| Playing / patches | 3 | CHOMPI boots into pass-through: keys sound dry until a preset is recalled | Recall the last preset at boot (option) | 4 |
| Knobs and knob pages | 3 | 3–4 pages per knob, the page is only implied by the ring colour; turning shows no per-setting feedback (2026-10-10 camera test inconclusive) | Cross-cutting 2, 3 | 4 |
| Menu navigation | 2 | Hidden gestures: hold KEY_22 1 s (presets), hold KEY_21 1 s (harmony), tap KEY_21 (parts); nothing shows the page | Cross-cutting 1, 3; label the page keys in the manual's panel diagram | 4 |
| Presets | 3 | Save is 4 steps; no names on CHOMPI or in the webapp library | Names in the bank files, shown on the virtual screen and webapp; last-preset boot | 4 |
| Sampler / recording | 4 | TAPE's proven flow, kept intact | — | 4 |
| Looper | 3 | First take inherits leftover speed; SW5's lights are off during scrub and plain cutoff turns (confirmed in `ComposeTransportLeds`: lit only while playing); SW5's cutoff does not reach the loop | First take at 1×; SW5 lights for scrub and cutoff; SW5 filters the loop (all DC approved) | 4–5 |
| **Harmony** | **2** | Two-level menu entry; mode (9) and chord size (6) have no visible value, only on/off colours; Static layout + Shift + colour chords need the manual (DC: "instructions are kinda weird", "don't know what Shift does"); Shift stuck after release (fixed, not installed) | Cross-cutting 1–3; on the harmony page light each white key by chord quality (major / minor / diminished colours) so the keys show what they play; Shift key lit while held; quick harmony on/off without entering the page | 4 |
| Arp / bass / tempo | 3 | Reached only through the harmony page; tempo has no readout (blink only); the parts page is 20 keys of functions | Direct entry (e.g. hold KEY_20 1 s); BPM on the virtual screen; tempo knob colour bands | 4 |
| Event recorder | 2 | Closing a take needs the menu reopened, three levels deep, then F#4 | Close / play / stop from the panel without the menu (e.g. tap CHOMPI in the menu position while recording); keep F#4 as the menu route | 4 |
| Effects | 3 | Usable but thin (one reverb+delay knob, saturation, DJ filter); DJ filter silenced the sound once (open bug) | Fix the DJ-filter bug; the effects round-out (backlog) with a consistent effects page | 4 |
| Power / battery | 4 | SW6 hold shows the battery; bridge and webapp show it too | — | 4 |
| Install / update / safe mode | 4 | One key press over USB; safe mode after crashes; build shown everywhere | — | 4 |
| Webapp (sound designer) | 2 | DC: "too clunky"; one long page, port pickers, Send step | Redesign (mockup v1 published) + virtual CHOMPI screen | 4–5 |

Not rated: Forge Bridge's own pages (not reviewed for UX yet).
