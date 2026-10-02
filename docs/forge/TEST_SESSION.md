# Forge 0.3 — the one consolidated hardware test

Status of the candidate: **software-tested, hardware-unverified.** This is the
single planned physical session. Work top to bottom. If a stage fails, record
it, stop that stage, and continue only where later stages do not depend on it.
Never mark a stage passed that you did not run. Expect roughly 90 minutes.

## 0. Before you start (no CHOMPI needed)

1. Unzip the bundle. In its folder run `python3 verify_bundle.py`.
   It must print `OK`. It also prints the firmware SHA-256 and the compiler that
   built it — copy both into your results.
2. `python3 -m pip install -r host/requirements.txt`
3. Optional, recommended: the live AI preflight in `docs/LIVE_AI_TEST.md`.
   It needs no hardware.
4. Back up your normal SD card and keep it aside. Use a separate test card.
   Have the stock firmware and the known restore procedure at hand. Do **not**
   install the repository's beta bootloader.

Shorthand below: `H = python3 host/forge_host.py`, with
`--input "IN" --output "OUT"` set to your exact CHOMPI port names from `H ports`.
Keep monitoring volume low. Avoid audio feedback loops. The mic is unused.

## 1. Flash and identity

| # | Do | Pass when |
| --- | --- | --- |
| 1.1 | Put `firmware/FORGE.bin` alone on the test card root; use the installed bootloader's normal SD update | Update completes uninterrupted |
| 1.2 | Power up; watch LED | Initialization completes; no output burst |
| 1.3 | `H status ...` | Firmware 0.3, version 1 aux patch, counters 0 |

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

## 4. Panic and recovery

| # | Do | Pass when |
| --- | --- | --- |
| 4.1 | Hold a long-release chord with echo; press SW5 | Voices and old echo tail stop at once |
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
| 6.1 | 10 minutes: four-voice playing with delay, recalling presets, turning knobs, normal MIDI clock if you have it | No hang, dropout, stuck note or noise burst |
| 6.2 | `H status` at the end | Peak CPU < 100% (fail at ≥100%; < 70% is the comfort target). Record average, peak, dropped, rejected |
| 6.3 | Reboot; `H status`; resend a saved patch | Boots to dry aux defaults; recall works |
| 6.4 | Optional: restore stock firmware with your normal card | Stock works again |

Do not deep-discharge the battery to test shutdown; record it as not run.

## Results — copy into docs/forge/TEST_RESULTS.md

```text
Date / tester:
Bundle source commit / firmware SHA-256 / compiler (from verify_bundle.py):
Board / bootloader / stock firmware / OS / Python / MIDI connection:
1 Flash & identity:
2 Aux path 2.1–2.5:
3 Instrument 3.1–3.9 (note clicks/aliasing, sustain, bend here):
4 Panic & recovery 4.1–4.3:
5 Webapp (+ AI provider/model/seconds or "not run"):
6 Sustained: minutes / avg CPU / peak CPU / dropped / rejected; reboot; restore:
Unexpected behavior:
Overall: pass / partial / blocked
```
