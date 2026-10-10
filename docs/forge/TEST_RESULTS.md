# Forge test results

## 2026-10-11 — automatic run on the SD-fix build (DC away, webcam + UMC)

- Rig trouble first: UMC MIX at "IN" closed a feedback loop (392 / 175 Hz howl through the
  dry patch and CHOMPI's delay); MIX fully to playback fixed it. Then PHONES (line-in feed)
  and the MIDAS gains were ~25 / ~9 dB hotter than on 2026-10-10 and clipped; after DC
  turned them down captures sat near -8 dB again.
- Result: 28/34 on the first pass, then 2.1, 3.4, 3.34 pass at proper levels. 4.1 panic
  works (-3 → -58 dB) but trips its silence threshold on the 2 kHz whine (DC: ignore the
  whine; it is inaudible). 0.n fails on the same whine. 3.42: the loop played back at
  2 kHz from a 1 kHz tone because the looper speed was left at -2.0 (reverse, 2×) from
  DC's playing; recording and playback themselves are fine.
- Done the same night (host only): 3.42 clicks SW5 (speed 1×) first; "silent" sets aside one
  steady narrow tone quieter than -45 dB (`forge_audio.without_steady_tone`, test added).
  Rerun on CHOMPI: 0.n, 3.42, 4.1 pass, so **all 34 automatic steps pass on the hardware**. DC questions: should a new first take start at 1×? Should SW5's
  cutoff also filter the loop (loop only, or a master filter after the looper)?
- Webcam framing fixed (whole panel visible, right way up).
- **Harmony (agent, USB MIDI + UMC audio + video `reports/cam/harmony_check.mp4`):** v7 patch,
  C major triads, static layout, voice leading off. All seven white-key chords identified
  correctly from the audio (C, Dm, Em, F, G, Am, B°); Shift (C5) alone silent; Shift + D =
  D major (V/V triad). **Bug:** after releasing C5, D still played D major: Shift stayed on
  because C5's release never reached the harmony player (no held chord, so `Holds` was
  false). Fixed in `core/harmony.h` (`Holds` reports the Static Shift key while Shift is
  on); engine-level test added (fails on the old code); not yet installed on CHOMPI.
- **Event recorder (agent, virtual panel keys + MIDI, plan `D:\tools\chompi-session\recorder_plan.json`,
  video `reports/cam/recorder_check.mp4`): 5/5.** Harmony page (KEY_21 held 1 s) → parts page
  (KEY_21) → F#4 armed; recording started on the next bar while C-E-G-C played over MIDI;
  F#4 closed the loop on the bar → playing; it played back alone (-15.7 dB peak); A#4 twice
  → empty. Covers TEST_SESSION 3.73's flow; the physical keys and lights are still DC's.
- **Demo presets (agent):** new 15 Harmony Pad, 16 Arp Bells, 17 Chord Arp (v7), starter
  slots 13-15; recorded on CHOMPI (`reports/cam/demo_*.mp4`, levels about -20 dB mean, -9 dB
  peak). Musical judgement is DC's.
