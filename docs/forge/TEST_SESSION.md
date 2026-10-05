# Forge 0.7 — the one consolidated hardware test

Status of the candidate: **software-tested, hardware-unverified.** This is the
single planned physical session. Work top to bottom. If a stage fails, record
it, stop that stage, and continue only where later stages do not depend on it.
Never mark a stage passed that you did not run. Expect roughly 2 hours.
Features were developed first and QA was deferred (DC, 2026-10-02); each
feature has its own steps below so results can be recorded per feature.

## 0. Before you start (no CHOMPI needed)

1. Unzip the bundle. In its folder run `python3 verify_bundle.py`.
   It must print `OK`. It also prints the firmware SHA-256 and the compiler that
   built it — copy both into your results.
2. `python3 -m pip install -r host/requirements.txt`
3. Optional, recommended: the live AI preflight in `docs/LIVE_AI_TEST.md`.
   It needs no hardware.
4. Back up your normal SD card and keep it aside. Use a separate test card.
   Have the stock firmware and the known restore procedure at hand. Exact
   factory card contents are in the repository under `firmware/card-profiles/`
   (TAPE 2.0, TEMPO 1.0, WAVE 1.0). Do **not** install the repository's beta bootloader.
5. **Stock reference (before flashing, still on your stock firmware):** set
   the volume (SW6) to a position you can find again (mark it), play a few
   keys and note how loud the stock sound is on headphones and on the main
   outputs. Steps 3.10–3.13 compare Forge against this. `docs/COMPATIBILITY.md`
   (in the bundle) lists what differs from stock.
6. **Samples for 3D:** copy the `jammi_a*.wav` and `cubbi_a*.wav` files
   (with their `_double` files) from `firmware/card-profiles/tape-2.0/` to the
   test card's root (the same card you put `FORGE.bin` on in 1.1). Optionally add one WAV
   of your own that is mono or 44.1 kHz, renamed `jammi_b1.wav`.

Shorthand below: `H = python3 host/forge_host.py`, with
`--input "IN" --output "OUT"` set to your exact CHOMPI port names from `H ports`.
Keep monitoring volume low. Avoid audio feedback loops. The mic is used only
when recording from it (3D); keep headphones on so the speaker cannot feed back.

## 1. Flash and identity

| # | Do | Pass when |
| --- | --- | --- |
| 1.1 | Put `firmware/FORGE.bin` on the test card root as the **only** `.bin` file. On macOS also remove `._FORGE.bin` (`dot_clean -m /Volumes/CARD` or delete it); the bootloader loads the first `.bin` it finds and would reject that metadata file. Use the installed bootloader's normal SD update | Update completes uninterrupted |
| 1.2 | Power up; watch LED | Initialization completes; no output burst |
| 1.3 | `H status ...` | Firmware 0.7, version 1 aux patch, counters 0 |

## 1A. Inspector development candidate (before the normal audio checks)

The [hardware test bridge](BRIDGE.md) provides this checklist in the browser,
with observations, state evidence and exports. With the development kit,
double-click `Start Forge bridge.cmd` and press Connect CHOMPI (from a clone:
`python host/forge_web.py --open`). With an audio interface wired in, Find
audio interface and Run automatic checks measure 1.3, 2.1–2.4, 3.1, 3.4, 3.10,
3.17, 3.28–3.30, 3.34, 3.42–3.45, 4.1 and 6.2b/c and record the results in the session
export; record those steps from it, and judge by ear what it cannot. Enable test controls only for
explicit actions. It coordinates polling with its own patch/MIDI/storage tests.
Disconnect before running the separate `H` CLI commands below. The terminal
Inspector remains an alternative; do not run both clients together.

Use the development `src/build-dev/FORGE.bin` for this session if inspecting
hardware. Release rejects probe reads; keep the two candidates separate and
record the exact binary hash. [Inspector guide](INSPECTOR.md) explains the
read-only viewer and measured versus unavailable fields. No agent has flashed
or verified it on hardware.

These four checks match the bridge's guided checks 1A.1–1A.4. The bridge
records state and events into its session export by itself. Terminal
alternative: `python host/forge_inspector.py --input "IN" --output "OUT" --watch
--record inspector-session.jsonl`; keep that JSONL through the later sampler,
record, jack and card tests (wanted patch versus loaded files, frame totals,
partial slots, recording source, lock/job state, card flags, errors).

