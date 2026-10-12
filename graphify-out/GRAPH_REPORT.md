# Graph Report - CHOMPI  (2026-10-12)

## Corpus Check
- 155 files · ~315,186 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 4, .css 2, .ld 1)

## Summary
- 4516 nodes · 8847 edges · 236 communities (200 shown, 36 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 1004 edges (avg confidence: 0.87)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `9e1335d1`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Synth
- Walk
- PanelController
- Card
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
- time
- Rig
- DjFilter
- secret_test.cpp
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
- preset_store.h
- Effects
- HandleFrame
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
- Dream
- runtime.h
- SimulatedMido
- file_transfer.h
- harmony.h
- InspectorEventKind
- EngineAndPanel
- RecordGesture
- Mailbox
- FatFsUploadFiles
- core_test.cpp
- BridgeTests
- FakeMido
- WebTests
- InspectorMailbox
- main
- SampleSlot
- ChargerUsb
- FaultRecord
- Bridge
- FatFsSampleFiles
- AnalysisTests
- Device
- Forge · Choowi: the user manual
- FakeChompi
- Chord
- secret.h
- Rig
- MidiPort
- SampleHandoff
- SlotPolicy
- harmony_test.cpp
- FileTransfer
- Performance
- CardUploads
- Roll
- FatFsStorage
- Mmf
- MidiFrame
- MenuAction
- Forge Resource Ledger
- Sequence
- BridgeAutomaticTests
- BridgeBrowserTests
- MemoryStorage
- send.js
- Kind
- Envelope
- Battery
- .Note
- Handler
- .Build
- power_test.cpp
- tape_bench.cpp
- .Process
- Conversation
- Options
- ResponseKind
- Transport
- CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12)
- .ApplyPatch
- Forge — developer resume checkpoint
- ProviderTests
- InspectorVoice
- Chime
- Forge 0.19.2 — the one consolidated hardware test
- Error
- .ProcessActive
- SaveFile
- Kind
- forge_ai.py
- MidiFramer
- CardTests
- FirmwarePanelSink
- .Valid
- Forge host controller
- SessionDevice
- SoundDeviceAudio
- PanelSink
- Rgb
- forge_bridge_app.py
- MenuMode
- .GetState
- KnobHold
- LoaderRig
- Sink
- InstrumentTests
- the forge family
- BridgeWebTests
- V4PatchTests
- ForgeServer
- synth.h
- AiCheckTests
- InspectorSnapshot
- Rig
- Chromatic
- Earlier checkpoint: boot fix and cleanup, 2026-10-03
- Forge hardware test bridge
- pack.js
- Forge sampling (roadmap item 4): design
- restart.h
- Restart
- Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark
- Stock TAPE: what every control does (reference for Forge parity)
- InspectorEvent
- PanelFeedback016
- InspectorTests
- Forge control protocol — firmware 0.15
- Sink
- Choowi: the first firmware of the forge
- Compressor
- forge_main.cpp
- First time home with Forge 0.19.2
- .run_walk
- SiteTests
- Bloom
- Forge looping (roadmap item 5): design
- Forge project brief
- SpscQueue
- node
- Outgoing
- LooperThroughThePanel
- Forge — live OpenAI / Gemini test
- the forge docs
- Forge developer guide
- Forge hardware test kit — instructions for an agent on the test PC
- Rng
- Choowi's sample pack
- BootCount
- PanelInput
- .prepare
- BridgeCardTests
- FakeChompi
- .TurnLoopSpeed
- SlotSettingsSchedule
- PresetProtocolTests
- ComposeTransportLeds
- BatteryLogTests
- MonitorMode
- RenderPlayLeds
- State
- MotifStep
- Slot
- .RecordParam
- LockoutGuard
- forge_bench.cpp
- Event
- Colour
- .audio
- Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10)
- Owner

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
- `Development probe and Inspector schema 1` --references--> `EncodePatchData()`  [INFERRED]
  docs/forge/PROTOCOL.md → firmware/chompi-forge/core/protocol.h
- `Hardware: 0.15.2 on CHOMPI, unattended run (2026-10-10 evening)` --references--> `detect()`  [INFERRED]
  docs/forge/CONTINUE.md → firmware/chompi-forge/host/forge_audio.py
- `0.17.0 Magic: wakes up singing, surprise me, mutate (tool: Wake-up Bell) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS)` --references--> `Magic017()`  [INFERRED]
  docs/forge/CHANGELOG.md → firmware/chompi-forge/tests/panel_test.cpp
- `2026-10-11 (latest): 0.17.0 magic items 1-3 built` --references--> `Magic017()`  [INFERRED]
  docs/forge/CONTINUE.md → firmware/chompi-forge/tests/panel_test.cpp
- `0.19.0 The synth edit page (tool: Tinker Wrench) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS)` --references--> `SynthPage019()`  [INFERRED]
  docs/forge/CHANGELOG.md → firmware/chompi-forge/tests/panel_test.cpp

## Import Cycles
- None detected.

## Communities (236 total, 36 thin omitted)

### Community 0 - "Synth"
Cohesion: 0.02
Nodes (77): Synth, age_, amp_, bend_slew_, bend_target_, cap_, coefficient_, cutoff_ (+69 more)

### Community 1 - "Walk"
Cohesion: 0.12
Nodes (6): Previous checkpoint: first hardware session and bridge test tooling, 2026-10-04, panel_gesture(), tap(), Stop, Walk, test()

### Community 2 - "PanelController"
Cohesion: 0.02
Nodes (77): PanelController, activity_, audition_, battery_view_, bending_, block_count_, cc_number_, cc_sent_ (+69 more)