- **Old presets A/B (agent, `reports/cam/ab_*.mp4`):** the bridge stores 04-06 upgraded v2 → v5
  (v3 per-voice filter). Measured B against A: Glass Keys +13 % brightness / +0.5 dB, Soft Pad
  -4 % / +0.9 dB, **Saw Bass -48 % brightness / +2.1 dB** (the upgraded filter is much darker).
  DC to listen; then retune the v2 → v3 cutoff mapping or Saw Bass itself.
  Measured on CHOMPI (spectral centroid against the v2 original, same notes): Saw Bass needs
  the v3 cutoff ×2.5 (×2 -14 %, ×3 +11 %); Soft Pad ×1.6 (×1 -10 %, ×2.5 +13 %); a sine has
  nothing to compensate. Proposal for `upgrade_patch` (not applied, DC's ear decides): v2 →
  v3 cutoff ×1 sine, ×1.6 triangle, ×2.5 saw / square (the one-pole 6 dB/oct becomes a
  12 dB/oct SVF).
- **Bridge (found while adding the demos):** after "Load starter presets" the bridge sent back
  the playing sound, but a pre-v7 patch keeps the live harmony / parts, so a demo's latched
  arp would have kept running. It now sends a neutral v7 (harmony and parts off) first.
- **Arp / bass (agent, CHOMPI's own MIDI out + video `reports/cam/arp_check.mp4`):** 120 BPM,
  1/8: up C E G…, updown C E G E…, down cycles G E C, steps 0.250 s; latch keeps playing
  after release (8 notes); bass alternate 1/4 on channel 2: C2 G2 C2 G2. **Small glitch:**
  the first steps of a phrase see a half-built chord (keys arrive a few ms apart), so
  "down" started E, E (71 ms) before settling. Fixed in code: a 15 ms chord-gathering
  window (`core/parts.h`, test `ChordGathering`); not yet installed on CHOMPI.
- **Rapid-key soak (agent, `D:\tools\chompi-session\soak.py`, log
  `reports/soak-20261011-015848.jsonl`): pass.** 20 min on the development build (c93357b +
  uncommitted changes), virtual panel key presses (same code path and key lights as real
  presses) at ~12 events/s, 1-3 keys at once, patch changed every 45 s through 07 / 16 / 09 /
  17 / 08 / 15 (pads, latched arp, harmony, bass), status every 5 s: 24,137 presses, USB
  never lost, no restart or safe mode, 0 dropped / 0 rejected messages, CPU peak 62.6 %,
  battery high and charging throughout (weak PC USB supply flagged, as before). The 0.9
  "random shut-off while playing rapidly" did not reproduce on this build. Not covered: the
  physical key matrix and the battery-only case (CHOMPI was on USB power), so DC's 0.9 report
  stays open until it is played unplugged.

## 2026-10-10 (late) — start-up stall found and fixed on CHOMPI

- DC: dim blue light for about a minute at every start; once the card did not mount;
  sound "gone" because a menu-selected sample slot never loaded (storage error 8).
- Agent, development builds with start-up step timing (Inspector "card" events): card
  mount 11 ms, preset scan 53 ms, `options.json` read **30,003 ms** (libDaisy's SD timeout).
  Cause: FatFs `FIL` objects on the DTCM stack (see CHANGELOG 0.15.2). With them static
  the next build hung at start with DC's card: the earlier failed writes had left 352 KB of
  lost clusters and three empty files; without the card it started in 2 s. After
  `chkdsk /F` and deleting the empty files (card otherwise byte-identical to the
  afternoon backup): **Forge answers 3 s after power-on, card mounted, no storage errors.**
- Five development builds were installed over USB tonight (DC pressing CHOMPI each time);
  `forge_card.py install` prints a harmless traceback when CHOMPI restarts under it.
- Still open from DC: no visible colour feedback while turning knobs; SW3's page 3 (DJ
  filter) can silence the sound ("kind of" restored by the long-press reset).
- Guided pass with a webcam (Logitech BRIO over the panel, frames in `reports/cam/`):
  at rest the knob rings show the predicted page-1 colours for Warm Pad (SW4 green, SW1
  orange, SW2 red, SW3 blue, SW6 green). Turning SW1 changes the attack (900 → 960 ms
  per click, USB log) but in the frames its ring stayed yellow; not conclusive yet (the
  USB log hung during the capture). Next session: repeat in sync ("go"), camera + log.
- During that capture the looper recorded an 18.4 s take and played it (LOOP and PLAY
  lit red / teal). Not yet known whether DC pressed LOOP / PLAY; if not, a phantom press.
- Earlier the menu opened and loaded a sampler slot during the knob test (menu left open
  until CHOMPI was pressed?); keys then picked slots instead of playing. Unexplained.
- Tools: `scratchpad` watchers (USB health, UMC audio) and `gp.py` snapshots worked;
  the USB watcher can hang after CHOMPI is unplugged and replugged (restart it).

## 2026-10-10 — 0.15.2 on CHOMPI: first install since 0.9, unattended automatic run

Build: development firmware, "Forge 0.15.2 (build c93357b, development, uncommitted
changes)", SHA-256 2b8e2b0d…, installed by DC from the SD card (card backed up first to
`D:\CHOMPI-card-backup\2026-10-10`; the card had never run Forge: no `FORGE/` folder).
Rig: UMC204HD, CHOMPI main out → IN 1-2, UMC headphone out → CHOMPI line in; USB-C to
the PC. Evidence below is **agent-measured over USB** (reports in
`firmware/chompi-forge/reports/`, not committed) unless marked DC.

- **Boots, right version (1.3):** yes. Status reports 0.15.2, build c93357b, development,
  dirty. DC: all lights white at first, other lights after pressing keys.
- **USB at start-up:** for the first minute or so Windows showed "Unknown USB Device
  (Device Descriptor Request Failed)" and the first status replies came out only when
  key presses pushed them; afterwards replies arrive in ~10 ms every time. Open: possibly
  the USB / charger hand-over settling (core/power.h `ChargerUsb`). Watch at every start.
- **Battery checker (0.15.2):** works: full, charged, USB power, weak supply (PC port),
  install allowed. **Last start** reset flags: power-on, brown-out, reset pin, software;
  no crash. Brown-out at a cold start is common on the H7; note it for the shut-off question.
