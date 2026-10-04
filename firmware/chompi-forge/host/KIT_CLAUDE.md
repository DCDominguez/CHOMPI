# Forge hardware test kit — instructions for an agent on the test PC

You are helping DC test Forge, development firmware for a CHOMPI (Daisy Seed)
synth, on DC's Windows PC. This folder is the unzipped development kit. The
source and the full docs live in the GitHub repo DCDominguez/CHOMPI, branch
`forge/foundation` (see `manifest.json` for the exact commit).

## What you can do here

- Check the kit: `python\python.exe verify_bundle.py` (must print `OK`; files
  replaced by a bridge update are listed — say which and why).
- Run the whole automatic session and write a report:
  `python\python.exe host\forge_audio.py run`
  It finds CHOMPI's MIDI ports and the audio interface itself, checks the test
  setup (cables, gain, hum, line input), runs the steps in
  `host\auto_checks.json` and writes `reports\<time>\` with `summary.md`,
  `report.json`, `session.json` and a WAV + spectrogram per recording.
- Only the setup: `... run --setup-only`. Only some steps: `... run --only 3.4 3.29`.
- Look at a recording: `python\python.exe host\forge_audio.py analyze reports\<time>\3.4-steal.wav`
  (prints levels, pitch, clicks, clipping per channel).
- The browser bridge with the guided panel walk (DC's hands needed):
  `python\python.exe host\forge_web.py --port 8766 --open`, then
  http://127.0.0.1:8766/inspector. Only one program may own CHOMPI's MIDI
  ports: stop any other bridge first.

## Rules

- Never claim a hardware result you did not measure; "the bridge measured X"
  is evidence only for what that step measures. Audio failures are often the
  rig (cables, interface gain, ground hum): read the setup findings first.
- Never flash firmware, write `FORGE.bin`, change the SD card or edit kit
  files yourself; ask DC. Never ask for or handle API keys.
- Keep monitoring volume low; the checks play quiet tones and notes.
- Report back: counts, each failure with its measured values, the setup
  findings, and what you think is CHOMPI versus the rig. DC sends the
  `reports\<time>` folder and `session.json` to the Forge development session.

## Reference

`README.md` (bridge guide), `docs\TEST_SESSION.md` (all steps),
`docs\TEST_RESULTS.md` (results so far), `docs\KNOBS.md`, `docs\PROTOCOL.md`.
CHOMPI's panel: knobs 1–4 are SW4, SW1, SW2, SW3; KEY_27 = PLAY, KEY_28 = LOOP,
KEY_26 = CHOMPI; white keys KEY_1–15 (C3–C5), black keys KEY_16–25.
