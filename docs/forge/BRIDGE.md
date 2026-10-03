# Forge hardware test bridge

The bridge is the local browser interface for a consolidated physical test:
live SYSTEM/PANEL/ENGINE/STORAGE, keys and LED shadow, indexed voices, retained
events, test controls, 73 guided checks, observations and downloadable evidence.
It uses the same `forge_inspector.collect` model as the terminal Inspector.
With an audio interface it also listens: automatic checks measure CHOMPI's
output (pitch, clicks, level, silence) while driving notes, tones and virtual
panel presses. Hardware verification is still pending. The bridge cannot
measure electrical levels, establish physical LED colours or verify tactile
behaviour, and its audio measurements do not replace listening.

## Quick start (Windows, development kit)

Nothing to install: the kit includes Python 3.12 and the MIDI and audio packages.

1. Unzip the kit. Optional: run `python\python.exe verify_bundle.py` in its folder;
   it must print `OK`.
2. Flash the kit's `firmware/FORGE.bin` (development firmware): put it on a test
   SD card's root as the only `.bin` file and use the installed bootloader's
   normal SD update. Do not install the beta bootloader. Back up your normal card.
3. Plug CHOMPI into the PC with a USB **data** cable. For the automatic audio
   checks also wire an audio interface (optional, see below).
4. Double-click **`Start Forge bridge.cmd`**. The browser opens the bridge. If
   Windows SmartScreen warns about a downloaded file, choose More info → Run
   anyway (or, before unzipping, right-click the ZIP → Properties → Unblock).
   Keep the black window open; closing it stops the bridge.
5. Press **Connect CHOMPI**. The bridge finds CHOMPI's MIDI ports by itself:
   only ports named CHOMPI or Daisy are tried, confirmed by Forge's reply.
6. Optional: **Find audio interface**, then **Run automatic checks**.
7. Work through the guided checks; record what you hear, touch and see.
8. **Export session JSON** and send it, with the `reports` folder, to whoever
   reviews the session (or share it with an agent).

Close other MIDI programs first (the webapp's workshop page, DAWs, MIDI
monitors): Windows lets only one program use a MIDI port.

### Audio interface wiring (for the automatic checks)

```
CHOMPI main out (L/R)  ──►  interface inputs 1/2      (the bridge listens)
interface outputs 1/2  ──►  CHOMPI line in            (test tones; aux path)
```

- **Turn the interface's direct monitoring / loopback off.** Otherwise CHOMPI
  hears itself through the dry aux path: feedback.
- **Find audio interface** plays a quiet C4 on CHOMPI and listens on each input,
  then sends a short −18 dBFS 1 kHz beep, trying the interface's own outputs
  first, to find the one wired to line in. You hear at most a few short beeps.
- Set input gain so nothing clips (each capture reports `clipped`). Windows
  devices at 44.1 kHz work; 48 kHz is preferred.
- Without an interface, everything else still works and the automatic audio
  steps are reported as skipped.

### Automatic checks

`host/auto_checks.json` runs the TEST_SESSION steps a computer can measure,
in about two and a half minutes: identity (1.3), noise floor, the aux path with a line-in
tone (2.1, 2.2, 2.4), pitch over MIDI and from a virtual key (3.1), voice
stealing without clicks (3.4), Warm Pad tail (3.10), menu and samples page with
LED read-back (3.17, 3.30), samples (3.28, 3.29), a line-in recording played
back at the right pitch (3.34; RAM only, nothing written to the card), the
looper (3.42: a line-in tone loop repeats at the right pitch without a seam
click; 3.42s–3.45: first take, overdub, pause and clear via the virtual keys,
read back from the Inspector; these need real time and are skipped in the
simulation), panic (4.1), CPU worst cases (6.2b, 6.2c) and the return to dry aux. It sends
patches, notes, −18 dBFS tones and virtual panel presses, and always hands the
panel back at the end. Each step shows pass / fail / error / skipped, the
failed expectations and, per recording, level, pitch, clicks and a
spectrogram. Recordings (WAV + PNG) and `summary.md` / `report.json` go to
`reports/<time>/` in the kit; the session export includes the results.

These are the bridge's measurements: a pass is hardware evidence only for
what that step measured. Thresholds are first estimates from simulation; when
a step fails, listen to its WAV before calling it a firmware bug.

