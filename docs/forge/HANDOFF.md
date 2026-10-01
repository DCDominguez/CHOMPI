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

1. Review the provisional M0 control mapping and bring up the physical unit
   using the README's checklist. Log exact board/card/bootloader and results.
2. Add on-device CPU/load and control-overflow reporting to guide expansion.
3. After hardware acceptance, define versioned patch data and atomic apply,
   including failure behavior, before implementing an AI host or Tab5 UI.

Avoid expanding into Wi-Fi, arbitrary scripts, or multiple new effects before
the basic audio/control path is measured. Do not describe M0 as a tested
replacement for the stock sampler or as an AI patch loader.
