# Forge 0.4 — the one consolidated hardware test

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

Shorthand below: `H = python3 host/forge_host.py`, with
`--input "IN" --output "OUT"` set to your exact CHOMPI port names from `H ports`.
Keep monitoring volume low. Avoid audio feedback loops. The mic is unused.

## 1. Flash and identity

| # | Do | Pass when |
| --- | --- | --- |
| 1.1 | Put `firmware/FORGE.bin` on the test card root as the **only** `.bin` file. On macOS also remove `._FORGE.bin` (`dot_clean -m /Volumes/CARD` or delete it); the bootloader loads the first `.bin` it finds and would reject that metadata file. Use the installed bootloader's normal SD update | Update completes uninterrupted |
| 1.2 | Power up; watch LED | Initialization completes; no output burst |
| 1.3 | `H status ...` | Firmware 0.4, version 1 aux patch, counters 0 |

## 2. External audio path (v1 compatibility)

| # | Do | Pass when |
| --- | --- | --- |
| 2.1 | Aux source in; `H send presets/01-dry.json ...`; play left-only then right-only | Correct L/R on headphones and main outs |
| 2.2 | `H send presets/02-slap.json`, then `03-long-echo.json`, while audio plays | Acknowledged; echoes change; no crash; time change glides in pitch |
| 2.3 | Turn SW1–SW4 and volume SW6; `H status` | Mix/time/feedback/level move as documented; level can mute |
| 2.4 | `H cc 24 127 --output "OUT"`, then `H cc 24 0` | Wet fades out then back; dry still follows level |
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
| 3.14 | With a v3 patch: `H cc 26 110`, `H cc 27 110`; `H capture cc-test.json ...` | Resonance and reverb mix audibly increase; captured JSON shows filter.resonance and reverb.mix near 0.87 |
| 3.15 | Webapp: load Soft Pad (v2), **Convert to v3 instrument**, Send | Sounds close to the v2 Soft Pad (filter slightly steeper); no error |
| 3.16 | Send a v2 preset, then a v3 preset, while holding a note | Sound stops cleanly at the format change (like a route change); next note plays |

## 3C. Device presets on the SD card (TAPE-style keys + encoder)

Use the test card from 1.1 (it may be otherwise empty). "Menu position" means the
toggle position in which TAPE's CHOMPI key opens its menu; note which way that is.

| # | Do | Pass when |
| --- | --- | --- |
| 3.17 | Toggle to menu position, hold the CHOMPI key | Key LEDs light: bank keys (KEY_16/17) in bank 1's colour, save/copy/erase dim blue/green/red, white keys off (empty card); playing keys makes no sound while held |
| 3.18 | `H send presets/07-warm-pad.json`. Menu: press SAVE (KEY_25), release CHOMPI, press white key 1 (it turns blue), press CHOMPI | Panel LED flashes green; white key 1 now dim (occupied). `FORGE/B1S01.FPR` exists on the card afterwards |
| 3.19 | Send `08-acid-bass.json`, save it to slot 2 the same way. Release CHOMPI | Menu closes; keys play Acid Bass again |
| 3.20 | Hold CHOMPI (menu), press white key 1, release CHOMPI, play | Warm Pad plays; key 1 shows white while the menu is open. Repeat with key 2 → Acid Bass |
| 3.21 | Menu: turn encoder 1 and press KEY_17 / KEY_16 | Bank colour changes on the bank keys; encoder stops at banks 1 and 8, keys wrap. Mix (encoder 1's normal job) does not change while the menu is open |
| 3.22 | Menu: COPY (KEY_24), white key 1 (green), KEY_17 to bank 2, white key 5 (blue), CHOMPI | Green flash; bank 2 key 5 occupied. ERASE (KEY_23), key 5, CHOMPI → key 5 empty |
| 3.23 | Send MIDI program change 1 on channel 1 from a keyboard or DAW | Acid Bass (bank 1 slot 2) loads; program 0 → Warm Pad |
| 3.24 | `H slots ...`, `H recall 1 1 ...`, `H store 1 3 ...`, `H erase 1 3 ...` | JSON lists occupied slots; recall returns the patch; store/erase acknowledged |
| 3.25 | Webapp Device presets: Read slots, select bank 1 slot 2, Recall; then Store into slot 4; Erase slot 4 (press twice) | Slot buttons show stored slots; recall loads the sound into the editor; erase asks for a second press |
| 3.26 | Power off and on; open the menu | Slots 1 and 2 still occupied and recall correctly (boot itself still starts in dry aux) |
| 3.27 | Optional: power off, remove the card, power on, open the menu; reinsert the card | White keys red without a card; nothing crashes; within ~1 s of reinserting, slots show again |

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
| 6.3 | Reboot; `H status`; resend a saved patch | Boots to dry aux defaults; recall works |
| 6.4 | Optional: restore stock firmware with your normal card (or copy a folder from `firmware/card-profiles/` to a card) | Stock works again |

Do not deep-discharge the battery to test shutdown; record it as not run.

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
4 Panic & recovery 4.1–4.3:
5 Webapp (+ AI provider/model/seconds or "not run"):
6 Sustained: minutes / avg CPU / peak CPU / dropped / rejected; reboot; restore:
6.2b CPU stress: avg / peak at 4 voices (and at 3 / 2 if needed):
Unexpected behavior:
Overall: pass / partial / blocked
```
