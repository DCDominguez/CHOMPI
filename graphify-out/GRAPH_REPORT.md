# Graph Report - CHOMPI  (2026-10-10)

## Corpus Check
- cluster-only mode — file stats not available

## Summary
- 3575 nodes · 7155 edges · 180 communities (149 shown, 31 thin omitted)
- Extraction: 90% EXTRACTED · 10% INFERRED · 0% AMBIGUOUS · INFERRED: 723 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `d617b2d4`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Community 0
- Community 1
- Community 2
- Community 3
- Community 4
- Community 5
- Community 6
- Community 7
- Community 8
- Community 9
- Community 10
- Community 11
- Community 12
- Community 13
- Community 14
- Community 15
- Community 16
- Community 17
- Community 18
- Community 19
- Community 20
- Community 21
- Community 22
- Community 23
- Community 24
- Community 25
- Community 26
- Community 27
- Community 28
- Community 29
- Community 30
- Community 31
- Community 32
- Community 33
- Community 34
- Community 35
- Community 36
- Community 37
- Community 38
- Community 39
- Community 40
- Community 41
- Community 42
- Community 43
- Community 44
- Community 45
- Community 46
- Community 47
- Community 48
- Community 49
- Community 50
- Community 51
- Community 52
- Community 53
- Community 54
- Community 55
- Community 56
- Community 57
- Community 58
- Community 59
- Community 60
- Community 61
- Community 62
- Community 63
- Community 64
- Community 66
- Community 67
- Community 68
- Community 69
- Community 70
- Community 71
- Community 72
- Community 73
- Community 74
- Community 75
- Community 76
- Community 77
- Community 78
- Community 79
- Community 80
- Community 81
- Community 82
- Community 83
- Community 84
- Community 85
- Community 86
- Community 87
- Community 88
- Community 89
- Community 90
- Community 91
- Community 92
- Community 93
- Community 94
- Community 95
- Community 96
- Community 97
- Community 98
- Community 99
- Community 100
- Community 101
- Community 102
- Community 103
- Community 104
- Community 105
- Community 106
- Community 107
- Community 108
- Community 109
- Community 110
- Community 111
- Community 112
- Community 113
- Community 114
- Community 115
- Community 116
- Community 117
- Community 118
- Community 119
- Community 120
- Community 121
- Community 122
- Community 123
- Community 124
- Community 125
- Community 126
- Community 127
- Community 128
- Community 129
- Community 130
- Community 131
- Community 132
- Community 133
- Community 134
- Community 135
- Community 137
- Community 138
- Community 139
- Community 140
- Community 141
- Community 142
- Community 143
- Community 144
- Community 145
- Community 146
- Community 147
- Community 148
- Community 149
- Community 150
- Community 151
- Community 152
- Community 153
- Community 154
- Community 155
- Community 156
- Community 158
- Community 159
- Community 160
- Community 161
- Community 162
- Community 163
- Community 164
- Community 165
- Community 166
- Community 167
- Community 168
- Community 169
- Community 170
- Community 171
- Community 172
- Community 173
- Community 174
- Community 175
- Community 176

## God Nodes (most connected - your core abstractions)
1. `Engine` - 147 edges
2. `Synth` - 129 edges
3. `PanelController` - 101 edges
4. `Parts` - 78 edges
5. `Parameters` - 71 edges
6. `InspectorAudio` - 69 edges
7. `Looper` - 69 edges
8. `SampleLoader` - 55 edges
9. `main()` - 54 edges
10. `Parameter` - 53 edges

## Surprising Connections (you probably didn't know these)
- `Layouts()` --calls--> `shifted`  [INFERRED]
  firmware/chompi-forge/tests/harmony_test.cpp → firmware/chompi-forge/core/harmony.h
- `CubicTableRead()` --calls--> `w`  [INFERRED]
  firmware/chompi-forge/tests/sampler_test.cpp → firmware/chompi-forge/core/synth.h
- `KnobPages()` --calls--> `page`  [INFERRED]
  firmware/chompi-forge/tests/panel_test.cpp → firmware/chompi-forge/core/protocol.h
- `RecordAndLoop()` --calls--> `balance`  [INFERRED]
  firmware/chompi-forge/tests/sequencer_test.cpp → firmware/chompi-forge/tests/knob_audio_test.cpp
