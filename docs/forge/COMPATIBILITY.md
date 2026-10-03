# Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark

Measured 2026-10-03 (refreshed after device presets, Forge `4fec6ac`) against the upstream sources in `firmware/chompi-tape`,
`firmware/chompi-tempo`, `firmware/chompi-wave` (read only) and the factory card
images in `firmware/card-profiles`. **Everything here is source analysis, builds
and emulation. Nothing was run on CHOMPI hardware.**

## Summary

| Question | Finding |
| --- | --- |
| Will the bootloader accept FORGE.bin? | Yes by its own rules: same stack/entry layout as all three factory binaries |
| Same audio setup? | Yes: 48 kHz, 24-frame blocks, mic in 0, aux in 2/3, outputs 0/1 + 2/3 |
| Same keybed notes? | Yes: identical 25-key map (MIDI 48–72) to TAPE, TEMPO and WAVE |
| Same MIDI conventions? | Partly: CC20–23 agree; CC24/CC25 differ; channel fixed to 1 (stock configurable) |
| Is Forge's CPU load plausible? | Worst case ~1,430 instructions/sample, about half of WAVE's shipping engine (~2,700) |
| Can Forge share an SD card with stock files? | Yes: same SD bus setup and FatFS config as TAPE/WAVE; stock apps ignore `FORGE/` (§3a) |
| Is our compiler equivalent to the pinned one? | For libDaisy and DaisySP, yes: identical machine code to the shipped Arm 10.3-2021.10 objects (§5) |
| Do stock sources rebuild like factory? | Within ~3.9 KB (TAPE, WAVE) and 388 B (TEMPO); the gap is the compiler's runtime libraries, not the source (§5) |

## 1. Bootloader acceptance

From `firmware/chompi-tape/code/Chompi_Bootloader/bootloader/src/bootloader.cpp`:

- It loads the **first** non-hidden directory entry whose name contains `.bin`
  or `.BIN`, whatever the name, and stops there.
- It accepts the file if word 0 (initial stack pointer − 1) is in RAM (not
  QSPI, internal flash or invalid) and word 1 (entry point) is in D1 SRAM or
  QSPI. An identical image already in QSPI is skipped.

| Binary | Size | Stack pointer | Entry | Accepted |
| --- | --- | --- | --- | --- |
| FORGE.bin 0.4 (`4fec6ac`) | 187,592 | 0x20020000 (DTCM) | 0x240008DD (D1 SRAM) | yes |
| TAPE 2.0 factory | 240,520 | 0x20020000 (DTCM) | 0x24001901 (D1 SRAM) | yes |
| TEMPO 1.0 factory | 263,112 | 0x20020000 (DTCM) | 0x2400174D (D1 SRAM) | yes |
| WAVE 1.0 factory | 200,028 | 0x20020000 (DTCM) | 0x240018F9 (D1 SRAM) | yes |

**Card risks this exposes:**
- **Exactly one `.bin` on the card.** With several, directory order decides
  which one loads.
