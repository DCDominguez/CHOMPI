# Graph Report - CHOMPI  (2026-10-13)

## Corpus Check
- 160 files · ~340,355 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 4, .css 2, .ld 1)

## Summary
- 4966 nodes · 9563 edges · 248 communities (205 shown, 43 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 1064 edges (avg confidence: 0.87)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `fcf29412`
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
- time
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
- cstdint
- forge_inspector.py
- Rig
- DjFilter
- Show
- tempo_bench.cpp
- Player
- Sequencer
- BatteryMonitor
- Request
- Response
- SampleFiles
- Action
- Pad
- Warble
- Rig
- LedView
- BrowserTests
- panel_test.cpp
- SampleCard
- parts_test.cpp
- .Block
- Settings
- PresetStore
- Effects
- Dream
- run_bench.py
- InspectorStorage
- power.h
- RequestKind
- wav.h
- TranslateChannel
- file_transfer_test.cpp
- Forge test results
- Choowi: how do I…?
- probe
- inspector.h
- InspectorSystem
- Reverb
- preset_test.cpp
- FakeMidi
- Rig
- MemoryCard
- TransferAndInstall
- Crush
- SampleJob
- Forge · Choowi changelog
- argparse
- SampleHandoff
- decode_response
- SimulatedMido
- FileRequest
- harmony_test.cpp
- EngineAndPanel
- State
- RecordGesture
- Mailbox
- FatFsUploadFiles
- core_test.cpp
- BridgeTests
- FakeMido
- WebTests
- InspectorMailbox
- .Process
- LevelCheck
- ChargerUsb
- FaultRecord
- Bloom
- FatFsSampleFiles
- ResponseKind
- Device
- Forge · Choowi: the user manual
- FakeChompi
- Function
- secret.h
- LossyTransport
- Conversation
- slot_settings_test.cpp
- SlotPolicy
- fx.h
- FileTransfer
- Performance
- CardUploads
- Phaser
- FatFsStorage
- Mmf
- MidiFrame
- MenuAction
- Chorus
- firmware_chompi_forge_core_sequencer
- Forge Resource Ledger
- BridgeBrowserTests
- MemoryStorage
- send.js
- Kind
- Event
- Battery
- .SampleFrame
- Handler
- SlotValues
- power_test.cpp
- CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12)
- Diffuser
- knob_layout.h
- secret_test.cpp
- Options
- Clocked
- Transport
- State
- Forge — developer resume checkpoint
- ProviderTests
- InspectorVoice
- MenuMode
- Forge 0.19.2 — the one consolidated hardware test
- Error
- Sink
- SaveFile
- Kind
- forge_ai.py
- ReadBack
- CardTests
- AGENTS.md
- Bell
- BridgeAutomaticTests
- SampleSlot
- SlotSettingsSchedule
- MenuPage
- panel_controller.h
- FakeCharger
- .test_low_power_refuses_the_install
- InspectorEventKind
- KnobHold
- LoaderRig
- Compressor
- InstrumentTests
- the forge family
- BridgeWebTests
- V4PatchTests
- ForgeServer
- Sink
- AiCheckTests
- Broken
- .ProcessSampler
- Squash
- AnalysisTests
- Forge hardware test bridge
- pack.js
- Forge sampling (roadmap item 4): design
- main
- harmony.h
- Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark
- Stock TAPE: what every control does (reference for Forge parity)
- Mode
- forge_bridge_app.py
- InspectorTests
- Forge control protocol — firmware 0.15
- Sink
- Choowi: the first firmware of the forge
- Sequence
- forge_main.cpp
- First time home with Forge 0.19.2
- fx_test.cpp
- SiteTests
- Slot
- Forge looping (roadmap item 5): design
- Forge project brief
- SpscQueue
- node
- MidiPort
- Sink
- Forge — live OpenAI / Gemini test
- the forge docs
- dream.h
- Forge hardware test kit — instructions for an agent on the test PC
- restart.h
- Rng
- Choowi's sample pack
- Route
- Chord
- Earlier checkpoint: boot fix and cleanup, 2026-10-03
- AudioCallback
- Stock firmware comparison and CPU benchmark (2026-10-03, the owner's request)
- Extension
- Branch and publishing
- Envelope
- BatteryLogTests
- MidiFramer
- PanelSink
- .GetState
- MemoryCard
- RunSampler
- FirmwarePanelSink
- HandleFrame
- The FX section (Choowi 0.20, development)
- InspectorSnapshot
- V3Fields
- EngineAndPanel
- State
- forge_bench.cpp
- FakePorts
- firmware_chompi_forge_core_protocol
- Rate
- .audio
- PresetProtocolTests
- MonitorMode
- Event
- Outgoing
- Forge UX review (2026-10-11)
- Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05
- BootCount

## God Nodes (most connected - your core abstractions)
1. `Engine` - 233 edges
2. `PanelController` - 173 edges
3. `Synth` - 139 edges
4. `Looper` - 108 edges
5. `Parts` - 85 edges
6. `Parameters` - 79 edges
7. `InspectorAudio` - 69 edges
8. `SampleLoader` - 58 edges
9. `Parameter` - 57 edges
10. `main()` - 55 edges

## Surprising Connections (you probably didn't know these)
- `Ratings` --references--> `ComposeTransportLeds()`  [INFERRED]
  docs/forge/UX_REVIEW.md → firmware/chompi-forge/core/panel_controller.h
- `Development probe and Inspector schema 1` --references--> `EncodePatchData()`  [INFERRED]
  docs/forge/PROTOCOL.md → firmware/chompi-forge/core/protocol.h
- `Hardware: 0.15.2 on CHOMPI, unattended run (2026-10-10 evening)` --references--> `detect()`  [INFERRED]
  docs/forge/CONTINUE.md → firmware/chompi-forge/host/forge_audio.py
- `6. How to make an effect` --references--> `upgrade_patch()`  [INFERRED]
  docs/forge/AGENT_GUIDE.md → firmware/chompi-forge/host/forge_host.py
- `0.17.0 Magic: wakes up singing, surprise me, mutate (tool: Wake-up Bell) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS)` --references--> `Magic017()`  [INFERRED]
  docs/forge/CHANGELOG.md → firmware/chompi-forge/tests/panel_test.cpp

## Import Cycles
- None detected.

## Communities (248 total, 43 thin omitted)

### Community 0 - "Synth"
Cohesion: 0.02
Nodes (78): Synth, age_, amp_, bend_slew_, bend_target_, cap_, chord_loop_, coefficient_ (+70 more)

### Community 1 - "Walk"
Cohesion: 0.11
Nodes (7): Previous checkpoint: first hardware session and bridge test tooling, 2026-10-04, panel_gesture(), tap(), show_presets_page(), Stop, Walk, test()

### Community 2 - "PanelController"
Cohesion: 0.02
Nodes (88): PanelController, activity_, audition_, battery_view_, bending_, block_count_, cc_number_, cc_sent_ (+80 more)

### Community 3 - "Bridge"
Cohesion: 0.06
Nodes (18): Checkpoint: firmware 0.11 install safety (2026-10-05), add(), Bridge, send_panel(), fetch(), body_(), say(), Card() (+10 more)

### Community 4 - "InspectorAudio"
Cohesion: 0.03
Nodes (61): InspectorAudio, bend, block, chord_count, chord_degree, chord_kind, chord_notes, chord_quality (+53 more)

### Community 5 - "Engine"
Cohesion: 0.02
Nodes (63): Engine, bell_, bloom_, broken_, capacity_, chorus_, cloud_, crush_ (+55 more)

### Community 6 - "Looper"
Cohesion: 0.03
Nodes (50): Looper, armed_now_, bp_, combo_, combo_used_, fade_, feedback_target_, filter_on_ (+42 more)

### Community 7 - "SlotSettings"
Cohesion: 0.11
Nodes (14): FollowSampleJob(), Entry, valid, value, SlotSettings, entries_, kBanks, kControls (+6 more)

### Community 8 - "Parameters"
Cohesion: 0.03
Nodes (57): Parameters, attack, bypass, cutoff, decay, feedback, filter_amount, filter_attack (+49 more)

### Community 9 - "forge_host.py"
Cohesion: 0.05
Nodes (51): 3. Patches, 0.6 knob pages and patch knobs — 2026-10-04, Apply request layout, PackArp(), power_ok(), load_starter_presets(), check_fx(), check_parts() (+43 more)

### Community 10 - "forge_audio.py"
Cohesion: 0.05
Nodes (29): analyze(), audible_change(), cli(), clicks(), compare(), db(), detect(), dominant_hz() (+21 more)

### Community 11 - "PresetMenu"
Cohesion: 0.07
Nodes (20): KeyToSlot(), PresetMenu, actions_, chompi_, chromatic_slot_, head_, kActions, kLoopSource (+12 more)

### Community 12 - "json"
Cohesion: 0.08
Nodes (6): integer(), midi_ports(), panel_message(), verify(), WindowsRuntimeTests, ManualPageTests

### Community 13 - "Parameter"
Cohesion: 0.04
Nodes (48): IsPerformance(), Parameter, Attack, Bypass, ColourCharacter, Compressor, Count, Cutoff (+40 more)

### Community 14 - "Voice"
Cohesion: 0.04
Nodes (46): Voice, age, amp, band, band_r, complete, end_frac, filter (+38 more)

### Community 15 - "Parts"
Cohesion: 0.06
Nodes (36): Parts, arp_hold_, arp_left_, arp_note_, bass_, bass_change_, bass_count_, bass_hold_ (+28 more)

### Community 16 - "main"
Cohesion: 0.07
Nodes (18): Recorder, dc_coefficient_, dc_l_, dc_r_, fade_, input_gain_, input_gain_target_, memory_ (+10 more)

### Community 17 - "time"
Cohesion: 0.15
Nodes (9): ask(), charge_name(), cli(), decode_registers(), describe(), flatten(), Logger, stamp() (+1 more)

### Community 18 - "TapeFx"
Cohesion: 0.05
Nodes (38): bench_block(), Noise(), TapeFx, cutoff_, cutoff_target_, dcblock_fx_l_, dcblock_fx_r_, del_ (+30 more)

### Community 19 - "inspector.js"
Cohesion: 0.13
Nodes (37): action(), answer(), api(), bind(), bridge(), checks, controls(), disconnected() (+29 more)

### Community 20 - "looper_test.cpp"
Cohesion: 0.18
Nodes (28): CcButton, down, AutoCloseAndShortTakes(), CcButtons(), CombinationsDoNotTriggerSingleKeys(), FirstTakeSeamAndPlayback(), LimiterKeepsOverdubsBounded(), LoopClosesIntoOverdubAndFeedback() (+20 more)

### Community 21 - "sampler_test.cpp"
Cohesion: 0.21
Nodes (20): PackSelection(), ChromaticAndKitLoading(), ChromaticPitch(), CubicTableRead(), Fill(), GateAndTrigger(), KitMapping(), LoadingAndHandoff() (+12 more)

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
Cohesion: 0.16
Nodes (19): FxWordValid(), PartsWordsValid(), DecodeRequest(), Read14(), ExecuteRequest(), PackUsbSysEx(), Framing(), main() (+11 more)

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
Cohesion: 0.10
Nodes (16): Phase 0 decisions (the owner, 2026-10-05), Clock, interval_, last_tap_, pending_, phase_, rate_, since_ (+8 more)

### Community 30 - "synth_test.cpp"
Cohesion: 0.15
Nodes (17): Roadmap item 1: sustain pedal and pitch bend (implemented; software-tested), Sound fixes (implemented 2026-10-02 UTC; software-tested only), AliasDb(), ChannelTranslation(), ClickFreeStealAndRetrigger(), Crossings(), EnvelopeVelocityAndFilter(), Instrument() (+9 more)

### Community 31 - "cstdint"
Cohesion: 0.11
Nodes (11): Find(), Parse(), Clamp(), TapeSpeedKnob(), AudioBlock(), CubicQ14(), CubicTable, w (+3 more)

### Community 32 - "forge_inspector.py"
Cohesion: 0.11
Nodes (20): control_value(), chord_label(), cli(), fetch(), collect(), decode(), byte(), raw() (+12 more)

### Community 33 - "Rig"
Cohesion: 0.11
Nodes (18): PanelInput, frames, jack, keys, toggle_up, tone_down, turns, Rig (+10 more)

### Community 34 - "DjFilter"
Cohesion: 0.12
Nodes (14): DjFilter, control_, hp_, hp_c_, hp_target_, hpl_, hpr_, lp_ (+6 more)

### Community 35 - "Show"
Cohesion: 0.08
Nodes (24): Held, left, Show, chord_, chord_left_, cluster_, count_, degree_ (+16 more)

### Community 37 - "Player"
Cohesion: 0.07
Nodes (22): 2026-10-11 — automatic run on the SD-fix build (the owner away, webcam + UMC), Held, count, key, note, source, used, Player (+14 more)

### Community 38 - "Sequencer"
Cohesion: 0.13
Nodes (9): Sequencer, cursor_, held_, pending_off_, playing_, pos_, sequence_, state_ (+1 more)

### Community 39 - "BatteryMonitor"
Cohesion: 0.26
Nodes (7): BatteryMonitor, d_, kPeriodicMs, periodic_at_, read_33_, SimulatedBattery(), BatteryInstrumentation()

### Community 40 - "Request"
Cohesion: 0.07
Nodes (29): Roadmap item 3: device presets on the SD card (implemented; software-tested), Request, action, bank, command, epoch, kind, mode (+21 more)

### Community 41 - "Response"
Cohesion: 0.07
Nodes (28): Response, action, bank, battery, capacity_ms, charge_state, cpu_average, cpu_max (+20 more)

### Community 42 - "SampleFiles"
Cohesion: 0.17
Nodes (8): 2. What the code does now, ParseSampleName(), SampleEvent, job, ok, SampleFiles, SamplePath(), Names()

### Community 43 - "Action"
Cohesion: 0.24
Nodes (7): Action, a, b, kind, Kind, Note, Param

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
Nodes (34): LedView, blink, count_in, dream, dream_colour, dream_glow, dream_level, flash (+26 more)

### Community 49 - "panel_test.cpp"
Cohesion: 0.16
Nodes (19): PackLooper(), TapeSpeedRatio(), BatteryHold(), DjFilterNeverSilent(), Dream018(), FxPage020(), KeysKnobsAndOverrides(), KnobPages() (+11 more)

### Community 50 - "SampleCard"
Cohesion: 0.08
Nodes (14): main(), PhysicalVersusInjectedAndVoiceEdges(), SampleCard, append_budget, fail_reads, fail_writes, files, kTempName (+6 more)

### Community 51 - "parts_test.cpp"
Cohesion: 0.13
Nodes (18): ValidWords(), Arp(), ArpPatterns(), BassModesAndMidi(), ChordGathering(), GateLatchAndOwnership(), main(), Note (+10 more)

### Community 53 - ".Block"
Cohesion: 0.10
Nodes (3): EditSynth(), MagicCombo(), QuantisedSpeedStep()

### Community 54 - "Settings"
Cohesion: 0.06
Nodes (31): Bass, Alternate, Fifth, Octave, Off, Root, BassRate, Chord (+23 more)

### Community 55 - "PresetStore"
Cohesion: 0.19
Nodes (6): Crc16(), DecodePresetRecord(), EncodePresetRecord(), PresetPath(), PresetStore, Storage

### Community 56 - "Effects"
Cohesion: 0.12
Nodes (16): Effects, compressor_, compressor_target_, dj_, dj_active_, left_, makeup_, pregain_ (+8 more)

### Community 57 - "Dream"
Cohesion: 0.06
Nodes (40): 0.18.0 Magic: chomp to instrument, dream mode, lights with character (tool: Dream Bellows) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS), Chime, kDelay, kLength, kNotes, kStep, kTotal, kVelocity (+32 more)