### Community 3 - "Card"
Cohesion: 0.14
Nodes (10): Checkpoint: firmware 0.11 install safety (2026-10-05), Card(), allowed(), Card, cli(), progress(), files_in(), message() (+2 more)

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
Cohesion: 0.06
Nodes (45): 1. What Forge is, 3. Patches, 5. Where things live (`firmware/chompi-forge/`), 6. How to make an effect, 7. How to edit the firmware, 8. Read next, Forge for other agents (Muse / Cosmo on the Tab5), 0.6 knob pages and patch knobs — 2026-10-04 (+37 more)

### Community 10 - "forge_audio.py"
Cohesion: 0.06
Nodes (29): Checkpoint: firmware 0.15.2, HANDOFF items 2-4 (2026-10-10, local PC), analyze(), audible_change(), cli(), clicks(), compare(), db(), detect() (+21 more)

### Community 11 - "PresetMenu"
Cohesion: 0.06
Nodes (25): KeyToSlot(), MenuPage, Harmony, Parts, Presets, Samples, Synth, PresetMenu (+17 more)

### Community 12 - "json"
Cohesion: 0.08
Nodes (6): check(), image_problems(), symbols(), WindowsRuntimeTests, wav(), ManualPageTests

### Community 13 - "Parameter"
Cohesion: 0.05
Nodes (44): IsPerformance(), Parameter, Attack, Bypass, Compressor, Count, Cutoff, Decay (+36 more)

### Community 14 - "Voice"
Cohesion: 0.05
Nodes (44): Voice, age, amp, band, band_r, complete, end_frac, filter (+36 more)

### Community 15 - "Parts"
Cohesion: 0.06
Nodes (34): Parts, arp_left_, arp_note_, bass_, bass_change_, bass_count_, bass_left_, bass_step_ (+26 more)

### Community 16 - "main"
Cohesion: 0.07
Nodes (19): Recorder, dc_coefficient_, dc_l_, dc_r_, fade_, input_gain_, input_gain_target_, memory_ (+11 more)

### Community 17 - "forge_inspector.py"
Cohesion: 0.06
Nodes (36): 2. How to talk to CHOMPI, Success/status response, env(), panel_ack(), decode_reply(), checksum(), control_value(), decode_identity() (+28 more)

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
Nodes (25): ChromaticAndKitLoading(), ChromaticPitch(), CubicTableRead(), FactoryTapeFiles(), Fill(), GateAndTrigger(), HeaderRoundTrip(), KitMapping() (+17 more)

### Community 22 - "app.js"
Cohesion: 0.12
Nodes (26): 0.15.2 Build identity, held keys per source — 2026-10-10 (development build on CHOMPI since 2026-10-10, 34/34 automatic steps; the Shift and arp phrase-start fixes are software-tested only, not installed yet), adopt(), api(), build(), CONTROLS, fromSlider(), GROUPS, KNOB_LABELS (+18 more)

### Community 23 - "SampleLoader"
Cohesion: 0.06
Nodes (27): Inspect(), SampleLoader, count_, current_, cursor_, handoff_, head_, index_ (+19 more)

### Community 24 - "BatteryDiag"
Cohesion: 0.07
Nodes (28): BatteryDiag, after_30, after_33, battery_low, check_state, checks, expect_33, input_limit (+20 more)

### Community 25 - ".Init"
Cohesion: 0.19
Nodes (17): Development plan (the owner, 2026-10-02): features first, QA per feature later, Roadmap item 2: v3 instrument palette (implemented; software-tested), DecodeRequest(), Read14(), ExecuteRequest(), PackUsbSysEx(), Framing(), main() (+9 more)

### Community 26 - "v3_test.cpp"
Cohesion: 0.15
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
Cohesion: 0.11
Nodes (21): Roadmap item 1: sustain pedal and pitch bend (implemented; software-tested), Sound fixes (implemented 2026-10-02 UTC; software-tested only), AudioCallback(), AliasDb(), Callback, gate, replies, ChannelTranslation() (+13 more)

### Community 31 - "panel_controller.h"
Cohesion: 0.11
Nodes (7): Find(), Parse(), ComposeHarmonyKnobLeds(), ExtensionStep(), Clamp(), TapeSpeedKnob(), AudioBlock()

### Community 32 - "time"
Cohesion: 0.12
Nodes (10): show_presets_page(), ask(), charge_name(), cli(), decode_registers(), describe(), flatten(), Logger (+2 more)

### Community 33 - "Rig"
Cohesion: 0.10
Nodes (17): Rig, engine, hw, l, panel, r, rec, recorder (+9 more)

### Community 34 - "DjFilter"
Cohesion: 0.12
Nodes (14): DjFilter, control_, hp_, hp_c_, hp_target_, hpl_, hpr_, lp_ (+6 more)

### Community 35 - "secret_test.cpp"
Cohesion: 0.46
Nodes (4): BloomInEngine(), main(), Mischief(), Panel()

### Community 37 - "Player"
Cohesion: 0.07
Nodes (21): 2026-10-11 — automatic run on the SD-fix build (the owner away, webcam + UMC), Held, count, key, note, source, used, Player (+13 more)

### Community 38 - "Sequencer"
Cohesion: 0.12
Nodes (9): Sequencer, cursor_, held_, pending_off_, playing_, pos_, sequence_, state_ (+1 more)

### Community 39 - "BatteryMonitor"
Cohesion: 0.20
Nodes (12): BatteryMonitor, d_, kPeriodicMs, periodic_at_, read_33_, ReadBack, After30, After33 (+4 more)