1. At the dry aux startup patch, hold then release physical button 15 (C3).
   Expect physical/logical bit 15, mapped note 48, down/up events whose value
   includes physical bit 1, no override. Aux intentionally starts no voice.
2. Turn logical knob 1 (SW4, encoder index 3) one step each way. Expect both
   hardware and merged counters to move at index 3 and mix to follow. Match the
   commanded LED shadow to actual board positions/colours.
3. Send preset 04 Glass Keys and repeat C3. Expect a panel-owned note-48 voice,
   release, then stop.
4. Compare CPU peak, audio continuity, MIDI RX/TX failures, request/event drops
   and emergency count with polling on versus paused (terminal: `--interval 5`
   on UART if one-second polling causes traffic loss). Check SD load progress
   with polling on/off during the sampler tests.

Record every discrepancy, polling interval, transport, binary hash and CPU
reading. Actual DMA underruns and MIDI framing errors are **unavailable**, not
zero. LED shadow is intent, not an electrical measurement. Do not count offline
injections or simulated file loading as a physical pass.

## 2. External audio path (v1 compatibility)

| # | Do | Pass when |
| --- | --- | --- |
| 2.1 | Aux source in; `H send presets/01-dry.json ...`; play left-only then right-only | Correct L/R on headphones and main outs |
| 2.2 | `H send presets/02-slap.json`, then `03-long-echo.json`, while audio plays | Acknowledged; echoes change; no crash; time change glides in pitch |
| 2.3 | Turn knobs 1–4 (hardware SW4, SW1, SW2, SW3, in panel order) and volume SW6; `H status` | Mix/time/feedback/level move in that knob order; level can mute. Note which physical knob is which |
| 2.4 | `H cc 85 127 --output "OUT"`, then `H cc 85 0` | Wet fades out then back; dry still follows level |
| 2.5 | `H capture saved-aux.json ...`; send another preset; `H send saved-aux.json` | Returns within 14-bit quantization |

## 3. Instrument

| # | Do | Pass when |
| --- | --- | --- |
| 3.1 | Unplug aux source. `H send presets/04-glass-keys.json ...`; play all 25 keys | Chromatic low→high (MIDI 48–72); sound on press, release on key-up, nothing stuck |
| 3.2 | `H note 60 --output "OUT"`; `H note 60 --velocity 30` | Correct pitch; second clearly quieter |
| 3.3 | `H note 60 64 67 --zero-velocity-off` | Chord sounds and releases (velocity-0 note-on = note-off) |
| 3.4 | `H note 60 64 67 71 74 --hold 3` | Four voices max; oldest note (60) stolen without a click; all release |
| 3.5 | Hold a key on CHOMPI; `H note` the same pitch; release the key | MIDI note keeps sounding until its own release |
| 3.6 | Send `05-soft-pad.json`, `06-saw-bass.json`; turn SW5 | Audibly different; SW5 sweeps tone. Steal/retrigger should not click and triangle should be clean high up (both fixed in software). **Record any click, or aliasing on high saw/square notes — don't fix during session** |
| 3.7 | In the webapp, change waveform, ADSR, cutoff; Send | Each change audible as described |
| 3.8 | `H note 60 64 67 --sustain --hold 3`; while it sustains, play a CHOMPI key | Chord rings ~3 s after keys release, stops when pedal lifts; keybed note unaffected by the MIDI pedal |
| 3.9 | `H note 69 --bend 8191 --hold 2`, then `--bend -8192` | Pitch glides up / down two semitones without zipper noise; returns to A afterwards |

## 3B. v3 instrument modules (firmware 0.4)