### Community 58 - "run_bench.py"
Cohesion: 0.11
Nodes (11): Bench, check(), demangle(), forge_runs(), setup(), main(), run(), upstream_runs() (+3 more)

### Community 59 - "InspectorStorage"
Cohesion: 0.12
Nodes (16): InspectorStorage, busy, errors, file_frames, file_loaded_frames, job, last_error, loaded_selection (+8 more)

### Community 60 - "power.h"
Cohesion: 0.14
Nodes (22): 2026-10-11 (night, later): shut-off caught, fix built, BatteryWarning(), InstallPowerOk(), Lockout, None, Unplugged, WeakSupply, LockoutText() (+14 more)

### Community 61 - "RequestKind"
Cohesion: 0.09
Nodes (22): RequestKind, Bend, Clock, Erase, List, Looper, ModWheel, Note (+14 more)

### Community 62 - "wav.h"
Cohesion: 0.13
Nodes (24): ConvertFrames(), ParseWav(), ReadLe16(), ReadLe32(), WavError, HeaderTooLarge, None, NotWav (+16 more)

### Community 63 - "TranslateChannel"
Cohesion: 0.09
Nodes (23): Audio path, Boundaries, Device presets (SD card), Execution and ownership, Forge architecture, Samples (SD card) and the memory handoff, Source map, Transport details (+15 more)