- `SecondOscillatorAndNoise()` --references--> `Synth`  [INFERRED]
  firmware/chompi-forge/tests/v3_test.cpp → firmware/chompi-forge/core/synth.h

## Import Cycles
- None detected.

## Communities (180 total, 31 thin omitted)

### Community 0 - "Community 0"
Cohesion: 0.02
Nodes (75): Synth, age_, amp_, bend_slew_, bend_target_, cap_, coefficient_, cutoff_ (+67 more)

### Community 1 - "Community 1"
Cohesion: 0.05
Nodes (13): compare(), FakeAudio, key_led(), lookup(), panel_gesture(), Runner, tap(), Skipped (+5 more)

### Community 2 - "Community 2"
Cohesion: 0.03
Nodes (54): Inspect(), MonitorMode, Both, Headphones, SendReturn, PanelController, battery_view_, cc_number_ (+46 more)

### Community 3 - "Community 3"
Cohesion: 0.06
Nodes (18): add(), Bridge, send_panel(), fetch(), body_(), say(), Card(), integer() (+10 more)

### Community 4 - "Community 4"
Cohesion: 0.03
Nodes (60): InspectorAudio, bend, block, chord_count, chord_degree, chord_kind, chord_notes, chord_quality (+52 more)

### Community 5 - "Community 5"
Cohesion: 0.04
Nodes (36): Engine, capacity_, effects_, feedback_, flushed_, focus_, fx_before_loop_, has_reverb_ (+28 more)

### Community 6 - "Community 6"
Cohesion: 0.06
Nodes (27): Looper, combo_, combo_used_, fade_, feedback_target_, frac_, gain_, gain_step_ (+19 more)

### Community 7 - "Community 7"
Cohesion: 0.07
Nodes (28): Entry, valid, value, SlotSettings, entries_, kBanks, kControls, kFileMax (+20 more)

### Community 8 - "Community 8"
Cohesion: 0.04
Nodes (51): Parameters, attack, bypass, cutoff, decay, feedback, filter_amount, filter_attack (+43 more)

### Community 9 - "Community 9"
Cohesion: 0.08
Nodes (31): load_starter_presets(), check_parts(), check_value(), cli(), effect_patch(), encode_patch(), encode_value(), encode_word() (+23 more)

### Community 10 - "Community 10"
Cohesion: 0.08
Nodes (21): analyze(), audible_change(), cli(), clicks(), data_url(), db(), detect(), dominant_hz() (+13 more)

### Community 11 - "Community 11"
Cohesion: 0.06
Nodes (25): KeyToSlot(), MenuPage, Harmony, Parts, Presets, Samples, PresetMenu, actions_ (+17 more)

### Community 12 - "Community 12"
Cohesion: 0.13
Nodes (3): midi_ports(), WindowsRuntimeTests, wav()

### Community 13 - "Community 13"
Cohesion: 0.05
Nodes (44): IsPerformance(), Parameter, Attack, Bypass, Compressor, Count, Cutoff, Decay (+36 more)

### Community 14 - "Community 14"
Cohesion: 0.05
Nodes (44): Voice, age, amp, band, band_r, complete, end_frac, filter (+36 more)

### Community 15 - "Community 15"
Cohesion: 0.05
Nodes (32): Parts, arp_left_, arp_note_, bass_, bass_change_, bass_count_, bass_left_, bass_step_ (+24 more)

### Community 16 - "Community 16"
Cohesion: 0.07
Nodes (18): Recorder, dc_coefficient_, dc_l_, dc_r_, fade_, input_gain_, input_gain_target_, memory_ (+10 more)

### Community 17 - "Community 17"
Cohesion: 0.08
Nodes (28): panel_ack(), decode_reply(), checksum(), control_value(), decode_response(), decode_v3(), from_unit(), harmony_from_word() (+20 more)

### Community 18 - "Community 18"
Cohesion: 0.05
Nodes (36): TapeFx, cutoff_, cutoff_target_, dcblock_fx_l_, dcblock_fx_r_, del_, dly_amt_, dly_amt_target_ (+28 more)

