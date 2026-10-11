# Graph Report - CHOMPI  (2026-10-12)

## Corpus Check
- 154 files · ~304,104 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 4, .css 2, .ld 1)

## Summary
- 4484 nodes · 8780 edges · 225 communities (185 shown, 40 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 995 edges (avg confidence: 0.87)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `45c34833`
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
- BatteryDiag
- .Init
- v3_test.cpp
- Harmony / Intent Engine — brief
- .Value
- Clock
- synth_test.cpp
- panel_controller.h
- forge_battery_log.py
- Rig
- DjFilter
- Dream
- tempo_bench.cpp
- Player
- Sequencer
- BatteryMonitor
- Request
- Response
- SampleFiles
- firmware_chompi_forge_core_sequencer
- Pad
- Warble
- Rig
- LedView
- BrowserTests
- panel_test.cpp
- SampleCard
- parts_test.cpp
- knob_layout.h
- .Block
- Settings
- runtime.h
- Effects
- LoaderRig
- run_bench.py
- InspectorStorage
- power.h
- RequestKind
- wav.h
- Clocked
- file_transfer_test.cpp
- Forge test results
- Choowi: how do I…?
- probe
- EncodeInspector
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
- RecoveryAfterLostNotes
- SimulatedMido
- FileRequest
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
- Storage virtualization / SDRAM reclamation — feasibility and design
- ChargerUsb
- FaultRecord
- synth.h
- FatFsSampleFiles
- AnalysisTests
- Device
- Forge · Choowi: the user manual
- FakeChompi
- Chord
- secret.h
- slot_settings_test.cpp
- Chime
- SampleHandoff
- SlotPolicy
- harmony_test.cpp
- FileTransfer
- Performance
- harmony.h
- tape_bench.cpp
- FatFsStorage
- Mmf
- MidiFramer
- MenuAction
- Forge Resource Ledger
- CardUploads
- BridgeAutomaticTests
- BridgeBrowserTests
- MemoryStorage
- send.js
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
- Conversation
- Options
- ResponseKind
- Transport
- CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12)
- SlotValues
- Forge — developer resume checkpoint
- ProviderTests
- InspectorVoice
- Action
- Forge 0.19.2 — the one consolidated hardware test
- Error
- .ProcessActive
- SaveFile
- Kind
- forge_ai.py
- EngineAndPanel
- CardTests
- Sink
- .Apply
- struct
- SoundDeviceAudio
- .Note
- Rgb
- forge_bridge.py
- MenuMode
- forge_bench.cpp
- KnobHold
- TranslateChannel
- InstrumentTests
- the forge family
- BridgeWebTests
- V4PatchTests
- ForgeServer
- FakePorts
- AiCheckTests
- InspectorSnapshot
- Rig
- Event
- Earlier checkpoint: boot fix and cleanup, 2026-10-03
- Forge hardware test bridge
- pack.js
- Forge sampling (roadmap item 4): design
- restart.h
- Colour
- Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark
- Stock TAPE: what every control does (reference for Forge parity)
- LockoutGuard
- State
- InspectorTests
- Forge control protocol — firmware 0.15
- Sink
- Choowi: the first firmware of the forge
- Compressor
- forge_main.cpp
- First time home with Forge 0.19.2
- .run_walk
- State
- Ponytail, lazy senior dev mode
- Forge looping (roadmap item 5): design
- Forge project brief
- InspectorEvent
- node
- Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10)
- Forge — live OpenAI / Gemini test
- the forge docs
- Forge developer guide
- Forge hardware test kit — instructions for an agent on the test PC
- Sequence
- Rng
- Choowi's sample pack
- build
- Branch and publishing
- Stock firmware comparison and CPU benchmark (2026-10-03, the owner's request)
- BridgeCardTests
- FakeChompi
- .RecordParam
- SlotSettingsSchedule
- PresetProtocolTests
- ComposeTransportLeds
- BatteryLogTests
- 0.16.0 Panel feedback and shortcuts (the owner's UX review; tool: Colour Lantern) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS)
- Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05

## God Nodes (most connected - your core abstractions)
1. `Engine` - 173 edges
2. `PanelController` - 149 edges
3. `Synth` - 134 edges
4. `Parts` - 82 edges
5. `Looper` - 81 edges
6. `Parameters` - 71 edges
7. `InspectorAudio` - 69 edges
8. `SampleLoader` - 58 edges
9. `main()` - 55 edges
10. `Parameter` - 53 edges

## Surprising Connections (you probably didn't know these)
- `Patch lifecycle` --references--> `V3Fields()`  [INFERRED]
  docs/forge/ARCHITECTURE.md → firmware/chompi-forge/core/protocol.h
- `Development probe and Inspector schema 1` --references--> `EncodePatchData()`  [INFERRED]
  docs/forge/PROTOCOL.md → firmware/chompi-forge/core/protocol.h
- `Hardware: 0.15.2 on CHOMPI, unattended run (2026-10-10 evening)` --references--> `detect()`  [INFERRED]
  docs/forge/CONTINUE.md → firmware/chompi-forge/host/forge_audio.py
- `0.17.0 Magic: wakes up singing, surprise me, mutate (tool: Wake-up Bell) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS)` --references--> `Magic017()`  [INFERRED]
  docs/forge/CHANGELOG.md → firmware/chompi-forge/tests/panel_test.cpp
- `2026-10-11 (latest): 0.17.0 magic items 1-3 built` --references--> `Magic017()`  [INFERRED]
  docs/forge/CONTINUE.md → firmware/chompi-forge/tests/panel_test.cpp

## Import Cycles
- None detected.

## Communities (225 total, 40 thin omitted)

### Community 0 - "Synth"
Cohesion: 0.02
Nodes (77): Synth, age_, amp_, bend_slew_, bend_target_, cap_, coefficient_, cutoff_ (+69 more)

### Community 1 - "Walk"
Cohesion: 0.18
Nodes (3): Stop, Walk, test()

### Community 2 - "PanelController"
Cohesion: 0.02
Nodes (81): MonitorMode, Both, Headphones, SendReturn, PanelController, activity_, audition_, battery_view_ (+73 more)

### Community 3 - "Bridge"
Cohesion: 0.06
Nodes (19): add(), Bridge, send_panel(), fetch(), body_(), say(), Card(), now() (+11 more)

### Community 4 - "InspectorAudio"
Cohesion: 0.03
Nodes (61): InspectorAudio, bend, block, chord_count, chord_degree, chord_kind, chord_notes, chord_quality (+53 more)

### Community 5 - "Engine"
Cohesion: 0.04
Nodes (38): Engine, bloom_, capacity_, effects_, feedback_, flushed_, focus_, fx_before_loop_ (+30 more)

### Community 6 - "Looper"
Cohesion: 0.05
Nodes (33): Looper, bp_, combo_, combo_used_, fade_, feedback_target_, filter_on_, frac_ (+25 more)

### Community 7 - "SlotSettings"
Cohesion: 0.11
Nodes (15): FollowSampleJob(), Entry, valid, value, SlotSettings, entries_, kBanks, kControls (+7 more)

### Community 8 - "Parameters"
Cohesion: 0.04
Nodes (51): Parameters, attack, bypass, cutoff, decay, feedback, filter_amount, filter_attack (+43 more)

### Community 9 - "forge_host.py"
Cohesion: 0.05
Nodes (57): 1. What Forge is, 2. How to talk to CHOMPI, 3. Patches, 5. Where things live (`firmware/chompi-forge/`), 6. How to make an effect, 7. How to edit the firmware, 8. Read next, Forge for other agents (Muse / Cosmo on the Tab5) (+49 more)

### Community 10 - "forge_audio.py"
Cohesion: 0.05
Nodes (31): Previous checkpoint: first hardware session and bridge test tooling, 2026-10-04, analyze(), audible_change(), cli(), clicks(), compare(), db(), detect() (+23 more)

### Community 11 - "PresetMenu"
Cohesion: 0.07
Nodes (20): KeyToSlot(), PresetMenu, actions_, chompi_, chromatic_slot_, head_, kActions, kLoopSource (+12 more)

### Community 12 - "json"
Cohesion: 0.09
Nodes (4): verify(), WindowsRuntimeTests, wav(), ManualPageTests

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
Cohesion: 0.08
Nodes (24): 0.6 knob pages and patch knobs — 2026-10-04, control_value(), _is_sampler(), knob_control(), knob_pages(), _layout(), chord_label(), cli() (+16 more)

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
Nodes (24): ChromaticAndKitLoading(), ChromaticPitch(), CubicTableRead(), FactoryTapeFiles(), Fill(), GateAndTrigger(), HeaderRoundTrip(), KitMapping() (+16 more)

### Community 22 - "app.js"
Cohesion: 0.14
Nodes (17): adopt(), api(), CONTROLS, GROUPS, KNOB_LABELS, KNOB_SWITCHES, KNOB_TARGETS, LFO_WAVES (+9 more)

### Community 23 - "SampleLoader"
Cohesion: 0.06
Nodes (27): Inspect(), SampleLoader, count_, current_, cursor_, handoff_, head_, index_ (+19 more)

### Community 24 - "BatteryDiag"
Cohesion: 0.07
Nodes (28): BatteryDiag, after_30, after_33, battery_low, check_state, checks, expect_33, input_limit (+20 more)

### Community 25 - ".Init"
Cohesion: 0.14
Nodes (24): 0.4 device presets on the SD card — 2026-10-03, Development plan (the owner, 2026-10-02): features first, QA per feature later, Roadmap item 2: v3 instrument palette (implemented; software-tested), Change discipline, DecodePatchData(), DecodeRequest(), EncodePatchData(), PatchDataSize() (+16 more)

### Community 26 - "v3_test.cpp"
Cohesion: 0.16
Nodes (25): GlideSnap(), Crossings(), Energy(), Hz(), LfoAndModWheel(), main(), NeutralV3IsAPlainVoice(), Peak() (+17 more)

### Community 27 - "Harmony / Intent Engine — brief"
Cohesion: 0.07
Nodes (30): AI compatibility, Arpeggiator, Bass generator, Chromatic keys, Clean-room requirement, Core architecture, Current resource guardrails, Desired result (+22 more)

### Community 28 - ".Value"
Cohesion: 0.22
Nodes (4): Command, parameter, value, DecodeCC()

### Community 29 - "Clock"
Cohesion: 0.11
Nodes (16): Phase 0 decisions (the owner, 2026-10-05), Clock, interval_, last_tap_, pending_, phase_, rate_, since_ (+8 more)

### Community 30 - "synth_test.cpp"
Cohesion: 0.17
Nodes (14): Roadmap item 1: sustain pedal and pitch bend (implemented; software-tested), AliasDb(), ChannelTranslation(), ClickFreeStealAndRetrigger(), Crossings(), EnvelopeVelocityAndFilter(), Instrument(), main() (+6 more)

### Community 31 - "panel_controller.h"
Cohesion: 0.16
Nodes (4): Find(), Parse(), PartsWordsValid(), AudioBlock()

### Community 32 - "forge_battery_log.py"
Cohesion: 0.14
Nodes (9): ask(), charge_name(), cli(), decode_registers(), describe(), flatten(), Logger, stamp() (+1 more)

### Community 33 - "Rig"
Cohesion: 0.07
Nodes (24): PanelInput, frames, jack, keys, toggle_up, tone_down, turns, Rig (+16 more)

### Community 34 - "DjFilter"
Cohesion: 0.12
Nodes (14): DjFilter, control_, hp_, hp_c_, hp_target_, hpl_, hpr_, lp_ (+6 more)

### Community 35 - "Dream"
Cohesion: 0.11
Nodes (18): 0.18.0 Magic: chomp to instrument, dream mode, lights with character (tool: Dream Bellows) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS), Dream, asleep_, degree_, elapsed_, held_, idle_, idle_after_ (+10 more)

### Community 37 - "Player"
Cohesion: 0.07
Nodes (20): Held, count, key, note, source, used, Player, bent_index_ (+12 more)

### Community 38 - "Sequencer"
Cohesion: 0.13
Nodes (9): Sequencer, cursor_, held_, pending_off_, playing_, pos_, sequence_, state_ (+1 more)

### Community 39 - "BatteryMonitor"
Cohesion: 0.20
Nodes (12): BatteryMonitor, d_, kPeriodicMs, periodic_at_, read_33_, ReadBack, After30, After33 (+4 more)

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
Cohesion: 0.10
Nodes (17): Pad, amp, attack, end, gate, loop, pan_l, pan_r (+9 more)

### Community 45 - "Warble"
Cohesion: 0.13
Nodes (12): Warble, coefficient_, end_, frequency_, kLength, left_, length_, mix_ (+4 more)

### Community 46 - "Rig"
Cohesion: 0.10
Nodes (18): Pack(), BloomInEngine(), main(), Mischief(), Panel(), Rig, engine, hw (+10 more)

### Community 47 - "LedView"
Cohesion: 0.06
Nodes (31): LedView, blink, count_in, dream, dream_level, flash, harmony, input_level (+23 more)

### Community 49 - "panel_test.cpp"
Cohesion: 0.18
Nodes (19): ComposeKnobLeds(), ComposeLeds(), TapeSpeedRatio(), BatteryHold(), Dream018(), KeysKnobsAndOverrides(), KnobPages(), LedComposition() (+11 more)

### Community 50 - "SampleCard"
Cohesion: 0.08
Nodes (14): SampleCard, append_budget, fail_reads, fail_writes, files, kTempName, opens, read_name_ (+6 more)

### Community 51 - "parts_test.cpp"
Cohesion: 0.14
Nodes (15): Arp(), ArpPatterns(), BassModesAndMidi(), ChordGathering(), GateLatchAndOwnership(), main(), Note, note (+7 more)

### Community 52 - "knob_layout.h"
Cohesion: 0.19
Nodes (20): Code, Fade(), Fade3(), Fade4(), HasPatchPage(), IsPatchPage(), Kind, Effects (+12 more)

### Community 53 - ".Block"
Cohesion: 0.09
Nodes (6): EditSynth(), MagicCombo(), PackLooper(), PanelSink, QuantisedSpeedStep(), TapeSpeedKnob()

### Community 54 - "Settings"
Cohesion: 0.05
Nodes (44): Apply request layout, Inspect(), Bass, Alternate, Fifth, Octave, Off, Root (+36 more)

### Community 55 - "runtime.h"
Cohesion: 0.14
Nodes (12): Roadmap item 3: device presets on the SD card (implemented; software-tested), Crc16(), DecodePresetRecord(), EncodePresetRecord(), PresetPath(), PresetStore, Storage, EraseReply() (+4 more)

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
Cohesion: 0.39
Nodes (14): Crc32(), Read35(), Begin(), CodecAndNames(), Data(), Decode(), Firmware(), FirmwareGuards() (+6 more)

### Community 65 - "Forge test results"
Cohesion: 0.12
Nodes (17): 2026-10-04 evening — first 0.7 session (the owner), development firmware (`b36ba99`), 2026-10-04 — first hardware session (the owner), firmware 0.6 development (`131776b`), 2026-10-05 — 0.9 on CHOMPI (the owner, verbal): open issue, random shut-off, 2026-10-05 — firmware 0.8 installed over USB: all automatic checks pass, 2026-10-05 — line in wired, 0.7 on CHOMPI, 0.8 bridge, 2026-10-05 — power on 0.8 (the owner, verbal): TEST_SESSION 8, 2026-10-05 — the owner's panel-map walk-through on 0.9 (verbal), 2026-10-10 (late) — start-up stall found and fixed on CHOMPI (+9 more)

### Community 66 - "Choowi: how do I…?"
Cohesion: 0.10
Nodes (21): before you start: names used here, cheat sheet, check it worked, Choowi: how do I…?, good to know, how do I find my way around the menu?, how do I get surprise sounds and keep one?, how do I loop? (+13 more)

### Community 68 - "EncodeInspector"
Cohesion: 0.27
Nodes (7): EncodeInspector(), InspectorLog, count_, events_, kCapacity, InspectorWord(), WireBoundsAndLog()

### Community 69 - "InspectorSystem"
Cohesion: 0.11
Nodes (19): InspectorSystem, battery, charge_state, crash_pc, crashed, dropped, emergencies, event_drops (+11 more)

### Community 70 - "Reverb"
Cohesion: 0.15
Nodes (12): Reverb, damp_, gain_, index_, kLines, length_, line_, longest_ (+4 more)

### Community 71 - "preset_test.cpp"
Cohesion: 0.25
Nodes (13): KnobOrder(), main(), MenuGestures(), Panel, menu, toggle, Protocol(), RecordsAndPaths() (+5 more)

### Community 72 - "FakeMidi"
Cohesion: 0.13
Nodes (4): FakeMidi, Message, FakePort, TransportTests

### Community 73 - "Rig"
Cohesion: 0.07
Nodes (28): 0.19.1 The DJ filter never goes silent (small tool: to be named) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS), Change(), Dependency(), Features, balance, envelope, rms, spectrum (+20 more)