An agent on the same PC (Claude Code in the kit folder) can run the same
session without the browser: `python\python.exe host\forge_audio.py run` finds
CHOMPI and the interface, runs the checks and writes `reports\<time>\`
including `session.json`.

## From a source checkout

`host/start_bridge.cmd` in a clone uses Python 3.10–3.12 (python-rtmidi has no
Windows build for 3.13+), creates `.bridge-venv` and installs
`host/requirements.txt` and `host/bridge-requirements.txt`. Other platforms,
from `firmware/chompi-forge`:

```sh
python -m pip install -r host/requirements.txt -r host/bridge-requirements.txt
python host/forge_web.py --open
```

The URL is `http://127.0.0.1:8765/inspector`. `--port 8766` selects another port.
"Choose MIDI ports yourself" on the page covers DIN/UART adapters and unusual
port names. The Sound workshop links to the bridge. Disconnect the bridge before
using the workshop's MIDI controls; its patch editor/AI authoring can still be
used without MIDI. You can import an edited patch into the bridge and send it.

## Try the interface before connecting CHOMPI

The Windows development kit can include `build/forge_probe.exe`; the launcher
then enables a SIMULATION option. Select it and Connect. It exercises the actual
C++ engine and protocol with synthetic samples, and reports CPU as unavailable.
Its sample memory, clock and storage differ from physical CHOMPI. It is not an
audio monitor or a substitute for a physical result.

From a source checkout, build `build/forge_probe`, then run:

```sh
python host/forge_web.py --probe build/forge_probe --open
```

Use `.exe` on Windows. The probe path is a server startup option; browser requests
cannot select executables or filesystem paths.

## Controls and evidence

- Physical and logical switch states remain distinct. Panel keys show switch
  indices and mapped MIDI notes, not a promised physical board layout. LEDs show
  commanded colours at renderer indices; confirm their board mapping yourself.
- All decoded telemetry is available in expandable field tables. Unavailable
  fields are labelled, including actual DMA underruns and stack/heap peaks.
- Polling pauses automatically during an action. Pause polling also lets you
  compare audio behaviour with and without telemetry traffic. Pause/resume is
  recorded. A heartbeat maintains the session while paused.
- Virtual panel tests provide key down/up, encoder turns, SW5 press, toggle and
  line-jack overrides. These can trigger menu/recording/storage actions. Release
  panel overrides before judging physical controls. A panel acknowledgement only
  means queued; inspect the next snapshot/event for its effect.
- MIDI tests send a timed note/chord (up to seven notes, 50–5000 ms), optional
  sustain or pitch bend, and individual CCs. Note-offs, pedal release and bend
  recentring are attempted even after failure. MIDI traffic has no acknowledgement.
- Patch, preset and sample operations reuse the normal protocol. SD writes,
  overwrites and erases require a one-operation checkbox. Panic and Release
  overrides remain available with test controls disabled.
- Timeout or invalid device replies end the connection without automatic retry.
  The last snapshot stays visibly stale. Reconnect and inspect before repeating
  an uncertain write. Override cleanup is best-effort if the cable/device fails;
  use Release overrides after reconnect, or reboot if cleanup is unconfirmed.
- One session owns the server's MIDI lock, including the entire multi-page
  snapshot. Other browser tabs/workshop MIDI requests cannot interleave. External
  MIDI programs are outside this lock and must be closed.
- Closing the page attempts disconnect. A 35-second missing-heartbeat timeout
  releases the ports and attempts override/pedal cleanup. The same browser tab
  retains its session identifier so a reload can recover report access; a server
  restart clears all in-memory records.
- Reports retain 1,200 trace records and all checklist results with their snapshot,
  snapshot age, mode and observation time. Omitted-record counts are explicit.
  Export regularly during long sessions. The screen shows the latest 300 events.
  Reports contain no AI provider key and never declare an automatic hardware pass.

## Exact first physical check

On the dry aux startup patch, hold/release C3 (switch 15): physical/logical key 15,
MIDI note 48, physical down/up events, no override. Aux starts no synth voice.
Turn SW4 (encoder 3) one step each direction; both encoder counts and mix follow.
Enable controls, send Glass Keys, then repeat physical C3: a panel-owned note-48
voice should start, release and stop. Record audio/LED observations and any loss
counters. Continue the full checklist in `docs/TEST_SESSION.md` in the kit.

## Software validation and firmware impact

The HTTP/session tests and real Chromium tests exercise the bridge using the C++
probe, including exclusivity, injected-versus-physical labels, cleanup, timeout,
storage operations, report export/reload and desktop/mobile layout. Existing
workshop browser tests remain part of `make browser-test`.

This bridge changes host tools only. Development firmware remains 229,912 bytes,
D1 SRAM 96,564 bytes; release remains 213,952 bytes / 92,588 bytes. No protocol
or firmware version change, new DSP work, or additional firmware memory cost.
The host routing decoder now correctly selects sampler parameter labels from
the string route. Bounded draining of rejected HTTP bodies fixes the Windows
connection-reset failure without changing validation or security checks.