### Community 19 - "Community 19"
Cohesion: 0.13
Nodes (37): action(), answer(), api(), bind(), bridge(), checks, controls(), disconnected() (+29 more)

### Community 20 - "Community 20"
Cohesion: 0.17
Nodes (26): CcButton, down, AutoCloseAndShortTakes(), CcButtons(), CombinationsDoNotTriggerSingleKeys(), FirstTakeSeamAndPlayback(), LimiterKeepsOverdubsBounded(), LoopClosesIntoOverdubAndFeedback() (+18 more)

### Community 21 - "Community 21"
Cohesion: 0.14
Nodes (26): ChromaticPitch(), CubicTableRead(), FactoryTapeFiles(), Fill(), GateAndTrigger(), HeaderRoundTrip(), KitMapping(), LoadingAndHandoff() (+18 more)

### Community 22 - "Community 22"
Cohesion: 0.11
Nodes (32): api(), build(), CONTROLS, drawSamples(), drawSlots(), fromSlider(), GROUPS, KNOB_LABELS (+24 more)

### Community 23 - "Community 23"
Cohesion: 0.06
Nodes (27): Inspect(), SampleLoader, count_, current_, cursor_, handoff_, head_, index_ (+19 more)

### Community 24 - "Community 24"
Cohesion: 0.09
Nodes (6): BridgeCardTests, sounds_like(), FakeChompi, FastTime, WalkTests, hand()

### Community 25 - "Community 25"
Cohesion: 0.17
Nodes (19): DecodePatchData(), DecodeRequest(), EncodePatchData(), EncodeResponse(), PatchDataSize(), Read14(), ExecuteRequest(), PackUsbSysEx() (+11 more)

### Community 26 - "Community 26"
Cohesion: 0.16
Nodes (24): Crossings(), Energy(), Hz(), LfoAndModWheel(), main(), NeutralV3IsAPlainVoice(), Peak(), Power() (+16 more)

### Community 27 - "Community 27"
Cohesion: 0.19
Nodes (13): AliasDb(), ChannelTranslation(), ClickFreeStealAndRetrigger(), Crossings(), EnvelopeVelocityAndFilter(), Instrument(), main(), PitchBend() (+5 more)

### Community 28 - "Community 28"
Cohesion: 0.19
Nodes (4): Command, parameter, value, DecodeCC()

### Community 29 - "Community 29"
Cohesion: 0.10
Nodes (15): Clock, interval_, last_tap_, pending_, phase_, rate_, since_, tap_sum_ (+7 more)

### Community 30 - "Community 30"
Cohesion: 0.09
Nodes (7): main(), run(), main(), selftest(), message(), ForgeServer, main()

### Community 31 - "Community 31"
Cohesion: 0.15
Nodes (7): Find(), Parse(), Clamp(), PartsWordsValid(), TapeSpeedKnob(), ValidWords(), AudioBlock()

### Community 32 - "Community 32"
Cohesion: 0.17
Nodes (22): Balance(), ChargerReadings(), CopyFile(), DrawLeds(), InspectorStorageError(), InstallFaultHandler(), InstallPowerOk(), LoadOptions() (+14 more)

### Community 33 - "Community 33"
Cohesion: 0.07
Nodes (24): PanelInput, frames, jack, keys, toggle_up, tone_down, turns, Rig (+16 more)

### Community 34 - "Community 34"
Cohesion: 0.09
Nodes (20): DjFilter, control_, hp_, hp_c_, hp_target_, hpl_, hpr_, lp_ (+12 more)

### Community 35 - "Community 35"
Cohesion: 0.08
Nodes (5): CardTests, DeadLink, LossyTransport, LostEndReply, Sequence

### Community 37 - "Community 37"
Cohesion: 0.10
Nodes (16): Held, count, key, note, source, used, Player, held_ (+8 more)

### Community 38 - "Community 38"
Cohesion: 0.13
Nodes (9): Sequencer, cursor_, held_, pending_off_, playing_, pos_, sequence_, state_ (+1 more)

### Community 40 - "Community 40"
Cohesion: 0.07
Nodes (28): Request, action, bank, command, epoch, kind, mode, note (+20 more)

### Community 41 - "Community 41"
Cohesion: 0.09
Nodes (26): Response, action, bank, capacity_ms, cpu_average, cpu_max, error, flags (+18 more)