### Community 40 - "Request"
Cohesion: 0.07
Nodes (28): Request, action, bank, command, epoch, kind, mode, note (+20 more)

### Community 41 - "Response"
Cohesion: 0.07
Nodes (27): 2026-10-12: RC tutorial and manual refresh, Forge handoff, Handoff: cloud session → the owner's local PC (2026-10-09), Response, action, bank, battery, capacity_ms (+19 more)

### Community 42 - "SampleFiles"
Cohesion: 0.18
Nodes (7): 2. What the code does now, ParseSampleName(), SampleEvent, job, ok, SampleFiles, SamplePath()

### Community 43 - "firmware_chompi_forge_core_sequencer"
Cohesion: 0.15
Nodes (14): , Action, a, b, kind, Exported, Exporting, Free (+6 more)

### Community 44 - "Pad"
Cohesion: 0.10
Nodes (17): Pad, amp, attack, end, gate, loop, pan_l, pan_r (+9 more)

### Community 45 - "Warble"
Cohesion: 0.13
Nodes (12): Warble, coefficient_, end_, frequency_, kLength, left_, length_, mix_ (+4 more)

### Community 46 - "Rig"
Cohesion: 0.11
Nodes (13): Rig, engine, hw, l, panel, player, r, rec (+5 more)

### Community 47 - "LedView"
Cohesion: 0.06
Nodes (31): LedView, blink, count_in, dream, dream_level, flash, harmony, input_level (+23 more)

### Community 49 - "panel_test.cpp"
Cohesion: 0.33
Nodes (15): ComposeLeds(), TapeSpeedRatio(), BatteryHold(), Dream018(), KeysKnobsAndOverrides(), KnobPages(), LedComposition(), LooperVoiceCap() (+7 more)

### Community 50 - "SampleCard"
Cohesion: 0.10
Nodes (12): SampleCard, append_budget, fail_reads, fail_writes, files, kTempName, opens, read_name_ (+4 more)

### Community 51 - "parts_test.cpp"
Cohesion: 0.15
Nodes (16): Arp(), ArpPatterns(), BassModesAndMidi(), ChordGathering(), GateLatchAndOwnership(), main(), Note, note (+8 more)

### Community 52 - "knob_layout.h"
Cohesion: 0.19
Nodes (20): Code, Fade(), Fade3(), Fade4(), HasPatchPage(), IsPatchPage(), Kind, Effects (+12 more)

### Community 53 - ".Block"
Cohesion: 0.10
Nodes (7): EditSynth(), MagicCombo(), RenderHarmonyLeds(), QuantisedSpeedStep(), Motif, count, step

### Community 54 - "Settings"
Cohesion: 0.05
Nodes (43): Apply request layout, Inspect(), Bass, Alternate, Fifth, Octave, Off, Root (+35 more)

### Community 55 - "preset_store.h"
Cohesion: 0.19
Nodes (5): Crc16(), DecodePresetRecord(), EncodePresetRecord(), PresetPath(), Storage

### Community 56 - "Effects"
Cohesion: 0.12
Nodes (16): Effects, active_, compressor_, compressor_target_, dj_, dj_active_, left_, makeup_ (+8 more)

### Community 57 - "HandleFrame"
Cohesion: 0.24
Nodes (14): FillBattery(), Flash(), forge::Error RecallPreset(), HandleFrame(), HandleStorage(), InspectorStorageError(), LoadSequence(), Queue() (+6 more)

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
Cohesion: 0.13
Nodes (6): Clocked, clock, out, s, MemoryCard, files

### Community 64 - "file_transfer_test.cpp"
Cohesion: 0.42
Nodes (13): Crc32(), Begin(), CodecAndNames(), Data(), Decode(), Firmware(), FirmwareGuards(), Fuzz() (+5 more)

### Community 65 - "Forge test results"
Cohesion: 0.12
Nodes (17): 2026-10-04 evening — first 0.7 session (the owner), development firmware (`b36ba99`), 2026-10-04 — first hardware session (the owner), firmware 0.6 development (`131776b`), 2026-10-05 — 0.9 on CHOMPI (the owner, verbal): open issue, random shut-off, 2026-10-05 — firmware 0.8 installed over USB: all automatic checks pass, 2026-10-05 — line in wired, 0.7 on CHOMPI, 0.8 bridge, 2026-10-05 — power on 0.8 (the owner, verbal): TEST_SESSION 8, 2026-10-05 — the owner's panel-map walk-through on 0.9 (verbal), 2026-10-10 (late) — start-up stall found and fixed on CHOMPI (+9 more)

### Community 66 - "Choowi: how do I…?"
Cohesion: 0.09
Nodes (22): before you start: names used here, cheat sheet, check it worked, Choowi: how do I…?, good to know, how do I find my way around the menu?, how do I get surprise sounds and keep one?, how do I install Choowi? (+14 more)

### Community 67 - "probe"
Cohesion: 0.17
Nodes (3): Checkpoint: per-source held keys (HANDOFF item 1, 2026-10-10, local PC), PatchTests, probe()

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
Cohesion: 0.23
Nodes (14): KnobOrder(), LedModel(), main(), MenuGestures(), Panel, menu, toggle, Protocol() (+6 more)

### Community 72 - "FakeMidi"
Cohesion: 0.13
Nodes (4): FakeMidi, Message, FakePort, TransportTests

