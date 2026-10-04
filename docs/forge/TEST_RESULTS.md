# Forge test results

## 2026-10-04 — first hardware session (DC), firmware 0.6 development (`131776b`)

Setup: CHOMPI over USB, Forge bridge (port 8766), Behringer UMC204HD. Source:
DC's session export and reports `20261004-161209`, `20261004-161342`.

Measured by the bridge on the hardware (automatic checks):
- Identity: firmware 0.6 (1.3). Flash via SD worked.
- Pitch: C3 130.76 Hz, C4 261.87 Hz, C5 523.57 Hz over MIDI; keybed C4
  261.99 Hz (3.1, 3.1k). Warm Pad plays with a 4 s tail (3.10).
- **CPU (status, real device):** 6.2b synth worst case peak **36.9 %**;
  6.2c seven sampler voices peak **54.5 %**; no drops. 6.2d (7 voices +
  overdub) not run; with this headroom the looper voice cap (6) may go to 7.
- Looper states via virtual keys: first take, overdub, pause, clear all as
  designed (3.42s–3.45) once started from an empty looper.
- Knob pages: page counts follow every virtual press exactly
  ([4,2,2,1] → [1,2,2,1] → [4,2,2,1]); the failures were the plan assuming
  page 1 at the start. Same for the menu page (3.17/3.30) and an existing loop.
  Fixed in the bridge (`ensure` actions, `7f04996`).

Not valid yet (setup):
- Line in: nothing reached CHOMPI's line input (detection picked a monitor
  speaker heard by CHOMPI's mic); 2.1, 2.2, 2.4, 3.34, 3.42 unmeasured.
  Detection now requires the line-jack flag (`7f04996`).
- Only CHOMPI's left output reached the interface: IN 2 carried the same
  signal 27 dB lower (correlation 1.0, crosstalk).
- Interface IN 1 noise ~−55 dB RMS with a 144 Hz series (144, 432, 576,
  720, 864, 1296, 1440 Hz) — likely the 144 Hz monitor / ground loop, not
  CHOMPI (IN 2 floor −89 dB). 0.n and 4.1 "silent" fail on it; panic itself
  dropped the pad from −7 dB to the floor (4.1 "after" end 0.19 s).
- 3.4: the 5-note chord clipped the interface input (66 samples at full scale
  in the first 50 ms); the 4 "clicks" are those clips. Re-run with less gain.
- 3.29: no audible kit sample (−40 dB, the floor); likely no kit-a files on
  the card. The check now needs the file and a −30 dB level.

Hardware observations by DC (not automatic): none recorded yet.