### Community 74 - "MemoryCard"
Cohesion: 0.14
Nodes (11): BootloaderMatches(), SetAsideName(), BootloaderNames(), MemoryCard, corrupt_write, fail_append, fail_rename, files (+3 more)

### Community 75 - "TransferAndInstall"
Cohesion: 0.16
Nodes (10): InstallGate, armed_at_, kArmed, kConfirmed, kIdle, kTimeoutMs, state_, swallow_ (+2 more)

### Community 76 - "firmware_chompi_forge_core_protocol"
Cohesion: 0.18
Nodes (20): , Checksum(), EncodeError(), EncodeIdentity(), EncodePanelAck(), EncodeProbeLeds(), EncodeResponse(), Header() (+12 more)

### Community 77 - "SampleJob"
Cohesion: 0.11
Nodes (17): Kind, Copy, Erase, Save, SampleJob, bank, frames, from_loop (+9 more)

### Community 78 - "Forge · Choowi changelog"
Cohesion: 0.06
Nodes (31): 0.10 TAPE parity — 2026-10-05 (ready to install; per-slot settings moved to 0.12), 0.11 Install safety — 2026-10-05 (software-tested; not installed yet), 0.12 Per-slot sample settings — 2026-10-05 (software-tested; not installed yet), 0.13 Harmony — 2026-10-05 (software-tested; not installed yet), 0.14 Arp, bass and tempo — 2026-10-05 (software-tested; not installed yet), 0.15.1 Review fixes — 2026-10-06 (software-tested; not installed yet), 0.15 Event recorder and projects — 2026-10-05 (software-tested; not installed yet), 0.17.0 Magic: wakes up singing, surprise me, mutate (tool: Wake-up Bell) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS) (+23 more)