### Community 64 - "file_transfer_test.cpp"
Cohesion: 0.39
Nodes (14): Crc32(), Read35(), Begin(), CodecAndNames(), Data(), Decode(), Firmware(), FirmwareGuards() (+6 more)

### Community 65 - "Forge test results"
Cohesion: 0.09
Nodes (22): 2026-10-04 evening — first 0.7 session (the owner), development firmware (`b36ba99`), 2026-10-04 — first hardware session (the owner), firmware 0.6 development (`131776b`), 2026-10-05 — 0.9 on CHOMPI (the owner, verbal): open issue, random shut-off, 2026-10-05 — firmware 0.8 installed over USB: all automatic checks pass, 2026-10-05 — line in wired, 0.7 on CHOMPI, 0.8 bridge, 2026-10-05 — power on 0.8 (the owner, verbal): TEST_SESSION 8, 2026-10-05 — the owner's panel-map walk-through on 0.9 (verbal), 2026-10-10 (late) — start-up stall found and fixed on CHOMPI (+14 more)

### Community 66 - "Choowi: how do I…?"
Cohesion: 0.09
Nodes (22): before you start: names used here, cheat sheet, check it worked, Choowi: how do I…?, good to know, how do I find my way around the menu?, how do I get surprise sounds and keep one?, how do I install Choowi? (+14 more)

