# Forge test results

## 2026-10-04 evening — first 0.7 session (DC), development firmware (`b36ba99`)

Source: DC's reports `20261004-195501` (still 0.6: 1.3 failed, "CPU peak since
boot" marked), `20261004-195751` (0.7, no audio interface: 13 pass, 0 fail,
14 skipped) and `20261004-195913` (0.7 with audio: 18 pass, 4 fail, 5 skipped).
Not received yet: Check setup, panel walk, USB install/upload.

On the hardware, 0.7:
- Identity 0.7 (1.3). Menu, samples page, sample list, looper states, knob
  pages and v5 knobs pass as on 0.6. Pitch C3 130.76 / C4 261.87 / C5 523.57 Hz,
  keybed C4 261.99 Hz; Warm Pad 4 s tail.
- **Per-step CPU works:** 6.2b synth worst case **39.6 %** peak in that step,
  no drops, no clicks. 6.2c read 24.5 %, but its capture was silent (no
  recording: line in unplugged, 3.34 skipped), so it is not a sampler load;
  6.2c now also requires sound.
- Panic: the pad stopped within 0.11 s (4.1 "after" end 0.109 s), then the floor.

Rig (all four failures):
- 0.n, 4.1: IN 1 floor −58.6 dB RMS against the −60 dB "silent" bar. The 144 Hz
  hum is down (−76 dB, was −72.5); left: a 3.2 kHz tone with ±25 Hz sidebands
  (and 1/2/6.4 kHz), −66 dB, already present at the same level on 0.6
  (`20261004-163535`), so not from 0.7. IN 2 is clean (−95 dB).
- 3.4: the chord still clips IN 1 (18 samples at full scale; the reported
  click at 0.035 s is the clipping). IN 1 gain still too high.
- IN 2 still carries only crosstalk (−26 dB below IN 1, correlation 1.0):
  the right output is not reaching IN 2.
- 3.29: no kit-a file on the card (sample list), so no sample sound.
- Line in unplugged: 2.1, 2.2, 2.4, 3.34, 3.42 skipped.

Later the same evening, after the rig fixes (reports `20261004-202421` …
`20261004-203526`, ten runs while DC adjusted gain and cables): the last two
full runs are **21 pass, 1 fail, 5 skipped**.
- Rig fixed: right output now on IN 2 (L/R within 1 dB, pad correlation
  0.78), no clipping (3.4 chord peak −9.4 dB), floor −67 dB RMS (0.n, 4.1 pass;
  panic leaves −64 dB). Notes peak about −20 dB.
- 6.2b synth worst case **36.9 % / 37.1 %** CPU in the step, no drops or clicks.
  6.2c still silent (no recording; that exe predates the 6.2c sound check), so
  its 36 % is not a sampler figure.
- Only failure: 3.29, no kit-a file on the card. Line in still unplugged.
- Check setup (DC's paste): noise IN 1 −68.4 / IN 2 −71.2 dB RMS, outputs
  L −20.4 / R −21.4 dB, loud chord −7.7 dB: all OK; line input FAIL (no plug).

Panel walk on 0.7 (DC's paste), first time on hardware:
- **Every key passes:** toggle up/down, white keys 1–15 (switches 15, 8–11,
  16–20, 24–28), black keys 16–25 (7, 12–14, 21–23, 29–31), CHOMPI 5, PLAY 33,
  LOOP 34, knob presses SW4 3, SW1 0, SW2 1, SW3 2, SW6 32. SW5 press = panic.
  Line jack removed/inserted detected. So the key map from TAPE is right.
- Knob turns: SW4, SW1, SW2, SW6 both ways (right +2, left −2). Failed: SW3
  right ("encoder 1 (SW2) moved"), SW5 right and left ("encoder 2 (SW3)
  moved"); SW3 left counted +2 and passed only because the walk did not hold it
  to the other knobs' direction. Not conclusive: the walk blamed the
  lowest-numbered encoder that moved at least 2 counts, not the one that moved
  most, and SW5 is on its own GPIO pins (D0/D20), so it cannot electrically
  move SW3's shift-register counter. Walk fixed (most-moved knob, all deltas
  in the detail, left checked against the other knobs); re-check with
  *Knobs only*.
- Lights: SW4 red, SW1 green, SW2 blue, LOOP red, menu keys all as expected,
  so the knob-light map from TAPE is right. SW3 page 1 "dim white" (12 % on
  all three colours) looked **teal**: at low levels the red LED is weakest, a
  real colour-balance finding. CHOMPI "dim blue" answered Blue (now
  accepted). PLAY answered **red** during a first take (expected teal; Forge
  drives through-hole LED 7 as TAPE's `led_map` for KEY_27): unexplained,
  re-check with *Light questions only* and a photo.
- Re-check (DC, verbal, no paste): knobs and lights all passed; DC thinks the
  earlier knob failures were extra clicks carried into the next question. So
  all six knobs turn both ways and the lights match, by DC's report; SW3's
  dim white still reads teal (cosmetic).

## 2026-10-04 — first hardware session (DC), firmware 0.6 development (`131776b`)

Setup: CHOMPI over USB, Forge bridge (port 8766), Behringer UMC204HD. Source:
DC's session export and reports `20261004-161209`, `20261004-161342`.

Measured by the bridge on the hardware (automatic checks):
- Identity: firmware 0.6 (1.3). Flash via SD worked.
- Pitch: C3 130.76 Hz, C4 261.87 Hz, C5 523.57 Hz over MIDI; keybed C4
  261.99 Hz (3.1, 3.1k). Warm Pad plays with a 4 s tail (3.10).
- **CPU (status, real device):** the status reports the callback's peak
  load **since boot**, so it is an upper bound for each step, not a per-step
  figure. After 6.2b (synth worst case) 36.9 % / 37.6 %; after 6.2c (seven
  sampler voices) 54.5 % in the first run and 37.6 % in the third; no drops.
  The 54.5 % may come from anything earlier in that boot. 6.2d (7 voices +
  overdub) not run. From the 0.7 firmware on, the automatic checks reset the peak at the
  start of each step, so later runs give per-step figures.
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

### Third run (`20261004-163535`, bridge update `fbd98c1`)

17 pass, 5 fail, 5 skipped. The state-setting checks now pass: menu
(3.17, 3.30), looper (3.42s–3.45), knob pages and v5 knobs (3.52, 3.53, 3.55).
Line-in steps are skipped with the reason (no plug). Still failing:
- 0.n, 4.1: IN 1 floor −57 dB RMS (setup, as above).
- 3.4: interface clipping again (24 samples at full scale; gain unchanged).
- 3.29: the sample list shows no kit-a file, so no sample sound.
- 3.1k: the capture holds only the C5 tail from 3.1; the virtual KEY_8 did
  not sound this time (it did in run 2). Cause not established: a panel state
  left from hand use (e.g. the menu) is likely. The step now starts with the
  toggle forced down (which closes the menu) and waits for the tail.