### Community 79 - "argparse"
Cohesion: 0.15
Nodes (9): files(), main(), verify(), data_url(), main(), render(), main(), repository_url() (+1 more)

### Community 80 - ".Valid"
Cohesion: 0.17
Nodes (13): note, Mutate(), Rng, s, Roll(), Stop(), Tick(), Base() (+5 more)

### Community 81 - "RecoveryAfterLostNotes"
Cohesion: 0.16
Nodes (11): 0.3 integration review and browser testing — 2026-10-03, Done in the integration checkpoint (`15681b8`), Sound fixes (implemented 2026-10-02 UTC; software-tested only), Atomicity and real-time boundary, RecoveryGate, AudioCallback(), Callback, gate (+3 more)

### Community 82 - "SimulatedMido"
Cohesion: 0.07
Nodes (10): _Input, Message, _Output, _Port, SimulatedMido, PresetCliAndWebTests, SampleMessageTests, SampleWebAndAiTests (+2 more)

### Community 83 - "FileRequest"
Cohesion: 0.11
Nodes (18): EncodeFileReply(), FileOp, Abort, Begin, Data, End, Install, Status (+10 more)

### Community 84 - "State"
Cohesion: 0.14
Nodes (14): Layout, Real, Static, State, block, enabled, extension, inversion (+6 more)

