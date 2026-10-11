# Graph Report - CHOMPI  (2026-10-11)

## Corpus Check
- 139 files · ~254,103 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 4, .css 2, .ld 1)

## Summary
- 4097 nodes · 8068 edges · 208 communities (180 shown, 28 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 925 edges (avg confidence: 0.87)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `e7e3f0a0`
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
- .Init
- v3_test.cpp
- Harmony / Intent Engine — brief
- .ApplyPatch
- Clock
- synth_test.cpp
- panel_controller.h
- main
- Rig
- DjFilter
- CardTests
- tempo_bench.cpp
- Player
- Sequencer
- .Block
- Request
- Response
- SampleFiles
- firmware_chompi_forge_core_sequencer
- Pad
- .Note
- slot_settings_test.cpp
- LedView
- BrowserTests
- PanelFeedback016
- SampleCard
- parts_test.cpp
- knob_layout.h
- Rgb
- Settings
- PresetStore
- Effects
- LoaderRig
- run_bench.py
- InspectorStorage
- power.h
- RequestKind
- wav.h
- Clocked
- file_transfer_test.cpp
- forge_ai.py
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
- Forge changelog
- argparse
- Roll
- Warble
- SimulatedMido
- file_transfer.h
- State
- InspectorEventKind
- SaveFile
- RecordGesture
- Mailbox
- FatFsUploadFiles
- core_test.cpp
- BridgeTests
- FakeMido
- WebTests
- InspectorMailbox
- SpscQueue
- SampleSlot
- ChargerUsb
- FaultRecord
- runtime.h
- FatFsSampleFiles
- AnalysisTests
- Device
- Forge for CHOMPI — user manual
- harmony.h
- Chord
- Card
- FakeChompi
- InstrumentTests
- InspectorTests
- SlotPolicy
- harmony_test.cpp
- FileTransfer
- Performance
- Battery
- tape_bench.cpp
- FatFsStorage
- Function
- MidiFrame
- MenuAction
- Error
- CardUploads
- MidiPort
- BridgeBrowserTests
- MemoryStorage
- SampleHandoff
- Kind
- Envelope
- BridgeAutomaticTests
- LossyTransport
- Handler
- Mode
- power_test.cpp
- Rig
- preset_menu.h
- .Process
- MidiFramer
- Options
- ResponseKind
- Transport
- Chime
- FirmwarePanelSink
- Forge — developer resume checkpoint
- ProviderTests
- InspectorVoice
- Compressor
- Forge 0.17.0 — the one consolidated hardware test
- Extension
- State
- SessionDevice
- Kind
- struct
- Action
- State
- Queue
- V3Field
- SoundDeviceAudio
- State
- Sink
- MenuMode
- Mmf
- forge_bridge.py
- forge_bench.cpp
- KnobHold
- Event
- Colour
- .ProcessActive
- FakePorts
- BridgeWebTests
- V4PatchTests
- MonitorMode
- Outgoing
- AiCheckTests
- SaveSlotSettings
- BridgeCardTests
- forge_main.cpp
- Message
- Forge hardware test bridge
- Forge host controller
- Forge sampling (roadmap item 4): design
- restart.h
- SlotSettingsSchedule
- Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark
- Stock TAPE: what every control does (reference for Forge parity)
- 5. Proposed design
- SlotValues
- .RecordParam
- Forge control protocol — firmware 0.15
- Sink
- Forge 0.5 — consolidated test candidate
- InspectorSnapshot
- .prepare
- First time home with Forge 0.17.0
- DrawLeds
- body_
- Ponytail, lazy senior dev mode
- Forge looping (roadmap item 5): design
- Forge project brief
- StoreQueued
- Forge — live OpenAI / Gemini test
- Forge documentation
- Forge hardware test kit — instructions for an agent on the test PC
- Rng

## God Nodes (most connected - your core abstractions)
1. `Engine` - 148 edges
2. `Synth` - 129 edges
3. `PanelController` - 128 edges
4. `Looper` - 81 edges
5. `Parts` - 81 edges
6. `Parameters` - 71 edges
7. `InspectorAudio` - 69 edges
8. `SampleLoader` - 58 edges
9. `main()` - 54 edges
10. `Parameter` - 53 edges

## Surprising Connections (you probably didn't know these)
- `Patch lifecycle` --references--> `V3Fields()`  [INFERRED]
  docs/forge/ARCHITECTURE.md → firmware/chompi-forge/core/protocol.h
- `Change discipline` --references--> `V3Fields()`  [INFERRED]
  docs/forge/DEVELOPMENT.md → firmware/chompi-forge/core/protocol.h
- `Development probe and Inspector schema 1` --references--> `EncodePatchData()`  [INFERRED]
  docs/forge/PROTOCOL.md → firmware/chompi-forge/core/protocol.h
- `0.3 playability: sustain pedal and pitch bend — 2026-10-02` --references--> `TranslateChannel()`  [INFERRED]
  docs/forge/CHANGELOG.md → firmware/chompi-forge/core/runtime.h
- `6. How to make an effect` --references--> `upgrade_patch()`  [INFERRED]
  docs/forge/AGENT_GUIDE.md → firmware/chompi-forge/host/forge_host.py

## Import Cycles
- None detected.

## Communities (208 total, 28 thin omitted)

### Community 0 - "Synth"
Cohesion: 0.02
Nodes (75): Synth, age_, amp_, bend_slew_, bend_target_, cap_, coefficient_, cutoff_ (+67 more)

### Community 1 - "Walk"
Cohesion: 0.06
Nodes (26): Previous checkpoint: first hardware session and bridge test tooling, 2026-10-04, Build firmware, Change discipline, CPU benchmark against stock firmware, Create a test bundle, Forge developer guide, Get the development branch, Requirements (+18 more)

### Community 2 - "PanelController"
Cohesion: 0.03
Nodes (69): PanelController, battery_view_, block_count_, cc_number_, cc_sent_, chime_, chime_request_, chompi_was_ (+61 more)

### Community 3 - "Bridge"
Cohesion: 0.22
Nodes (4): Bridge, fetch(), now(), sounds_like()

### Community 4 - "InspectorAudio"
Cohesion: 0.03
Nodes (61): InspectorAudio, bend, block, chord_count, chord_degree, chord_kind, chord_notes, chord_quality (+53 more)

### Community 5 - "Engine"
Cohesion: 0.04
Nodes (34): Engine, capacity_, effects_, feedback_, flushed_, focus_, fx_before_loop_, has_reverb_ (+26 more)

### Community 6 - "Looper"
Cohesion: 0.05
Nodes (33): Looper, bp_, combo_, combo_used_, fade_, feedback_target_, filter_on_, frac_ (+25 more)

### Community 7 - "SlotSettings"
Cohesion: 0.11
Nodes (14): FollowSampleJob(), Entry, valid, value, SlotSettings, entries_, kBanks, kControls (+6 more)

### Community 8 - "Parameters"
Cohesion: 0.04
Nodes (51): Parameters, attack, bypass, cutoff, decay, feedback, filter_amount, filter_attack (+43 more)

### Community 9 - "forge_host.py"
Cohesion: 0.06
Nodes (39): 0.6 knob pages and patch knobs — 2026-10-04, load_starter_presets(), midi_ports(), message(), check_parts(), check_value(), cli(), describe_power() (+31 more)

### Community 10 - "forge_audio.py"
Cohesion: 0.05
Nodes (32): Hardware: 0.15.2 on CHOMPI, unattended run (2026-10-10 evening), 2026-10-10 — 0.15.2 on CHOMPI: first install since 0.9, unattended automatic run, analyze(), audible_change(), cli(), clicks(), compare(), data_url() (+24 more)

### Community 11 - "PresetMenu"
Cohesion: 0.07
Nodes (20): KeyToSlot(), PresetMenu, actions_, chompi_, chromatic_slot_, head_, kActions, kLoopSource (+12 more)

### Community 12 - "json"
Cohesion: 0.11
Nodes (3): verify(), WindowsRuntimeTests, wav()

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
Nodes (20): Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05, Recorder, dc_coefficient_, dc_l_, dc_r_, fade_, input_gain_, input_gain_target_ (+12 more)

### Community 17 - "forge_inspector.py"
Cohesion: 0.05
Nodes (41): 1. What Forge is, 2. How to talk to CHOMPI, 3. Patches, 5. Where things live (`firmware/chompi-forge/`), 6. How to make an effect, 7. How to edit the firmware, 8. Read next, Forge for other agents (Muse / Cosmo on the Tab5) (+33 more)

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
Cohesion: 0.18
Nodes (24): ChromaticAndKitLoading(), ChromaticPitch(), CubicTableRead(), FactoryTapeFiles(), Fill(), GateAndTrigger(), HeaderRoundTrip(), KitMapping() (+16 more)

### Community 22 - "app.js"
Cohesion: 0.10
Nodes (33): 0.15.2 Build identity, held keys per source — 2026-10-10 (development build on CHOMPI since 2026-10-10, 34/34 automatic steps; the Shift and arp phrase-start fixes are software-tested only, not installed yet), api(), build(), CONTROLS, drawSamples(), drawSlots(), fromSlider(), GROUPS (+25 more)

### Community 23 - "SampleLoader"
Cohesion: 0.06
Nodes (27): Inspect(), SampleLoader, count_, current_, cursor_, handoff_, head_, index_ (+19 more)

### Community 24 - "EngineAndPanel"
Cohesion: 0.09
Nodes (9): Inspect(), MidiOut, data1, data2, status, EngineAndPanel(), PhysicalVersusInjectedAndVoiceEdges(), EngineAndPanel() (+1 more)

### Community 25 - ".Init"
Cohesion: 0.16
Nodes (24): 0.4 device presets on the SD card — 2026-10-03, Roadmap item 2: v3 instrument palette (implemented; software-tested), DecodePatchData(), DecodeRequest(), EncodePatchData(), EncodeResponse(), PatchDataSize(), Read14() (+16 more)

### Community 26 - "v3_test.cpp"
Cohesion: 0.16
Nodes (25): GlideSnap(), Crossings(), Energy(), Hz(), LfoAndModWheel(), main(), NeutralV3IsAPlainVoice(), Peak() (+17 more)

### Community 27 - "Harmony / Intent Engine — brief"
Cohesion: 0.07
Nodes (30): AI compatibility, Arpeggiator, Bass generator, Chromatic keys, Clean-room requirement, Core architecture, Current resource guardrails, Desired result (+22 more)

### Community 28 - ".ApplyPatch"
Cohesion: 0.19
Nodes (4): Command, parameter, value, DecodeCC()

### Community 29 - "Clock"
Cohesion: 0.13
Nodes (11): Phase 0 decisions (DC, 2026-10-05), Clock, interval_, last_tap_, pending_, phase_, rate_, since_ (+3 more)

### Community 30 - "synth_test.cpp"
Cohesion: 0.10
Nodes (26): Execution and ownership, 0.3 integration review and browser testing — 2026-10-03, Done in the integration checkpoint (`15681b8`), Roadmap item 1: sustain pedal and pitch bend (implemented; software-tested), Sound fixes (implemented 2026-10-02 UTC; software-tested only), Atomicity and real-time boundary, RecoveryGate, AudioCallback() (+18 more)

### Community 31 - "panel_controller.h"
Cohesion: 0.11
Nodes (10): Find(), Parse(), MagicCombo(), Clamp(), TapeSpeedKnob(), AudioBlock(), CubicQ14(), CubicTable (+2 more)

### Community 32 - "main"
Cohesion: 0.26
Nodes (15): BootStep(), FlashSafeMode(), InspectorEvent(), InspectorStorageError(), LoadOptions(), LoadSlotSettings(), LogLockout(), LogRestart() (+7 more)

### Community 33 - "Rig"
Cohesion: 0.10
Nodes (17): Rig, engine, hw, l, panel, r, rec, recorder (+9 more)

### Community 34 - "DjFilter"
Cohesion: 0.12
Nodes (14): DjFilter, control_, hp_, hp_c_, hp_target_, hpl_, hpr_, lp_ (+6 more)

### Community 35 - "CardTests"
Cohesion: 0.15
Nodes (3): Checkpoint: per-source held keys (HANDOFF item 1, 2026-10-10, local PC), CardTests, Sequence

### Community 37 - "Player"
Cohesion: 0.09
Nodes (17): 2026-10-11 — automatic run on the SD-fix build (DC away, webcam + UMC), Held, count, key, note, source, used, Player (+9 more)

### Community 38 - "Sequencer"
Cohesion: 0.13
Nodes (9): Sequencer, cursor_, held_, pending_off_, playing_, pos_, sequence_, state_ (+1 more)

### Community 39 - ".Block"
Cohesion: 0.08
Nodes (11): PackLooper(), PanelInput, frames, jack, keys, toggle_up, tone_down, turns (+3 more)

### Community 40 - "Request"
Cohesion: 0.08
Nodes (24): Request, action, bank, command, epoch, kind, mode, note (+16 more)

### Community 41 - "Response"
Cohesion: 0.07
Nodes (27): Response, action, bank, battery, capacity_ms, charge_state, cpu_average, cpu_max (+19 more)

### Community 42 - "SampleFiles"
Cohesion: 0.18
Nodes (7): 2. What the code does now, ParseSampleName(), SampleEvent, job, ok, SampleFiles, SamplePath()

### Community 43 - "firmware_chompi_forge_core_sequencer"
Cohesion: 0.13
Nodes (19): , DecodeFile(), EncodeFile(), Event, a, b, kind, tick (+11 more)

### Community 44 - "Pad"
Cohesion: 0.09
Nodes (17): Pad, amp, attack, end, gate, loop, pan_l, pan_r (+9 more)

### Community 46 - "slot_settings_test.cpp"
Cohesion: 0.13
Nodes (14): Chromatic(), Kit(), main(), Near(), Rig, engine, l, r (+6 more)

### Community 47 - "LedView"
Cohesion: 0.07
Nodes (27): LedView, blink, count_in, flash, harmony, input_level, install, keys_down (+19 more)

### Community 49 - "PanelFeedback016"
Cohesion: 0.21
Nodes (14): TapeSpeedRatio(), BatteryHold(), KeysKnobsAndOverrides(), KnobPages(), LedComposition(), LooperSaveGesture(), LooperThroughThePanel(), LooperVoiceCap() (+6 more)

### Community 50 - "SampleCard"
Cohesion: 0.08
Nodes (14): SampleCard, append_budget, fail_reads, fail_writes, files, kTempName, opens, read_name_ (+6 more)

### Community 51 - "parts_test.cpp"
Cohesion: 0.17
Nodes (15): Arp(), ArpPatterns(), BassModesAndMidi(), ChordGathering(), GateLatchAndOwnership(), main(), Note, note (+7 more)

### Community 52 - "knob_layout.h"
Cohesion: 0.15
Nodes (24): Code, Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10), Patch knobs (patch version 5), Tests, What the player sees, Fade(), Fade3(), Fade4() (+16 more)