| # | Do | Pass when |
| --- | --- | --- |
| 3.10 | `H send presets/07-warm-pad.json ...`; hold chords on the keys | Two detuned oscillators (slow beating), filter swells open over ~1 s, gentle vibrato, reverb tail after release. At the marked SW6 position, record loudness vs the stock reference from 0.5 (quieter / similar / louder), on headphones and main outs |
| 3.11 | `H send presets/08-acid-bass.json`; play overlapping notes low on the keys | Monophonic; pitch glides between notes; resonant filter "snap" on each note |
| 3.12 | With Acid Bass: `H cc 1 127 --output "OUT"`, hold a note, then `H cc 1 0` | Mod wheel brings in a filter wobble; at 0 it stops |
| 3.13 | `H send presets/09-bell-keys.json`; play single notes | Bell-like tone (second oscillator a 12th above); reverb tail; slight vibrato only with mod wheel up |
| 3.14 | With a v3 patch: `H cc 71 110`, `H cc 91 110`; `H capture cc-test.json ...` | Resonance and reverb mix audibly increase; captured JSON shows filter.resonance and reverb.mix near 0.87 |
| 3.15 | Webapp: load Soft Pad (v2), **Convert to v3 instrument**, Send | Sounds close to the v2 Soft Pad (filter slightly steeper); no error |
| 3.16 | Send a v2 preset, then a v3 preset, while holding a note | Sound stops cleanly at the format change (like a route change); next note plays |

## 3C. Device presets on the SD card (TAPE-style keys + encoder)

Use the test card from 1.1 (it may be otherwise empty). "Menu position" means the
toggle position in which TAPE's CHOMPI key opens its menu (down on DC's unit; up is the record position).