### Community 85 - "InspectorEventKind"
Cohesion: 0.12
Nodes (16): InspectorEventKind, Card, KeyDown, KeyUp, Knob, PatchApply, QueueError, RecordingStart (+8 more)

### Community 86 - "EngineAndPanel"
Cohesion: 0.07
Nodes (15): Forge handoff, Handoff: cloud session → the owner's local PC (2026-10-09), Inspect(), MidiOut, data1, data2, status, StoreQueued() (+7 more)

### Community 87 - "RecordGesture"
Cohesion: 0.13
Nodes (10): Event, None, Start, Stop, RecordGesture, chompi_, count_, counting_ (+2 more)

### Community 88 - "Mailbox"
Cohesion: 0.17
Nodes (11): Mailbox, bank, owner, sequence, slot, state, Owner, Host (+3 more)

### Community 89 - "FatFsUploadFiles"
Cohesion: 0.17
Nodes (7): FatFsUploadFiles, aside_, check_, file_, kAsideBatch, target_, verify_buffer_

### Community 90 - "iostream"
Cohesion: 0.14
Nodes (13): DelayImpulseAndBypass(), Fixture, engine, left, right, InitializationAndDryStereo(), main(), Near() (+5 more)

### Community 92 - "FakeMido"
Cohesion: 0.16
Nodes (3): FakeMido, TestTrafficCommands, interrupt()

### Community 93 - "WebTests"
Cohesion: 0.13
Nodes (7): Audit of the starting head, Exact next hardware test, Forge Inspector — first development slice, Realtime ownership, Run the Inspector, Validation at this checkpoint, WebTests

### Community 94 - "InspectorMailbox"
Cohesion: 0.31
Nodes (4): InspectorMailbox, owner_, state_, MailboxOwnership()