- **CPU (first device figures):** idle on the dry patch 14.8 % average, 18.7 % peak;
  6.2b synth stress (four notes, 20 s) peak 40.8 / 44.8 / 44.4 % over three runs; 6.2c
  seven sampler voices 55.0 %; 6.2e sampler + saturation + DJ filter 61.4 % (limit 70).
  No dropped messages in any step.
- **Automatic run:** 32 of 34 pass on the first run (`20261010-205826`); after fixes and
  reruns all 34 pass. The two first-run failures:
  - 2.1 (dry line in, both channels): rig selection, not firmware. The detector chose
    "IN 2" alone (1 dB louder than "IN 1-2"), so channel 2 was never captured. Fixed in
    the host (`forge_audio.detect` prefers a stereo pair; test added); rerun passes,
    -26.1 / -25.2 dB, 1 kHz on both channels.
  - 6.2b (synth CPU stress): CPU fine, but three clicks once (3.36–3.44 s; waveform jumps
    up to 0.8 of full scale, not dropouts, all at the same offset in a 24-sample block,
    which also fits the PC's 1 ms USB audio frames). Two reruns: no clicks. **Open:**
    not reproduced; could be CHOMPI or the PC's USB audio. Repeat 6.2b at later sessions.
- **Setup check:** after DC lowered the input gains ~10 dB: noise -60.5 dB, loud chord
  -6.4 dB, line in OK (first check: gain clipped, input 1 noise -53.9 dB at 2 kHz).
- **Not covered (needs a person):** physical keys, knobs, toggle, how the lights look,
  speaker, headphones, mic, SD swap, power-off and battery shut-off, and everything judged
  by ear. These automatic results are evidence for their steps, not a TEST_SESSION pass.

## 2026-10-05 — DC's panel-map walk-through on 0.9 (verbal)

- **Works:** SW5 filter cutoff (clearly audible on Acid Bass); SW6 volume; webapp
  preset loading; saving to device presets (bank 1 slots 1, 2, 5); preset recall.
- **SW6 press does nothing.** TAPE: a short press switches SW6 between output
  volume and **input gain** (mic/line, light blue → red; 75 % at power-on); holding
  2 s still shows the battery. Forge has no input gain control. To add (TAPE).
- **SW5 while a loop exists:** DC wants SW5 to stay on filter cutoff and get loop
  speed/scrub with a modifier (CHOMPI held or SW5 held were suggested; SW5 press
  is panic now). Note: in TAPE SW5 is only the looper transport (speed/scrub, press
  resets speed); TAPE's filter is a knob page. Decision pending.
- **Direction from DC:** keep TAPE's workflow wherever possible (familiar to DC and
  to CHOMPI owners).
- Not tested yet: the AI patch author; device samples on the SD card (DC did not
  find how; the manual needs a clearer how-to). Webapp text still says "toggle
  down + hold CHOMPI" for recording and its footer says candidate 0.5 (stale).
- Automated tests: add audio-based knob checks (planned below).

## 2026-10-05 — 0.9 on CHOMPI (DC, verbal): open issue, random shut-off

- **Recording works** on hardware (toggle + hold CHOMPI, play back).
- **Preset recall works** on hardware (toggle down, hold CHOMPI, white key, release).
- **Toggle labels are backwards in the docs and bridge:** on DC's unit the menu
  position is toggle **down** and recording is **up**. Firmware behaviour equals
  TAPE (`GetToggleState()` true = menu); only Forge's naming ("up = menu") was
  wrong. Fix pending DC's findings (docs, panel map, walk prompts, check titles).
- **OPEN: random shut-off while playing keys** (firmware 0.9). Not reproduced
  yet; not understood. To capture next time: on battery or USB; did it restart
  by itself (lights come back) or stay off until the switch; amber flashing
  before it (the stock low-battery shutdown); SW6 battery colour just before and
  after; patch (synth/sampler); how many keys held; looper running; how long
  after power-on. Candidates to rule out: low-battery shutdown (the battery ran
  flat once on 0.7; 0.9's key lights draw more LED current, so a weak battery
  could dip lower), a brown-out reset, or a firmware fault (Forge does not yet
  record the reset cause, so a crash and a power loss look the same).
- **Knob/LED feedback (DC):** "white" knob lights look light blue. TAPE drives
  white the same way (full R, G, B, no correction), so it is the LEDs; a white
  balance trim is a candidate for the next firmware. **SW4 on Glass Keys: no
  audible change.** In the simulation SW4 (page 1, dim white) does change delay mix,
  but by 1/127 per click (20 clicks: 0.30 → 0.46), and Glass Keys' echo is quiet,
  so a few clicks are inaudible. Also found: on v1/v2 patches (Glass Keys is v2)
  the newer page 2–4 controls (resonance, LFO, filter envelope, reverb…) are
  refused, so those pages do nothing there. Candidates: encoder acceleration,
  v1/v2 page fallbacks. Requested for later: **long-press a knob to reset** its
  control to the patch's value.