| # | Do | Pass when |
| --- | --- | --- |
| 3.17 | Toggle down (menu position), hold the CHOMPI key; then hold KEY_22 for 1 s | The menu opens on TAPE's page (0.10): KEY_16/17 chromatic/kit, the record source key lit; after holding KEY_22 1 s, Forge's presets page: bank keys (KEY_16/17) in bank 1's colour, save/copy/erase dim blue/green/red, white keys off (empty card); playing keys makes no sound while held |
| 3.18 | `H send presets/07-warm-pad.json`. Menu: press SAVE (KEY_25), release CHOMPI, press white key 1 (it turns blue), press CHOMPI | Panel LED flashes green; white key 1 now dim (occupied). `FORGE/B1S01.FPR` exists on the card afterwards |
| 3.19 | Send `08-acid-bass.json`, save it to slot 2 the same way. Release CHOMPI | Menu closes; keys play Acid Bass again |
| 3.20 | Hold CHOMPI (menu), press white key 1, release CHOMPI, play | Warm Pad plays; key 1 shows white while the menu is open. Repeat with key 2 → Acid Bass |
| 3.21 | Menu: turn knob 1 (hw SW4) and press KEY_17 / KEY_16 | Bank colour changes on the bank keys; encoder stops at banks 1 and 8, keys wrap. Mix (knob 1's normal job) does not change while the menu is open |
| 3.22 | Menu: COPY (KEY_24), white key 1 (green), KEY_17 to bank 2, white key 5 (blue), CHOMPI | Green flash; bank 2 key 5 occupied. ERASE (KEY_23), key 5, CHOMPI → key 5 empty |
| 3.23 | Send MIDI program change 1 on channel 1 from a keyboard or DAW | Acid Bass (bank 1 slot 2) loads; program 0 → Warm Pad |
| 3.24 | `H slots ...`, `H recall 1 1 ...`, `H store 1 3 ...`, `H erase 1 3 ...` | JSON lists occupied slots; recall returns the patch; store/erase acknowledged |
| 3.25 | Webapp Device presets: Read slots, select bank 1 slot 2, Recall; then Store into slot 4; Erase slot 4 (press twice) | Slot buttons show stored slots; recall loads the sound into the editor; erase asks for a second press |
| 3.26 | Power off and on; open the menu | Slots 1 and 2 still occupied and recall correctly (boot itself still starts in dry aux) |
| 3.27 | Optional: power off, remove the card, power on, open the menu; reinsert the card | White keys red without a card; nothing crashes; within ~1 s of reinserting, slots show again |

## 3D. Sampler (TAPE-style, firmware 0.5)

Card prepared in 0.6. "Menu" = toggle down (the menu position) + CHOMPI key, as in 3.17.

| # | Do | Pass when |
| --- | --- | --- |
| 3.28 | `H samples ...` | JSON lists chromatic a and kit a slots matching the files you copied; `card` true; no recording yet |
| 3.29 | `H send presets/12-tape-kit-a.json`; play the white keys | Each white key plays its TAPE kit sample (one-shots), as on stock TAPE; black keys silent; no clicks at sample ends. Note how long after Send the first key sounds (loading time) |
| 3.30 | Menu (opens on TAPE's page; from Forge's presets page tap KEY_22) | Page key magenta; KEY_17 lit in bank a's colour; occupied kit slots dim/white; KEY_19 (line) or KEY_18 (mic) lit depending on the line-in jack. On this page a KEY_22 tap = effects after the looper, KEY_21 = before (TAPE) |
| 3.31 | Samples page: KEY_16 (chromatic), white key 1; close the menu; play keys across the keybed | `jammi_a1` plays chromatically, KEY_8 (middle C) at original pitch; press KEY_16 again in the menu → bank b (your own WAV if added in 0.6 plays at the right pitch) |
| 3.32 | Turn knobs 1–3 while holding a key | Pitch, start and end change like TAPE's first page; knob 4 changes the delay mix |
| 3.33 | Webapp: Capture to editor (chromatic `jammi_a1` playing), set Start 0.2, End 0.4, Loop on, Crossfade 50 ms, Send, hold a key | The loop repeats without a click or level dip at the loop point; Reverse on → plays backwards |
| 3.34 | **Record (line):** line in plugged, source KEY_19 lit. Toggle up (the record position), hold CHOMPI while playing audio into line in for ~6 s, release | CHOMPI and the white keys blink red three times (1.5 s count-in; letting go earlier records nothing), then CHOMPI is red while recording; the input is heard in the headphones in the record position (TAPE); on release the keys play the recording chromatically at once, normalised (similar loudness to the kit) and without clicks at its start/end |
| 3.35 | **Record (mic):** unplug line in (source switches to the mic), record a few words, release | Plays back; record the level (too quiet / ok / distorted) and any hum |
| 3.36 | **Resample:** menu, KEY_20; send `07-warm-pad.json` (oscillators), toggle up (record position), hold CHOMPI through the count-in while playing a chord, release | The recording is the instrument's own output and plays chromatically |
| 3.37 | Menu, Samples page: KEY_25 (save), KEY_17 kit, white key 9, CHOMPI | CHOMPI LED blinks pink, then green flash; key 9 of kit bank a now occupied. `cubbi_a9.wav` on the card afterwards |
| 3.38 | Put the card in a computer: open `cubbi_a9.wav` | Plays in any audio app (16-bit stereo 48 kHz) |
| 3.39 | Menu: KEY_24 copy kit a9 → chromatic c2 (KEY_16, bank c, key 2), CHOMPI; then KEY_23 erase kit a9, CHOMPI | Copy and erase confirmed (green flashes); `H samples` agrees |
| 3.40 | `H sample-save chromatic d 1 ...`, `H sample-copy chromatic d 1 kit e 14 ...`, `H sample-erase chromatic d 1 ...` | Each acknowledged; webapp Device samples shows the same slots after Read samples |
| 3.41 | Optional, **TAPE compatibility:** put stock TAPE (`firmware/card-profiles/tape-2.0` .bin) on this card, boot | TAPE plays the samples Forge saved (after regenerating their `_double` files at boot); Forge's `FORGE/` folder does not disturb it. Then restore `FORGE.bin` |

## 3E. Looper (TAPE-style, roadmap item 5)

KEY_28 = LOOP, KEY_27 = PLAY (the two keys TAPE uses); their LEDs are the big
key lights. The loop records what you hear (effects before the loop). Up to
~83 s. Nothing is written to the card except in 3.49.

| # | Do | Pass when |
| --- | --- | --- |
| 3.42 | Send `07-warm-pad.json`. Tap LOOP, play a phrase for a few seconds, tap PLAY | PLAY LED teal while recording, LOOP LED red; after PLAY the phrase repeats seamlessly (no click or gap at the loop point) while you can play over it |
| 3.43 | Tap LOOP (overdub), play another phrase for one pass, tap LOOP again | LOOP LED yellow while overdubbing; both phrases play back; tap LOOP during the first take of a new loop instead of PLAY: it goes straight into overdub (TAPE) |
| 3.44 | Tap PLAY (pause), tap PLAY (resume); pause again and hold PLAY 2 s, then tap PLAY | Pause and resume fade without clicks; after the 2 s hold the loop restarts from its beginning |
| 3.45 | Hold PLAY + LOOP 2 s | The loop fades out and is gone (LEDs dark); SW5 turns the cutoff again |
| 3.46 | With no loop: press PLAY + LOOP together, release, then play a key | LOOP LED blinks red (armed); recording starts with the first note |
| 3.47 | With a loop playing: turn SW5, press SW5; pause and turn SW5 | Loop pitch/speed follows (reverse below zero), press = back to normal; paused, turning scrubs (tape-like); SW5 never changes the cutoff while a loop exists |
| 3.48 | Menu (toggle down + CHOMPI): PLAY / LOOP a few times; KEY_20 then KEY_21 | Overdub feedback down / up (older layers fade faster or stay); KEY_20 = effects after the loop (the delay/reverb applies to the loop too), KEY_21 = before (default); the lit key shows which |
| 3.49 | Menu, Samples page: KEY_24 (copy), LOOP, a white key, CHOMPI | CHOMPI LED pink, then green; the loop is now that sample slot and plays on the keys; while saving, LOOP cannot overdub |
| 3.50 | `H cc 27 127`, `H cc 27 0`, `H cc 26 127`, `H cc 26 0`, `H cc 24 100` | CC 27 = LOOP, CC 26 = PLAY (as TAPE); CC 24 changes the loop speed while a loop exists |
| 3.51 | Panic (`H panic`, SW4 + SW3 held together 1 s, CC 120) while a loop plays; then switch presets | Panic stops the loop at once (it stays, PLAY resumes it); a preset change does not stop the loop; the two knobs do not change page |

## 3F. Knob pages and patch knobs (firmware 0.6)

Each of the four knobs (SW4, SW1, SW2, SW3 = knobs 1–4) has four pages.
**Press a knob** to step its page; the light at that knob shows which:
page 1 dim white (the patch's own knob jobs), 2 red, 3 green, 4 blue. Pages
belong to the panel (they stay when you change presets). See
`docs/forge/KNOBS.md` (in the repository) for the full map. The knob lights are
TAPE's through-hole LEDs 1–4; if a light shows up at a different knob, write
down which one lit — that mapping is not yet hardware-verified.

| # | Do | Pass when |
| --- | --- | --- |
| 3.52 | Send `07-warm-pad.json`, toggle in the menu position (down). Look at the knob lights; turn each knob while holding a chord; then press and release SW4 once and turn it | Lights in TAPE's colours (SW4 green-yellow at 1x pitch, SW1 yellow-orange, SW2 orange-red, SW3 teal-blue); SW4 = pitch (TAPE: centre stops, below plays backwards on samples), SW1 = attack, SW2 = release, SW3 = reverb + delay together; one click is clearly audible (TAPE's step: 3 % on SW1-SW3, pitch finer). After the release SW4 = voice gain (blue to pink to red) |
| 3.53 | Step each knob through its pages (press and release): SW4 pitch, gain, resonance (red), filter envelope (green); SW1 attack, decay, LFO speed (red); SW2 release, sustain, LFO filter (red), detune (green); SW3 reverb + delay, saturation (yellow to red), DJ filter (purple to pink to white: left of centre low-pass, right high-pass) | Each page audibly does its job; the page changes when the knob is released (as TAPE); pages are per knob and wrap back to page 1 |
| 3.54 | Hold a knob 1.5 s without turning; hold SW4 + SW3 together 1 s while a note plays; flip the toggle up (record position); short-press SW6 and turn it, then short-press again | The held knob's light flashes white and its control goes back to the preset's value (no page change); SW4 + SW3 stops all sound; in the record position the knob lights go out, PLAY/LOOP dim and CHOMPI's light shows the input level; SW6's light turns blue to red for input gain (TAPE), then back to volume |
| 3.55 | Send `14-knob-pad.json` (a v5 patch). Step SW4 to page 5, SW1 to page 4, SW2 to page 5, SW3 to page 4 (each knob's last page, dim white light); turn them; then `H cc 20 0` and `H cc 20 127` | SW4 = cutoff, SW1 = LFO speed, SW2 = reverb size, SW3 = reverb mix (the patch chose them, on the last page); CC 20 moves the cutoff the same way |
| 3.56 | Send `12-tape-kit-a.json` and play: SW1 page 1 = sample start, SW2 page 1 = sample end, SW2 page 4 = loop crossfade. Send `02-slap.json` (effects only) and turn the knobs while playing into line in. Webapp: open any preset, **Convert to v5**, choose a job for each knob under *Panel knobs*, Send | Sampler: start/end move in fine steps (TAPE), crossfade changes on looped samples only, no crash; effects-only: SW4 = delay mix (page 2 level), SW1 = delay time, SW2 = feedback, SW3 = TAPE's effects on the line input; the webapp patch's knobs are on each knob's last page |
| 3.57 | **Automatic (bridge):** SW4 pitch, SW1 attack, SW2 release, SW3 reverb + delay and DJ filter, SW5 cutoff: a held note recorded before and after 10 clicks | Each pair of recordings differs by at least 5 % (spectrum, envelope, loudness or stereo balance); the same check runs on every page of every starter preset in the simulation (`tests/knob_audio_test.cpp`) |
| 3.58 | **TAPE's menu knobs:** sampler patch, open the menu (TAPE's page) and hold CHOMPI. SW4: turn (pitch in fifths/octaves, 4 clicks a step), press (1×); SW1/SW2: turn (moves the start-end window), press (SW1 auto-loop, SW2 sustain on/off: their lights white/dim); SW3: turn (delay time; page 2 warble, page 3 DJ resonance), press (all effects back to default); SW5: turn (loop speed, also when paused); SW6: turn (output compressor) | Each does what TAPE's shift menu does; knob pages never change in the menu; holding a knob in the menu never resets it; closing the menu gives the knobs back |
| 3.59 | **Monitor positions:** in the menu press SW6 (cycles: orange = headphones, blue = both, yellow = send/return). With line in playing: headphones = heard in the headphones only in the record position (toggle up); both = always heard, through the effects, on both outputs; send/return = line in always in the headphones | As described; the main out carries the input only in *both* (and the mic in *send/return* in the record position); the CHOMPI light meters the input |

## 4. Panic and recovery

| # | Do | Pass when |
| --- | --- | --- |
| 4.1 | Hold a long-release chord with echo (use Warm Pad for reverb too); press SW5 | Voices, old echo and reverb tails stop at once |
| 4.2 | Repeat with `H cc 123 0`, `H cc 120 0`, `H panic ...`, webapp Panic | Same each time; notes retrigger normally afterwards |
| 4.3 | Switch route aux↔synth (webapp Signal path, Send) while notes ring | Sound stops cleanly; next keypress plays; aux stays stereo |

## 5. Webapp end to end

`python3 host/forge_web.py`, open `http://localhost:8765`.
Load preset → edit → Save JSON → Import JSON → Refresh ports → select both →
Send → Read device status → Capture to editor → Panic.
Pass: every step works; saved file re-imports; capture matches what was sent.
If you have keys: Generate an instrument with each provider, review, Send, play.
Record provider/model/seconds, never the key.

## 6. Sustained run and power

| # | Do | Pass when |
| --- | --- | --- |
| 6.1 | 10 minutes: four-voice playing with delay and reverb, recalling presets (v1, v2 and v3), turning knobs, normal MIDI clock if you have it | No hang, dropout, stuck note or noise burst |
| 6.2 | `H status` at the end | Peak CPU < 100% (fail at ≥100%; < 70% is the comfort target). Record average, peak, dropped, rejected |
| 6.2b | **Worst case:** reboot (resets peak), `H send presets/10-cpu-stress.json`, hold four keys for 1 minute, `H status` | Record average and peak. If peak ≥ 70 %, set voices to 3 then 2 in the webapp, Send, repeat, and record each. This sets the v3 CPU budget |
| 6.2c | **Sampler worst case:** record a ≥ 5 s take (3.34), reboot is not needed but note the peak first, `H send presets/13-sampler-stress.json`, hold seven keys for 1 minute, `H status` | Record average and peak. If peak ≥ 70 %, lower voices to 5 then 4 and repeat. Sample reads come from SDRAM, so this is the number the emulator cannot predict |
| 6.2d | **Sampler + looper:** as 6.2c, then tap LOOP and overdub for 1 minute while holding the seven keys, `H status` | Record average and peak. While the looper records or overdubs, a seventh sampler voice is released (6 voices max, by design); report whether that is noticeable |
| 6.2e | **Sampler + TAPE effects:** as 6.2c, with SW3 on page 2 (saturation) turned well up and page 3 (DJ filter) turned left (low-pass), and the warble and compressor up in the menu once they exist (stage 2); hold seven keys for 1 minute, `H status` | Record average and peak; must stay < 70 % (the emulator puts every TAPE effect at once about 15 % above WAVE's engine, ~63 % on CHOMPI by projection) |
| 6.3 | Reboot; `H status`; resend a saved patch | Boots to dry aux defaults; recall works |
| 6.4 | Optional: restore stock firmware with your normal card (or copy a folder from `firmware/card-profiles/` to a card) | Stock works again |

Do not deep-discharge the battery to test shutdown; record it as not run.

## 7. USB card and firmware (firmware 0.7)

From 0.7 on, files and firmware go to the card over the USB cable (bridge:
*Card & firmware*, or `host/forge_card.py`). The card stays in CHOMPI.

| # | Do | Pass when |
| --- | --- | --- |
| 7.1 | Put two TAPE samples (e.g. `cubbi_b1.wav`, `jammi_b1.wav`) in the kit's `card` folder; bridge: Refresh list, Copy selected to CHOMPI. Then send a kit patch for bank b and play | Both copied (time per MB noted); the sample list shows them; they play. Nothing else on the card changed |
| 7.2 | Pull the USB cable in the middle of a copy; reconnect; copy again | The interrupted file never appears half-written (the old one, or none, stays); the second copy completes |
| 7.3 | Install this kit's firmware; do **not** press CHOMPI for 15 s | CHOMPI's light blinks white, then stops; nothing installed; Forge keeps running |
| 7.4 | Install again and press the CHOMPI key | CHOMPI restarts; rainbow lights while the bootloader flashes; Forge starts; Connect CHOMPI shows the expected firmware version. Any other firmware file on the card was renamed so the bootloader ignores it (`CHOMPI_TAPEv2_0.bin` → `CHOMPI_TAPEv2_0_bin.old`; rename it back to use TAPE again) |

## 8. Power and battery (firmware 0.8)

As stock TAPE: the start-up storage (shipping-mode) gesture, the SW6 battery
light, and the USB/charger hand-over when power is plugged in while CHOMPI runs.
The power switch is the normal on/off. Do not run the battery flat on purpose.

| # | Do | Pass when |
| --- | --- | --- |
| 8.1 | Unplug the USB cable and play for a few minutes on the battery (keys, a preset, the looper) | Forge keeps running normally on the battery: sound, keys, lights |
| 8.2 | Hold the SW6 (volume) knob pressed for 2 s, then let go; also give it a short press | While held after 2 s its light shows the battery: white = charged (on the charger), green = good, yellow = low (below ~3.3 V). Dark again on release. A short press does nothing |
| 8.3 | Plug the USB cable back in (wall charger first, then the PC), wait 5 s; in the bridge press Connect CHOMPI and Check setup | CHOMPI keeps playing; the bridge connects (USB may drop for a moment while the charger identifies the source); Check setup's Power line says on USB power and a charge state; SW6 held shows yellow/green while charging, white when charged |
| 8.4 | Storage mode: switch CHOMPI off, unplug USB, then hold CHOMPI + PLAY + LOOP while switching it on (the stock gesture, same as TAPE); keep holding about 1 s | All lights go out and stay out, even with the switch on: the battery is disconnected for storage. Plugging USB in brings CHOMPI back. (Normal on/off is the power switch; charging also works with it off) |

## Results — copy into docs/forge/TEST_RESULTS.md

```text
Date / tester:
Bundle source commit / firmware SHA-256 / compiler (from verify_bundle.py):
Board / bootloader / stock firmware / OS / Python / MIDI connection:
1 Flash & identity:
2 Aux path 2.1–2.5:
3 Instrument 3.1–3.9 (note clicks/aliasing, sustain, bend here):
3B v3 modules 3.10–3.16 (incl. loudness vs stock reference):
3C Device presets 3.17–3.27 (note the toggle "menu position"):
3D Sampler 3.28–3.41 (loading time, recording levels per source, TAPE compatibility):
4 Panic & recovery 4.1–4.3:
5 Webapp (+ AI provider/model/seconds or "not run"):
6 Sustained: minutes / avg CPU / peak CPU / dropped / rejected; reboot; restore:
6.2b CPU stress: avg / peak at 4 voices (and at 3 / 2 if needed):
6.2c Sampler stress: avg / peak at 7 voices (and lower counts if needed):
Unexpected behavior:
Overall: pass / partial / blocked
```