### Community 42 - "Community 42"
Cohesion: 0.19
Nodes (6): ParseSampleName(), SampleEvent, job, ok, SampleFiles, SamplePath()

### Community 43 - "Community 43"
Cohesion: 0.10
Nodes (20): , DecodeFile(), EncodeFile(), Event, a, b, kind, tick (+12 more)

### Community 44 - "Community 44"
Cohesion: 0.10
Nodes (17): Pad, amp, attack, end, gate, loop, pan_l, pan_r (+9 more)

### Community 46 - "Community 46"
Cohesion: 0.12
Nodes (16): SelectSample(), Chromatic(), FileFormat(), Kit(), main(), Near(), Rig, engine (+8 more)

### Community 47 - "Community 47"
Cohesion: 0.08
Nodes (26): LedView, blink, count_in, flash, harmony, input_level, install, keys_down (+18 more)

### Community 49 - "Community 49"
Cohesion: 0.25
Nodes (12): TapeSpeedRatio(), BatteryHold(), KeysKnobsAndOverrides(), KnobPages(), LedComposition(), LooperSaveGesture(), LooperThroughThePanel(), LooperVoiceCap() (+4 more)

### Community 50 - "Community 50"
Cohesion: 0.10
Nodes (12): SampleCard, append_budget, fail_reads, fail_writes, files, kTempName, opens, read_name_ (+4 more)

### Community 51 - "Community 51"
Cohesion: 0.16
Nodes (14): Arp(), ArpPatterns(), BassModesAndMidi(), GateLatchAndOwnership(), main(), Note, note, source (+6 more)

### Community 52 - "Community 52"
Cohesion: 0.18
Nodes (19): Fade(), Fade3(), Fade4(), HasPatchPage(), IsPatchPage(), Kind, Effects, Sampler (+11 more)

### Community 53 - "Community 53"
Cohesion: 0.17
Nodes (20): ComposeHarmonyKnobLeds(), ComposeKnobLeds(), ComposeLeds(), ComposeLooperLeds(), ComposeMenuKnobLeds(), ComposePartsKnobLeds(), ComposeTransportLeds(), PackLooper() (+12 more)

### Community 54 - "Community 54"
Cohesion: 0.09
Nodes (22): Inspect(), BassRate, Chord, Eighth, Half, Quarter, PackArp(), PackClock() (+14 more)

### Community 55 - "Community 55"
Cohesion: 0.19
Nodes (6): Crc16(), DecodePresetRecord(), EncodePresetRecord(), PresetPath(), PresetStore, Storage

### Community 56 - "Community 56"
Cohesion: 0.10
Nodes (16): Effects, active_, compressor_, compressor_target_, dj_, dj_active_, left_, makeup_ (+8 more)

### Community 57 - "Community 57"
Cohesion: 0.15
Nodes (16): PackSelection(), ChromaticAndKitLoading(), LoaderRig, card, events, handoff, loader, loop (+8 more)

### Community 58 - "Community 58"
Cohesion: 0.15
Nodes (8): Bench, check(), demangle(), forge_runs(), setup(), main(), run(), upstream_runs()

### Community 59 - "Community 59"
Cohesion: 0.10
Nodes (21): InspectorSnapshot, audio, generation, storage, system, InspectorStorage, busy, errors (+13 more)

### Community 60 - "Community 60"
Cohesion: 0.16
Nodes (20): BatteryWarning(), InstallPowerOk(), Lockout, None, Unplugged, WeakSupply, LockoutText(), Readings (+12 more)

### Community 61 - "Community 61"
Cohesion: 0.10
Nodes (21): RequestKind, Bend, Clock, Erase, List, Looper, ModWheel, Note (+13 more)

### Community 62 - "Community 62"
Cohesion: 0.14
Nodes (19): ConvertFrames(), ParseWav(), ReadLe16(), ReadLe32(), WavError, HeaderTooLarge, None, NotWav (+11 more)

### Community 63 - "Community 63"
Cohesion: 0.12
Nodes (8): Clocked, clock, out, s, main(), MemoryCard, files, RecordAndLoop()

