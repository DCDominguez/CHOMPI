# Forge 0.2 — one consolidated hardware test

Candidate status: software-tested, hardware-unverified. This is the planned
single session, not a demand for separate tests at each milestone. Stop and log
a failing stage; fix it before proceeding. A hardware-dependent defect may
require a focused retest. Expected scope is one stereo-delay engine with live
control, host-managed presets, diagnostics and optional model authoring.

## Prepare once

Record board/CHOMPI variant, installed bootloader if known, current stock
firmware, operating system, sample source, and connections. Back up the working
SD card. Have the matching known-good stock firmware/card available and confirm
the existing restoration procedure. Do not install the repository's beta
bootloader for this test. Use a separate test card and stable power.

The bundle contains `firmware/FORGE.bin`, host tools, three presets, simulated
audio references, and a manifest with hashes. Verify the file hash matches the
manifest (`Get-FileHash` on PowerShell; `shasum -a 256` on macOS;
`sha256sum` on Linux). Keep your original card intact.

On the TEST card, place FORGE.bin in the root and remove other firmware `.bin`
files from that test card. Follow the installed CHOMPI bootloader's normal SD
update procedure, as described in the upstream firmware README. Do not interrupt
the update. If your bootloader procedure differs, resolve that before flashing.

Start with low external monitoring level. Connect a known stereo line source
to aux input and USB MIDI to the computer. Do not form an audio feedback loop.
The microphone is unused by Forge. Install the host dependencies once and run
`ports` as described in `host/README.md`.

## Run this sequence in one session

| Stage | Action | Pass evidence |
| --- | --- | --- |
| Boot and identity | Power up, watch initialization LED, query status | Initialization completes; status says firmware 0.2; no unexpected output burst |
| Dry routing | Play left-only then right-only signals; send `01-dry.json` | Correct stereo separation on both headphones and main outputs |
| Local controls | Turn SW1/SW2/SW3/SW4 and physical volume SW6; query status | Mix/time/feedback/level targets move as documented; level can mute |
| Patch recall | Send `02-slap.json`, then `03-long-echo.json` during audio | Matching acknowledgements; expected echo changes, no crash/dropout; time glides in pitch |
| Save and recall | Capture knob-adjusted settings, change patch, resend captured file | Captured targets return within 14-bit quantization |
| Bypass | Send CC24 values 127 then 0 on MIDI channel 1 | Wet sound fades out/in; dry path still obeys output level |
| Transport | Check CC20–23 and patch send/status on USB; repeat on bidirectional TRS if available | Both paths control the engine and return replies; no unverified path marked passed |
| Rejection | Attempt a host JSON with feedback >0.85; validate it | Host rejects it before MIDI transmission; current sound remains unchanged |
| Webapp | Start the local bridge; load/edit/import/export a preset; select ports; capture targets and send | Browser controls work, downloaded JSON validates, matching device acknowledgement |
| Optional cloud AI | Generate with your OpenAI or Gemini API key; review, edit, then send. Repeat with the other provider if available | Actual output validates; record provider/model and elapsed time, never keys; generation alone leaves device unchanged |
| Optional local AI | Generate using an installed Ollama model, inspect JSON, then send | Actual output validates and is acknowledged; record model/time or skipped |
| Sustained run | Play 10 minutes while recalling presets and using knobs, with normal MIDI clock traffic | No hangs, dropout or non-finite audio; record final average/peak load and counters |
| Power/recovery | Reboot, resend saved patch, verify normal power behavior, then restore stock if desired | Defaults on reboot, host recall works, known-good stock restoration confirmed |

Do not intentionally deep-discharge the battery to force a shutdown. Existing
battery protection is inherited; record low-battery behavior as untested unless
it naturally occurs and can be checked responsibly. Do not mark optional AI,
TRS, battery shutdown, or stock restoration as passed if you did not run them.

For the sustained run, investigate any audible dropout regardless of the load
number. Treat peak callback load at or above 100% as a timing failure. A peak
below 70% is a provisional headroom target, not a proof of worst-case safety.
Record whether drop/rejection counters increase under ordinary use. Deliberate
overload may drop controls; audio must continue and a later status request must
recover. Do not flood the device merely to satisfy this first session.

## Results record

Copy and fill this into `docs/forge/TEST_RESULTS.md` when the session occurs:

```text
Date / tester:
Firmware SHA-256:
Board / variant / bootloader / stock firmware:
Computer OS / Python / MIDI interface:
Audio source / output connections:
Boot / dry routing / encoders / recall / capture / bypass:
USB / TRS:
Webapp browser / edit / import-export / capture-send results:
AI provider / model and result, or not run (never record keys):
Duration / average CPU / peak CPU / dropped / rejected:
Audible glitches or unexpected behavior:
Power behavior / low-battery check, or not run:
Stock restore result, or not run:
Overall: pass / blocked / partial
Follow-up:
```