### Community 73 - "Rig"
Cohesion: 0.08
Nodes (25): Change(), Dependency(), Features, balance, envelope, spectrum, Fft(), main() (+17 more)

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
Nodes (34): 0.10 TAPE parity — 2026-10-05 (ready to install; per-slot settings moved to 0.12), 0.11 Install safety — 2026-10-05 (software-tested; not installed yet), 0.12 Per-slot sample settings — 2026-10-05 (software-tested; not installed yet), 0.13 Harmony — 2026-10-05 (software-tested; not installed yet), 0.14 Arp, bass and tempo — 2026-10-05 (software-tested; not installed yet), 0.15.1 Review fixes — 2026-10-06 (software-tested; not installed yet), 0.15 Event recorder and projects — 2026-10-05 (software-tested; not installed yet), 0.17.0 Magic: wakes up singing, surprise me, mutate (tool: Wake-up Bell) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS) (+26 more)

### Community 79 - "argparse"
Cohesion: 0.10
Nodes (12): files(), main(), verify(), main(), run(), data_url(), main(), render() (+4 more)

### Community 80 - "Dream"
Cohesion: 0.13
Nodes (16): 0.18.0 Magic: chomp to instrument, dream mode, lights with character (tool: Dream Bellows) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS), Dream, asleep_, degree_, elapsed_, held_, idle_, idle_after_ (+8 more)

### Community 81 - "runtime.h"
Cohesion: 0.16
Nodes (16): Source map, 0.3 playability: sustain pedal and pitch bend — 2026-10-02, Roadmap item 3: device presets on the SD card (implemented; software-tested), PresetStore, EraseReply(), Ingress, Control, Critical (+8 more)

### Community 82 - "SimulatedMido"
Cohesion: 0.07
Nodes (10): _Input, Message, _Output, _Port, SimulatedMido, PresetCliAndWebTests, SampleMessageTests, SampleWebAndAiTests (+2 more)

### Community 83 - "file_transfer.h"
Cohesion: 0.12
Nodes (22): AllowedUploadName(), DecodeFileRequest(), EncodeFileReply(), FileOp, Abort, Begin, Data, End (+14 more)

### Community 84 - "harmony.h"
Cohesion: 0.08
Nodes (37): InKey(), Layout, Real, Static, Lead(), Mode, Dorian, HarmonicMinor (+29 more)

### Community 85 - "InspectorEventKind"
Cohesion: 0.12
Nodes (16): InspectorEventKind, Card, KeyDown, KeyUp, Knob, PatchApply, QueueError, RecordingStart (+8 more)

### Community 86 - "EngineAndPanel"
Cohesion: 0.09
Nodes (8): Inspect(), MidiOut, data1, data2, status, EngineAndPanel(), EngineAndPanel(), EngineAndPanel()

### Community 87 - "RecordGesture"
Cohesion: 0.13
Nodes (10): Event, None, Start, Stop, RecordGesture, chompi_, count_, counting_ (+2 more)

### Community 88 - "Mailbox"
Cohesion: 0.16
Nodes (13): DecodeFile(), EncodeFile(), FileCrc(), Mailbox, bank, owner, sequence, slot (+5 more)

### Community 89 - "FatFsUploadFiles"
Cohesion: 0.17
Nodes (7): FatFsUploadFiles, aside_, check_, file_, kAsideBatch, target_, verify_buffer_

### Community 90 - "core_test.cpp"
Cohesion: 0.23
Nodes (11): DelayImpulseAndBypass(), Fixture, engine, left, right, InitializationAndDryStereo(), main(), Near() (+3 more)

### Community 92 - "FakeMido"
Cohesion: 0.16
Nodes (3): FakeMido, TestTrafficCommands, interrupt()

### Community 93 - "WebTests"
Cohesion: 0.13
Nodes (7): Audit of the starting head, Exact next hardware test, Forge Inspector — first development slice, Realtime ownership, Run the Inspector, Validation at this checkpoint, WebTests

### Community 94 - "InspectorMailbox"
Cohesion: 0.31
Nodes (4): InspectorMailbox, owner_, state_, MailboxOwnership()

### Community 95 - "main"
Cohesion: 0.32
Nodes (12): FlashSafeMode(), LoadOptions(), LoadSlotSettings(), LogLockout(), LogRestart(), main(), MountCard(), ReadBackThreshold() (+4 more)

### Community 96 - "SampleSlot"
Cohesion: 0.08
Nodes (22): 10. Answers to the brief's questions, 3. Precedent: stock TAPE already streams from SD on this hardware, 5.1 Sample streaming: shared page cache + pinned heads (one pool), 5.2 Underrun and failure semantics, 5.3 Streaming recorder, 5.4 Looper, 5.5 QSPI (Tier 2), 5. Proposed design (+14 more)

### Community 97 - "ChargerUsb"
Cohesion: 0.11
Nodes (18): 2026-10-11 (night, later): shut-off caught, fix built, 2026-10-10 — 0.15.2 on CHOMPI: first install since 0.9, unattended automatic run, 2026-10-11 (night) — power bank: no charging after a re-plug while on, Actions, force_detection, read, usb_to_daisy, ChargerUsb (+10 more)

### Community 98 - "FaultRecord"
Cohesion: 0.20
Nodes (10): Describe(), FaultRecord, cfsr, count, hfsr, kMagic, lr, magic (+2 more)

### Community 99 - "Bridge"
Cohesion: 0.21
Nodes (6): add(), Bridge, fetch(), body_(), say(), now()

### Community 100 - "FatFsSampleFiles"
Cohesion: 0.17
Nodes (5): FatFsSampleFiles, read_, reading_, write_, writing_

### Community 101 - "AnalysisTests"
Cohesion: 0.21
Nodes (3): AnalysisTests, walk(), sine()

