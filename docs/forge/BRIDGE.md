# Forge hardware test bridge

The bridge is the local browser interface for a consolidated physical test:
live SYSTEM/PANEL/ENGINE/STORAGE, keys and LED shadow, indexed voices, retained
events, test controls, 62 guided checks, observations and downloadable evidence.
It uses the same `forge_inspector.collect` model as the terminal Inspector.
Hardware verification is still pending. The bridge cannot hear audio, measure
electrical levels, establish physical LED colours or verify tactile behaviour.

## Start on Windows

1. Extract the development kit, then run `python verify_bundle.py` from its root.
   Require `OK`. Record the printed firmware hash in Session details.
2. With Python 3.10+ installed and on PATH, double-click `host/start_bridge.cmd`.
   The launcher creates an isolated Python environment beside itself and installs
   the two pinned MIDI dependencies on first use (internet required then).
   It opens the bridge in your browser. Keep its terminal open; Ctrl+C stops it.
3. For a hardware session, back up the normal card and use a test card. Put the
   kit's `firmware/FORGE.bin` on the test-card root as the only `.bin` file. Use
   the installed bootloader's normal SD update; do not install the beta bootloader.
   Connect a USB data cable. Release firmware rejects Inspector requests.
4. Refresh ports, select both exact CHOMPI MIDI ports, add session details and
   Connect. Use a five-second read interval for DIN/UART if needed. The bridge
   opens only the explicitly chosen ports. No patch or test event is sent by Connect.
5. Follow the physical checks while watching state and events. Enable test controls
   when you need to send a patch, play MIDI, inject a panel event or operate storage.
   Mark each check yourself and record what you heard, touched and saw.
6. Disconnect, then Export session JSON and/or Export trace JSONL. Export before
   starting another session or stopping the server.

For other platforms, from `firmware/chompi-forge` (or the extracted kit):

```sh
python -m pip install -r host/requirements.txt
python host/forge_web.py --open
```

The URL is `http://127.0.0.1:8765/inspector`. `--port 8766` selects another port.
The Sound workshop links to the bridge. Disconnect the bridge before using the
workshop's MIDI controls; its patch editor/AI authoring can still be used without
MIDI. You can import an edited patch into the bridge and explicitly send it.

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
