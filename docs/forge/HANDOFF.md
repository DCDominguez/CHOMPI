# Forge handoff

Updated: 2026-10-01. Branch: `forge/foundation`.

## Start here

Read [PROJECT.md](PROJECT.md) and the
[firmware README](../../firmware/chompi-forge/README.md).
The first code lives in `firmware/chompi-forge/`.

## Implemented

- A separate BOOT_SRAM application reusing WAVE hardware and linker definitions.
- Stereo aux input to a 10–1000 ms delay, copied to headphone and main outputs.
- Mix/time/feedback/level/bypass with bounded values and smoothing.
- USB/TRS MIDI CC controls on channel 1 and physical encoder controls.
- Fixed-capacity control queue; audio owns DSP state; no allocation in the core.
- Explicit SDRAM buffer clearing before audio; inherited battery protection.
- Host tests and a source build of the vendored libDaisy into Forge's build tree.

## Verification

- `make test`: passes with host GCC, C++14, warnings treated as errors.
- ASan/UBSan: passes with `ASAN_OPTIONS=detect_leaks=0`; this environment cannot
  run LeakSanitizer's `/proc` inspection. Leak checking was not performed.
- Tests exercise invalid controls/CC filtering, queue full/wrap/FIFO behavior,
  100,000 concurrently transferred commands, initialization, dry stereo
  separation, timed impulse response, wet bypass, output gain smoothing,
  ten seconds of full-feedback/time-change stress, and non-finite input.
- ARM application and the vendored libDaisy both compile from source and link
  successfully with GNU Arm Embedded 10.3-2021.10. The link map confirms use
  of Forge's freshly built `build/libdaisy/libdaisy.a`.
- Final link usage: SRAM_EXEC 91,192 / 237,568 bytes (38.39%); SRAM
  111,708 / 286,720 (38.96%); RAM_D2 22,304 / 32,768 (68.07%); SDRAM
  384,016 / 67,108,864 (0.57%). These are linker allocations, not CPU load
  or measured worst-case stack usage.
- No device attached: no flash, audio audition, USB/TRS hardware check,
  callback CPU measurement, or stock restore test has been performed.

## Environment note

The official compiler archive hash matches the README. Its compiler subprocess
crashed even on trivial C when run from the workspace mount. Extracting it to
`/tmp/forge-toolchain/` with `tar --no-same-owner` resolved the crash. This is an
environment workaround, not a firmware code change.

## Next work

DC requested **one consolidated hardware test session** on 2026-10-01.
Do not ask for an M0-only flash now or interrupt each software milestone for
physical testing. Continue automated host tests and ARM builds throughout.

1. Add on-device CPU/load and control-overflow reporting for the eventual test.
2. Define and implement versioned patch data and atomic apply, including
   rejection/failure behavior; verify offline before building the minimal host.
3. Assemble the bounded first candidate and minimal external-authoring/control
   host, with offline integration tests. Dedicated Tab5/Wi-Fi work remains later.
4. Prepare one firmware candidate, controller/test harness, restoration steps,
   and one sequenced checklist covering boot/audio, controls, patch recall,
   host control, stress/load, and power behavior. Then run one guided hardware
   session and record exact board/card/bootloader and results.

Keep hardware assumptions explicit while those checks are deferred. If the
session exposes a hardware-dependent defect, a targeted retest may be necessary;
do not promise that exactly one flash will be sufficient.

Avoid expanding into Wi-Fi, arbitrary scripts, or multiple new effects before
the basic audio/control path is measured. Do not describe M0 as a tested
replacement for the stock sampler or as an AI patch loader.