### Community 102 - "Device"
Cohesion: 0.16
Nodes (3): BridgeWalkTests, SetupCheckTests, Device

### Community 103 - "Forge · Choowi: the user manual"
Cohesion: 0.07
Nodes (30): 10. On the computer, 11. What Forge keeps on the SD card, 12. Troubleshooting, 1. Quick start, 2. Installing and updating, 3. Power and battery, 4. The panel, 5. Knobs and knob pages (+22 more)

### Community 104 - "FakeChompi"
Cohesion: 0.14
Nodes (3): DetectionTests, louder_single(), FakeChompi

### Community 105 - "Chord"
Cohesion: 0.08
Nodes (26): Chord, degree, kind, quality, root, shifted, tone, Function (+18 more)

### Community 106 - "secret.h"
Cohesion: 0.15
Nodes (14): Advance(), Blooms(), FamilyOf(), Interrupt(), Mode, BloomDown, BloomRandom, BloomUp (+6 more)

### Community 107 - "Rig"
Cohesion: 0.20
Nodes (8): Rig, engine, l, r, rv, sine, slots, table

### Community 108 - "MidiPort"
Cohesion: 0.17
Nodes (7): MidiPort, clock_dropped, dropped, framer, frames, transport, PollMidi()

### Community 109 - "SampleHandoff"
Cohesion: 0.14
Nodes (9): Samples (SD card) and the memory handoff, PackSelection(), SampleHandoff, ack_, published_, request_, FailedLoadIsVisible(), main() (+1 more)

### Community 110 - "SlotPolicy"
Cohesion: 0.40
Nodes (5): Checkpoint: firmware 0.12 per-slot sample settings (2026-10-05), SlotPolicy, Patch, Recall, Select

### Community 111 - "harmony_test.cpp"
Cohesion: 0.16
Nodes (16): Extension, Eleventh, Fifth, Ninth, Seventh, Thirteenth, Triad, Tones() (+8 more)

### Community 112 - "FileTransfer"
Cohesion: 0.13
Nodes (12): FileTransfer, crc_, done_, head_, kTemp, name_, sample_written_, size_ (+4 more)

### Community 113 - "Performance"
Cohesion: 0.17
Nodes (10): Performance, compressor, dj_filter, dj_resonance, input_gain, pan, saturation, speed (+2 more)

### Community 114 - "CardUploads"
Cohesion: 0.10
Nodes (6): CardUploads, open, temp, MemoryCard, files, SineWav()

### Community 115 - "Roll"
Cohesion: 0.21
Nodes (9): Held, frames, note, Mutate(), Rng, s, Roll(), Stop() (+1 more)

### Community 116 - "FatFsStorage"
Cohesion: 0.21
Nodes (3): FatFsStorage, file_, mounted_

### Community 118 - "Mmf"
Cohesion: 0.24
Nodes (6): Mmf, buf0_, buf1_, MmfCoefficients, feedback, freq

### Community 119 - "MidiFrame"
Cohesion: 0.17
Nodes (12): Kind, CC, Clock, NoteOff, NoteOn, PitchBend, ProgramChange, SysEx (+4 more)

### Community 120 - "MenuAction"
Cohesion: 0.08
Nodes (12): MenuAction, bank, kind, mode, slot, to_bank, to_mode, to_slot (+4 more)

### Community 121 - "Forge Resource Ledger"
Cohesion: 0.18
Nodes (11): Forge Resource Ledger, Memory map and working set, QA and CPU baseline, Reproduced baseline, Snapshot 2026-10-05: CPU fix A (0.13, not installed), Snapshot 2026-10-05: firmware 0.13 harmony (Phase 1 complete), Snapshot 2026-10-05: firmware 0.14 clock, arp and bass (not installed), Snapshot 2026-10-05: firmware 0.15 event recorder and projects (not installed) (+3 more)

### Community 122 - "Sequence"
Cohesion: 0.20
Nodes (9): Event, a, b, kind, tick, Sequence, count, events (+1 more)

### Community 125 - "MemoryStorage"
Cohesion: 0.17
Nodes (5): MemoryStorage, drop_writes, fail_writes, files, ready

### Community 126 - "send.js"
Cohesion: 0.27
Nodes (8): api(), connected(), exchange(), notice(), pick(), PREFIX, refresh(), run()

### Community 127 - "Kind"
Cohesion: 0.17
Nodes (11): Kind, Jack, Key, Press, Release, Toggle, Turn, PanelEvent (+3 more)

### Community 128 - "Envelope"
Cohesion: 0.20
Nodes (10): Envelope, release_step, stage, value, Stage, Attack, Decay, Off (+2 more)

### Community 129 - "Battery"
Cohesion: 0.17
Nodes (13): Battery, Full, High, Low, Medium, Unknown, DecodeStatus(), Status (+5 more)

### Community 132 - ".Build"
Cohesion: 0.19
Nodes (7): Family, Call, Echo, Groove, Sparkle, Rand, s

### Community 133 - "power_test.cpp"
Cohesion: 0.29
Nodes (8): BootGesture, held_, BootGestureOff(), Colours(), LockoutDip(), main(), OptionsFile(), StatusDecode()

### Community 135 - "tape_bench.cpp"
Cohesion: 0.18
Nodes (3): bench_block(), bench_init(), Noise()

### Community 136 - ".Process"
Cohesion: 0.14
Nodes (3): 1. Baseline (verified from source and the ELF), 4. Architectural blockers and couplings, GlideStep()

### Community 137 - "Conversation"
Cohesion: 0.15
Nodes (12): Conversation, armed_, at_, count_, kMaxPhrase, last_family_, last_motif_, phrase_ (+4 more)