### Community 64 - "Community 64"
Cohesion: 0.32
Nodes (15): Crc32(), Begin(), CodecAndNames(), Data(), Decode(), Firmware(), FirmwareGuards(), Fuzz() (+7 more)

### Community 66 - "Community 66"
Cohesion: 0.14
Nodes (5): check(), image_problems(), symbols(), ConsistencyTests, session_ids()

### Community 67 - "Community 67"
Cohesion: 0.15
Nodes (3): PatchTests, probe(), PresetProtocolTests

### Community 68 - "Community 68"
Cohesion: 0.18
Nodes (13): EncodeInspector(), InspectorEvent, id, kind, serial, time_ms, value, InspectorLog (+5 more)

### Community 69 - "Community 69"
Cohesion: 0.11
Nodes (19): InspectorSystem, battery, charge_state, crash_pc, crashed, dropped, emergencies, event_drops (+11 more)

### Community 70 - "Community 70"
Cohesion: 0.15
Nodes (12): Reverb, damp_, gain_, index_, kLines, length_, line_, longest_ (+4 more)

### Community 71 - "Community 71"
Cohesion: 0.25
Nodes (14): KnobOrder(), main(), MenuGestures(), Panel, menu, toggle, Protocol(), RecordsAndPaths() (+6 more)

### Community 72 - "Community 72"
Cohesion: 0.13
Nodes (4): FakeMidi, Message, FakePort, TransportTests

### Community 73 - "Community 73"
Cohesion: 0.20
Nodes (12): Change(), Dependency(), Features, balance, envelope, rms, spectrum, Fft() (+4 more)

### Community 74 - "Community 74"
Cohesion: 0.14
Nodes (11): BootloaderMatches(), SetAsideName(), BootloaderNames(), MemoryCard, corrupt_write, fail_append, fail_rename, files (+3 more)

### Community 75 - "Community 75"
Cohesion: 0.18
Nodes (10): InstallGate, armed_at_, kArmed, kConfirmed, kIdle, kTimeoutMs, state_, swallow_ (+2 more)

### Community 76 - "Community 76"
Cohesion: 0.19
Nodes (16): Checksum(), EncodeError(), EncodePanelAck(), EncodeProbeLeds(), Header(), V3Field, index, kind (+8 more)

### Community 77 - "Community 77"
Cohesion: 0.11
Nodes (17): Kind, Copy, Erase, Save, SampleJob, bank, frames, from_loop (+9 more)

### Community 78 - "Community 78"
Cohesion: 0.12
Nodes (13): OnePole(), Warble, coefficient_, end_, frequency_, kLength, left_, length_ (+5 more)

### Community 79 - "Community 79"
Cohesion: 0.18
Nodes (6): files(), main(), verify(), main(), windows_runtime(), verify()

### Community 80 - "Community 80"
Cohesion: 0.16
Nodes (9): check_samples(), describe(), generate_patch(), NoRedirect, provider_schema(), ProviderError, sample_summary(), parse_json() (+1 more)

### Community 81 - "Community 81"
Cohesion: 0.13
Nodes (13): Rig, engine, hw, l, panel, r, rec, recorder (+5 more)

### Community 82 - "Community 82"
Cohesion: 0.12
Nodes (5): _Input, Message, _Output, _Port, SimulatedMido

### Community 83 - "Community 83"
Cohesion: 0.12
Nodes (16): FileOp, Abort, Begin, Data, End, Install, Status, FileRequest (+8 more)

### Community 84 - "Community 84"
Cohesion: 0.12
Nodes (16): InKey(), Layout, Real, Static, Pack(), State, block, enabled (+8 more)

### Community 85 - "Community 85"
Cohesion: 0.12
Nodes (16): InspectorEventKind, Card, KeyDown, KeyUp, Knob, PatchApply, QueueError, RecordingStart (+8 more)

### Community 86 - "Community 86"
Cohesion: 0.25
Nodes (7): CopyFile(), EraseFile(), LoadFile(), SaveFile(), FilePath(), EngineAndPanel(), ProjectsOnTheCard()

### Community 87 - "Community 87"
Cohesion: 0.13
Nodes (10): Event, None, Start, Stop, RecordGesture, chompi_, count_, counting_ (+2 more)

### Community 88 - "Community 88"
Cohesion: 0.16
Nodes (10): Mailbox, bank, owner, sequence, slot, state, Owner, Host (+2 more)