### Community 68 - "inspector.h"
Cohesion: 0.18
Nodes (13): EncodeInspector(), InspectorEvent, id, kind, serial, time_ms, value, InspectorLog (+5 more)

### Community 69 - "InspectorSystem"
Cohesion: 0.11
Nodes (19): InspectorSystem, battery, charge_state, crash_pc, crashed, dropped, emergencies, event_drops (+11 more)

### Community 70 - "Reverb"
Cohesion: 0.13
Nodes (13): bench_init(), Reverb, damp_, gain_, index_, kLines, length_, line_ (+5 more)

### Community 71 - "preset_test.cpp"
Cohesion: 0.21
Nodes (14): KnobOrder(), main(), MenuGestures(), Panel, menu, toggle, Protocol(), RecordsAndPaths() (+6 more)

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

### Community 76 - "Crush"
Cohesion: 0.09
Nodes (17): Crush, count_, held_l_, held_r_, hold_, levels_, mix_, on_ (+9 more)

### Community 77 - "SampleJob"
Cohesion: 0.11
Nodes (17): Kind, Copy, Erase, Save, SampleJob, bank, frames, from_loop (+9 more)

### Community 78 - "Forge · Choowi changelog"
Cohesion: 0.06
Nodes (33): 0.10 TAPE parity — 2026-10-05 (ready to install; per-slot settings moved to 0.12), 0.11 Install safety — 2026-10-05 (software-tested; not installed yet), 0.12 Per-slot sample settings — 2026-10-05 (software-tested; not installed yet), 0.13 Harmony — 2026-10-05 (software-tested; not installed yet), 0.14 Arp, bass and tempo — 2026-10-05 (software-tested; not installed yet), 0.15.1 Review fixes — 2026-10-06 (software-tested; not installed yet), 0.15 Event recorder and projects — 2026-10-05 (software-tested; not installed yet), 0.16.0 Panel feedback and shortcuts (the owner's UX review; tool: Colour Lantern) — 2026-10-11 (on CHOMPI since 0.19.2 build 8b51764; checks in TEST_RESULTS) (+25 more)

### Community 79 - "argparse"
Cohesion: 0.11
Nodes (11): files(), main(), verify(), main(), run(), data_url(), main(), render() (+3 more)

### Community 80 - "SampleHandoff"
Cohesion: 0.14
Nodes (7): SampleHandoff, ack_, published_, request_, SampleTable, slots, FailedLoadIsVisible()

### Community 81 - "decode_response"
Cohesion: 0.13
Nodes (19): 1. What Forge is, 2. How to talk to CHOMPI, 5. Where things live (`firmware/chompi-forge/`), 6. How to make an effect, 7. How to edit the firmware, 8. Read next, Forge for other agents (Muse / Cosmo on the Tab5), Checkpoint: firmware 0.15.2, HANDOFF items 2-4 (2026-10-10, local PC) (+11 more)

### Community 82 - "SimulatedMido"
Cohesion: 0.07
Nodes (10): _Input, Message, _Output, _Port, SimulatedMido, PresetCliAndWebTests, SampleMessageTests, SampleWebAndAiTests (+2 more)

### Community 83 - "FileRequest"
Cohesion: 0.11
Nodes (18): EncodeFileReply(), FileOp, Abort, Begin, Data, End, Install, Status (+10 more)

### Community 84 - "harmony_test.cpp"
Cohesion: 0.24
Nodes (8): StaticFunction(), Degree(), Identity(), InScale(), Layouts(), main(), Notes(), PitchClasses()

### Community 85 - "EngineAndPanel"
Cohesion: 0.09
Nodes (4): Render(), Word(), EngineAndPanel(), EngineAndPanel()

### Community 86 - "State"
Cohesion: 0.29
Nodes (6): State, Armed, Empty, FirstTake, Paused, Playing

### Community 87 - "RecordGesture"
Cohesion: 0.13
Nodes (10): Event, None, Start, Stop, RecordGesture, chompi_, count_, counting_ (+2 more)

### Community 88 - "Mailbox"
Cohesion: 0.14
Nodes (10): Mailbox, bank, owner, sequence, slot, state, Owner, Host (+2 more)

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
Cohesion: 0.13
Nodes (7): Audit of the starting head, Exact next hardware test, Forge Inspector — first development slice, Realtime ownership, Run the Inspector, Validation at this checkpoint, WebTests

### Community 94 - "InspectorMailbox"
Cohesion: 0.31
Nodes (4): InspectorMailbox, owner_, state_, MailboxOwnership()

### Community 96 - "LevelCheck"
Cohesion: 0.07
Nodes (24): Abort(), LevelCheck, at_, began_, given_up_, k30, k31, k33 (+16 more)

### Community 97 - "ChargerUsb"
Cohesion: 0.12
Nodes (17): 2026-10-10 — 0.15.2 on CHOMPI: first install since 0.9, unattended automatic run, 2026-10-11 (night) — power bank: no charging after a re-plug while on, Actions, force_detection, read, usb_to_daisy, ChargerUsb, asked_ (+9 more)

### Community 98 - "FaultRecord"
Cohesion: 0.20
Nodes (10): Describe(), FaultRecord, cfsr, count, hfsr, kMagic, lr, magic (+2 more)

### Community 99 - "Bloom"
Cohesion: 0.13
Nodes (11): Bend(), Bloom, kQueue, Family, Call, Echo, Groove, Sparkle (+3 more)

### Community 100 - "FatFsSampleFiles"
Cohesion: 0.17
Nodes (5): FatFsSampleFiles, read_, reading_, write_, writing_

### Community 101 - "ResponseKind"
Cohesion: 0.22
Nodes (9): ResponseKind, Erased, Occupancy, SampleDone, SampleOccupancy, SampleSnapshot, Snapshot, Status (+1 more)

### Community 102 - "Device"
Cohesion: 0.16
Nodes (3): BridgeWalkTests, SetupCheckTests, Device

### Community 103 - "Forge · Choowi: the user manual"
Cohesion: 0.06
Nodes (31): 10. On the computer, 11. What Forge keeps on the SD card, 12. Troubleshooting, 1. Quick start, 2. Installing and updating, 3. Power and battery, 4. The panel, 5. Knobs and knob pages (+23 more)

### Community 105 - "Function"
Cohesion: 0.15
Nodes (13): Function, degree, kind, octave, quality, root, Kind, Borrowed (+5 more)

### Community 106 - "secret.h"
Cohesion: 0.10
Nodes (23): Advance(), Blooms(), FamilyOf(), Interrupt(), Mode, BloomDown, BloomRandom, BloomUp (+15 more)

### Community 108 - "Conversation"
Cohesion: 0.14
Nodes (13): InKey(), Conversation, armed_, at_, count_, kMaxPhrase, last_family_, last_motif_ (+5 more)

### Community 109 - "slot_settings_test.cpp"
Cohesion: 0.12
Nodes (15): SelectSample(), Chromatic(), Kit(), main(), Near(), Rig, engine, l (+7 more)

### Community 110 - "SlotPolicy"
Cohesion: 0.40
Nodes (5): Checkpoint: firmware 0.12 per-slot sample settings (2026-10-05), SlotPolicy, Patch, Recall, Select

### Community 111 - "fx.h"
Cohesion: 0.09
Nodes (26): Built(), ClassicRank(), ColourChoice, Crush, Fold, Saturation, Module, Colour (+18 more)

### Community 112 - "FileTransfer"
Cohesion: 0.11
Nodes (16): AllowedUploadName(), DecodeFileRequest(), FileTransfer, crc_, done_, head_, kTemp, name_ (+8 more)

### Community 113 - "Performance"
Cohesion: 0.12
Nodes (14): Performance, colour_character, compressor, dj_filter, dj_resonance, input_gain, motion, motion_rate (+6 more)

### Community 114 - "CardUploads"
Cohesion: 0.10
Nodes (6): CardUploads, open, temp, MemoryCard, files, SineWav()

### Community 115 - "Phaser"
Cohesion: 0.11
Nodes (14): Phaser, a_, depth_, feedback_, increment_, last_, mix_, on_ (+6 more)

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
Cohesion: 0.13
Nodes (11): MenuAction, bank, kind, mode, slot, to_bank, to_mode, to_slot (+3 more)

### Community 121 - "Chorus"
Cohesion: 0.13
Nodes (13): Chorus, depth_, dirty_, increment_, kLength, left_, mix_, mix_target_ (+5 more)

### Community 122 - "firmware_chompi_forge_core_sequencer"
Cohesion: 0.16
Nodes (15): , DecodeFile(), EncodeFile(), Exported, Exporting, FileCrc(), Free, Imported (+7 more)

### Community 123 - "Forge Resource Ledger"
Cohesion: 0.18
Nodes (11): Forge Resource Ledger, Memory map and working set, QA and CPU baseline, Reproduced baseline, Snapshot 2026-10-05: CPU fix A (0.13, not installed), Snapshot 2026-10-05: firmware 0.13 harmony (Phase 1 complete), Snapshot 2026-10-05: firmware 0.14 clock, arp and bass (not installed), Snapshot 2026-10-05: firmware 0.15 event recorder and projects (not installed) (+3 more)

### Community 125 - "MemoryStorage"
Cohesion: 0.18
Nodes (5): MemoryStorage, drop_writes, fail_writes, files, ready

### Community 126 - "send.js"
Cohesion: 0.27
Nodes (8): api(), connected(), exchange(), notice(), pick(), PREFIX, refresh(), run()

### Community 127 - "Kind"
Cohesion: 0.17
Nodes (11): Kind, Jack, Key, Press, Release, Toggle, Turn, PanelEvent (+3 more)

### Community 128 - "Event"
Cohesion: 0.22
Nodes (6): Event, a, b, kind, tick, Recordable()

### Community 129 - "Battery"
Cohesion: 0.12
Nodes (18): Battery, Full, High, Low, Medium, Unknown, BatteryColour(), Colour (+10 more)

### Community 130 - ".SampleFrame"
Cohesion: 0.24
Nodes (5): 1. Baseline (verified from source and the ELF), 4. Architectural blockers and couplings, Dual16(), PackHigh(), PackLow()

### Community 132 - "SlotValues"
Cohesion: 0.20
Nodes (10): SlotValues, attack, end, gain, loop, pan, pitch, release (+2 more)

### Community 133 - "power_test.cpp"
Cohesion: 0.27
Nodes (10): BootGesture, held_, BootGestureOff(), Colours(), LevelCheckBands(), LevelCheckFailures(), main(), OptionsFile() (+2 more)

### Community 134 - "CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12)"
Cohesion: 0.06
Nodes (15): CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12), Forge 0.19.2 level check (after release candidate 2), How the readings are made (stock code: `chompi-wave/code/src/hardware.h`), Hypotheses and how to tell them apart, Instrumented build (development only, 2026-10-12; software-tested, not run on CHOMPI yet), Problems in the reading itself (code), Proposed next steps (not done), The hardware (+7 more)