### Community 53 - "Rgb"
Cohesion: 0.14
Nodes (21): 4. The panel, for visualizations, Cross-cutting fixes (they lift most rows), Forge UX review (2026-10-11), Ratings, StepColour(), ChordColour(), ComposeHarmonyKnobLeds(), ComposeKnobLeds() (+13 more)

### Community 54 - "Settings"
Cohesion: 0.05
Nodes (45): Apply request layout, Inspect(), PartsWordsValid(), Bass, Alternate, Fifth, Octave, Off (+37 more)

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

### Community 63 - "Clocked"
Cohesion: 0.12
Nodes (6): Clocked, clock, out, s, MemoryCard, files

### Community 64 - "file_transfer_test.cpp"
Cohesion: 0.42
Nodes (13): Crc32(), Begin(), CodecAndNames(), Data(), Decode(), Firmware(), FirmwareGuards(), Fuzz() (+5 more)

### Community 66 - "forge_ai.py"
Cohesion: 0.10
Nodes (10): Current checkpoint: firmware 0.15.1 review fixes (2026-10-06), check_samples(), describe(), generate_patch(), NoRedirect, provider_schema(), ProviderError, sample_summary() (+2 more)

### Community 67 - "probe"
Cohesion: 0.14
Nodes (3): PatchTests, probe(), PresetProtocolTests