- **macOS AppleDouble files.** macOS can create `._FORGE.bin` on FAT cards. It
  is not marked hidden, so if it comes first the bootloader rejects it ("file
  does not contain executable code") and does not try `FORGE.bin`. Remove `._*`
  files (`dot_clean -m /Volumes/CARD`, or delete them) before ejecting.
  TEST_SESSION 1.1 includes this.

## 2. Memory layout

All four apps link as BOOT_SRAM into the 512 KB D1 SRAM, split differently:

| App | Code region (SRAM_EXEC) | Data region (SRAM) | Notes |
| --- | --- | --- | --- |
| TAPE | 235.25 KB | 276.75 KB | Factory image leaves 376 B of code space |
| TEMPO | 282 KB | 230 KB | xPack build: 89.5 % code, 70 % data, 50 % DTCM, 11 % SDRAM |
| WAVE | 232 KB | 280 KB | Forge uses this linker script |
| Forge 0.4 (`4fec6ac`) | 232 KB (79 % used) | 280 KB (23 %) | ~48 KB code headroom; DTCM 27 %, SDRAM 0.6 % |

The stock apps keep their reverb in DTCM (`DSY_DTCMRAM_BSS`). Forge now does
the same: its 34 KB reverb moved from SDRAM to DTCM (26.6 % of DTCM), because
the reverb's lines exceed the 16 KB data cache. The delay stays in SDRAM, as
in TAPE and WAVE. DTCM is not zeroed at boot; a test proves garbage there
never reaches the output.

## 3. Audio and controls

- 48 kHz, 24-frame blocks, identical `hardware.h` audio configuration.
- **Outputs:** stock apps apply output gain stages and a soft limiter to each
  output (TAPE also uses different headphone and line-out gains, 0.2 vs 0.3).
  Forge sends the same signal to headphone and line outs, with only a hard clip
  at ±1. So Forge's line-out
  level relative to the headphones differs from stock. TEST_SESSION section 0,
  step 5, records a stock loudness reference to compare.
- **Keybed:** Forge's note table equals the stock `NormalPage::key_map` for all
  25 note keys in TAPE, TEMPO and WAVE (checked programmatically).

## 3a. SD card coexistence (device presets)

| | TAPE 2.0 | TEMPO 1.0 | WAVE 1.0 | Forge 0.4 |
| --- | --- | --- | --- | --- |
| SDMMC | FAST, 4-bit | VERY_FAST, 4-bit | FAST, 4-bit | FAST, 4-bit (TAPE's sequence) |
| FatFS config (`ffconf.h`) | same | same | same | WAVE's libDaisy (same) |
| What it reads | its sample names, `options.json`, `presets.json` | `/Chromatic`, `/Slice`, `/Buffer`, options/presets | root names containing `.wav` | `FORGE/BnSnn.FPR` only |
| What it writes | presets/options in root; unlink + rename for presets | options/presets | options/presets | `FORGE/TMP.FPR`, then unlink + rename (TAPE's pattern) |

- No stock app scans subdirectories other than TEMPO's three fixed ones, and
  WAVE only takes root names containing `.wav`, so a `FORGE/` folder is
  invisible to all three. Forge never opens stock files.
- The bootloader only looks at root names containing `.bin`; `.FPR` records
  and the `FORGE` directory never match.
- The upstream build guide warns that compilers newer than 10.3 "can create
  issues with the SD card communication". Forge's libDaisy SD/FatFS code is
  compiled by GCC 10.3.1 and is byte-for-byte the same machine code as the
  shipped Arm build (§5).
- Forge sends MIDI replies with `BlockingTransmit` via `GetUartHandle()`, an
  accessor that exists only in WAVE's/TEMPO's patched libDaisy (TAPE's copy
  lacks it and keeps UART DMA queuing). Forge builds against WAVE's, so this is
  consistent; it is a reason not to switch Forge to TAPE's libDaisy.

## 4. MIDI conventions

| | Stock TAPE/TEMPO/WAVE | Forge 0.4 |
| --- | --- | --- |
| Input channel | Configurable (`options.json` midi_ch_in); CC input can be disabled | Fixed channel 1 |
| CC20–23 | Turn encoders SW1–SW4 | SW1–SW4's functions (mix, time, feedback, level) |
| CC24 | Ignored (encoder 4 skipped) | Wet bypass |
| CC25 | Encoder SW6 | Cutoff (SW5's function) |
| CC14/15 | Emulate two buttons | Not used |
| Note/CC output | Keys sent as MIDI | None (SysEx replies only) |
| Also in Forge | — | CC1, 26, 27, 64, 120, 121, 123, pitch bend, SysEx patches |

A controller template built for stock CHOMPI works for CC20–23 but differs on
CC24/25. Aligning these is a product decision (see CONTINUE "Next actions").

Device presets (0.4) follow TAPE's panel gestures: toggle in TAPE's menu
position + CHOMPI key opens the menu; white keys select slots; KEY_23/24/25
are erase/copy/save; CHOMPI confirms. KEY_16/17 select banks and encoder 1
also turns banks. TAPE's own files (`presets.json`, samples) are not read or
written; Forge's live in `FORGE/` and are never `.bin`.

## 5. Toolchains and build reproducibility

**Which compiler built what** (from strings in the factory binaries and the
`.comment` sections of the libraries shipped in the upstream repo):

| Item | Compiler |
| --- | --- |
| TAPE 2.0, WAVE 1.0 factory binaries | Arm GNU Embedded 10.3-2021.10 (newlib paths from Arm's 2021-10-18 build) |
| TEMPO 1.0 factory binary | Arm GNU Toolchain 13.x (TEMPO's README requires 13.3.rel1) |
| Shipped `libdaisy.a` / `libdaisysp.a` (all three apps) | Arm GNU Embedded 10.3-2021.10 |
| Forge | xPack 10.3.1-2.3 (checksum-verified; Arm's server is unreachable here) |

**Is xPack 10.3.1 the same compiler?** For everything that matters to Forge's
hardware code, yes. WAVE's libDaisy rebuilt with xPack produces the same
disassembly as the shipped Arm-built objects for **188 of 188** objects
(including the SDMMC, FatFS, UART, I2C and audio drivers), and DaisySP matches
for **56 of 56**. Both are GCC 10.3.1 20210824 built from the same sources.

**Factory rebuilds** (scratch copies; upstream untouched; shipped libraries used):

| App | Compiler used | Rebuilt | Factory | Difference |
| --- | --- | --- | --- | --- |
| TAPE 2.0 | xPack 10.3.1 | 244,420 (overflows SRAM_EXEC by 3,524) | 240,520 | +3,900 |
| WAVE 1.0 | xPack 10.3.1 | 203,920 | 200,028 | +3,892 |
| TEMPO 1.0 | xPack 13.3.1-1.1 | 263,500 | 263,112 | +388 |

The TAPE and WAVE gaps are nearly identical, and the libraries are proven
identical, so the extra ~3.9 KB comes from the compiler's own runtime
libraries (newlib/libgcc as packaged by xPack). A string comparison of WAVE
shows the same messages in both binaries, apart from those runtime build paths.
TEMPO with the right compiler (13.3) lands within 388 bytes; the earlier
"4.6 KB smaller" figure came from building it with 10.3, which TEMPO's README
rules out.

**Consequences for Forge:**
- An Arm-10.3 build of Forge would be roughly 3.9 KB smaller; code generation
  for its own code and libDaisy is otherwise the same. With ~48 KB of code
  headroom this is harmless.
- Building TAPE on Linux still needs a `Limiter.h` → `limiter.h` link
  (case-sensitive filesystem); TAPE does not fit with xPack 10.3 at all.

## 6. CPU benchmark (`make bench`)

**Method.** Each workload is compiled with the firmware's compiler and flags
(Cortex-M7, FPv5-D16 hard float, `-O3`) against that app's own vendored
libDaisy/DaisySP. TEMPO is built with GCC 13.3 (its upstream compiler,
`TEMPO_GCC_PATH`); TAPE, WAVE and Forge with 10.3, then run in the Unicorn emulator. Every executed instruction
of a 24-sample audio block is counted. Stock code is called unmodified:
- **TAPE:** `DSPEngine::ApplyFx` plus the output stage, reproduced statement
  for statement with TAPE's classes.
- **TEMPO:** `fxEngine::Process` + `ApplyOutputFX`.
- **WAVE:** `myEngine::Process` with all 8 voices sounding.

Forge runs its real presets through the real SysEx decoder.

| Workload | Instructions / sample | at 1 instr/cycle, % of 480 MHz |
| --- | --- | --- |
| Forge v1 delay (aux) | 209 | 2.1 % |
| Forge v2 Soft Pad, 4 voices | 621 | 6.2 % |
| Forge v3 Warm Pad, 4 voices | 1,209 | 12.1 % |
| Forge v3 Acid Bass, mono | 625 | 6.3 % |
| Forge v3 Bell Keys, 4 voices | 1,428 | 14.3 % |
| Forge v3 CPU Stress, 4 voices | 1,300 | 13.0 % |
| TAPE 2.0 FX + output (voices **not** included) | 1,249 | 12.5 % |
| TEMPO 1.0 FX + output (sample engines **not** included) | 1,352 | 13.5 % |
| WAVE 1.0 engine, 8 voices + delay | 2,747 | 27.5 % |
| WAVE 1.0 engine, 8 voices + reverb | 2,695 | 26.9 % |

**Reading it.** WAVE's engine ships and runs on this chip, so it is a
known-good load. Forge's heaviest case is about 53 % of WAVE's, and similar to
TAPE's or TEMPO's effects stage alone. Those two run their voices on top of
it. `make bench` fails if any Forge scenario exceeds WAVE (`--check`).

**Limits.**
- These are instruction counts, not cycles. The M7 dual-issues, and cache and
  SDRAM stalls are not modelled; real CPI can be above or below 1.
- UI, control scanning, SD streaming and MIDI are excluded for every app alike.
  Forge's preset menu runs in the audio callback but only on key/encoder edges
  and once per block (small; not separately measured); SD access is in the main loop.
- Forge numbers are unchanged by device presets (DSP untouched).
- Bell Keys is the most expensive Forge preset because its eight sine
  oscillators call `sinf` (~46 instructions each). A cheaper sine is an easy
  optimisation if CPU proves tight.
- The real number is TEST_SESSION 6.2b (`H status` peak CPU).

Files: `firmware/chompi-forge/bench/` (`run_bench.py`, `*_bench.cpp`,
`bench.ld`). Needs `arm-none-eabi-gcc` and `pip install unicorn pyelftools`.
