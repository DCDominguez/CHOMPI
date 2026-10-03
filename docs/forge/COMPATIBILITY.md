# Forge vs stock TAPE / TEMPO / WAVE: compatibility and CPU benchmark

Measured 2026-10-03 against the upstream sources in `firmware/chompi-tape`,
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
| Do stock sources rebuild like factory? | No: TAPE overflows with the xPack compiler; TEMPO builds 4.6 KB smaller |

## 1. Bootloader acceptance

From `firmware/chompi-tape/code/Chompi_Bootloader/bootloader/src/bootloader.cpp`:

- It loads the **first** non-hidden directory entry whose name contains `.bin`
  or `.BIN`, whatever the name, and stops there.
- It accepts the file if word 0 (initial stack pointer − 1) is in RAM (not
  QSPI, internal flash or invalid) and word 1 (entry point) is in D1 SRAM or
  QSPI. An identical image already in QSPI is skipped.

| Binary | Size | Stack pointer | Entry | Accepted |
| --- | --- | --- | --- | --- |
| FORGE.bin 0.4 | 142,520 | 0x20020000 (DTCM) | 0x240008DD (D1 SRAM) | yes |
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
| Forge 0.4 | 232 KB (60 % used) | 280 KB (21 %) | ~92 KB code headroom |

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

## 5. Upstream build reproducibility

The stock sources were built in a scratch copy with the same xPack GCC
10.3.1-2.3 used for Forge (the pinned Arm 10.3-2021.10 archive is unreachable
from the agent sandbox):

- **TAPE does not build on Linux as shipped:** it includes `Limiter.h`, but the
  file is `limiter.h` (works only on case-insensitive filesystems).
- **TAPE overflows** SRAM_EXEC by 3,524 bytes with xPack. The factory image,
  built with the Arm archive, fits with 376 bytes to spare.
- **TEMPO builds 4,660 bytes smaller** than the factory image (258,452 vs
  263,112).

So "repo sources + xPack" does not reproduce the factory builds. Either the
sources differ from what shipped, or the compilers generate different code;
the pinned archive is needed to tell which. For Forge, ~92 KB of code headroom
makes either effect harmless, but it is one more reason to prefer the pinned
compiler for release bundles.

## 6. CPU benchmark (`make bench`)

**Method.** Each workload is compiled with the firmware's compiler and flags
(Cortex-M7, FPv5-D16 hard float, `-O3`) against that app's own vendored
libDaisy/DaisySP, then run in the Unicorn emulator. Every executed instruction
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
| TEMPO 1.0 FX + output (sample engines **not** included) | 1,362 | 13.6 % |
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
- Bell Keys is the most expensive Forge preset because its eight sine
  oscillators call `sinf` (~46 instructions each). A cheaper sine is an easy
  optimisation if CPU proves tight.
- The real number is TEST_SESSION 6.2b (`H status` peak CPU).

Files: `firmware/chompi-forge/bench/` (`run_bench.py`, `*_bench.cpp`,
`bench.ld`). Needs `arm-none-eabi-gcc` and `pip install unicorn pyelftools`.