### Community 138 - "Options"
Cohesion: 0.22
Nodes (8): Options, midi_in, midi_out, monitor, quantise_menu, record_latch, split_delay, tape_slew

### Community 139 - "ResponseKind"
Cohesion: 0.22
Nodes (9): ResponseKind, Erased, Occupancy, SampleDone, SampleOccupancy, SampleSnapshot, Snapshot, Status (+1 more)

### Community 141 - "CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12)"
Cohesion: 0.29
Nodes (7): CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12), How the readings are made (stock code: `chompi-wave/code/src/hardware.h`), Hypotheses and how to tell them apart, Problems in the reading itself (code), Proposed next steps (not done), The hardware, What we measured (power log, `D:\tools\chompi-session\power_log.py`, 10 readings a second)

### Community 142 - ".ApplyPatch"
Cohesion: 0.12
Nodes (10): SlotValues, attack, end, gain, loop, pan, pitch, release (+2 more)

### Community 143 - "Forge — developer resume checkpoint"
Cohesion: 0.06
Nodes (36): 2026-10-11: agent hardware checks, soak, UX review, 2026-10-11 (end of day): 0.19 staged on the card, stress test done, 2026-10-11 (late night): 0.19.0 synth page built, 2026-10-11 (late night): guided test nearly done, repo refreshed, 2026-10-11 (later): 0.16.0 Batch 1 + 2 built, 2026-10-11 (latest): 0.17.0 magic items 1-3 built, 2026-10-11 (night): 0.17.1 + 0.18.0 built, Spark chosen, 2026-10-12: battery readings under investigation (+28 more)

### Community 145 - "InspectorVoice"
Cohesion: 0.25
Nodes (8): InspectorVoice, age, envelope, flags, note, slot, source, stage

### Community 146 - "Chime"
Cohesion: 0.21
Nodes (9): Chime, kDelay, kLength, kNotes, kStep, kTotal, kVelocity, t_ (+1 more)

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
Nodes (20): 0.19.2 Secret modes, the forge site, bank purple (small tool: Whatzit) — 2026-10-12 (release candidate; on CHOMPI as build b462bea; checks in TEST_RESULTS), Current checkpoint: firmware 0.15.1 review fixes (2026-10-06), data_js(), editor_js(), main(), outputs(), chat_prompt(), check_prompt() (+12 more)

### Community 153 - "MidiFramer"
Cohesion: 0.22
Nodes (8): MidiFramer, bytes_, cc_, cc_used_, overflow_, status_, sysex_, used_

### Community 154 - "CardTests"
Cohesion: 0.09
Nodes (5): CardTests, DeadLink, LossyTransport, LostEndReply, Sequence

### Community 155 - "FirmwarePanelSink"
Cohesion: 0.22
Nodes (4): FirmwarePanelSink, PanelAction, action, patch

### Community 156 - ".Valid"
Cohesion: 0.32
Nodes (7): PartsWordsValid(), Base(), ChompRollsKeepTheSample(), main(), MutationsDriftInsideTheirBounds(), RollsArePlayable(), RollsAreValidAndBounded()

### Community 157 - "Forge host controller"
Cohesion: 0.17
Nodes (12): Card files, installs, battery log, sample pack, Command-line controller, Development Forge Inspector, Device presets on the SD card, Forge host controller, Instrument patches and playing, Live provider check (developer, CLI only), Making sounds: the forge page and the local workshop (+4 more)

### Community 159 - "SoundDeviceAudio"
Cohesion: 0.13
Nodes (3): FakeAudio, Skipped, SoundDeviceAudio

### Community 161 - "Rgb"
Cohesion: 0.16
Nodes (18): 4. The panel, for visualizations, StepColour(), ComposeLooperLeds(), ComposeMenuKnobLeds(), ComposePartsKnobLeds(), ComposeSynthKnobLeds(), RenderPartsLeds(), RenderSynthLeds() (+10 more)

### Community 163 - "MenuMode"
Cohesion: 0.29
Nodes (6): MenuMode, CopyDest, CopySource, Erase, None, Save

### Community 164 - ".GetState"
Cohesion: 0.29
Nodes (6): State, Armed, Empty, Playing, Recording, Stopped

### Community 165 - "KnobHold"
Cohesion: 0.40
Nodes (5): KnobHold, down, frames, turned, used

### Community 166 - "LoaderRig"
Cohesion: 0.18
Nodes (10): LoaderRig, card, events, handoff, loader, loop, pool, recorder (+2 more)

### Community 168 - "InstrumentTests"
Cohesion: 0.15
Nodes (3): envelope(), InstrumentTests, opener()

### Community 169 - "the forge family"
Cohesion: 0.14
Nodes (11): Forge development guidance, Choowi, the first of the family, how it talks, the forge family, the look, the rules, the tree, whose it is (+3 more)

### Community 173 - "synth.h"
Cohesion: 0.29
Nodes (7): CubicQ14(), CubicTable, w, Dual16(), MakeCubicTable(), PackHigh(), PackLow()

### Community 175 - "InspectorSnapshot"
Cohesion: 0.33
Nodes (6): InspectorSnapshot, audio, battery, generation, storage, system

### Community 176 - "Rig"
Cohesion: 0.15
Nodes (9): SampleTable, slots, Rig, engine, l, memory, r, rv (+1 more)

### Community 177 - "Chromatic"
Cohesion: 0.29
Nodes (6): SelectSample(), Chromatic(), Kit(), main(), Near(), Sampler()

