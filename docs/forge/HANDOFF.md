# Forge handoff — candidate 0.2

Updated: 2026-10-02 (Asia/Manila). Remote branch: `forge/foundation`, draft PR #1.

## Start here

Read [PROJECT.md](PROJECT.md), [PROTOCOL.md](PROTOCOL.md), the
[host instructions](../../firmware/chompi-forge/host/README.md), and the
[one-session checklist](TEST_SESSION.md).

DC wants one consolidated hardware test. Do not request a separate M0 flash
or a physical test after each feature. The software candidate is now assembled;
physical acceptance remains outstanding. A defect may still require a retest.

## Implemented

- BOOT_SRAM app using WAVE's hardware, encoder and linker definitions.
- Stereo aux input through a 10–1000 ms delay to headphone/main outputs.
- Smoothed mix/time/feedback/level/wet bypass, local encoders and MIDI CC.
- Versioned host JSON presets, validation, exclusive file saving, capture of
  current device targets, and atomic full-patch recall at an audio block boundary.
- USB/TRS SysEx apply/status/reject replies with sequences and checksums.
- A dedicated MIDI framer tolerating interleaved real-time bytes, overflow and
  resynchronization; complete USB SysEx packetization and persistent TX buffers.
- Bounded queues and callback control work, response backpressure, drop/reject
  counts, and average/peak callback CPU reporting. No TX in the audio callback.
- Host Python CLI, three presets, offline renderer sharing the actual DSP core,
  and an optional Ollama structured-output adapter. AI generation validates and
  saves a preset; sending remains a separate explicit command.
- A bundle-generation script with source identity, firmware/file checksums,
  synthetic audio references, licensing and the consolidated test checklist.

## Verification and limits

- Host DSP/queue tests pass with C++14 and warnings as errors.
- Protocol tests pass: atomic rejection, corrupt/truncated messages, unsupported
  versions, MIDI running status and clock interruption, resynchronization,
  100,000 fuzz bytes, and USB final-packet sizes.
- Twelve Python integration tests pass. They include 250 seeded random patch
  round trips through the real C++ protocol/runtime, malformed-patch state
  preservation, persistence, strict JSON, mock AI responses, acknowledgement
  mismatch/timeout handling, explicit port selection, and offline WAV output.
- ASan/UBSan pass for the DSP/queue and protocol suites. LeakSanitizer is disabled
  because this execution environment cannot perform its `/proc` inspection.
- GNU Arm Embedded 10.3-2021.10 builds libDaisy and Forge from source and links
  the application. FORGE.bin is 100,592 bytes. Link allocation: SRAM_EXEC 42.34%,
  SRAM 15.55%, RAM_D2 68.07%, SDRAM 0.57%. These are not CPU/stack measurements.
- Pinned Mido 1.3.3 and python-rtmidi 1.5.8 downloads/imports and MIDI message
  construction checked on the execution host; no physical MIDI port tested.
- No device attached: no flash, audio audition, USB enumeration, TRS I/O,
  callback-load measurement, battery validation or stock restore performed.
- No running Ollama model used: adapter requests/responses are mock-tested.
  Actual model compatibility, response time and musical interpretation are pending.

## Deliberate limits

One compiled stereo-delay engine. Host-managed preset files; device targets are
volatile and reset on reboot. No sampler, looper, on-device preset files,
patch graph, generated DSP, Tab5 UI, Wi-Fi, or onboard AI. Bypass fades only the
wet contribution and retains level. Time changes glide in pitch. Hard clipping
bounds numeric output but is not a mastering limiter.

Only one host and one outstanding acknowledged request at a time. Timeout can
mean the patch applied but its reply was lost; query status before retrying.
Read PROTOCOL.md for counter meanings, overload and transport limits.

## Build and package

From `firmware/chompi-forge`:

```sh
make test
ASAN_OPTIONS=detect_leaks=0 make sanitize
make firmware GCC_PATH=/path/to/gcc-arm-none-eabi-10.3-2021.10/bin
# Commit tested source first; outputs must be outside the tracked source tree.
python host/package_candidate.py /absolute/path/Forge_0.2_Test_Candidate.zip
```

The bundle's manifest identifies its exact source commit/tree and binary hash.
Generated binaries and bundles are not committed to the source repository.
The bundle is experimental and does not constitute a hardware-approved release.

The compiler initially crashed from the workspace mount but worked after
extracting the verified official archive to `/tmp/forge-toolchain/` with
`tar --no-same-owner`. Temporary compiler files may need restoring after an
idle session. Do not trust a partial archive; verify the README's SHA-256.

## Next action

Run the single consolidated session using TEST_SESSION.md when DC has the unit
and computer ready. Record passes, failures and explicitly skipped sections in
`docs/forge/TEST_RESULTS.md`. Do not mark hardware or actual-model acceptance as
complete based on software tests. Use the measured result to decide further DSP
or controller work; keep expansion outside this first candidate.