### Community 89 - "Community 89"
Cohesion: 0.17
Nodes (7): FatFsUploadFiles, aside_, check_, file_, kAsideBatch, target_, verify_buffer_

### Community 90 - "Community 90"
Cohesion: 0.21
Nodes (11): DelayImpulseAndBypass(), Fixture, engine, left, right, InitializationAndDryStereo(), main(), Near() (+3 more)

### Community 92 - "Community 92"
Cohesion: 0.16
Nodes (3): FakeMido, TestTrafficCommands, interrupt()

### Community 94 - "Community 94"
Cohesion: 0.18
Nodes (6): InspectorMailbox, owner_, state_, MailboxOwnership(), main(), PhysicalVersusInjectedAndVoiceEdges()

### Community 95 - "Community 95"
Cohesion: 0.14
Nodes (7): SpscQueue, head_, storage_, tail_, ObserveVoiceEdges(), SetInspectorEvents(), ObserveVoiceEdges()

### Community 96 - "Community 96"
Cohesion: 0.13
Nodes (10): SampleSlot, channels, data, frames, gain, loaded, partial, rate_ratio (+2 more)

### Community 97 - "Community 97"
Cohesion: 0.15
Nodes (13): Actions, force_detection, read, usb_to_daisy, ChargerUsb, asked_, handoff_, handoff_at_ (+5 more)

### Community 98 - "Community 98"
Cohesion: 0.16
Nodes (10): Describe(), FaultRecord, cfsr, count, hfsr, kMagic, lr, magic (+2 more)

### Community 99 - "Community 99"
Cohesion: 0.14
Nodes (13): EraseReply(), Ingress, Control, Critical, Emergency, Ignore, Storage, IsPerformance() (+5 more)

### Community 100 - "Community 100"
Cohesion: 0.17
Nodes (5): FatFsSampleFiles, read_, reading_, write_, writing_

### Community 102 - "Community 102"
Cohesion: 0.21
Nodes (3): BridgeWalkTests, SetupCheckTests, Device

### Community 103 - "Community 103"
Cohesion: 0.27
Nodes (9): AllowedUploadName(), DecodeFileRequest(), EncodeFileReply(), IsFirmwareName(), PlausibleFirmware(), Read35(), ServeFileRequest(), Write35() (+1 more)

### Community 104 - "Community 104"
Cohesion: 0.30
Nodes (11): Lead(), Movement(), Parallel(), Place(), QualityStack(), RealFunction(), Resolve(), StaticFunction() (+3 more)

### Community 105 - "Community 105"
Cohesion: 0.14
Nodes (14): Chord, degree, kind, quality, root, shifted, tone, Kind (+6 more)

### Community 106 - "Community 106"
Cohesion: 0.21
Nodes (7): RecoveryGate, AudioCallback(), Callback, gate, replies, NoteRequest(), RecoveryAfterLostNotes()

### Community 108 - "Community 108"
Cohesion: 0.15
Nodes (3): envelope(), InstrumentTests, opener()

### Community 110 - "Community 110"
Cohesion: 0.21
Nodes (9): CubicQ14(), CubicTable, w, Dual16(), GlideSnap(), Inspect(), MakeCubicTable(), PackHigh() (+1 more)

### Community 111 - "Community 111"
Cohesion: 0.32
Nodes (9): Tones(), Degree(), Identity(), InScale(), Layouts(), main(), Notes(), PitchClasses() (+1 more)

### Community 113 - "Community 113"
Cohesion: 0.17
Nodes (10): Performance, compressor, dj_filter, dj_resonance, input_gain, pan, saturation, speed (+2 more)

### Community 114 - "Community 114"
Cohesion: 0.17
Nodes (13): Battery, Full, High, Low, Medium, Unknown, DecodeStatus(), Status (+5 more)

### Community 115 - "Community 115"
Cohesion: 0.18
Nodes (3): bench_block(), bench_init(), Noise()

### Community 116 - "Community 116"
Cohesion: 0.21
Nodes (3): FatFsStorage, file_, mounted_

### Community 117 - "Community 117"
Cohesion: 0.17
Nodes (9): FileTransfer, crc_, done_, head_, kTemp, name_, sample_written_, size_ (+1 more)