### Community 95 - "SpscQueue"
Cohesion: 0.22
Nodes (5): First optimization decisions to investigate, SpscQueue, head_, storage_, tail_

### Community 96 - "Storage virtualization / SDRAM reclamation — feasibility and design"
Cohesion: 0.17
Nodes (12): 10. Answers to the brief's questions, 3. Precedent: stock TAPE already streams from SD on this hardware, 5.1 Sample streaming: shared page cache + pinned heads (one pool), 5.2 Underrun and failure semantics, 5.3 Streaming recorder, 5.4 Looper, 5.5 QSPI (Tier 2), 5. Proposed design (+4 more)

### Community 97 - "ChargerUsb"
Cohesion: 0.11
Nodes (18): 2026-10-11 (night, later): shut-off caught, fix built, 2026-10-10 — 0.15.2 on CHOMPI: first install since 0.9, unattended automatic run, 2026-10-11 (night) — power bank: no charging after a re-plug while on, Actions, force_detection, read, usb_to_daisy, ChargerUsb (+10 more)

### Community 98 - "FaultRecord"
Cohesion: 0.20
Nodes (10): Describe(), FaultRecord, cfsr, count, hfsr, kMagic, lr, magic (+2 more)

### Community 99 - "synth.h"
Cohesion: 0.29
Nodes (7): CubicQ14(), CubicTable, w, Dual16(), MakeCubicTable(), PackHigh(), PackLow()

### Community 100 - "FatFsSampleFiles"
Cohesion: 0.17
Nodes (5): FatFsSampleFiles, read_, reading_, write_, writing_

### Community 101 - "AnalysisTests"
Cohesion: 0.14
Nodes (4): AnalysisTests, walk(), wait(), sine()

### Community 102 - "Device"
Cohesion: 0.18
Nodes (4): 4. Patch v4 (wire and JSON), SetupCheckTests, Device, source()

### Community 103 - "Forge · Choowi: the user manual"
Cohesion: 0.06
Nodes (31): 10. On the computer, 11. What Forge keeps on the SD card, 12. Troubleshooting, 1. Quick start, 2. Installing and updating, 3. Power and battery, 4. The panel, 5. Knobs and knob pages (+23 more)

### Community 104 - "FakeChompi"
Cohesion: 0.14
Nodes (3): DetectionTests, louder_single(), FakeChompi

### Community 105 - "Chord"
Cohesion: 0.14
Nodes (14): Chord, degree, kind, quality, root, shifted, tone, Kind (+6 more)

### Community 106 - "secret.h"
Cohesion: 0.10
Nodes (19): Advance(), Bend(), Bloom, kQueue, Blooms(), Interrupt(), Mode, BloomDown (+11 more)

### Community 107 - "slot_settings_test.cpp"
Cohesion: 0.14
Nodes (14): SelectSample(), Chromatic(), Kit(), main(), Near(), Rig, engine, l (+6 more)

### Community 108 - "Chime"
Cohesion: 0.21
Nodes (9): Chime, kDelay, kLength, kNotes, kStep, kTotal, kVelocity, t_ (+1 more)

### Community 109 - "SampleHandoff"
Cohesion: 0.16
Nodes (7): Samples (SD card) and the memory handoff, PackSelection(), SampleHandoff, ack_, published_, request_, FailedLoadIsVisible()

### Community 110 - "SlotPolicy"
Cohesion: 0.40
Nodes (5): Checkpoint: firmware 0.12 per-slot sample settings (2026-10-05), SlotPolicy, Patch, Recall, Select

### Community 111 - "harmony_test.cpp"
Cohesion: 0.16
Nodes (16): Extension, Eleventh, Fifth, Ninth, Seventh, Thirteenth, Triad, Tones() (+8 more)

### Community 112 - "FileTransfer"
Cohesion: 0.11
Nodes (16): AllowedUploadName(), DecodeFileRequest(), FileTransfer, crc_, done_, head_, kTemp, name_ (+8 more)

### Community 113 - "Performance"
Cohesion: 0.18
Nodes (10): Performance, compressor, dj_filter, dj_resonance, input_gain, pan, saturation, speed (+2 more)

### Community 114 - "harmony.h"
Cohesion: 0.13
Nodes (23): Function, degree, kind, octave, quality, root, Lead(), Movement() (+15 more)

### Community 115 - "tape_bench.cpp"
Cohesion: 0.18
Nodes (3): bench_block(), bench_init(), Noise()

### Community 116 - "FatFsStorage"
Cohesion: 0.21
Nodes (3): FatFsStorage, file_, mounted_

### Community 118 - "Mmf"
Cohesion: 0.24
Nodes (6): Mmf, buf0_, buf1_, MmfCoefficients, feedback, freq

### Community 119 - "MidiFramer"
Cohesion: 0.10
Nodes (20): Kind, CC, Clock, NoteOff, NoteOn, PitchBend, ProgramChange, SysEx (+12 more)

### Community 120 - "MenuAction"
Cohesion: 0.08
Nodes (12): MenuAction, bank, kind, mode, slot, to_bank, to_mode, to_slot (+4 more)

### Community 121 - "Forge Resource Ledger"
Cohesion: 0.18
Nodes (11): Forge Resource Ledger, Memory map and working set, QA and CPU baseline, Reproduced baseline, Snapshot 2026-10-05: CPU fix A (0.13, not installed), Snapshot 2026-10-05: firmware 0.13 harmony (Phase 1 complete), Snapshot 2026-10-05: firmware 0.14 clock, arp and bass (not installed), Snapshot 2026-10-05: firmware 0.15 event recorder and projects (not installed) (+3 more)