### Community 135 - "Diffuser"
Cohesion: 0.13
Nodes (14): Diffuser, dirty_, increment_, kGain, kPad, kStages, kSwing, line_ (+6 more)

### Community 136 - "knob_layout.h"
Cohesion: 0.14
Nodes (25): Code, Forge knobs: TAPE's layout plus Forge's pages (firmware 0.10), Patch knobs (patch version 5), Tests, What the player sees, Fade(), Fade3(), Fade4() (+17 more)

### Community 137 - "secret_test.cpp"
Cohesion: 0.25
Nodes (9): 0.19.2, 0.19.2 Secret modes, the forge site, bank purple (small tool: Whatzit) — 2026-10-12 (release candidate; on CHOMPI as build b462bea; checks in TEST_RESULTS), Development line (branch forge/fx, not released; the version number for it is still open), BloomInEngine(), ChimeAlwaysSounds(), DreamShow(), main(), Mischief() (+1 more)

### Community 138 - "Options"
Cohesion: 0.22
Nodes (8): Options, midi_in, midi_out, monitor, quantise_menu, record_latch, split_delay, tape_slew

### Community 139 - "Clocked"
Cohesion: 0.24
Nodes (6): Clocked, clock, out, s, main(), RecordAndLoop()

### Community 141 - "State"
Cohesion: 0.14
Nodes (14): Layout, Real, Static, State, block, enabled, extension, inversion (+6 more)

