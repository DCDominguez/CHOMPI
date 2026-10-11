# Graph Report - CHOMPI  (2026-10-11)

## Corpus Check
- 140 files · ~271,467 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 4, .css 2, .ld 1)

## Summary
- 4178 nodes · 8223 edges · 220 communities (176 shown, 44 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 945 edges (avg confidence: 0.87)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `df4aad6f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Synth
- Walk
- PanelController
- Bridge
- InspectorAudio
- Engine
- Looper
- SlotSettings
- Parameters
- forge_host.py
- forge_audio.py
- PresetMenu
- json
- Parameter
- Voice
- Parts
- main
- forge_inspector.py
- TapeFx
- inspector.js
- looper_test.cpp
- sampler_test.cpp
- app.js
- SampleLoader
- EngineAndPanel
- .ApplyPatch
- v3_test.cpp
- Harmony / Intent Engine — brief
- FORGE_COLD
- Clock
- synth_test.cpp
- parameters.h
- InstrumentTests
- Rig
- DjFilter
- CardTests
- tempo_bench.cpp
- Player
- Sequencer
- Rig
- Request
- Response
- SampleFiles
- firmware_chompi_forge_core_sequencer
- Pad
- slot_settings_test.cpp
- InspectorTests
- LedView
- BrowserTests
- panel_test.cpp
- SampleCard
- parts_test.cpp
- knob_layout.h
- panel_controller.h
- Settings
- PresetStore
- Effects
- LoaderRig
- run_bench.py
- InspectorStorage
- power.h
- RequestKind
- wav.h
- sequencer_test.cpp
- file_transfer_test.cpp
- Forge test results
- probe
- inspector.h
- InspectorSystem
- Reverb
- preset_test.cpp
- FakeMidi
- Rig
- MemoryCard
- TransferAndInstall
- firmware_chompi_forge_core_protocol
- SampleJob
- Forge · Choowi changelog
- argparse
- .Valid
- Warble
- SimulatedMido
- file_transfer.h
- State
- InspectorEventKind
- EngineAndPanel
- RecordGesture
- Mailbox
- FatFsUploadFiles
- iostream
- BridgeTests
- FakeMido
- WebTests
- InspectorMailbox
- SpscQueue
- 5. Proposed design
- ChargerUsb
- FaultRecord
- runtime.h
- FatFsSampleFiles
- AnalysisTests
- Device
- Forge · Choowi: the user manual
- FakeChompi
- Chord
- main
- synth.h
- forge_probe.cpp
- Forge Resource Ledger
- SlotPolicy
- harmony_test.cpp
- FileTransfer
- Performance
- harmony.h
- tape_bench.cpp
- FatFsStorage
- Function
- MidiFrame
- MenuAction
- FakePort
- CardUploads
- BridgeAutomaticTests
- BridgeBrowserTests
- MemoryStorage
- SampleHandoff
- Kind
- Envelope
- Battery
- LossyTransport
- Handler
- Mode
- power_test.cpp
- SampleSlot
- MenuPage
- .ProcessSampler
- MidiPort
- Options
- ResponseKind
- Transport
- BridgeCardTests
- build
- Forge — developer resume checkpoint
- ProviderTests
- InspectorVoice
- Compressor
- Forge 0.19.0 — the one consolidated hardware test
- PanelSink
- State
- Mmf
- Kind
- forge_ai.py
- MidiFramer
- Pattern
- FakeChompi
- Sink
- SoundDeviceAudio
- State
- FirmwarePanelSink
- preset_menu.h
- ForgeServer
- forge_bridge_app.py
- forge_bench.cpp
- KnobHold
- Queue
- .RecordParam
- Rate
- the forge family
- BridgeWebTests
- V4PatchTests
- MonitorMode
- FakePorts
- AiCheckTests
- Sequence
- .run_walk
- HandleFrame
- Earlier checkpoint: boot fix and cleanup, 2026-10-03
- Forge hardware test bridge
- Forge host controller
- Forge sampling (roadmap item 4): design
- restart.h
- SlotSettingsSchedule
- Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark
- Stock TAPE: what every control does (reference for Forge parity)
- Event
- SlotValues
- MidiOut
- Forge control protocol — firmware 0.15
- Sink
- Choowi: the first firmware of the forge
- PresetProtocolTests
- Action
- First time home with Forge 0.19.0
- forge_main.cpp
- .ProcessActive
- Ponytail, lazy senior dev mode
- Forge looping (roadmap item 5): design
- Forge project brief
- .MenuPacked
- BootCount
- Sink
- Forge — live OpenAI / Gemini test
- the forge docs
- Colour
- Forge hardware test kit — instructions for an agent on the test PC
- ProjectsOnTheCard
- Rng
- start
- Outgoing
- Forge UX review (2026-10-11)
- Extension
- Branch and publishing
- Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05
- Stock firmware comparison and CPU benchmark (2026-10-03, the owner's request)

## God Nodes (most connected - your core abstractions)
1. `Engine` - 153 edges
2. `PanelController` - 147 edges
3. `Synth` - 134 edges
4. `Parts` - 82 edges
5. `Looper` - 81 edges
6. `Parameters` - 71 edges
7. `InspectorAudio` - 69 edges
8. `SampleLoader` - 58 edges
9. `main()` - 54 edges
10. `Parameter` - 53 edges

## Surprising Connections (you probably didn't know these)
- `Ratings` --references--> `ComposeTransportLeds()`  [INFERRED]
  docs/forge/UX_REVIEW.md → firmware/chompi-forge/core/panel_controller.h
- `Phase 0 decisions (the owner, 2026-10-05)` --references--> `Clock`  [INFERRED]
  docs/forge/HARMONY_BRIEF.md → firmware/chompi-forge/core/parts.h
- `Development probe and Inspector schema 1` --references--> `EncodePatchData()`  [INFERRED]
  docs/forge/PROTOCOL.md → firmware/chompi-forge/core/protocol.h
- `Hardware: 0.15.2 on CHOMPI, unattended run (2026-10-10 evening)` --references--> `detect()`  [INFERRED]
  docs/forge/CONTINUE.md → firmware/chompi-forge/host/forge_audio.py
- `0.17.0 Magic: wakes up singing, surprise me, mutate (tool: Wake-up Bell) — 2026-10-11 (software-tested; not installed yet)` --references--> `Magic017()`  [INFERRED]
  docs/forge/CHANGELOG.md → firmware/chompi-forge/tests/panel_test.cpp

## Import Cycles
- None detected.

## Communities (220 total, 44 thin omitted)

### Community 0 - "Synth"
Cohesion: 0.02
Nodes (77): Synth, age_, amp_, bend_slew_, bend_target_, cap_, coefficient_, cutoff_ (+69 more)

### Community 1 - "Walk"
Cohesion: 0.12
Nodes (11): Build firmware, CPU benchmark against stock firmware, Create a test bundle, Forge developer guide, Get the development branch, Requirements, Troubleshooting, Validate software (+3 more)

### Community 2 - "PanelController"
Cohesion: 0.02
Nodes (76): PanelController, activity_, audition_, battery_view_, block_count_, cc_number_, cc_sent_, chime_ (+68 more)

### Community 3 - "Bridge"
Cohesion: 0.06
Nodes (20): add(), Bridge, send_panel(), fetch(), body_(), say(), Card(), integer() (+12 more)

### Community 4 - "InspectorAudio"
Cohesion: 0.03
Nodes (60): InspectorAudio, bend, block, chord_count, chord_degree, chord_kind, chord_notes, chord_quality (+52 more)

### Community 5 - "Engine"
Cohesion: 0.04
Nodes (34): Engine, capacity_, effects_, feedback_, flushed_, focus_, fx_before_loop_, has_reverb_ (+26 more)

### Community 6 - "Looper"
Cohesion: 0.05
Nodes (34): Looper, bp_, combo_, combo_used_, fade_, feedback_target_, filter_on_, frac_ (+26 more)

### Community 7 - "SlotSettings"
Cohesion: 0.11
Nodes (14): Entry, valid, value, SlotSettings, entries_, kBanks, kControls, kFileMax (+6 more)

### Community 8 - "Parameters"
Cohesion: 0.04
Nodes (51): Parameters, attack, bypass, cutoff, decay, feedback, filter_amount, filter_attack (+43 more)

### Community 9 - "forge_host.py"
Cohesion: 0.05
Nodes (61): 1. What Forge is, 2. How to talk to CHOMPI, 3. Patches, 5. Where things live (`firmware/chompi-forge/`), 6. How to make an effect, 7. How to edit the firmware, 8. Read next, Forge for other agents (Muse / Cosmo on the Tab5) (+53 more)

### Community 10 - "forge_audio.py"
Cohesion: 0.05
Nodes (32): Previous checkpoint: first hardware session and bridge test tooling, 2026-10-04, analyze(), audible_change(), cli(), clicks(), compare(), data_url(), db() (+24 more)

### Community 11 - "PresetMenu"
Cohesion: 0.07
Nodes (20): KeyToSlot(), PresetMenu, actions_, chompi_, chromatic_slot_, head_, kActions, kLoopSource (+12 more)

### Community 12 - "json"
Cohesion: 0.09
Nodes (4): main(), verify(), WindowsRuntimeTests, wav()

### Community 13 - "Parameter"
Cohesion: 0.05
Nodes (44): IsPerformance(), Parameter, Attack, Bypass, Compressor, Count, Cutoff, Decay (+36 more)

### Community 14 - "Voice"
Cohesion: 0.05
Nodes (44): Voice, age, amp, band, band_r, complete, end_frac, filter (+36 more)

### Community 15 - "Parts"
Cohesion: 0.05
Nodes (34): Parts, arp_left_, arp_note_, bass_, bass_change_, bass_count_, bass_left_, bass_step_ (+26 more)

### Community 16 - "main"
Cohesion: 0.07
Nodes (18): Recorder, dc_coefficient_, dc_l_, dc_r_, fade_, input_gain_, input_gain_target_, memory_ (+10 more)

### Community 17 - "forge_inspector.py"
Cohesion: 0.11
Nodes (19): control_value(), chord_label(), cli(), fetch(), collect(), decode(), byte(), u14() (+11 more)

### Community 18 - "TapeFx"
Cohesion: 0.05
Nodes (36): TapeFx, cutoff_, cutoff_target_, dcblock_fx_l_, dcblock_fx_r_, del_, dly_amt_, dly_amt_target_ (+28 more)

### Community 19 - "inspector.js"
Cohesion: 0.13
Nodes (37): action(), answer(), api(), bind(), bridge(), checks, controls(), disconnected() (+29 more)

### Community 20 - "looper_test.cpp"
Cohesion: 0.17
Nodes (27): CcButton, down, AutoCloseAndShortTakes(), CcButtons(), CombinationsDoNotTriggerSingleKeys(), FirstTakeSeamAndPlayback(), LimiterKeepsOverdubsBounded(), LoopClosesIntoOverdubAndFeedback() (+19 more)

### Community 21 - "sampler_test.cpp"
Cohesion: 0.17
Nodes (26): SampleDoneReply(), ChromaticAndKitLoading(), ChromaticPitch(), CubicTableRead(), FactoryTapeFiles(), Fill(), GateAndTrigger(), HeaderRoundTrip() (+18 more)

### Community 22 - "app.js"
Cohesion: 0.13
Nodes (20): api(), CONTROLS, drawSamples(), drawSlots(), GROUPS, KNOB_LABELS, KNOB_SWITCHES, KNOB_TARGETS (+12 more)

### Community 23 - "SampleLoader"
Cohesion: 0.06
Nodes (27): Inspect(), SampleLoader, count_, current_, cursor_, handoff_, head_, index_ (+19 more)

### Community 25 - ".ApplyPatch"
Cohesion: 0.19
Nodes (18): Development plan (the owner, 2026-10-02): features first, QA per feature later, Roadmap item 2: v3 instrument palette (implemented; software-tested), DecodeRequest(), Read14(), ExecuteRequest(), PackUsbSysEx(), PhysicalVersusInjectedAndVoiceEdges(), Framing() (+10 more)

### Community 26 - "v3_test.cpp"
Cohesion: 0.16
Nodes (24): Crossings(), Energy(), Hz(), LfoAndModWheel(), main(), NeutralV3IsAPlainVoice(), Peak(), Power() (+16 more)

### Community 27 - "Harmony / Intent Engine — brief"
Cohesion: 0.06
Nodes (31): AI compatibility, Arpeggiator, Bass generator, Chromatic keys, Clean-room requirement, Core architecture, Current resource guardrails, Desired result (+23 more)

### Community 28 - "FORGE_COLD"
Cohesion: 0.20
Nodes (4): Command, parameter, value, DecodeCC()

### Community 29 - "Clock"
Cohesion: 0.11
Nodes (15): Clock, interval_, last_tap_, pending_, phase_, rate_, since_, tap_sum_ (+7 more)

### Community 30 - "synth_test.cpp"
Cohesion: 0.11
Nodes (25): 0.3 integration review and browser testing — 2026-10-03, Done in the integration checkpoint (`15681b8`), Roadmap item 1: sustain pedal and pitch bend (implemented; software-tested), Sound fixes (implemented 2026-10-02 UTC; software-tested only), Atomicity and real-time boundary, RecoveryGate, AudioCallback(), AliasDb() (+17 more)

### Community 31 - "parameters.h"
Cohesion: 0.15
Nodes (5): Find(), Parse(), Clamp(), TapeSpeedKnob(), AudioBlock()

### Community 32 - "InstrumentTests"
Cohesion: 0.15
Nodes (3): envelope(), InstrumentTests, opener()

### Community 33 - "Rig"
Cohesion: 0.07
Nodes (24): PanelInput, frames, jack, keys, toggle_up, tone_down, turns, Rig (+16 more)

### Community 34 - "DjFilter"
Cohesion: 0.12
Nodes (14): DjFilter, control_, hp_, hp_c_, hp_target_, hpl_, hpr_, lp_ (+6 more)

### Community 37 - "Player"
Cohesion: 0.10
Nodes (16): Held, count, key, note, source, used, Player, held_ (+8 more)

### Community 38 - "Sequencer"
Cohesion: 0.12
Nodes (9): Sequencer, cursor_, held_, pending_off_, playing_, pos_, sequence_, state_ (+1 more)

### Community 39 - "Rig"
Cohesion: 0.15
Nodes (9): SampleTable, slots, Rig, engine, l, memory, r, rv (+1 more)

### Community 40 - "Request"
Cohesion: 0.07
Nodes (28): Request, action, bank, command, epoch, kind, mode, note (+20 more)

### Community 41 - "Response"
Cohesion: 0.06
Nodes (38): Error, Busy, Checksum, Empty, Length, None, Opcode, Patch (+30 more)

### Community 42 - "SampleFiles"
Cohesion: 0.17
Nodes (8): First optimization decisions to investigate, ParseSampleName(), SampleEvent, job, ok, SampleFiles, SamplePath(), FollowSampleJob()

### Community 43 - "firmware_chompi_forge_core_sequencer"
Cohesion: 0.13
Nodes (19): , DecodeFile(), EncodeFile(), Event, a, b, kind, tick (+11 more)

### Community 44 - "Pad"
Cohesion: 0.10
Nodes (17): Pad, amp, attack, end, gate, loop, pan_l, pan_r (+9 more)

### Community 45 - "slot_settings_test.cpp"
Cohesion: 0.14
Nodes (14): SelectSample(), Chromatic(), Kit(), main(), Near(), Rig, engine, l (+6 more)

### Community 47 - "LedView"
Cohesion: 0.06
Nodes (31): LedView, blink, count_in, dream, dream_level, flash, harmony, input_level (+23 more)

### Community 49 - "panel_test.cpp"
Cohesion: 0.23
Nodes (17): ComposeTransportLeds(), TapeSpeedRatio(), BatteryHold(), Dream018(), KeysKnobsAndOverrides(), KnobPages(), LedComposition(), LooperThroughThePanel() (+9 more)

### Community 50 - "SampleCard"
Cohesion: 0.10
Nodes (12): SampleCard, append_budget, fail_reads, fail_writes, files, kTempName, opens, read_name_ (+4 more)

### Community 51 - "parts_test.cpp"
Cohesion: 0.14
Nodes (15): Arp(), ArpPatterns(), BassModesAndMidi(), ChordGathering(), GateLatchAndOwnership(), main(), Note, note (+7 more)

### Community 52 - "knob_layout.h"
Cohesion: 0.14
Nodes (26): 4. The panel, for visualizations, Code, Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10), Patch knobs (patch version 5), Tests, What the player sees, Fade(), Fade3() (+18 more)

### Community 53 - "panel_controller.h"
Cohesion: 0.08
Nodes (22): StepColour(), ChordColour(), ComposeHarmonyKnobLeds(), ComposeLeds(), ComposeLooperLeds(), ComposeMenuKnobLeds(), ComposePartsKnobLeds(), ComposeSynthKnobLeds() (+14 more)

### Community 54 - "Settings"
Cohesion: 0.08
Nodes (31): Apply request layout, Inspect(), PartsWordsValid(), Bass, Alternate, Fifth, Octave, Off (+23 more)

### Community 55 - "PresetStore"
Cohesion: 0.19
Nodes (6): Crc16(), DecodePresetRecord(), EncodePresetRecord(), PresetPath(), PresetStore, Storage

### Community 56 - "Effects"
Cohesion: 0.12
Nodes (16): Effects, active_, compressor_, compressor_target_, dj_, dj_active_, left_, makeup_ (+8 more)

### Community 57 - "LoaderRig"
Cohesion: 0.18
Nodes (10): LoaderRig, card, events, handoff, loader, loop, pool, recorder (+2 more)

### Community 58 - "run_bench.py"
Cohesion: 0.15
Nodes (8): Bench, check(), demangle(), forge_runs(), setup(), main(), run(), upstream_runs()

### Community 59 - "InspectorStorage"
Cohesion: 0.12
Nodes (16): InspectorStorage, busy, errors, file_frames, file_loaded_frames, job, last_error, loaded_selection (+8 more)

### Community 60 - "power.h"
Cohesion: 0.16
Nodes (20): BatteryWarning(), InstallPowerOk(), Lockout, None, Unplugged, WeakSupply, LockoutText(), Readings (+12 more)

### Community 61 - "RequestKind"
Cohesion: 0.09
Nodes (22): RequestKind, Bend, Clock, Erase, List, Looper, ModWheel, Note (+14 more)

### Community 62 - "wav.h"
Cohesion: 0.14
Nodes (19): ConvertFrames(), ParseWav(), ReadLe16(), ReadLe32(), WavError, HeaderTooLarge, None, NotWav (+11 more)

### Community 63 - "sequencer_test.cpp"
Cohesion: 0.12
Nodes (8): Clocked, clock, out, s, main(), MemoryCard, files, RecordAndLoop()

### Community 64 - "file_transfer_test.cpp"
Cohesion: 0.42
Nodes (13): Crc32(), Begin(), CodecAndNames(), Data(), Decode(), Firmware(), FirmwareGuards(), Fuzz() (+5 more)

### Community 65 - "Forge test results"
Cohesion: 0.15
Nodes (13): 2026-10-04 evening — first 0.7 session (the owner), development firmware (`b36ba99`), 2026-10-04 — first hardware session (the owner), firmware 0.6 development (`131776b`), 2026-10-05 — 0.9 on CHOMPI (the owner, verbal): open issue, random shut-off, 2026-10-05 — firmware 0.8 installed over USB: all automatic checks pass, 2026-10-05 — line in wired, 0.7 on CHOMPI, 0.8 bridge, 2026-10-05 — power on 0.8 (the owner, verbal): TEST_SESSION 8, 2026-10-05 — the owner's panel-map walk-through on 0.9 (verbal), 2026-10-10 (late) — start-up stall found and fixed on CHOMPI (+5 more)

### Community 68 - "inspector.h"
Cohesion: 0.13
Nodes (18): EncodeInspector(), InspectorEvent, id, kind, serial, time_ms, value, InspectorLog (+10 more)

### Community 69 - "InspectorSystem"
Cohesion: 0.11
Nodes (19): InspectorSystem, battery, charge_state, crash_pc, crashed, dropped, emergencies, event_drops (+11 more)

### Community 70 - "Reverb"
Cohesion: 0.15
Nodes (12): Reverb, damp_, gain_, index_, kLines, length_, line_, longest_ (+4 more)

### Community 71 - "preset_test.cpp"
Cohesion: 0.21
Nodes (13): KnobOrder(), main(), MenuGestures(), Panel, menu, toggle, Protocol(), RecordsAndPaths() (+5 more)

### Community 72 - "FakeMidi"
Cohesion: 0.24
Nodes (3): FakeMidi, Message, TransportTests

### Community 73 - "Rig"
Cohesion: 0.08
Nodes (26): Change(), Dependency(), Features, balance, envelope, rms, spectrum, Fft() (+18 more)

### Community 74 - "MemoryCard"
Cohesion: 0.14
Nodes (11): BootloaderMatches(), SetAsideName(), BootloaderNames(), MemoryCard, corrupt_write, fail_append, fail_rename, files (+3 more)

### Community 75 - "TransferAndInstall"
Cohesion: 0.16
Nodes (10): InstallGate, armed_at_, kArmed, kConfirmed, kIdle, kTimeoutMs, state_, swallow_ (+2 more)

### Community 76 - "firmware_chompi_forge_core_protocol"
Cohesion: 0.12
Nodes (29): Patch lifecycle, 0.4 device presets on the SD card — 2026-10-03, Change discipline, , Checksum(), DecodePatchData(), EncodeError(), EncodeIdentity() (+21 more)

### Community 77 - "SampleJob"
Cohesion: 0.11
Nodes (17): Kind, Copy, Erase, Save, SampleJob, bank, frames, from_loop (+9 more)

### Community 78 - "Forge · Choowi changelog"
Cohesion: 0.06
Nodes (32): 0.10 TAPE parity — 2026-10-05 (ready to install; per-slot settings moved to 0.12), 0.11 Install safety — 2026-10-05 (software-tested; not installed yet), 0.12 Per-slot sample settings — 2026-10-05 (software-tested; not installed yet), 0.13 Harmony — 2026-10-05 (software-tested; not installed yet), 0.14 Arp, bass and tempo — 2026-10-05 (software-tested; not installed yet), 0.15.1 Review fixes — 2026-10-06 (software-tested; not installed yet), 0.15 Event recorder and projects — 2026-10-05 (software-tested; not installed yet), 0.16.0 Panel feedback and shortcuts (the owner's UX review; tool: Colour Lantern) — 2026-10-11 (software-tested; not installed yet) (+24 more)

### Community 79 - "argparse"
Cohesion: 0.14
Nodes (8): files(), main(), verify(), main(), run(), main(), repository_url(), windows_runtime()

### Community 80 - ".Valid"
Cohesion: 0.06
Nodes (40): 0.18.0 Magic: chomp to instrument, dream mode, lights with character (tool: Dream Bellows) — 2026-10-11 (software-tested; not installed yet), Chime, kDelay, kLength, kNotes, kStep, kTotal, kVelocity (+32 more)

### Community 81 - "Warble"
Cohesion: 0.13
Nodes (12): Warble, coefficient_, end_, frequency_, kLength, left_, length_, mix_ (+4 more)

### Community 82 - "SimulatedMido"
Cohesion: 0.07
Nodes (10): _Input, Message, _Output, _Port, SimulatedMido, PresetCliAndWebTests, SampleMessageTests, SampleWebAndAiTests (+2 more)

### Community 83 - "file_transfer.h"
Cohesion: 0.12
Nodes (22): AllowedUploadName(), DecodeFileRequest(), EncodeFileReply(), FileOp, Abort, Begin, Data, End (+14 more)

### Community 84 - "State"
Cohesion: 0.12
Nodes (16): InKey(), Layout, Real, Static, Pack(), State, block, enabled (+8 more)

### Community 85 - "InspectorEventKind"
Cohesion: 0.12
Nodes (16): InspectorEventKind, Card, KeyDown, KeyUp, Knob, PatchApply, QueueError, RecordingStart (+8 more)

### Community 86 - "EngineAndPanel"
Cohesion: 0.12
Nodes (10): Forge handoff — instrument candidate 0.15.1, Handoff: cloud session → the owner's local PC (2026-10-09), StoreQueued(), State, Armed, Empty, Playing, Recording (+2 more)

### Community 87 - "RecordGesture"
Cohesion: 0.13
Nodes (10): Event, None, Start, Stop, RecordGesture, chompi_, count_, counting_ (+2 more)

### Community 88 - "Mailbox"
Cohesion: 0.16
Nodes (10): Mailbox, bank, owner, sequence, slot, state, Owner, Host (+2 more)

### Community 89 - "FatFsUploadFiles"
Cohesion: 0.17
Nodes (7): FatFsUploadFiles, aside_, check_, file_, kAsideBatch, target_, verify_buffer_

### Community 90 - "iostream"
Cohesion: 0.16
Nodes (11): DelayImpulseAndBypass(), Fixture, engine, left, right, InitializationAndDryStereo(), main(), Near() (+3 more)

### Community 92 - "FakeMido"
Cohesion: 0.16
Nodes (3): FakeMido, TestTrafficCommands, interrupt()

### Community 93 - "WebTests"
Cohesion: 0.14
Nodes (7): Audit of the starting head, Exact next hardware test, Forge Inspector — first development slice, Realtime ownership, Run the Inspector, Validation at this checkpoint, WebTests

### Community 94 - "InspectorMailbox"
Cohesion: 0.27
Nodes (5): InspectorMailbox, owner_, state_, MailboxOwnership(), main()

### Community 95 - "SpscQueue"
Cohesion: 0.14
Nodes (7): SpscQueue, head_, storage_, tail_, ObserveVoiceEdges(), SetInspectorEvents(), ObserveVoiceEdges()

### Community 96 - "5. Proposed design"
Cohesion: 0.33
Nodes (6): 5.1 Sample streaming: shared page cache + pinned heads (one pool), 5.2 Underrun and failure semantics, 5.3 Streaming recorder, 5.4 Looper, 5.5 QSPI (Tier 2), 5. Proposed design

### Community 97 - "ChargerUsb"
Cohesion: 0.14
Nodes (13): 2026-10-10 — 0.15.2 on CHOMPI: first install since 0.9, unattended automatic run, Actions, force_detection, read, usb_to_daisy, ChargerUsb, asked_, handoff_ (+5 more)

### Community 98 - "FaultRecord"
Cohesion: 0.20
Nodes (10): Describe(), FaultRecord, cfsr, count, hfsr, kMagic, lr, magic (+2 more)

### Community 99 - "runtime.h"
Cohesion: 0.18
Nodes (15): Source map, 0.3 playability: sustain pedal and pitch bend — 2026-10-02, Roadmap item 3: device presets on the SD card (implemented; software-tested), EraseReply(), Ingress, Control, Critical, Emergency (+7 more)

### Community 100 - "FatFsSampleFiles"
Cohesion: 0.17
Nodes (5): FatFsSampleFiles, read_, reading_, write_, writing_

### Community 101 - "AnalysisTests"
Cohesion: 0.14
Nodes (4): AnalysisTests, walk(), wait(), sine()

### Community 103 - "Forge · Choowi: the user manual"
Cohesion: 0.07
Nodes (30): 10. On the computer, 11. What Forge keeps on the SD card, 12. Troubleshooting, 1. Quick start, 2. Installing and updating, 3. Power and battery, 4. The panel, 5. Knobs and knob pages (+22 more)

### Community 104 - "FakeChompi"
Cohesion: 0.14
Nodes (3): DetectionTests, louder_single(), FakeChompi

### Community 105 - "Chord"
Cohesion: 0.14
Nodes (14): Chord, degree, kind, quality, root, shifted, tone, Kind (+6 more)

### Community 106 - "main"
Cohesion: 0.25
Nodes (16): BootStep(), FlashSafeMode(), InspectorEvent(), InspectorStorageError(), LoadOptions(), LoadSlotSettings(), LogLockout(), LogRestart() (+8 more)

### Community 107 - "synth.h"
Cohesion: 0.21
Nodes (9): CubicQ14(), CubicTable, w, Dual16(), GlideSnap(), Inspect(), MakeCubicTable(), PackHigh() (+1 more)

### Community 109 - "Forge Resource Ledger"
Cohesion: 0.18
Nodes (11): Forge Resource Ledger, Memory map and working set, QA and CPU baseline, Reproduced baseline, Snapshot 2026-10-05: CPU fix A (0.13, not installed), Snapshot 2026-10-05: firmware 0.13 harmony (Phase 1 complete), Snapshot 2026-10-05: firmware 0.14 clock, arp and bass (not installed), Snapshot 2026-10-05: firmware 0.15 event recorder and projects (not installed) (+3 more)

### Community 110 - "SlotPolicy"
Cohesion: 0.40
Nodes (5): Checkpoint: firmware 0.12 per-slot sample settings (2026-10-05), SlotPolicy, Patch, Recall, Select

### Community 111 - "harmony_test.cpp"
Cohesion: 0.32
Nodes (9): Tones(), Degree(), Identity(), InScale(), Layouts(), main(), Notes(), PitchClasses() (+1 more)

### Community 112 - "FileTransfer"
Cohesion: 0.13
Nodes (12): FileTransfer, crc_, done_, head_, kTemp, name_, sample_written_, size_ (+4 more)

### Community 113 - "Performance"
Cohesion: 0.18
Nodes (10): Performance, compressor, dj_filter, dj_resonance, input_gain, pan, saturation, speed (+2 more)

### Community 114 - "harmony.h"
Cohesion: 0.30
Nodes (11): Lead(), Movement(), Parallel(), Place(), QualityStack(), RealFunction(), Resolve(), StaticFunction() (+3 more)

### Community 115 - "tape_bench.cpp"
Cohesion: 0.18
Nodes (3): bench_block(), bench_init(), Noise()

### Community 116 - "FatFsStorage"
Cohesion: 0.13
Nodes (9): Audio path, Boundaries, Device presets (SD card), Execution and ownership, Forge architecture, Transport details, FatFsStorage, file_ (+1 more)

### Community 118 - "Function"
Cohesion: 0.17
Nodes (12): Function, degree, kind, octave, quality, root, Quality, Dominant (+4 more)

### Community 119 - "MidiFrame"
Cohesion: 0.17
Nodes (12): Kind, CC, Clock, NoteOff, NoteOn, PitchBend, ProgramChange, SysEx (+4 more)

### Community 120 - "MenuAction"
Cohesion: 0.13
Nodes (11): MenuAction, bank, kind, mode, slot, to_bank, to_mode, to_slot (+3 more)

### Community 122 - "CardUploads"
Cohesion: 0.10
Nodes (6): CardUploads, open, temp, MemoryCard, files, SineWav()

### Community 125 - "MemoryStorage"
Cohesion: 0.18
Nodes (5): MemoryStorage, drop_writes, fail_writes, files, ready

### Community 126 - "SampleHandoff"
Cohesion: 0.16
Nodes (7): Samples (SD card) and the memory handoff, PackSelection(), SampleHandoff, ack_, published_, request_, FailedLoadIsVisible()

### Community 127 - "Kind"
Cohesion: 0.17
Nodes (11): Kind, Jack, Key, Press, Release, Toggle, Turn, PanelEvent (+3 more)

### Community 128 - "Envelope"
Cohesion: 0.18
Nodes (10): Envelope, release_step, stage, value, Stage, Attack, Decay, Off (+2 more)

### Community 129 - "Battery"
Cohesion: 0.17
Nodes (13): Battery, Full, High, Low, Medium, Unknown, DecodeStatus(), Status (+5 more)

### Community 130 - "LossyTransport"
Cohesion: 0.17
Nodes (3): DeadLink, LossyTransport, LostEndReply

### Community 132 - "Mode"
Cohesion: 0.20
Nodes (10): Mode, Dorian, HarmonicMinor, Locrian, Lydian, Major, MelodicMinor, Mixolydian (+2 more)

### Community 133 - "power_test.cpp"
Cohesion: 0.29
Nodes (8): BootGesture, held_, BootGestureOff(), ChargerHandover(), Colours(), main(), OptionsFile(), StatusDecode()

### Community 134 - "SampleSlot"
Cohesion: 0.12
Nodes (17): 10. Answers to the brief's questions, 1. Baseline (verified from source and the ELF), 2. What the code does now, 3. Precedent: stock TAPE already streams from SD on this hardware, 4. Architectural blockers and couplings, 6. SDRAM scenarios (estimates — not production values), 8. Hardware benchmark (Phase 0) — required before any buffer size is fixed, 9. Staged plan (each phase separately testable and revertible) (+9 more)

### Community 135 - "MenuPage"
Cohesion: 0.29
Nodes (6): MenuPage, Harmony, Parts, Presets, Samples, Synth

### Community 137 - "MidiPort"
Cohesion: 0.17
Nodes (7): MidiPort, clock_dropped, dropped, framer, frames, transport, PollMidi()

### Community 138 - "Options"
Cohesion: 0.22
Nodes (8): Options, midi_in, midi_out, monitor, quantise_menu, record_latch, split_delay, tape_slew

### Community 139 - "ResponseKind"
Cohesion: 0.22
Nodes (9): ResponseKind, Erased, Occupancy, SampleDone, SampleOccupancy, SampleSnapshot, Snapshot, Status (+1 more)

### Community 142 - "build"
Cohesion: 0.31
Nodes (9): 0.15.2 Build identity, held keys per source — 2026-10-10 (development build on CHOMPI since 2026-10-10, 34/34 automatic steps; the Shift and arp phrase-start fixes are software-tested only, not installed yet), build(), fromSlider(), loadPatch(), path(), read(), showJSON(), toSlider() (+1 more)

### Community 143 - "Forge — developer resume checkpoint"
Cohesion: 0.08
Nodes (26): 2026-10-11: agent hardware checks, soak, UX review, 2026-10-11 (end of day): 0.19 staged on the card, stress test done (resume here), 2026-10-11 (late night): 0.19.0 synth page built, 2026-10-11 (later): 0.16.0 Batch 1 + 2 built, 2026-10-11 (latest): 0.17.0 magic items 1-3 built, 2026-10-11 (night): 0.17.1 + 0.18.0 built, Spark chosen, Build environment notes, Checkpoint: firmware 0.10 TAPE parity, ready for the owner's install (2026-10-05) (+18 more)

### Community 145 - "InspectorVoice"
Cohesion: 0.25
Nodes (8): InspectorVoice, age, envelope, flags, note, slot, source, stage

### Community 146 - "Compressor"
Cohesion: 0.33
Nodes (5): Compressor, gain_, peak_, SoftClip(), SoftLimit()

### Community 147 - "Forge 0.19.0 — the one consolidated hardware test"
Cohesion: 0.08
Nodes (24): 0. Before you start (no CHOMPI needed), 1. Flash and identity, 1A. Inspector development candidate (before the normal audio checks), 2. External audio path (v1 compatibility), 3. Instrument, 3B. v3 instrument modules (firmware 0.4), 3C. Device presets on the SD card (TAPE-style keys + encoder), 3D. Sampler (TAPE-style, firmware 0.5) (+16 more)

### Community 149 - "State"
Cohesion: 0.29
Nodes (6): State, Armed, Empty, FirstTake, Paused, Playing

### Community 150 - "Mmf"
Cohesion: 0.24
Nodes (6): Mmf, buf0_, buf1_, MmfCoefficients, feedback, freq

### Community 151 - "Kind"
Cohesion: 0.20
Nodes (10): Kind, Copy, Erase, Recall, RecordSource, SampleCopy, SampleErase, SampleSave (+2 more)

### Community 152 - "forge_ai.py"
Cohesion: 0.08
Nodes (13): Current checkpoint: firmware 0.15.1 review fixes (2026-10-06), check(), image_problems(), symbols(), check_samples(), describe(), generate_patch(), NoRedirect (+5 more)

### Community 153 - "MidiFramer"
Cohesion: 0.22
Nodes (8): MidiFramer, bytes_, cc_, cc_used_, overflow_, status_, sysex_, used_

### Community 154 - "Pattern"
Cohesion: 0.29
Nodes (7): Pattern, Down, Off, Order, Random, Up, UpDown

### Community 158 - "SoundDeviceAudio"
Cohesion: 0.13
Nodes (3): FakeAudio, Skipped, SoundDeviceAudio

### Community 159 - "State"
Cohesion: 0.33
Nodes (6): State, Detaching, Headers, Idle, Job, Streaming

### Community 160 - "FirmwarePanelSink"
Cohesion: 0.22
Nodes (4): FirmwarePanelSink, PanelAction, action, patch

### Community 161 - "preset_menu.h"
Cohesion: 0.20
Nodes (11): BlackLed(), MenuMode, CopyDest, CopySource, Erase, None, Save, RenderMenuLeds() (+3 more)

### Community 164 - "forge_bench.cpp"
Cohesion: 0.60
Nodes (3): bench_block(), bench_init(), Noise()

### Community 165 - "KnobHold"
Cohesion: 0.40
Nodes (5): KnobHold, down, frames, turned, used

### Community 166 - "Queue"
Cohesion: 0.60
Nodes (5): Flash(), forge::Error RecallPreset(), LoadSequence(), Queue(), RunSampler()

### Community 168 - "Rate"
Cohesion: 0.29
Nodes (7): Rate, Eighth, EighthTriplet, Quarter, Sixteenth, SixteenthTriplet, ThirtySecond

### Community 169 - "the forge family"
Cohesion: 0.33
Nodes (6): Choowi, the first of the family, how it talks, the forge family, the look, the rules, the tree

### Community 172 - "MonitorMode"
Cohesion: 0.33
Nodes (4): MonitorMode, Both, Headphones, SendReturn

### Community 176 - ".run_walk"
Cohesion: 0.22
Nodes (3): BridgeWalkTests, WalkTests, hand()

### Community 177 - "HandleFrame"
Cohesion: 0.29
Nodes (10): CaptureInspector(), ChargerReadings(), CollectInspector(), HandleFrame(), HandleStorage(), InstallPowerOk(), RaiseEmergency(), ReadPower() (+2 more)

### Community 178 - "Earlier checkpoint: boot fix and cleanup, 2026-10-03"
Cohesion: 0.67
Nodes (3): Browser hardware test bridge (supersedes terminal-only delivery below), Earlier checkpoint: boot fix and cleanup, 2026-10-03, Prior telemetry implementation and validation

### Community 179 - "Forge hardware test bridge"
Cohesion: 0.17
Nodes (12): Audio interface wiring (for the automatic checks), Automatic checks, Controls and evidence, Easiest: Forge Bridge.exe, Exact first physical check, Firmware updates and card files without removing the card (firmware 0.7), Forge hardware test bridge, From a source checkout (+4 more)

### Community 180 - "Forge host controller"
Cohesion: 0.17
Nodes (12): Command-line controller, Development Forge Inspector, Device presets on the SD card, Forge host controller, Instrument patches and playing, Live control and saving, Live provider check, Offline DSP reference (+4 more)

### Community 181 - "Forge sampling (roadmap item 4): design"
Cohesion: 0.18
Nodes (11): 1. What TAPE does (the behaviour to keep), 2. What Forge changes, and why, 3. Memory, 4. Patch v4 (wire and JSON), 5. Ownership and data flow, 6. Panel, 7. Host, AI and webapp, 8. Verification plan (+3 more)

### Community 182 - "restart.h"
Cohesion: 0.29
Nodes (9): CountCrash(), DescribeEvent(), RanStably(), SafeModeStart(), StartupGuard, crashes, kMagic, magic (+1 more)

### Community 183 - "SlotSettingsSchedule"
Cohesion: 0.33
Nodes (6): SlotSettingsSchedule, changed_at, kMaxWait, kSettle, seen, wait

### Community 184 - "Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark"
Cohesion: 0.20
Nodes (10): 1. Bootloader acceptance, 2. Memory layout, 3. Audio and controls, 3a. SD card coexistence (device presets, samples), 4. MIDI conventions, 5. Toolchains and build reproducibility, 6. CPU benchmark (`make bench`), 7. Power and system behaviour (firmware 0.8 review, 2026-10-05) (+2 more)

### Community 185 - "Stock TAPE: what every control does (reference for Forge parity)"
Cohesion: 0.20
Nodes (10): Boot and power, CHOMPI, PLAY, LOOP, Effects and looper, Framework, Keys, Knobs, normal play, Menu (shift layer while CHOMPI held, toggle TRUE), MIDI, panic, files (+2 more)

### Community 186 - "Event"
Cohesion: 0.40
Nodes (4): Event, note, source, velocity

### Community 187 - "SlotValues"
Cohesion: 0.18
Nodes (10): SlotValues, attack, end, gain, loop, pan, pitch, release (+2 more)

### Community 188 - "MidiOut"
Cohesion: 0.33
Nodes (4): MidiOut, data1, data2, status

### Community 189 - "Forge control protocol — firmware 0.15"
Cohesion: 0.22
Nodes (9): Development probe and Inspector schema 1, Device presets (SD card), firmware 0.4, Envelope and operations, Event recorder loops on the SD card (firmware 0.15), Forge control protocol — firmware 0.15, MIDI clock (firmware 0.14), Notes, controls and recovery, Sampler, firmware 0.5 (+1 more)

### Community 191 - "Choowi: the first firmware of the forge"
Cohesion: 0.25
Nodes (8): Audio and controls, Build and test, Choowi: the first firmware of the forge, Device presets (SD card), First hardware session, Implementation boundaries, Sampler and recording (v4 patches), Synth and module patches

### Community 193 - "Action"
Cohesion: 0.29
Nodes (7): Action, a, b, kind, Kind, Note, Param

### Community 194 - "First time home with Forge 0.19.0"
Cohesion: 0.25
Nodes (7): 0. Power first (before any install), 1. Get the files, 2. Install 0.15.1, 3. Fix the rig (from the last session's results), 4. Run the bridge, 5. Send the results back, First time home with Forge 0.19.0

### Community 195 - "forge_main.cpp"
Cohesion: 0.18
Nodes (11): Balance(), CopyFile(), DrawLeds(), InstallFaultHandler(), Pth(), Restart(), SaveSlotSettings(), SendPanelMidi() (+3 more)

### Community 197 - "Ponytail, lazy senior dev mode"
Cohesion: 0.33
Nodes (4): Forge development guidance, Before you write, Ponytail, lazy senior dev mode, The smallest complete change

### Community 198 - "Forge looping (roadmap item 5): design"
Cohesion: 0.33
Nodes (6): 1. What TAPE does (the behaviour to keep), 2. Forge plan, 3. Decisions (the owner, 2026-10-03), 4. Risks, 5. Build order (each step tested before the next), Forge looping (roadmap item 5): design

### Community 199 - "Forge project brief"
Cohesion: 0.33
Nodes (6): Architecture and acceptance, Current milestone: instrument candidate 0.15.1, First milestone: instrument candidate 0.4, Forge project brief, Goal and user decisions, Roadmap (priority order agreed 2026-10-02)

### Community 201 - "BootCount"
Cohesion: 0.67
Nodes (3): BootCount, boots, magic

### Community 204 - "Forge — live OpenAI / Gemini test"
Cohesion: 0.40
Nodes (5): Forge — live OpenAI / Gemini test, If it fails, Rules, Step 1 — command-line preflight (one request per provider), Step 2 — webapp

### Community 205 - "the forge docs"
Cohesion: 0.40
Nodes (5): Choose a starting point, Documentation map, Maintaining these docs, the forge docs, What is implemented and what is pending

### Community 206 - "Colour"
Cohesion: 0.40
Nodes (5): BatteryColour(), Colour, b, g, r

### Community 207 - "Forge hardware test kit — instructions for an agent on the test PC"
Cohesion: 0.40
Nodes (4): Forge hardware test kit — instructions for an agent on the test PC, Reference, Rules, What you can do here

### Community 208 - "ProjectsOnTheCard"
Cohesion: 0.56
Nodes (6): CopyFile(), EraseFile(), LoadFile(), SaveFile(), FilePath(), ProjectsOnTheCard()

### Community 210 - "start"
Cohesion: 0.67
Nodes (4): notice(), run(), start(), updateButtons()

### Community 211 - "Outgoing"
Cohesion: 0.50
Nodes (4): Outgoing, bytes, size, source

### Community 212 - "Forge UX review (2026-10-11)"
Cohesion: 0.67
Nodes (3): Cross-cutting fixes (they lift most rows), Forge UX review (2026-10-11), Ratings

### Community 215 - "Extension"
Cohesion: 0.29
Nodes (7): Extension, Eleventh, Fifth, Ninth, Seventh, Thirteenth, Triad

## Knowledge Gaps
- **1355 isolated node(s):** `reverb_`, `del_`, `filter_`, `warble_`, `dcblock_fx_l_` (+1350 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 2159 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **44 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `PanelController` connect `PanelController` to `Options`, `PresetMenu`, `main`, `PanelSink`, `forge_ai.py`, `EngineAndPanel`, `.ApplyPatch`, `Clock`, `Rig`, `KnobHold`, `MonitorMode`, `panel_controller.h`, `MidiOut`, `.MenuPacked`, `Rig`, `TransferAndInstall`, `.Valid`, `EngineAndPanel`, `RecordGesture`, `Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05`, `.Source`, `SpscQueue`, `Kind`?**
  _High betweenness centrality (0.141) - this node is a cross-community bridge._
- **Are the 9 inferred relationships involving `Engine` (e.g. with `main()` and `PhysicalVersusInjectedAndVoiceEdges()`) actually correct?**
  _`Engine` has 9 INFERRED edges - model-reasoned connections that need verification._
- **What connects `reverb_`, `del_`, `filter_` to the rest of the system?**
  _1355 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Synth` be split into smaller, more focused modules?**
  _Cohesion score 0.022982635342185902 - nodes in this community are weakly interconnected._
- **Why does `Engine` connect `Engine` to `SlotSettings`, `main`, `looper_test.cpp`, `PanelSink`, `EngineAndPanel`, `.ApplyPatch`, `v3_test.cpp`, `FORGE_COLD`, `Clock`, `synth_test.cpp`, `parameters.h`, `Rig`, `Player`, `Sequencer`, `Rig`, `slot_settings_test.cpp`, `panel_controller.h`, `SlotValues`, `forge_main.cpp`, `Reverb`, `Rig`, `EngineAndPanel`, `Mailbox`, `iostream`, `runtime.h`, `forge_probe.cpp`, `Performance`, `SampleHandoff`?**
  _High betweenness centrality (0.117) - this node is a cross-community bridge._
- **Are the 5 inferred relationships involving `PanelController` (e.g. with `Current checkpoint: firmware 0.15.1 review fixes (2026-10-06)` and `Handoff: cloud session → the owner's local PC (2026-10-09)`) actually correct?**
  _`PanelController` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Should `Walk` be split into smaller, more focused modules?**
  _Cohesion score 0.12012012012012012 - nodes in this community are weakly interconnected._