### Community 122 - "CardUploads"
Cohesion: 0.10
Nodes (6): CardUploads, open, temp, MemoryCard, files, SineWav()

### Community 125 - "MemoryStorage"
Cohesion: 0.17
Nodes (5): MemoryStorage, drop_writes, fail_writes, files, ready

### Community 126 - "send.js"
Cohesion: 0.27
Nodes (8): api(), connected(), exchange(), notice(), pick(), PREFIX, refresh(), run()

### Community 127 - "Kind"
Cohesion: 0.18
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
Nodes (8): BootGesture, held_, BootGestureOff(), Colours(), LockoutDip(), main(), OptionsFile(), StatusDecode()

### Community 134 - "SampleSlot"
Cohesion: 0.15
Nodes (11): SampleSlot, channels, data, frames, gain, loaded, partial, rate_ratio (+3 more)

### Community 135 - "MenuPage"
Cohesion: 0.29
Nodes (6): MenuPage, Harmony, Parts, Presets, Samples, Synth

### Community 136 - ".ProcessSampler"
Cohesion: 0.16
Nodes (4): 1. Baseline (verified from source and the ELF), 4. Architectural blockers and couplings, 7. Code, CPU and main-loop cost (estimates), GlideStep()

### Community 137 - "Conversation"
Cohesion: 0.12
Nodes (12): InKey(), Conversation, armed_, at_, count_, kMaxPhrase, phrase_, sixteenth_ (+4 more)

### Community 138 - "Options"
Cohesion: 0.22
Nodes (8): Options, midi_in, midi_out, monitor, quantise_menu, record_latch, split_delay, tape_slew

### Community 139 - "ResponseKind"
Cohesion: 0.22
Nodes (9): ResponseKind, Erased, Occupancy, SampleDone, SampleOccupancy, SampleSnapshot, Snapshot, Status (+1 more)

### Community 141 - "CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12)"
Cohesion: 0.15
Nodes (9): CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12), How the readings are made (stock code: `chompi-wave/code/src/hardware.h`), Hypotheses and how to tell them apart, Instrumented build (development only, 2026-10-12; software-tested, not run on CHOMPI yet), Problems in the reading itself (code), Proposed next steps (not done), The hardware, What we measured (power log, `D:\tools\chompi-session\power_log.py`, 10 readings a second) (+1 more)

### Community 142 - "SlotValues"
Cohesion: 0.20
Nodes (10): SlotValues, attack, end, gain, loop, pan, pitch, release (+2 more)

### Community 143 - "Forge — developer resume checkpoint"
Cohesion: 0.07
Nodes (30): 2026-10-11: agent hardware checks, soak, UX review, 2026-10-11 (end of day): 0.19 staged on the card, stress test done, 2026-10-11 (late night): 0.19.0 synth page built, 2026-10-11 (late night): guided test nearly done, repo refreshed, 2026-10-11 (later): 0.16.0 Batch 1 + 2 built, 2026-10-11 (latest): 0.17.0 magic items 1-3 built, 2026-10-11 (night): 0.17.1 + 0.18.0 built, Spark chosen, 2026-10-12: battery readings under investigation (+22 more)

### Community 145 - "InspectorVoice"
Cohesion: 0.25
Nodes (8): InspectorVoice, age, envelope, flags, note, slot, source, stage

### Community 146 - "Action"
Cohesion: 0.24
Nodes (7): Action, a, b, kind, Kind, Note, Param

### Community 147 - "Forge 0.19.2 — the one consolidated hardware test"
Cohesion: 0.08
Nodes (24): 0. Before you start (no CHOMPI needed), 1. Flash and identity, 1A. Inspector development candidate (before the normal audio checks), 2. External audio path (v1 compatibility), 3. Instrument, 3B. v3 instrument modules (firmware 0.4), 3C. Device presets on the SD card (TAPE-style keys + encoder), 3D. Sampler (TAPE-style, firmware 0.5) (+16 more)

### Community 148 - "Error"
Cohesion: 0.13
Nodes (14): Error, Busy, Checksum, Empty, Length, None, Opcode, Patch (+6 more)

### Community 150 - "SaveFile"
Cohesion: 0.57
Nodes (5): CopyFile(), EraseFile(), LoadFile(), SaveFile(), FilePath()

### Community 151 - "Kind"
Cohesion: 0.20
Nodes (10): Kind, Copy, Erase, Recall, RecordSource, SampleCopy, SampleErase, SampleSave (+2 more)

### Community 152 - "forge_ai.py"
Cohesion: 0.08
Nodes (21): 0.19.2 What you save is what wakes up (small tool: to be named) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS), Current checkpoint: firmware 0.15.1 review fixes (2026-10-06), data_js(), editor_js(), main(), outputs(), chat_prompt(), check_prompt() (+13 more)

### Community 158 - "struct"
Cohesion: 0.39
Nodes (3): check(), image_problems(), symbols()

### Community 159 - "SoundDeviceAudio"
Cohesion: 0.13
Nodes (3): FakeAudio, Skipped, SoundDeviceAudio

### Community 161 - "Rgb"
Cohesion: 0.12
Nodes (23): 4. The panel, for visualizations, StepColour(), ChordColour(), ComposeHarmonyKnobLeds(), ComposeLooperLeds(), ComposeMenuKnobLeds(), ComposePartsKnobLeds(), ComposeSynthKnobLeds() (+15 more)

### Community 162 - "forge_bridge.py"
Cohesion: 0.11
Nodes (7): main(), run(), main(), selftest(), integer(), midi_ports(), panel_message()