### Community 143 - "Forge — developer resume checkpoint"
Cohesion: 0.05
Nodes (40): 2026-10-11: agent hardware checks, soak, UX review, 2026-10-11 (end of day): 0.19 staged on the card, stress test done, 2026-10-11 (late night): 0.19.0 synth page built, 2026-10-11 (late night): guided test nearly done, repo refreshed, 2026-10-11 (later): 0.16.0 Batch 1 + 2 built, 2026-10-11 (latest): 0.17.0 magic items 1-3 built, 2026-10-11 (night): 0.17.1 + 0.18.0 built, Spark chosen, 2026-10-12: battery readings under investigation (+32 more)

### Community 145 - "InspectorVoice"
Cohesion: 0.25
Nodes (8): InspectorVoice, age, envelope, flags, note, slot, source, stage

### Community 146 - "MenuMode"
Cohesion: 0.29
Nodes (6): MenuMode, CopyDest, CopySource, Erase, None, Save

### Community 147 - "Forge 0.19.2 — the one consolidated hardware test"
Cohesion: 0.08
Nodes (24): 0. Before you start (no CHOMPI needed), 1. Flash and identity, 1A. Inspector development candidate (before the normal audio checks), 2. External audio path (v1 compatibility), 3. Instrument, 3B. v3 instrument modules (firmware 0.4), 3C. Device presets on the SD card (TAPE-style keys + encoder), 3D. Sampler (TAPE-style, firmware 0.5) (+16 more)

### Community 148 - "Error"
Cohesion: 0.13
Nodes (14): Error, Busy, Checksum, Empty, Length, None, Opcode, Patch (+6 more)

### Community 149 - "Sink"
Cohesion: 0.18
Nodes (6): Sink, accept, failures, flashes, jobs, presets

### Community 150 - "SaveFile"
Cohesion: 0.57
Nodes (5): CopyFile(), EraseFile(), LoadFile(), SaveFile(), FilePath()

### Community 151 - "Kind"
Cohesion: 0.20
Nodes (10): Kind, Copy, Erase, Recall, RecordSource, SampleCopy, SampleErase, SampleSave (+2 more)

### Community 152 - "forge_ai.py"
Cohesion: 0.08
Nodes (19): Current checkpoint: firmware 0.15.1 review fixes (2026-10-06), data_js(), editor_js(), main(), outputs(), chat_prompt(), check_prompt(), check_samples() (+11 more)

### Community 153 - "ReadBack"
Cohesion: 0.33
Nodes (5): ReadBack, After30, After33, None, Periodic

### Community 154 - "CardTests"
Cohesion: 0.24
Nodes (3): CardTests, DeadLink, wav()

### Community 155 - "AGENTS.md"
Cohesion: 0.33
Nodes (4): Forge development guidance, Before you write, Ponytail, lazy senior dev mode, The smallest complete change

### Community 156 - "Bell"
Cohesion: 0.12
Nodes (11): Bell, decay_, kVoices, rate_, voices_, Voice, level, phase (+3 more)

### Community 158 - "SampleSlot"
Cohesion: 0.09
Nodes (21): 10. Answers to the brief's questions, 3. Precedent: stock TAPE already streams from SD on this hardware, 5.1 Sample streaming: shared page cache + pinned heads (one pool), 5.2 Underrun and failure semantics, 5.3 Streaming recorder, 5.4 Looper, 5.5 QSPI (Tier 2), 5. Proposed design (+13 more)

### Community 159 - "SlotSettingsSchedule"
Cohesion: 0.33
Nodes (6): SlotSettingsSchedule, changed_at, kMaxWait, kSettle, seen, wait

### Community 160 - "MenuPage"
Cohesion: 0.25
Nodes (7): MenuPage, Fx, Harmony, Parts, Presets, Samples, Synth

### Community 161 - "panel_controller.h"
Cohesion: 0.15
Nodes (28): 4. The panel, for visualizations, ChordColour(), ComposeFxKnobLeds(), ComposeHarmonyKnobLeds(), ComposeKnobLeds(), ComposeLeds(), ComposeLooperLeds(), ComposeMenuKnobLeds() (+20 more)

### Community 162 - "FakeCharger"
Cohesion: 0.25
Nodes (6): FakeCharger, fail_reads, fail_writes, reg0c, volts, writes

### Community 164 - "InspectorEventKind"
Cohesion: 0.12
Nodes (16): InspectorEventKind, Card, KeyDown, KeyUp, Knob, PatchApply, QueueError, RecordingStart (+8 more)

### Community 165 - "KnobHold"
Cohesion: 0.40
Nodes (5): KnobHold, down, frames, turned, used

### Community 166 - "LoaderRig"
Cohesion: 0.10
Nodes (17): LoaderRig, card, events, handoff, loader, loop, pool, recorder (+9 more)

### Community 167 - "Compressor"
Cohesion: 0.50
Nodes (3): Compressor, gain_, peak_

### Community 168 - "InstrumentTests"
Cohesion: 0.13
Nodes (3): envelope(), InstrumentTests, opener()

### Community 169 - "the forge family"
Cohesion: 0.29
Nodes (7): Choowi, the first of the family, how it talks, the forge family, the look, the rules, the tree, whose it is

### Community 175 - "Broken"
Cohesion: 0.07
Nodes (19): Broken, amount_, character_, floor_, gain_, left_, rand_, rate_ (+11 more)

### Community 177 - "Squash"
Cohesion: 0.15
Nodes (10): Clip(), Squash, attack_, env_, makeup_, mix_, on_, rate_ (+2 more)

### Community 178 - "AnalysisTests"
Cohesion: 0.19
Nodes (4): AnalysisTests, walk(), louder_single(), sine()

### Community 179 - "Forge hardware test bridge"
Cohesion: 0.17
Nodes (12): Audio interface wiring (for the automatic checks), Automatic checks, Controls and evidence, Easiest: Forge Bridge.exe, Exact first physical check, Firmware updates and card files without removing the card (firmware 0.7), Forge hardware test bridge, From a source checkout (+4 more)

### Community 180 - "pack.js"
Cohesion: 0.08
Nodes (39): Build firmware, CPU benchmark against stock firmware, Create a test bundle, Forge developer guide, Get the development branch, Requirements, Troubleshooting, Validate software (+31 more)

