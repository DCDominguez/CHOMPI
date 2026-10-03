# Forge Inspector — first development slice

Implemented 2026-10-03, software-tested; **no physical CHOMPI verification**.
This is observability work, with no musical or DSP feature changes.

## Audit of the starting head

Audited `7e8fd1955872ed91fba9a44b76dbb145b93f677b`, confirmed as the remote
`forge/foundation` head before editing. Draft PR #1 remains the integration PR.

| Group | Already observable | Added by Inspector schema 1 | Still unavailable |
| --- | --- | --- | --- |
| SYSTEM | Firmware minor; current quantized patch; average/peak completed callback load; aggregate ingress/control/reply drops and recognized request rejections | Protocol version, uptime, audio-state timestamp/block number, per-UART/USB accepted RX frames, accepted TX submissions, TX failures, ingress losses, panel/sample queue losses, emergency count, event losses | Actual codec/DMA underruns, framing/parser discard count, heap use and stack high-water mark; USB delivery confirmation |
| PANEL | Packed menu, panel override flag, 25 key LED RGB values plus CHOMPI LED; development key/turn/toggle/jack injection | All 40 debounced physical switch bits separately from merged logical keys, key-to-note map, six physical/merged signed encoder accumulators, physical/merged toggle and line-jack state, SW5 press edge, logical knob targets | Electrical quadrature pin traces, analogue ADC readings (these knobs are encoders), continuous SW5 held state, other jack sensing not supplied by the board API |
| ENGINE | Patch targets/source/sampler settings via normal status; aggregate active voice count via probe | Seven indexed voices with note, source, envelope stage/level, sample slot, sustained/sample/reverse flags; smoothed cutoff, bend ratios, pedal mask, LFO/wheel, resolved delay/mix/feedback/level/reverb mix | Per-voice playback position/window, per-voice instantaneous filter modulation, delay/reverb buffer contents, patch name (never on device) |
| STORAGE | Card-ready flags, slot occupancy, recording duration/capacity, loader busy/loading, wanted sample selection | Actual loader file selection, file readable/allocated frame totals, pool allocation/capacity, truncated-slot count, recording source/lock/frames/capacity, queued and active sample jobs, mount-configured versus card-present flags, counted load/job/mount/preset failures | Free SD space, individual kit-slot progress, exact FatFS error codes, time remaining for storage work; synchronous preset writes cannot be observed mid-write |

`sd_present` means the existing disk-status driver reports ready, not an
independent mechanical insertion sensor. `sd_mount_configured` is the cached
mount result; it can stay true after removal. `sd_ready` requires both.
Pool usage is reserved bytes; file frames are totals across slots 0–13, excluding
the separate recording buffer. A partly loaded slot can already play.

## Run the Inspector

The [browser hardware test bridge](BRIDGE.md) now provides a dashboard, explicit
test controls, guided checks and report export using this same telemetry model.
The read-only terminal workflow below remains available.

Build the development firmware with `make -C firmware/chompi-forge firmware-dev`
and the documented GCC_PATH. The Inspector requires this build; release firmware
rejects development opcodes. The development binary is
`firmware/chompi-forge/src/build-dev/FORGE.bin`. Firmware flashing is a user-run
step in the consolidated [hardware session](TEST_SESSION.md); none was performed
while implementing this change. Keep the release binary separate.

From `firmware/chompi-forge` on the computer connected to CHOMPI:

```sh
python -m pip install -r host/requirements.txt
python host/forge_host.py ports
python host/forge_inspector.py --input "EXACT IN" --output "EXACT OUT" --watch --record inspector-session.jsonl
```

The viewer only sends probe reads (`0B`); it never applies a patch, injects a
panel event, plays notes, saves samples or calls AI. Stop with Ctrl+C. To change
the sound, use the physical panel or another Forge tool, with the Inspector
stopped during that tool's exchange. One host owns acknowledged exchanges and
the latched state snapshot at a time. The output names must be explicit.

Default polling is once per second; `--interval 5` reduces traffic on DIN/UART
or during heavy storage work. UART replies block the main loop using the
existing transport, while audio continues by interrupt. Several pages per poll
cost more main-loop time than one normal status request. Validate RX losses,
storage progress and audible continuity with polling on and off. No automatic
retry after timeout: stop and inspect the connection; restart explicitly.

`--json` emits machine-readable snapshots. `--record FILE` appends the same
four groups and new events as JSONL. Full patch targets and LED RGB values are
in the JSON; terminal output shows active voices and lit LED indices. LED page 1
is the latest rendered shadow (about 30 Hz), read separately from the frozen
Inspector snapshot. It is the commanded colour, not proof a physical LED lit.

Without hardware, use the existing C++ probe:

```sh
python host/forge_inspector.py --offline build/forge_probe --watch
```

On Windows use `build/forge_probe.exe`. This is explicitly labelled SIMULATION;
CPU readings are unavailable, not measured zero. The simulated sample pool and
recording capacities differ from the device, and its clock advances by simulated
audio blocks rather than wall time. It cannot validate physical controls, sound,
USB/DIN timing, CPU or card presence. `--offline` and MIDI mode use the same
decoder/collector/display, not different telemetry implementations.

## Realtime ownership

`core/inspector.h` is the shared C++ telemetry model and development codec;
`host/forge_inspector.py` is the matching host decoder and reusable collector.
Future desktop/bridge/Tab5 clients should consume this schema, with deliberate
schema revisions if fields change. Do not create new device-side telemetry for
each client. No new transport or unsolicited stream was added.