### Community 178 - "Earlier checkpoint: boot fix and cleanup, 2026-10-03"
Cohesion: 0.67
Nodes (3): Browser hardware test bridge (supersedes terminal-only delivery below), Earlier checkpoint: boot fix and cleanup, 2026-10-03, Prior telemetry implementation and validation

### Community 179 - "Forge hardware test bridge"
Cohesion: 0.17
Nodes (12): Audio interface wiring (for the automatic checks), Automatic checks, Controls and evidence, Easiest: Forge Bridge.exe, Exact first physical check, Firmware updates and card files without removing the card (firmware 0.7), Forge hardware test bridge, From a source checkout (+4 more)

### Community 180 - "pack.js"
Cohesion: 0.24
Nodes (18): adsr(), chain(), CHROMATIC, env(), finish(), fm(), gain(), hiss() (+10 more)

### Community 181 - "Forge sampling (roadmap item 4): design"
Cohesion: 0.18
Nodes (11): 1. What TAPE does (the behaviour to keep), 2. What Forge changes, and why, 3. Memory, 4. Patch v4 (wire and JSON), 5. Ownership and data flow, 6. Panel, 7. Host, AI and webapp, 8. Verification plan (+3 more)

### Community 182 - "restart.h"
Cohesion: 0.29
Nodes (9): CountCrash(), DescribeEvent(), RanStably(), SafeModeStart(), StartupGuard, crashes, kMagic, magic (+1 more)

### Community 183 - "Restart"
Cohesion: 0.40
Nodes (5): CopyFile(), Restart(), SaveSlotSettings(), TransmitPending(), WritePresetsFile()

### Community 184 - "Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark"
Cohesion: 0.20
Nodes (10): 1. Bootloader acceptance, 2. Memory layout, 3. Audio and controls, 3a. SD card coexistence (device presets, samples), 4. MIDI conventions, 5. Toolchains and build reproducibility, 6. CPU benchmark (`make bench`), 7. Power and system behaviour (firmware 0.8 review, 2026-10-05) (+2 more)

### Community 185 - "Stock TAPE: what every control does (reference for Forge parity)"
Cohesion: 0.20
Nodes (10): Boot and power, CHOMPI, PLAY, LOOP, Effects and looper, Framework, Keys, Knobs, normal play, Menu (shift layer while CHOMPI held, toggle TRUE), MIDI, panic, files (+2 more)

### Community 186 - "InspectorEvent"
Cohesion: 0.20
Nodes (8): ObserveVoiceEdges(), InspectorEvent, id, kind, serial, time_ms, value, SetInspectorEvents()