### Community 181 - "Forge sampling (roadmap item 4): design"
Cohesion: 0.18
Nodes (11): 1. What TAPE does (the behaviour to keep), 2. What Forge changes, and why, 3. Memory, 4. Patch v4 (wire and JSON), 5. Ownership and data flow, 6. Panel, 7. Host, AI and webapp, 8. Verification plan (+3 more)

### Community 182 - "main"
Cohesion: 0.32
Nodes (12): FlashSafeMode(), LoadOptions(), LoadSlotSettings(), LogLockout(), LogRestart(), main(), MountCard(), ReadBackThreshold() (+4 more)

### Community 183 - "harmony.h"
Cohesion: 0.31
Nodes (12): Lead(), Movement(), Parallel(), Place(), QualityStack(), RealFunction(), Resolve(), Tones() (+4 more)

### Community 184 - "Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark"
Cohesion: 0.20
Nodes (10): 1. Bootloader acceptance, 2. Memory layout, 3. Audio and controls, 3a. SD card coexistence (device presets, samples), 4. MIDI conventions, 5. Toolchains and build reproducibility, 6. CPU benchmark (`make bench`), 7. Power and system behaviour (firmware 0.8 review, 2026-10-05) (+2 more)

### Community 185 - "Stock TAPE: what every control does (reference for Forge parity)"
Cohesion: 0.20
Nodes (10): Boot and power, CHOMPI, PLAY, LOOP, Effects and looper, Framework, Keys, Knobs, normal play, Menu (shift layer while CHOMPI held, toggle TRUE), MIDI, panic, files (+2 more)

### Community 186 - "Mode"
Cohesion: 0.20
Nodes (10): Mode, Dorian, HarmonicMinor, Locrian, Lydian, Major, MelodicMinor, Mixolydian (+2 more)

### Community 189 - "Forge control protocol — firmware 0.15"
Cohesion: 0.22
Nodes (9): Development probe and Inspector schema 1, Device presets (SD card), firmware 0.4, Envelope and operations, Event recorder loops on the SD card (firmware 0.15), Forge control protocol — firmware 0.15, MIDI clock (firmware 0.14), Notes, controls and recovery, Sampler, firmware 0.5 (+1 more)

### Community 191 - "Choowi: the first firmware of the forge"
Cohesion: 0.40
Nodes (5): A release, Build and test, Choowi: the first firmware of the forge, Rules that stay, Where the details live

### Community 193 - "forge_main.cpp"
Cohesion: 0.19
Nodes (10): Balance(), CaptureInspector(), ChargerReadings(), CollectInspector(), DrawLeds(), InstallFaultHandler(), InstallPowerOk(), Pth() (+2 more)