### Community 118 - "Community 118"
Cohesion: 0.17
Nodes (12): Function, degree, kind, octave, quality, root, Quality, Dominant (+4 more)

### Community 119 - "Community 119"
Cohesion: 0.17
Nodes (12): Kind, CC, Clock, NoteOff, NoteOn, PitchBend, ProgramChange, SysEx (+4 more)

### Community 120 - "Community 120"
Cohesion: 0.17
Nodes (8): MenuAction, bank, kind, mode, slot, to_bank, to_mode, to_slot

### Community 121 - "Community 121"
Cohesion: 0.17
Nodes (12): Error, Busy, Checksum, Empty, Length, None, Opcode, Patch (+4 more)

### Community 122 - "Community 122"
Cohesion: 0.17
Nodes (3): MemoryCard, files, SineWav()

### Community 123 - "Community 123"
Cohesion: 0.17
Nodes (7): MidiPort, clock_dropped, dropped, framer, frames, transport, PollMidi()

### Community 125 - "Community 125"
Cohesion: 0.17
Nodes (5): MemoryStorage, drop_writes, fail_writes, files, ready

### Community 126 - "Community 126"
Cohesion: 0.20
Nodes (5): SampleHandoff, ack_, published_, request_, FailedLoadIsVisible()

### Community 127 - "Community 127"
Cohesion: 0.18
Nodes (11): Kind, Jack, Key, Press, Release, Toggle, Turn, PanelEvent (+3 more)

### Community 128 - "Community 128"
Cohesion: 0.18
Nodes (10): Envelope, release_step, stage, value, Stage, Attack, Decay, Off (+2 more)

### Community 130 - "Community 130"
Cohesion: 0.22
Nodes (3): AnalysisTests, walk(), sine()

### Community 132 - "Community 132"
Cohesion: 0.20
Nodes (10): Mode, Dorian, HarmonicMinor, Locrian, Lydian, Major, MelodicMinor, Mixolydian (+2 more)

### Community 133 - "Community 133"
Cohesion: 0.31
Nodes (7): BootGesture, held_, BootGestureOff(), Colours(), main(), OptionsFile(), StatusDecode()

### Community 134 - "Community 134"
Cohesion: 0.20
Nodes (10): Kind, Copy, Erase, Recall, RecordSource, SampleCopy, SampleErase, SampleSave (+2 more)

### Community 135 - "Community 135"
Cohesion: 0.33
Nodes (10): Flash(), forge::Error RecallPreset(), HandleFrame(), HandleStorage(), LoadSequence(), Queue(), RaiseEmergency(), RunSampler() (+2 more)

### Community 137 - "Community 137"
Cohesion: 0.22
Nodes (8): MidiFramer, bytes_, cc_, cc_used_, overflow_, status_, sysex_, used_

### Community 138 - "Community 138"
Cohesion: 0.22
Nodes (8): Options, midi_in, midi_out, monitor, quantise_menu, record_latch, split_delay, tape_slew

### Community 139 - "Community 139"
Cohesion: 0.22
Nodes (9): ResponseKind, Erased, Occupancy, SampleDone, SampleOccupancy, SampleSnapshot, Snapshot, Status (+1 more)

### Community 141 - "Community 141"
Cohesion: 0.25
Nodes (3): CardUploads, open, temp

### Community 142 - "Community 142"
Cohesion: 0.22
Nodes (4): FirmwarePanelSink, PanelAction, action, patch

### Community 143 - "Community 143"
Cohesion: 0.31
Nodes (3): SampleWebAndAiTests, capture(), reply()

### Community 145 - "Community 145"
Cohesion: 0.25
Nodes (8): InspectorVoice, age, envelope, flags, note, slot, source, stage

### Community 146 - "Community 146"
Cohesion: 0.36
Nodes (5): Compressor, gain_, peak_, SoftClip(), SoftLimit()

### Community 147 - "Community 147"
Cohesion: 0.25
Nodes (3): Sink, actions, full

### Community 148 - "Community 148"
Cohesion: 0.29
Nodes (7): Extension, Eleventh, Fifth, Ninth, Seventh, Thirteenth, Triad