### Community 187 - "PanelFeedback016"
Cohesion: 0.12
Nodes (9): 0.16.0 Panel feedback and shortcuts (the owner's UX review; tool: Colour Lantern) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS), State, Armed, Empty, FirstTake, Paused, Playing, ComposeKnobLeds() (+1 more)

### Community 189 - "Forge control protocol — firmware 0.15"
Cohesion: 0.10
Nodes (19): Audio path, Boundaries, Device presets (SD card), Execution and ownership, Forge architecture, Transport details, 0.3 integration review and browser testing — 2026-10-03, Done in the integration checkpoint (`15681b8`) (+11 more)

### Community 191 - "Choowi: the first firmware of the forge"
Cohesion: 0.40
Nodes (5): A release, Build and test, Choowi: the first firmware of the forge, Rules that stay, Where the details live

### Community 192 - "Compressor"
Cohesion: 0.33
Nodes (5): Compressor, gain_, peak_, SoftClip(), SoftLimit()

### Community 193 - "forge_main.cpp"
Cohesion: 0.16
Nodes (13): Balance(), BootStep(), CaptureInspector(), ChargerReadings(), CollectInspector(), DrawLeds(), InspectorEvent(), InstallFaultHandler() (+5 more)

### Community 194 - "First time home with Forge 0.19.2"
Cohesion: 0.29
Nodes (7): 0. Power first (before any install), 1. Get the files, 2. Install the newest Choowi, 3. Fix the rig (from the last session's results), 4. Run the bridge, 5. Send the results back, First time home with Forge 0.19.2

### Community 195 - ".run_walk"
Cohesion: 0.17
Nodes (4): Instrumented build (development only, 2026-10-12; software-tested, not run on CHOMPI yet), FastTime, WalkTests, hand()

### Community 197 - "Bloom"
Cohesion: 0.31
Nodes (4): Bend(), Bloom, kQueue, BloomUnit()

### Community 198 - "Forge looping (roadmap item 5): design"
Cohesion: 0.29
Nodes (6): 1. What TAPE does (the behaviour to keep), 2. Forge plan, 3. Decisions (the owner, 2026-10-03), 4. Risks, 5. Build order (each step tested before the next), Forge looping (roadmap item 5): design

### Community 199 - "Forge project brief"
Cohesion: 0.29
Nodes (7): Architecture and acceptance, Current milestone: Choowi 0.19.2 → final release (2026-10-12), First milestone: instrument candidate 0.4, Forge project brief, Goal and user decisions, Instrument candidate 0.15.1 (history), Roadmap (priority order agreed 2026-10-02)

### Community 200 - "SpscQueue"
Cohesion: 0.18
Nodes (6): First optimization decisions to investigate, SpscQueue, head_, storage_, tail_, ObserveVoiceEdges()

### Community 201 - "node"
Cohesion: 0.25
Nodes (3): node(), random_patch(), WireJsTests

### Community 202 - "Outgoing"
Cohesion: 0.50
Nodes (4): Outgoing, bytes, size, source

### Community 204 - "Forge — live OpenAI / Gemini test"
Cohesion: 0.40
Nodes (5): Forge — live OpenAI / Gemini test, If it fails, Rules, Step 1 — command-line preflight (one request per provider), Step 2 — webapp

### Community 205 - "the forge docs"
Cohesion: 0.50
Nodes (4): Documentation map, Maintaining these docs, the forge docs, Where Choowi stands (0.19.2 release candidate)

### Community 206 - "Forge developer guide"
Cohesion: 0.12
Nodes (10): Build firmware, CPU benchmark against stock firmware, Create a test bundle, Forge developer guide, Get the development branch, Requirements, Troubleshooting, Validate software (+2 more)

### Community 207 - "Forge hardware test kit — instructions for an agent on the test PC"
Cohesion: 0.40
Nodes (4): Forge hardware test kit — instructions for an agent on the test PC, Reference, Rules, What you can do here

### Community 211 - "BootCount"
Cohesion: 0.67
Nodes (3): BootCount, boots, magic

### Community 212 - "PanelInput"
Cohesion: 0.29
Nodes (7): PanelInput, frames, jack, keys, toggle_up, tone_down, turns

### Community 213 - ".prepare"
Cohesion: 0.33
Nodes (3): send_panel(), integer(), panel_message()

### Community 217 - "SlotSettingsSchedule"
Cohesion: 0.33
Nodes (6): SlotSettingsSchedule, changed_at, kMaxWait, kSettle, seen, wait

### Community 219 - "ComposeTransportLeds"
Cohesion: 0.50
Nodes (4): Cross-cutting fixes (they lift most rows), Forge UX review (2026-10-11), Ratings, ComposeTransportLeds()

### Community 223 - "MonitorMode"
Cohesion: 0.33
Nodes (4): MonitorMode, Both, Headphones, SendReturn

### Community 224 - "RenderPlayLeds"
Cohesion: 0.40
Nodes (3): Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05, ChordColour(), RenderPlayLeds()

### Community 225 - "State"
Cohesion: 0.33
Nodes (6): State, Detaching, Headers, Idle, Job, Streaming

### Community 226 - "MotifStep"
Cohesion: 0.33
Nodes (6): MotifStep, len, octave, scale, tone, vel

### Community 227 - "Slot"
Cohesion: 0.33
Nodes (6): Slot, note, source, used, velocity, wait

### Community 229 - "LockoutGuard"
Cohesion: 0.40
Nodes (5): 2026-10-11 (night) — shut-off caught by a power log; 0.19.2 installed and retested, LockoutGuard, held_, kCheckMs, kSustainChecks

### Community 230 - "forge_bench.cpp"
Cohesion: 0.60
Nodes (3): bench_block(), bench_init(), Noise()

### Community 231 - "Event"
Cohesion: 0.40
Nodes (4): Event, note, source, velocity

### Community 232 - "Colour"
Cohesion: 0.40
Nodes (5): BatteryColour(), Colour, b, g, r

### Community 234 - "Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10)"
Cohesion: 0.50
Nodes (4): Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10), Patch knobs (patch version 5), Tests, What the player sees

### Community 235 - "Owner"
Cohesion: 0.67
Nodes (3): Owner, Host, Panel

## Knowledge Gaps
- **1450 isolated node(s):** `reverb_`, `del_`, `filter_`, `warble_`, `dcblock_fx_l_` (+1445 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 2327 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **36 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Engine` connect `Engine` to `.Note`, `.Build`, `FORGE_COLD`, `SlotSettings`, `Conversation`, `.ApplyPatch`, `main`, `looper_test.cpp`, `.Init`, `v3_test.cpp`, `.Value`, `synth_test.cpp`, `panel_controller.h`, `PanelSink`, `Rig`, `Player`, `Sequencer`, `Response`, `Rig`, `Rig`, `Chromatic`, `.Block`, `forge_main.cpp`, `Bloom`, `Reverb`, `Rig`, `runtime.h`, `EngineAndPanel`, `Mailbox`, `core_test.cpp`, `Rig`, `SampleHandoff`, `Performance`?**
  _High betweenness centrality (0.135) - this node is a cross-community bridge._
- **Are the 9 inferred relationships involving `Engine` (e.g. with `main()` and `PhysicalVersusInjectedAndVoiceEdges()`) actually correct?**
  _`Engine` has 9 INFERRED edges - model-reasoned connections that need verification._
- **What connects `reverb_`, `del_`, `filter_` to the rest of the system?**
  _1450 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Synth` be split into smaller, more focused modules?**
  _Cohesion score 0.022982635342185902 - nodes in this community are weakly interconnected._
- **Why does `PanelController` connect `PanelController` to `Options`, `PresetMenu`, `main`, `Chime`, `forge_ai.py`, `panel_controller.h`, `PanelSink`, `Rig`, `KnobHold`, `Response`, `Rig`, `.Block`, `InspectorEvent`, `SpscQueue`, `Rig`, `TransferAndInstall`, `Dream`, `EngineAndPanel`, `RecordGesture`, `.TurnLoopSpeed`, `MonitorMode`, `RenderPlayLeds`, `SampleHandoff`, `Kind`?**
  _High betweenness centrality (0.100) - this node is a cross-community bridge._
- **Are the 5 inferred relationships involving `PanelController` (e.g. with `Current checkpoint: firmware 0.15.1 review fixes (2026-10-06)` and `Handoff: cloud session → the owner's local PC (2026-10-09)`) actually correct?**
  _`PanelController` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Should `Walk` be split into smaller, more focused modules?**
  _Cohesion score 0.12462462462462462 - nodes in this community are weakly interconnected._