### Community 163 - "MenuMode"
Cohesion: 0.29
Nodes (6): MenuMode, CopyDest, CopySource, Erase, None, Save

### Community 164 - "forge_bench.cpp"
Cohesion: 0.60
Nodes (3): bench_block(), bench_init(), Noise()

### Community 165 - "KnobHold"
Cohesion: 0.40
Nodes (5): KnobHold, down, frames, turned, used

### Community 166 - "TranslateChannel"
Cohesion: 0.12
Nodes (16): Audio path, Boundaries, Device presets (SD card), Execution and ownership, Forge architecture, Patch lifecycle, Source map, Transport details (+8 more)

### Community 168 - "InstrumentTests"
Cohesion: 0.15
Nodes (3): envelope(), InstrumentTests, opener()

### Community 169 - "the forge family"
Cohesion: 0.33
Nodes (6): Choowi, the first of the family, how it talks, the forge family, the look, the rules, the tree

### Community 175 - "InspectorSnapshot"
Cohesion: 0.33
Nodes (6): InspectorSnapshot, audio, battery, generation, storage, system

### Community 176 - "Rig"
Cohesion: 0.15
Nodes (9): SampleTable, slots, Rig, engine, l, memory, r, rv (+1 more)

### Community 177 - "Event"
Cohesion: 0.40
Nodes (4): Event, note, source, velocity

### Community 178 - "Earlier checkpoint: boot fix and cleanup, 2026-10-03"
Cohesion: 0.67
Nodes (3): Browser hardware test bridge (supersedes terminal-only delivery below), Earlier checkpoint: boot fix and cleanup, 2026-10-03, Prior telemetry implementation and validation

### Community 179 - "Forge hardware test bridge"
Cohesion: 0.17
Nodes (12): Audio interface wiring (for the automatic checks), Automatic checks, Controls and evidence, Easiest: Forge Bridge.exe, Exact first physical check, Firmware updates and card files without removing the card (firmware 0.7), Forge hardware test bridge, From a source checkout (+4 more)

### Community 180 - "pack.js"
Cohesion: 0.12
Nodes (29): Command-line controller, Development Forge Inspector, Device presets on the SD card, Forge host controller, Instrument patches and playing, Live provider check, Offline DSP reference, Optional local-model authoring (+21 more)

### Community 181 - "Forge sampling (roadmap item 4): design"
Cohesion: 0.22
Nodes (9): 1. What TAPE does (the behaviour to keep), 2. What Forge changes, and why, 3. Memory, 5. Ownership and data flow, 6. Panel, 7. Host, AI and webapp, 8. Verification plan, 9. Not in this item (+1 more)

### Community 182 - "restart.h"
Cohesion: 0.29
Nodes (9): CountCrash(), DescribeEvent(), RanStably(), SafeModeStart(), StartupGuard, crashes, kMagic, magic (+1 more)

### Community 183 - "Colour"
Cohesion: 0.40
Nodes (5): BatteryColour(), Colour, b, g, r

### Community 184 - "Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark"
Cohesion: 0.20
Nodes (10): 1. Bootloader acceptance, 2. Memory layout, 3. Audio and controls, 3a. SD card coexistence (device presets, samples), 4. MIDI conventions, 5. Toolchains and build reproducibility, 6. CPU benchmark (`make bench`), 7. Power and system behaviour (firmware 0.8 review, 2026-10-05) (+2 more)

### Community 185 - "Stock TAPE: what every control does (reference for Forge parity)"
Cohesion: 0.20
Nodes (10): Boot and power, CHOMPI, PLAY, LOOP, Effects and looper, Framework, Keys, Knobs, normal play, Menu (shift layer while CHOMPI held, toggle TRUE), MIDI, panic, files (+2 more)

### Community 186 - "LockoutGuard"
Cohesion: 0.40
Nodes (5): 2026-10-11 (night) — shut-off caught by a power log; 0.19.2 installed and retested, LockoutGuard, held_, kCheckMs, kSustainChecks

### Community 187 - "State"
Cohesion: 0.33
Nodes (6): State, Armed, Empty, FirstTake, Paused, Playing

### Community 189 - "Forge control protocol — firmware 0.15"
Cohesion: 0.22
Nodes (9): Development probe and Inspector schema 1, Device presets (SD card), firmware 0.4, Envelope and operations, Event recorder loops on the SD card (firmware 0.15), Forge control protocol — firmware 0.15, MIDI clock (firmware 0.14), Notes, controls and recovery, Sampler, firmware 0.5 (+1 more)

### Community 191 - "Choowi: the first firmware of the forge"
Cohesion: 0.25
Nodes (8): Audio and controls, Build and test, Choowi: the first firmware of the forge, Device presets (SD card), First hardware session, Implementation boundaries, Sampler and recording (v4 patches), Synth and module patches

### Community 192 - "Compressor"
Cohesion: 0.33
Nodes (5): Compressor, gain_, peak_, SoftClip(), SoftLimit()

### Community 193 - "forge_main.cpp"
Cohesion: 0.06
Nodes (62): Balance(), BootCount, boots, magic, BootStep(), CaptureInspector(), ChargerReadings(), CollectInspector() (+54 more)