- **All knobs, simulation sweep (2026-10-05, for later):** every knob and page on
  the 12 starter presets, 20 clicks each way. Every knob moves 1/127 of its range
  per click, so the "small steps" issue is the same for all of them (and SW5/SW6).
  Pages that change nothing, by patch version:
  - v1 (Dry, Slap, Long echoes; effects only, no synth): SW4 p2–p4, SW1 p2–p4,
    SW2 p2–p4, SW3 p4.
  - v2 (Glass Keys, Soft Pad, Saw Bass): SW4 p3–p4 (resonance, filter envelope),
    SW2 p2–p4 (LFO, detune), SW3 p4 (reverb).
  - v3, v4, v5 (Warm Pad, Acid Bass, Bell Keys, Recorded Keys, TAPE Kit A, Knob
    Pad): none dead.
  Options for later: upgrade the starter presets to v3+ (same sound, all pages
  live), or on v1/v2 patches skip dead pages; plus encoder acceleration.
  **Tests to add with those fixes (DC: check the audio, not just the values):**
  (1) a native test that renders each starter preset through the engine, turns
  every knob/page a fixed number of clicks and fails unless the output changes by
  a measurable amount (level, spectrum or echo energy) — runs in CI, catches dead
  or inaudible pages; (2) an automatic hardware check per knob: hold a note,
  capture, turn N clicks, capture, compare (same measures), so CHOMPI's real knob
  path and audio are covered. Today's checks only confirm the parameter value
  changed.
- Requested (to build after DC's findings, one install): 1–2 s record count-in
  with blinking lights after pressing CHOMPI.

## 2026-10-05 — power on 0.8 (DC, verbal): TEST_SESSION 8

- 8.1 pass: unplugged, Forge runs normally on the battery.
- 8.2 pass: SW6 held 2 s shows the battery light, **green** (battery above the
  3.3 V check, consistent with the Power line seen earlier).
- 8.3 pass: plugged back in, the bridge reconnects. (Charge state on the Check
  setup Power line not reported in this run.)
- The hardware power switch turns CHOMPI on and off normally. The flat battery on
  2026-10-04 was simply the battery running out, not firmware.
- 8.4 (storage gesture) not run — optional.

## 2026-10-05 — firmware 0.8 installed over USB: all automatic checks pass

- **USB firmware install verified on hardware:** DC installed 0.8 with the bridge's
  *Card & firmware → Install* (CHOMPI-key confirmation, restart, bootloader flash);
  1.3 then reported firmware 0.8 (report `20261005-113455`). First card-free update.
- Report `20261005-113455`: 25 pass, 2 fail (0.n, 4.1 noise floor: a loose cable,
  DC fixed it); re-run of those two (`20261005-113653`): both pass. With
  `20261005-113009` (everything else on the same rig), **every automatic check passes
  on 0.8**. CPU per step on 0.8: synth 38.9 %, seven sampler voices 54.8 %, no drops.
- Still open on hardware: TEST_SESSION 8 (battery, SW6 light, plug-in hand-over,
  storage gesture), 6.2d (sampler + looper CPU), the QA-sheet listening checks.

## 2026-10-05 — line in wired, 0.7 on CHOMPI, 0.8 bridge

DC rewired: CHOMPI main out → interface inputs 1/2, interface output → CHOMPI
line in. Report `20261005-113009`: **26 pass, 1 fail, 0 skipped**; the failure is
1.3 (the 0.8 bridge expects firmware 0.8; CHOMPI still ran 0.7).
- First hardware passes of the line-in steps: 2.1 dry tone 1,000 Hz both channels
  (−24.9/−27.0 dB), 2.2 slap echo, 2.4 bypass, 3.34 recording 440 Hz played back
  at 440.01 Hz, 3.42 line-in loop repeats without a click; 3.29 kit sample plays.
- **CPU per step:** 6.2b synth worst case 37.5 %; **6.2c seven sampler voices on a
  real recording 54.6 %** (the first valid sampler figure; under the 70 % comfort
  target); no drops anywhere.
- Floor −70.4 dB RMS; panic leaves −82 dB; no clipping; no clicks except two in
  the 3.34 recording capture (the take's edges; the check passed).
- Not run: 6.2d (sampler + looper), so the looper voice cap stays at 6.
Next: install 0.8 over USB (first real USB install), re-run 1.3, TEST_SESSION 8.

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