### Community 149 - "Community 149"
Cohesion: 0.29
Nodes (6): State, Armed, Empty, FirstTake, Paused, Playing

### Community 150 - "Community 150"
Cohesion: 0.29
Nodes (7): Pattern, Down, Off, Order, Random, Up, UpDown

### Community 151 - "Community 151"
Cohesion: 0.29
Nodes (7): Rate, Eighth, EighthTriplet, Quarter, Sixteenth, SixteenthTriplet, ThirtySecond

### Community 152 - "Community 152"
Cohesion: 0.29
Nodes (6): MenuMode, CopyDest, CopySource, Erase, None, Save

### Community 153 - "Community 153"
Cohesion: 0.29
Nodes (7): Action, a, b, kind, Kind, Note, Param

### Community 154 - "Community 154"
Cohesion: 0.29
Nodes (6): State, Armed, Empty, Playing, Recording, Stopped

### Community 156 - "Community 156"
Cohesion: 0.33
Nodes (4): MidiOut, data1, data2, status

### Community 158 - "Community 158"
Cohesion: 0.33
Nodes (6): Bass, Alternate, Fifth, Octave, Off, Root

### Community 159 - "Community 159"
Cohesion: 0.33
Nodes (6): State, Detaching, Headers, Idle, Job, Streaming

### Community 164 - "Community 164"
Cohesion: 0.60
Nodes (3): bench_block(), bench_init(), Noise()

### Community 165 - "Community 165"
Cohesion: 0.40
Nodes (5): KnobHold, down, frames, turned, used

### Community 166 - "Community 166"
Cohesion: 0.40
Nodes (4): Event, note, source, velocity

### Community 167 - "Community 167"
Cohesion: 0.40
Nodes (5): BatteryColour(), Colour, b, g, r

### Community 172 - "Community 172"
Cohesion: 0.50
Nodes (4): SlotPolicy, Patch, Recall, Select

### Community 173 - "Community 173"
Cohesion: 0.50
Nodes (4): Outgoing, bytes, size, source

### Community 175 - "Community 175"
Cohesion: 0.67
Nodes (3): BootCount, boots, magic

### Community 176 - "Community 176"
Cohesion: 0.67
Nodes (3): CaptureInspector(), CollectInspector(), InspectorEvent()

## Knowledge Gaps
- **1281 isolated node(s):** `ready`, `ready`, `open`, `ready`, `age_` (+1276 more)
  These have ≤1 connection - possible missing edges. (Counts symbols only; 1783 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **31 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Engine` connect `Community 5` to `Community 7`, `Community 16`, `Community 20`, `Community 21`, `Community 25`, `Community 26`, `Community 27`, `Community 28`, `Community 29`, `Community 31`, `Community 32`, `Community 33`, `Community 37`, `Community 38`, `Community 39`, `Community 45`, `Community 46`, `Community 70`, `Community 71`, `Community 81`, `Community 88`, `Community 90`, `Community 94`, `Community 96`, `Community 99`, `Community 106`, `Community 113`, `Community 126`?**
  _High betweenness centrality (0.133) - this node is a cross-community bridge._
- **Are the 9 inferred relationships involving `Engine` (e.g. with `main()` and `PhysicalVersusInjectedAndVoiceEdges()`) actually correct?**
  _`Engine` has 9 INFERRED edges - model-reasoned connections that need verification._
- **What connects `ready`, `ready`, `open` to the rest of the system?**
  _1281 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Community 0` be split into smaller, more focused modules?**
  _Cohesion score 0.023790430366212242 - nodes in this community are weakly interconnected._
- **Why does `Synth` connect `Community 0` to `Community 96`, `Community 128`, `Community 129`, `Community 44`, `Community 110`, `Community 14`, `Community 26`, `Community 27`?**
  _High betweenness centrality (0.093) - this node is a cross-community bridge._
- **Are the 3 inferred relationships involving `PanelController` (e.g. with `main()` and `PhysicalVersusInjectedAndVoiceEdges()`) actually correct?**
  _`PanelController` has 3 INFERRED edges - model-reasoned connections that need verification._
- **Should `Community 1` be split into smaller, more focused modules?**
  _Cohesion score 0.05328218243819267 - nodes in this community are weakly interconnected._