Audio remains the only owner of mutable engine, panel and recorder state. A
single-slot mailbox has explicit idle/requested/writing/ready ownership. Main
requests a refresh at most 20 Hz. Audio publishes cheap scalar state only when
requested; it never waits, serializes, allocates, performs IO or formats text.
Main copies the published state and adds its own transport/storage state when
page 2 latches a snapshot. No shared non-atomic engine reads or interrupt-masked
engine copies occur in main. A slow reader cannot cause audio to rewrite an
unread publication. The current callback meter includes telemetry work; the
published CPU values describe earlier completed callbacks.

Panel edges and encoder turns enqueue compact numeric observations into a
63-entry usable SPSC queue. Seven voice identities/stages are compared once per
block. Patch revision, sample selection and recording transitions are observed
at block boundaries. Main drains records and maintains the 64-event retained
log, assigns serials, performs wire encoding and transmits. Overflow drops the
new observation and increments a counter; it cannot delay audio or overwrite an
unread record. Multiple voice retriggers or patch applications within one block
can collapse to the final state; this is a diagnostic trace, not an exact MIDI
performance recorder. Knob/key events are taken before musical/menu routing,
so a menu-swallowed key remains visible.

Events: key down/up (value bits: 1 physical, 2 injected), merged knob increment,
voice start/stop (indexed voice; packed note/source/sample slot), patch apply
(revision), sample selection (PackSelection), recording start/stop (frames),
card driver/mount status, queue losses, storage failures, file load completion
and sample-job completion. Voice stop means no longer active, after release,
or replacement; it is not a MIDI note-off acknowledgement. Event timestamps
and counters wrap at 32 bits. Retention overwrites and cursor gaps are separate
from audio-to-main event drops. Polling never consumes the retained log; readers
can replay it. A cursor from a previous boot is reset by the host.

The host groups queue/emergency/event-loss counters under SYSTEM, although some
are packed on storage page 5 to keep each wire reply within existing buffers.
SYSTEM memory capacity summarizes the same sample/recording buffers shown in
STORAGE. SW5's dormant target is unavailable in patch v1, whose legacy patch
DATA does not carry cutoff; the viewer reports it as null. The persistent panel
override flag covers virtual keys/toggle/jack; a one-off injected turn is still
distinguishable by the separate physical and merged encoder accumulators.

## Validation at this checkpoint

- Eight native suites pass, including the new concurrency, bounds/canary,
  cursor retention/overflow, physical versus injected input, voice transition
  and failed-load tests. The sampler suite checks three factory TAPE files.
- Seven Inspector Python integration tests pass through the actual C++ codec;
  all v1–v4 patch layouts, frozen snapshot generations, LED reads, physical
  identity, voices/recording, malformed data and release rejection are covered.
- Full Python suite: 61/62 pass on Windows Python 3.11. The existing
  `test_validation_and_malformed_request_bodies` fails with WinError 10054 for
  the oversized HTTP body. Reproduced against unmodified HEAD host/tests;
  its 400-status assertion was preserved. This is **not an all-green test run**.
- ARM release and development builds pass with checksum-verified xPack GCC
  10.3.1-2.3 Windows x64 (archive SHA-256
  `169744f784fb04ae10c60bc6a2cd69cff93cff0bf5657e9333776036f347f9c4`).
  Portable native GCC 15.2 used the existing strict flags plus an M_PI definition
  for the unchanged Linux-oriented DSP tests. No Forge ARM compiler warnings.
- Release binary is byte-identical to a rebuild of the original head, SHA-256
  `f0a18b13d95a328f92b060f35d746ae7f4b32781959d25a17e0954d5b7b682e1`.
  No Inspector symbols are linked into release.
- ASan/UBSan execution could not be completed: the installed Windows LLVM lacks
  the MinGW sanitizer runtime libraries. The sanitizer target includes the new
  suite for the documented Linux build environment. No sanitizer pass is claimed.
- Real-browser tests were not rerun (no browser UI changes). Hardware, live AI,
  flashing and benchmark instruction-count runs were not performed; no DSP
  algorithm changed.

| Build | Code/binary bytes | D1 SRAM bytes | DTCM | RAM D2 | SDRAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original release / new release | 213,952 | 92,588 | 34,816 | 23,800 | 59,104,272 |
| Original development hooks | 215,344 | 92,684 | 34,816 | 23,800 | 59,104,272 |
| Inspector development | 229,912 | 96,564 | 34,816 | 23,800 | 59,104,272 |

Development delta: +14,568 code bytes and +3,880 SRAM bytes. Release delta: zero.
Inspector development leaves 58,856 bytes in the 282 KiB code region. Allocation
figures are linker-map measurements, not peak stack/heap use or physical CPU.
See CONTINUE for any later checkpoint figures.

## Exact next hardware test

After the user installs this development candidate in the consolidated session,
start the read-only viewer at one-second polling and record JSONL. With the dry
aux startup patch, press and release physical button 15 (C3/MIDI 48), then turn
logical knob 1 (hardware SW4/index 3) one step in each direction. Pass only if:

1. Button 15 appears in both physical and logical key lists while held, maps
   to note 48, and has key down/up events with physical identity bit 1.
2. Physical and merged encoder counters both move at index 3; mix changes;
   no override flag is set. Do not inject virtual controls in this step.
3. LED command values can be matched to the actual board, audio remains
   continuous, and queue/event losses stay unchanged.

The aux patch intentionally starts no musical voice. After recording this
identity check, stop the Inspector, send preset 04 Glass Keys, restart it and
repeat the key: expect a panel-owned note-48 voice followed by release/stop.
This separates the input mapping test from engine routing. Continue the existing
sampler, jack/recording, SD and CPU stress steps with polling on/off comparisons.