### Community 68 - "inspector.h"
Cohesion: 0.13
Nodes (14): ObserveVoiceEdges(), InspectorEvent, id, kind, serial, time_ms, value, InspectorLog (+6 more)

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
Cohesion: 0.13
Nodes (4): FakeMidi, Message, FakePort, TransportTests

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
Cohesion: 0.21
Nodes (16): EncodeInspector(), , Checksum(), EncodeError(), EncodeIdentity(), EncodePanelAck(), EncodeProbeLeds(), Header() (+8 more)

### Community 77 - "SampleJob"
Cohesion: 0.11
Nodes (17): Kind, Copy, Erase, Save, SampleJob, bank, frames, from_loop (+9 more)

### Community 78 - "Forge changelog"
Cohesion: 0.06
Nodes (31): 0.10 TAPE parity — 2026-10-05 (ready to install; per-slot settings moved to 0.12), 0.11 Install safety — 2026-10-05 (software-tested; not installed yet), 0.12 Per-slot sample settings — 2026-10-05 (software-tested; not installed yet), 0.13 Harmony — 2026-10-05 (software-tested; not installed yet), 0.14 Arp, bass and tempo — 2026-10-05 (software-tested; not installed yet), 0.15.1 Review fixes — 2026-10-06 (software-tested; not installed yet), 0.15 Event recorder and projects — 2026-10-05 (software-tested; not installed yet), 0.16.0 Panel feedback and shortcuts (DC's UX review) — 2026-10-11 (software-tested; not installed yet) (+23 more)

### Community 79 - "argparse"
Cohesion: 0.13
Nodes (7): files(), main(), verify(), ForgeServer, main(), main(), windows_runtime()

### Community 80 - "Roll"
Cohesion: 0.20
Nodes (10): Mutate(), Rng, s, Roll(), Tick(), Base(), main(), MutationsDriftInsideTheirBounds() (+2 more)

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

### Community 86 - "SaveFile"
Cohesion: 0.57
Nodes (5): CopyFile(), EraseFile(), LoadFile(), SaveFile(), FilePath()

### Community 87 - "RecordGesture"
Cohesion: 0.13
Nodes (10): Event, None, Start, Stop, RecordGesture, chompi_, count_, counting_ (+2 more)

### Community 88 - "Mailbox"
Cohesion: 0.17
Nodes (11): Mailbox, bank, owner, sequence, slot, state, Owner, Host (+3 more)

### Community 89 - "FatFsUploadFiles"
Cohesion: 0.17
Nodes (7): FatFsUploadFiles, aside_, check_, file_, kAsideBatch, target_, verify_buffer_

### Community 90 - "core_test.cpp"
Cohesion: 0.21
Nodes (11): DelayImpulseAndBypass(), Fixture, engine, left, right, InitializationAndDryStereo(), main(), Near() (+3 more)

### Community 92 - "FakeMido"
Cohesion: 0.16
Nodes (3): FakeMido, TestTrafficCommands, interrupt()

### Community 93 - "WebTests"
Cohesion: 0.14
Nodes (7): Audit of the starting head, Exact next hardware test, Forge Inspector — first development slice, Realtime ownership, Run the Inspector, Validation at this checkpoint, WebTests

### Community 94 - "InspectorMailbox"
Cohesion: 0.31
Nodes (4): InspectorMailbox, owner_, state_, MailboxOwnership()

### Community 95 - "SpscQueue"
Cohesion: 0.08
Nodes (18): First optimization decisions to investigate, Forge Resource Ledger, Memory map and working set, QA and CPU baseline, Reproduced baseline, Snapshot 2026-10-05: CPU fix A (0.13, not installed), Snapshot 2026-10-05: firmware 0.13 harmony (Phase 1 complete), Snapshot 2026-10-05: firmware 0.14 clock, arp and bass (not installed) (+10 more)

### Community 96 - "SampleSlot"
Cohesion: 0.09
Nodes (21): 10. Answers to the brief's questions, 1. Baseline (verified from source and the ELF), 3. Precedent: stock TAPE already streams from SD on this hardware, 4. Architectural blockers and couplings, 6. SDRAM scenarios (estimates — not production values), 7. Code, CPU and main-loop cost (estimates), 8. Hardware benchmark (Phase 0) — required before any buffer size is fixed, 9. Staged plan (each phase separately testable and revertible) (+13 more)

### Community 97 - "ChargerUsb"
Cohesion: 0.15
Nodes (13): Actions, force_detection, read, usb_to_daisy, ChargerUsb, asked_, handoff_, handoff_at_ (+5 more)

### Community 98 - "FaultRecord"
Cohesion: 0.20
Nodes (10): Describe(), FaultRecord, cfsr, count, hfsr, kMagic, lr, magic (+2 more)

### Community 99 - "runtime.h"
Cohesion: 0.21
Nodes (13): Roadmap item 3: device presets on the SD card (implemented; software-tested), EraseReply(), Ingress, Control, Critical, Emergency, Ignore, Storage (+5 more)

### Community 100 - "FatFsSampleFiles"
Cohesion: 0.17
Nodes (5): FatFsSampleFiles, read_, reading_, write_, writing_

### Community 101 - "AnalysisTests"
Cohesion: 0.14
Nodes (4): AnalysisTests, walk(), wait(), sine()

### Community 102 - "Device"
Cohesion: 0.16
Nodes (3): BridgeWalkTests, SetupCheckTests, Device

### Community 103 - "Forge for CHOMPI — user manual"
Cohesion: 0.07
Nodes (29): 10. On the computer, 11. What Forge keeps on the SD card, 12. Troubleshooting, 1. Quick start, 2. Installing and updating, 3. Power and battery, 4. The panel, 5. Knobs and knob pages (+21 more)

### Community 104 - "harmony.h"
Cohesion: 0.30
Nodes (11): Lead(), Movement(), Parallel(), Place(), QualityStack(), RealFunction(), Resolve(), StaticFunction() (+3 more)

### Community 105 - "Chord"
Cohesion: 0.14
Nodes (14): Chord, degree, kind, quality, root, shifted, tone, Kind (+6 more)

### Community 106 - "Card"
Cohesion: 0.16
Nodes (8): Checkpoint: firmware 0.11 install safety (2026-10-05), allowed(), Card, cli(), progress(), files_in(), pack7(), word35()

### Community 107 - "FakeChompi"
Cohesion: 0.14
Nodes (3): DetectionTests, louder_single(), FakeChompi

### Community 108 - "InstrumentTests"
Cohesion: 0.15
Nodes (3): envelope(), InstrumentTests, opener()

### Community 110 - "SlotPolicy"
Cohesion: 0.40
Nodes (5): Checkpoint: firmware 0.12 per-slot sample settings (2026-10-05), SlotPolicy, Patch, Recall, Select

### Community 111 - "harmony_test.cpp"
Cohesion: 0.17
Nodes (10): Tones(), Degree(), Identity(), InScale(), Layouts(), main(), Notes(), PitchClasses() (+2 more)

### Community 112 - "FileTransfer"
Cohesion: 0.13
Nodes (12): FileTransfer, crc_, done_, head_, kTemp, name_, sample_written_, size_ (+4 more)

### Community 113 - "Performance"
Cohesion: 0.18
Nodes (10): Performance, compressor, dj_filter, dj_resonance, input_gain, pan, saturation, speed (+2 more)

### Community 114 - "Battery"
Cohesion: 0.17
Nodes (13): Battery, Full, High, Low, Medium, Unknown, DecodeStatus(), Status (+5 more)

### Community 115 - "tape_bench.cpp"
Cohesion: 0.18
Nodes (3): bench_block(), bench_init(), Noise()

### Community 116 - "FatFsStorage"
Cohesion: 0.21
Nodes (3): FatFsStorage, file_, mounted_

### Community 118 - "Function"
Cohesion: 0.17
Nodes (12): Function, degree, kind, octave, quality, root, Quality, Dominant (+4 more)

### Community 119 - "MidiFrame"
Cohesion: 0.17
Nodes (12): Kind, CC, Clock, NoteOff, NoteOn, PitchBend, ProgramChange, SysEx (+4 more)

### Community 120 - "MenuAction"
Cohesion: 0.12
Nodes (11): MenuAction, bank, kind, mode, slot, to_bank, to_mode, to_slot (+3 more)

### Community 121 - "Error"
Cohesion: 0.13
Nodes (14): Error, Busy, Checksum, Empty, Length, None, Opcode, Patch (+6 more)

### Community 122 - "CardUploads"
Cohesion: 0.10
Nodes (6): CardUploads, open, temp, MemoryCard, files, SineWav()

### Community 123 - "MidiPort"
Cohesion: 0.17
Nodes (7): MidiPort, clock_dropped, dropped, framer, frames, transport, PollMidi()

### Community 125 - "MemoryStorage"
Cohesion: 0.18
Nodes (5): MemoryStorage, drop_writes, fail_writes, files, ready

### Community 126 - "SampleHandoff"
Cohesion: 0.11
Nodes (14): Audio path, Boundaries, Device presets (SD card), Forge architecture, Patch lifecycle, Samples (SD card) and the memory handoff, Source map, Transport details (+6 more)

### Community 127 - "Kind"
Cohesion: 0.17
Nodes (11): Kind, Jack, Key, Press, Release, Toggle, Turn, PanelEvent (+3 more)

### Community 128 - "Envelope"
Cohesion: 0.20
Nodes (10): Envelope, release_step, stage, value, Stage, Attack, Decay, Off (+2 more)

### Community 130 - "LossyTransport"
Cohesion: 0.16
Nodes (3): DeadLink, LossyTransport, LostEndReply

### Community 132 - "Mode"
Cohesion: 0.20
Nodes (10): Mode, Dorian, HarmonicMinor, Locrian, Lydian, Major, MelodicMinor, Mixolydian (+2 more)

### Community 133 - "power_test.cpp"
Cohesion: 0.31
Nodes (7): BootGesture, held_, BootGestureOff(), Colours(), main(), OptionsFile(), StatusDecode()

### Community 134 - "Rig"
Cohesion: 0.15
Nodes (9): SampleTable, slots, Rig, engine, l, memory, r, rv (+1 more)

### Community 135 - "preset_menu.h"
Cohesion: 0.22
Nodes (10): BlackLed(), MenuPage, Harmony, Parts, Presets, Samples, RenderMenuLeds(), RenderSampleLeds() (+2 more)

### Community 137 - "MidiFramer"
Cohesion: 0.22
Nodes (8): MidiFramer, bytes_, cc_, cc_used_, overflow_, status_, sysex_, used_

### Community 138 - "Options"
Cohesion: 0.22
Nodes (8): Options, midi_in, midi_out, monitor, quantise_menu, record_latch, split_delay, tape_slew

### Community 139 - "ResponseKind"
Cohesion: 0.22
Nodes (9): ResponseKind, Erased, Occupancy, SampleDone, SampleOccupancy, SampleSnapshot, Snapshot, Status (+1 more)

### Community 141 - "Chime"
Cohesion: 0.21
Nodes (9): Chime, kDelay, kLength, kNotes, kStep, kTotal, kVelocity, t_ (+1 more)

### Community 142 - "FirmwarePanelSink"
Cohesion: 0.22
Nodes (4): FirmwarePanelSink, PanelAction, action, patch

### Community 143 - "Forge — developer resume checkpoint"
Cohesion: 0.07
Nodes (29): 2026-10-11: agent hardware checks, soak, UX review, 2026-10-11 (later): 0.16.0 Batch 1 + 2 built, 2026-10-11 (latest): 0.17.0 magic items 1-3 built (resume here), Branch and publishing, Browser hardware test bridge (supersedes terminal-only delivery below), Build environment notes, Checkpoint: firmware 0.10 TAPE parity, ready for DC's install (2026-10-05), Checkpoint: firmware 0.15 event recorder (2026-10-05; 0.14 parts and 0.13 harmony below) (+21 more)

### Community 145 - "InspectorVoice"
Cohesion: 0.25
Nodes (8): InspectorVoice, age, envelope, flags, note, slot, source, stage

### Community 146 - "Compressor"
Cohesion: 0.33
Nodes (5): Compressor, gain_, peak_, SoftClip(), SoftLimit()

### Community 147 - "Forge 0.17.0 — the one consolidated hardware test"
Cohesion: 0.10
Nodes (21): 0. Before you start (no CHOMPI needed), 1. Flash and identity, 1A. Inspector development candidate (before the normal audio checks), 2. External audio path (v1 compatibility), 3. Instrument, 3B. v3 instrument modules (firmware 0.4), 3C. Device presets on the SD card (TAPE-style keys + encoder), 3D. Sampler (TAPE-style, firmware 0.5) (+13 more)

### Community 148 - "Extension"
Cohesion: 0.25
Nodes (8): Extension, Eleventh, Fifth, Ninth, Seventh, Thirteenth, Triad, ExtensionStep()

### Community 149 - "State"
Cohesion: 0.29
Nodes (6): State, Armed, Empty, FirstTake, Paused, Playing

### Community 151 - "Kind"
Cohesion: 0.20
Nodes (10): Kind, Copy, Erase, Recall, RecordSource, SampleCopy, SampleErase, SampleSave (+2 more)

### Community 152 - "struct"
Cohesion: 0.39
Nodes (3): check(), image_problems(), symbols()

### Community 153 - "Action"
Cohesion: 0.24
Nodes (7): Action, a, b, kind, Kind, Note, Param

### Community 154 - "State"
Cohesion: 0.29
Nodes (6): State, Armed, Empty, Playing, Recording, Stopped

### Community 155 - "Queue"
Cohesion: 0.36
Nodes (8): Flash(), forge::Error RecallPreset(), LoadSequence(), Queue(), Restart(), RunSampler(), SendResponses(), TransmitPending()

### Community 156 - "V3Field"
Cohesion: 0.40
Nodes (5): V3Field, index, kind, max, Parameters

### Community 158 - "SoundDeviceAudio"
Cohesion: 0.13
Nodes (3): FakeAudio, Skipped, SoundDeviceAudio

### Community 159 - "State"
Cohesion: 0.33
Nodes (6): State, Detaching, Headers, Idle, Job, Streaming

### Community 161 - "MenuMode"
Cohesion: 0.29
Nodes (6): MenuMode, CopyDest, CopySource, Erase, None, Save

### Community 162 - "Mmf"
Cohesion: 0.24
Nodes (6): Mmf, buf0_, buf1_, MmfCoefficients, feedback, freq

### Community 163 - "forge_bridge.py"
Cohesion: 0.14
Nodes (6): main(), run(), main(), selftest(), integer(), panel_message()

### Community 164 - "forge_bench.cpp"
Cohesion: 0.60
Nodes (3): bench_block(), bench_init(), Noise()

### Community 165 - "KnobHold"
Cohesion: 0.40
Nodes (5): KnobHold, down, frames, turned, used

### Community 166 - "Event"
Cohesion: 0.40
Nodes (4): Event, note, source, velocity

### Community 167 - "Colour"
Cohesion: 0.40
Nodes (5): BatteryColour(), Colour, b, g, r

### Community 172 - "MonitorMode"
Cohesion: 0.33
Nodes (4): MonitorMode, Both, Headphones, SendReturn

### Community 173 - "Outgoing"
Cohesion: 0.50
Nodes (4): Outgoing, bytes, size, source

### Community 175 - "SaveSlotSettings"
Cohesion: 0.27
Nodes (6): BootCount, boots, magic, CopyFile(), SaveSlotSettings(), WritePresetsFile()

### Community 176 - "BridgeCardTests"
Cohesion: 0.09
Nodes (5): BridgeCardTests, FakeChompi, FastTime, WalkTests, hand()

### Community 177 - "forge_main.cpp"
Cohesion: 0.18
Nodes (13): CaptureInspector(), ChargerReadings(), CollectInspector(), HandleFrame(), HandleStorage(), InstallFaultHandler(), InstallPowerOk(), RaiseEmergency() (+5 more)

### Community 178 - "Message"
Cohesion: 0.33
Nodes (5): Message, Continue, Start, Stop, Tick

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

### Community 186 - "5. Proposed design"
Cohesion: 0.33
Nodes (6): 5.1 Sample streaming: shared page cache + pinned heads (one pool), 5.2 Underrun and failure semantics, 5.3 Streaming recorder, 5.4 Looper, 5.5 QSPI (Tier 2), 5. Proposed design

### Community 187 - "SlotValues"
Cohesion: 0.20
Nodes (10): SlotValues, attack, end, gain, loop, pan, pitch, release (+2 more)

### Community 189 - "Forge control protocol — firmware 0.15"
Cohesion: 0.22
Nodes (9): Development probe and Inspector schema 1, Device presets (SD card), firmware 0.4, Envelope and operations, Event recorder loops on the SD card (firmware 0.15), Forge control protocol — firmware 0.15, MIDI clock (firmware 0.14), Notes, controls and recovery, Sampler, firmware 0.5 (+1 more)

### Community 191 - "Forge 0.5 — consolidated test candidate"
Cohesion: 0.25
Nodes (8): Audio and controls, Build and test, Device presets (SD card), First hardware session, Forge 0.5 — consolidated test candidate, Implementation boundaries, Sampler and recording (v4 patches), Synth and module patches

### Community 192 - "InspectorSnapshot"
Cohesion: 0.40
Nodes (5): InspectorSnapshot, audio, generation, storage, system

### Community 194 - "First time home with Forge 0.17.0"
Cohesion: 0.25
Nodes (7): 0. Power first (before any install), 1. Get the files, 2. Install 0.15.1, 3. Fix the rig (from the last session's results), 4. Run the bridge, 5. Send the results back, First time home with Forge 0.17.0

### Community 195 - "DrawLeds"
Cohesion: 1.00
Nodes (3): Balance(), DrawLeds(), Pth()

### Community 196 - "body_"
Cohesion: 0.40
Nodes (3): body_(), say(), Card()

### Community 197 - "Ponytail, lazy senior dev mode"
Cohesion: 0.33
Nodes (4): Forge development guidance, Before you write, Ponytail, lazy senior dev mode, The smallest complete change

### Community 198 - "Forge looping (roadmap item 5): design"
Cohesion: 0.33
Nodes (6): 1. What TAPE does (the behaviour to keep), 2. Forge plan, 3. Decisions (DC, 2026-10-03), 4. Risks, 5. Build order (each step tested before the next), Forge looping (roadmap item 5): design

### Community 199 - "Forge project brief"
Cohesion: 0.33
Nodes (6): Architecture and acceptance, Current milestone: instrument candidate 0.15.1, First milestone: instrument candidate 0.4, Forge project brief, Goal and user decisions, Roadmap (priority order agreed 2026-10-02)

### Community 202 - "StoreQueued"
Cohesion: 0.67
Nodes (3): Forge handoff — instrument candidate 0.15.1, Handoff: cloud session → DC's local PC (2026-10-09), StoreQueued()

### Community 204 - "Forge — live OpenAI / Gemini test"
Cohesion: 0.40
Nodes (5): Forge — live OpenAI / Gemini test, If it fails, Rules, Step 1 — command-line preflight (one request per provider), Step 2 — webapp

### Community 205 - "Forge documentation"
Cohesion: 0.40
Nodes (5): Choose a starting point, Documentation map, Forge documentation, Maintaining these docs, What is implemented and what is pending

### Community 207 - "Forge hardware test kit — instructions for an agent on the test PC"
Cohesion: 0.40
Nodes (4): Forge hardware test kit — instructions for an agent on the test PC, Reference, Rules, What you can do here

## Knowledge Gaps
- **1327 isolated node(s):** `reverb_`, `del_`, `filter_`, `warble_`, `dcblock_fx_l_` (+1322 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 2110 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **28 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `PanelController` connect `PanelController` to `Rig`, `forge_ai.py`, `KnobHold`, `.Block`, `Rig`, `StoreQueued`, `TransferAndInstall`, `Options`, `Chime`, `MonitorMode`, `Kind`, `main`, `PresetMenu`, `RecordGesture`, `EngineAndPanel`, `panel_controller.h`, `Clock`, `SpscQueue`?**
  _High betweenness centrality (0.143) - this node is a cross-community bridge._
- **Are the 9 inferred relationships involving `Engine` (e.g. with `main()` and `PhysicalVersusInjectedAndVoiceEdges()`) actually correct?**
  _`Engine` has 9 INFERRED edges - model-reasoned connections that need verification._
- **What connects `reverb_`, `del_`, `filter_` to the rest of the system?**
  _1327 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Synth` be split into smaller, more focused modules?**
  _Cohesion score 0.023803009575923392 - nodes in this community are weakly interconnected._
- **Why does `Engine` connect `Engine` to `Rig`, `SlotSettings`, `main`, `looper_test.cpp`, `EngineAndPanel`, `.Init`, `v3_test.cpp`, `.ApplyPatch`, `Clock`, `synth_test.cpp`, `panel_controller.h`, `Rig`, `Player`, `Sequencer`, `.Block`, `.Note`, `slot_settings_test.cpp`, `forge_main.cpp`, `Reverb`, `Rig`, `StoreQueued`, `Mailbox`, `core_test.cpp`, `runtime.h`, `Performance`, `SampleHandoff`?**
  _High betweenness centrality (0.103) - this node is a cross-community bridge._
- **Are the 5 inferred relationships involving `PanelController` (e.g. with `Current checkpoint: firmware 0.15.1 review fixes (2026-10-06)` and `Handoff: cloud session → DC's local PC (2026-10-09)`) actually correct?**
  _`PanelController` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Should `Walk` be split into smaller, more focused modules?**
  _Cohesion score 0.06453634085213032 - nodes in this community are weakly interconnected._