### Community 194 - "First time home with Forge 0.19.2"
Cohesion: 0.25
Nodes (7): 0. Power first (before any install), 1. Get the files, 2. Install the newest Choowi, 3. Fix the rig (from the last session's results), 4. Run the bridge, 5. Send the results back, First time home with Forge 0.19.2

### Community 195 - ".run_walk"
Cohesion: 0.22
Nodes (3): BridgeWalkTests, WalkTests, hand()

### Community 196 - "State"
Cohesion: 0.33
Nodes (6): State, Detaching, Headers, Idle, Job, Streaming

### Community 197 - "Ponytail, lazy senior dev mode"
Cohesion: 0.33
Nodes (4): Forge development guidance, Before you write, Ponytail, lazy senior dev mode, The smallest complete change

### Community 198 - "Forge looping (roadmap item 5): design"
Cohesion: 0.33
Nodes (6): 1. What TAPE does (the behaviour to keep), 2. Forge plan, 3. Decisions (the owner, 2026-10-03), 4. Risks, 5. Build order (each step tested before the next), Forge looping (roadmap item 5): design

### Community 199 - "Forge project brief"
Cohesion: 0.29
Nodes (7): Architecture and acceptance, Current milestone: Choowi 0.19 release (2026-10-11), First milestone: instrument candidate 0.4, Forge project brief, Goal and user decisions, Instrument candidate 0.15.1 (history), Roadmap (priority order agreed 2026-10-02)

### Community 200 - "InspectorEvent"
Cohesion: 0.17
Nodes (9): ObserveVoiceEdges(), InspectorEvent, id, kind, serial, time_ms, value, SetInspectorEvents() (+1 more)

### Community 201 - "node"
Cohesion: 0.25
Nodes (3): node(), random_patch(), WireJsTests

### Community 202 - "Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10)"
Cohesion: 0.50
Nodes (4): Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10), Patch knobs (patch version 5), Tests, What the player sees

### Community 204 - "Forge — live OpenAI / Gemini test"
Cohesion: 0.40
Nodes (5): Forge — live OpenAI / Gemini test, If it fails, Rules, Step 1 — command-line preflight (one request per provider), Step 2 — webapp

### Community 205 - "the forge docs"
Cohesion: 0.40
Nodes (5): Choose a starting point, Documentation map, Maintaining these docs, the forge docs, What is implemented and what is pending

### Community 206 - "Forge developer guide"
Cohesion: 0.20
Nodes (9): Build firmware, CPU benchmark against stock firmware, Create a test bundle, Forge developer guide, Get the development branch, Requirements, Troubleshooting, Validate software (+1 more)

### Community 207 - "Forge hardware test kit — instructions for an agent on the test PC"
Cohesion: 0.40
Nodes (4): Forge hardware test kit — instructions for an agent on the test PC, Reference, Rules, What you can do here

### Community 211 - "build"
Cohesion: 0.31
Nodes (9): 0.15.2 Build identity, held keys per source — 2026-10-10 (development build on CHOMPI since 2026-10-10, 34/34 automatic steps; the Shift and arp phrase-start fixes are software-tested only, not installed yet), build(), fromSlider(), loadPatch(), path(), read(), showJSON(), toSlider() (+1 more)

### Community 217 - "SlotSettingsSchedule"
Cohesion: 0.33
Nodes (6): SlotSettingsSchedule, changed_at, kMaxWait, kSettle, seen, wait

### Community 219 - "ComposeTransportLeds"
Cohesion: 0.50
Nodes (4): Cross-cutting fixes (they lift most rows), Forge UX review (2026-10-11), Ratings, ComposeTransportLeds()

## Knowledge Gaps
- **1434 isolated node(s):** `reverb_`, `del_`, `filter_`, `warble_`, `dcblock_fx_l_` (+1429 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 2308 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **40 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Engine` connect `Engine` to `SlotSettings`, `Conversation`, `main`, `looper_test.cpp`, `.Init`, `EngineAndPanel`, `v3_test.cpp`, `.Value`, `synth_test.cpp`, `panel_controller.h`, `.Note`, `Rig`, `Player`, `Sequencer`, `FORGE_COLD`, `Rig`, `Rig`, `.Block`, `runtime.h`, `forge_main.cpp`, `Reverb`, `Rig`, `RecoveryAfterLostNotes`, `EngineAndPanel`, `Mailbox`, `iostream`, `secret.h`, `slot_settings_test.cpp`, `SampleHandoff`, `Performance`?**
  _High betweenness centrality (0.141) - this node is a cross-community bridge._
- **Are the 9 inferred relationships involving `Engine` (e.g. with `main()` and `PhysicalVersusInjectedAndVoiceEdges()`) actually correct?**
  _`Engine` has 9 INFERRED edges - model-reasoned connections that need verification._
- **What connects `reverb_`, `del_`, `filter_` to the rest of the system?**
  _1434 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Synth` be split into smaller, more focused modules?**
  _Cohesion score 0.02272159800249688 - nodes in this community are weakly interconnected._
- **Why does `PanelController` connect `PanelController` to `Options`, `PresetMenu`, `main`, `forge_ai.py`, `EngineAndPanel`, `panel_controller.h`, `Rig`, `Dream`, `KnobHold`, `Rig`, `.Block`, `InspectorEvent`, `Rig`, `TransferAndInstall`, `EngineAndPanel`, `RecordGesture`, `iostream`, `SpscQueue`, `Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05`, `Chime`?**
  _High betweenness centrality (0.092) - this node is a cross-community bridge._
- **Are the 5 inferred relationships involving `PanelController` (e.g. with `Current checkpoint: firmware 0.15.1 review fixes (2026-10-06)` and `Handoff: cloud session → the owner's local PC (2026-10-09)`) actually correct?**
  _`PanelController` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Should `PanelController` be split into smaller, more focused modules?**
  _Cohesion score 0.020606060606060607 - nodes in this community are weakly interconnected._