### Community 194 - "First time home with Forge 0.19.2"
Cohesion: 0.29
Nodes (7): 0. Power first (before any install), 1. Get the files, 2. Install the newest Choowi, 3. Fix the rig (from the last session's results), 4. Run the bridge, 5. Send the results back, First time home with Forge 0.19.2

### Community 195 - "fx_test.cpp"
Cohesion: 0.18
Nodes (13): DiffuserUnit(), EngineAllTwelve(), EngineFx(), Fingerprint(), main(), Rig, engine, fxm (+5 more)

### Community 197 - "Slot"
Cohesion: 0.33
Nodes (6): Slot, note, source, used, velocity, wait

### Community 198 - "Forge looping (roadmap item 5): design"
Cohesion: 0.33
Nodes (6): 1. What TAPE does (the behaviour to keep), 2. Forge plan, 3. Decisions (the owner, 2026-10-03), 4. Risks, 5. Build order (each step tested before the next), Forge looping (roadmap item 5): design

### Community 199 - "Forge project brief"
Cohesion: 0.29
Nodes (7): Architecture and acceptance, Current milestone: Choowi 0.19.2 → final release (2026-10-12), First milestone: instrument candidate 0.4, Forge project brief, Goal and user decisions, Instrument candidate 0.15.1 (history), Roadmap (priority order agreed 2026-10-02)

### Community 200 - "SpscQueue"
Cohesion: 0.13
Nodes (8): First optimization decisions to investigate, SpscQueue, head_, storage_, tail_, ObserveVoiceEdges(), SetInspectorEvents(), ObserveVoiceEdges()

### Community 201 - "node"
Cohesion: 0.25
Nodes (3): node(), random_patch(), WireJsTests

### Community 202 - "MidiPort"
Cohesion: 0.17
Nodes (7): MidiPort, clock_dropped, dropped, framer, frames, transport, PollMidi()

### Community 204 - "Forge — live OpenAI / Gemini test"
Cohesion: 0.40
Nodes (5): Forge — live OpenAI / Gemini test, If it fails, Rules, Step 1 — command-line preflight (one request per provider), Step 2 — webapp

### Community 205 - "the forge docs"
Cohesion: 0.50
Nodes (4): Documentation map, Maintaining these docs, the forge docs, Where Choowi stands (0.19.2 release candidate)

### Community 206 - "dream.h"
Cohesion: 0.36
Nodes (6): note, NextChord(), Play(), Stop(), StyleFor(), Tick()

### Community 207 - "Forge hardware test kit — instructions for an agent on the test PC"
Cohesion: 0.40
Nodes (4): Forge hardware test kit — instructions for an agent on the test PC, Reference, Rules, What you can do here

### Community 208 - "restart.h"
Cohesion: 0.29
Nodes (9): CountCrash(), DescribeEvent(), RanStably(), SafeModeStart(), StartupGuard, crashes, kMagic, magic (+1 more)

### Community 211 - "Route"
Cohesion: 0.25
Nodes (5): Route, Classic, SideBySide, WashFirst, WearLast

### Community 212 - "Chord"
Cohesion: 0.12
Nodes (16): Inspect(), Chord, degree, kind, quality, root, shifted, tone (+8 more)

### Community 213 - "Earlier checkpoint: boot fix and cleanup, 2026-10-03"
Cohesion: 0.67
Nodes (3): Browser hardware test bridge (supersedes terminal-only delivery below), Earlier checkpoint: boot fix and cleanup, 2026-10-03, Prior telemetry implementation and validation

### Community 214 - "AudioCallback"
Cohesion: 0.20
Nodes (6): AudioCallback(), ChargerIo, CopyFile(), SaveSlotSettings(), ServiceChargerUsb(), WritePresetsFile()

### Community 216 - "Extension"
Cohesion: 0.29
Nodes (7): Extension, Eleventh, Fifth, Ninth, Seventh, Thirteenth, Triad

### Community 218 - "Envelope"
Cohesion: 0.18
Nodes (10): Envelope, release_step, stage, value, Stage, Attack, Decay, Off (+2 more)

### Community 223 - "MidiFramer"
Cohesion: 0.22
Nodes (8): MidiFramer, bytes_, cc_, cc_used_, overflow_, status_, sysex_, used_

### Community 226 - ".GetState"
Cohesion: 0.29
Nodes (6): State, Armed, Empty, Playing, Recording, Stopped

### Community 228 - "RunSampler"
Cohesion: 0.28
Nodes (9): BootStep(), Flash(), InspectorEvent(), InspectorStorageError(), Restart(), RunPanelActions(), RunSampler(), SendResponses() (+1 more)

### Community 229 - "FirmwarePanelSink"
Cohesion: 0.22
Nodes (4): FirmwarePanelSink, PanelAction, action, patch

### Community 230 - "HandleFrame"
Cohesion: 0.33
Nodes (9): FillBattery(), forge::Error RecallPreset(), HandleFrame(), HandleStorage(), LoadSequence(), Queue(), RaiseEmergency(), Send() (+1 more)

### Community 232 - "The FX section (Choowi 0.20, development)"
Cohesion: 0.33
Nodes (5): Costs, For the listening session, Getting there, Saved with the sound, The FX section (Choowi 0.20, development)

### Community 233 - "InspectorSnapshot"
Cohesion: 0.33
Nodes (6): InspectorSnapshot, audio, battery, generation, storage, system

### Community 234 - "V3Fields"
Cohesion: 0.20
Nodes (10): Patch lifecycle, Development plan (the owner, 2026-10-02): features first, QA per feature later, Roadmap item 2: v3 instrument palette (implemented; software-tested), Change discipline, V3Field, index, kind, max (+2 more)

### Community 236 - "EngineAndPanel"
Cohesion: 0.11
Nodes (10): 2026-10-12: RC tutorial and manual refresh, Forge handoff, Handoff: cloud session → the owner's local PC (2026-10-09), Inspect(), MidiOut, data1, data2, status (+2 more)

### Community 238 - "State"
Cohesion: 0.33
Nodes (6): State, Detaching, Headers, Idle, Job, Streaming

### Community 239 - "forge_bench.cpp"
Cohesion: 0.60
Nodes (3): bench_block(), bench_init(), Noise()

### Community 241 - "firmware_chompi_forge_core_protocol"
Cohesion: 0.20
Nodes (20): 0.4 device presets on the SD card — 2026-10-03, , Checksum(), DecodePatchData(), EncodeError(), EncodeIdentity(), EncodePanelAck(), EncodePatchData() (+12 more)

### Community 242 - "Rate"
Cohesion: 0.29
Nodes (7): Rate, Eighth, EighthTriplet, Quarter, Sixteenth, SixteenthTriplet, ThirtySecond

### Community 245 - "MonitorMode"
Cohesion: 0.33
Nodes (4): MonitorMode, Both, Headphones, SendReturn

### Community 248 - "Event"
Cohesion: 0.50
Nodes (4): Event, note, source, velocity

### Community 249 - "Outgoing"
Cohesion: 0.50
Nodes (4): Outgoing, bytes, size, source

### Community 250 - "Forge UX review (2026-10-11)"
Cohesion: 0.67
Nodes (3): Cross-cutting fixes (they lift most rows), Forge UX review (2026-10-11), Ratings

### Community 252 - "BootCount"
Cohesion: 0.67
Nodes (3): BootCount, boots, magic

## Knowledge Gaps
- **1680 isolated node(s):** `reverb_`, `del_`, `filter_`, `warble_`, `dcblock_fx_l_` (+1675 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 2625 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **43 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Engine` connect `Engine` to `SlotSettings`, `Diffuser`, `.ApplyPatch`, `main`, `looper_test.cpp`, `.Init`, `v3_test.cpp`, `.Value`, `Bell`, `synth_test.cpp`, `cstdint`, `Rig`, `Player`, `Sequencer`, `LoaderRig`, `Rig`, `SampleCard`, `FORGE_COLD`, `.Block`, `TranslateChannel`, `forge_main.cpp`, `fx_test.cpp`, `Reverb`, `preset_test.cpp`, `Rig`, `SampleHandoff`, `Route`, `EngineAndPanel`, `Mailbox`, `core_test.cpp`, `PanelSink`, `Bloom`, `Conversation`, `slot_settings_test.cpp`, `EngineAndPanel`, `fx.h`, `Performance`?**
  _High betweenness centrality (0.157) - this node is a cross-community bridge._
- **Are the 9 inferred relationships involving `Engine` (e.g. with `main()` and `PhysicalVersusInjectedAndVoiceEdges()`) actually correct?**
  _`Engine` has 9 INFERRED edges - model-reasoned connections that need verification._
- **What connects `reverb_`, `del_`, `filter_` to the rest of the system?**
  _1680 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Synth` be split into smaller, more focused modules?**
  _Cohesion score 0.021972884525479196 - nodes in this community are weakly interconnected._
- **Why does `PanelController` connect `PanelController` to `Options`, `PresetMenu`, `main`, `forge_ai.py`, `panel_controller.h`, `Rig`, `Show`, `KnobHold`, `Rig`, `SampleCard`, `.Block`, `Dream`, `SpscQueue`, `Rig`, `TransferAndInstall`, `EngineAndPanel`, `RecordGesture`, `PanelSink`, `EngineAndPanel`, `fx.h`, `MonitorMode`, `Previous checkpoint: firmware 0.9, key lights while playing, 2026-10-05`, `Kind`?**
  _High betweenness centrality (0.101) - this node is a cross-community bridge._
- **Are the 5 inferred relationships involving `PanelController` (e.g. with `Current checkpoint: firmware 0.15.1 review fixes (2026-10-06)` and `Handoff: cloud session → the owner's local PC (2026-10-09)`) actually correct?**
  _`PanelController` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Should `Walk` be split into smaller, more focused modules?**
  _Cohesion score 0.11219512195121951 - nodes in this community are weakly interconnected._