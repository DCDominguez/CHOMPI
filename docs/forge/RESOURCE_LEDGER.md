# Forge Resource Ledger

Reviewed 2026-10-04 UTC. Branch: `forge/foundation`. Reviewed head:
`fec241622a68adc52075e242941ac0f1ed293bec`; firmware source is unchanged from
`131776b4a7c04538dc52cb4dd7354d0d102a3d56` (0.6, knob pages / v5 patches).
This ledger supersedes older resource summaries, not historical test records.
No firmware implementation, hardware test, flash, live AI call or main merge
was performed for this review.

## Reproduced baseline

Both the handoff commit and reviewed head were independently built with
checksum-verified **xPack GCC 10.3.1-2.3**, `-O3`, `BOOT_SRAM`; development adds
`FORGE_TEST_HOOKS`. All four builds pass the layout guard. Current binary hashes
match the kit hashes recorded in CONTINUE. Exact hashes, ELF sections, region
sizes, toolchain hashes and benchmark results: [machine-readable evidence](resources/2026-10-04-fec2416.json).

| Metric | Handoff `b7d098b` | Current `fec2416` | Delta |
| --- | ---: | ---: | ---: |
| Release image / SRAM_EXEC used | 223,444 B | 227,060 B (78.63%) | +3,616 B |
| Development image / SRAM_EXEC used | 239,632 B | 242,512 B (83.98%) | +2,880 B |
| Release executable headroom | 65,324 B | **61,708 B / 60.26 KiB** | −3,616 B |
| Development executable headroom | 49,136 B | **46,256 B / 45.17 KiB** | −2,880 B |
| D1 data SRAM, release | 92,716 B | 93,532 B | +816 B |
| D1 data SRAM, development | 96,780 B | 97,596 B | +816 B |
| SDRAM static reservation | 66,715,664 B | 66,715,664 B (99.414%) | 0 |
| DTCM static reservation | 34,816 B | 34,816 B | 0 |
| RAM_D2 static reservation | 23,800 B | 23,800 B | 0 |

These deltas cover the knob-page implementation; the immediate parent of
`fec2416` is `131776b` and its delta is **zero** (CONTINUE-only kit record).
Always record both the previous commit and previous measured firmware baseline.
The earlier full looper increment was +9,728 B release versus `1d1618e`
(213,716 B, repository-reported); that is 9.50 KiB, not new growth in 0.6.

**Image size is not pure instruction size.** The linker puts `.data` load images
in SRAM_EXEC as well as code/constant tables. Current release `.text` = 194,776 B
and `.data` = 31,320 B. The knob increment includes +3,072 B `.text` and +544 B
`.data`; development includes +2,328 B and +552 B respectively. ELF file size
(debug information), image bytes and runtime RAM consumption are distinct.

## Memory map and working set

| Region | Capacity | Current static used, release / development | Linker headroom, release / development |
| --- | ---: | ---: | ---: |
| SRAM_EXEC | 288,768 B (282 KiB) | 227,060 / 242,512 B | 61,708 / 46,256 B |
| D1 data SRAM | 235,520 B (230 KiB) | 93,532 / 97,596 B | 141,988 / 137,924 B |
| DTCM | 131,072 B | 34,816 B, both | 96,256 B before runtime stack |
| RAM_D2 | 32,768 B | 23,800 B, both | 8,968 B |
| RAM_D2CACHE | 262,144 B | 0 | 262,144 B |
| RAM_D3 | 65,536 B | 0 | 65,536 B |
| ITCM | 65,536 B | 0 | 65,536 B |
| SDRAM | 67,108,864 B (64 MiB) | 66,715,664 B, both | **393,200 B / 383.98 KiB** |
| QSPI address region | 8,388,608 B | 0 linked by Forge | Contents/reservations not inspected |
| Backup SRAM | 4,096 B | 12 B (`boot_info` at `0x38800000`) | 4,084 B; preserve boot contract |

Free linker bytes are not a verified runtime budget. Stack starts at
`0x20020000` in DTCM; stack/heap peaks and DMA/cache constraints remain unmeasured.
Internal FLASH is also 0 linked by this application, but is not claimed free
of bootloader use. Unused regions are architectural candidates, not approved
drop-in capacity or proof of hardware availability.

| SDRAM object | Exact reservation | Interpretation |
| --- | ---: | --- |
| `sample_pool` | 33,554,432 B | 32 MiB, ~174.76 s stereo PCM16 at 48 kHz |
| `record_memory` | 16,777,216 B | 16 MiB, ~87.38 s at the same format |
| `loop_memory` | 16,000,000 B | **15.259 MiB**, 4,000,000 stereo frames, 83.33 s |
| `delay_left` + `delay_right` | 384,016 B | Two 48,002-element float arrays |
| Total | 66,715,664 B | All statically reserved, even when unused |

Source: `firmware/chompi-forge/src/forge_main.cpp` constants and globals,
`src/forge_sram.lds`, rebuilt ELF sections and linker reports. The 99.4% figure
describes reservation, not live sample content or a permanent working-set floor.
Reclaiming it requires a buffer-ownership/cache design; simply leaving a buffer
empty does not release its reservation. No 8 MiB cache or recording-ring size
has been approved or validated.

## QA and CPU baseline

| Check at `fec2416` | Independent result on 2026-10-04 |
| --- | --- |
| `make test` | PASS: 9 native suites + 95 Python tests |
| `make sanitize` | Initial LeakSanitizer environment failure (`/proc`/ptrace); no leak-test pass |
| `ASAN_OPTIONS=detect_leaks=0 make sanitize` | PASS: 9 ASan/UBSan suites |
| `make browser-test` | PASS: 11 workshop + 4 bridge Chromium tests; device simulated, AI mocked |
| `make firmware`, `make firmware-dev` | PASS: current and handoff baseline, layout checks included |
| `make bench` (runner uses `--check`) | PASS: 12 Forge scenarios <= WAVE gate |
| Physical audio/CPU, SD timing, Windows kit, live AI | NOT RUN in this review; no Forge hardware evidence found |

Current worst tested Forge workload: **2,671.0 instructions/sample**, versus
the stricter WAVE baseline **2,694.9** (only 0.89% below that comparator).
Looper overdub workload: **2,555.4**, with the firmware's **six-voice writing cap**;
the benchmark label says seven requested notes, not seven sustained active voices.
The repo reports unchanged CPU versus `b7d098b`; its rounded 2,671 / 2,555
figures agree with this run, but prior CPU was not independently rerun, so an
exact measured CPU delta is unavailable. TEMPO comparison uses GCC 13.3.1;
Forge/TAPE/WAVE use 10.3.1.

Instruction counts are not cycles or device utilization. This harness measures
DSP blocks, not the full audio callback, control bursts, ISR/DMA scheduling,
SDRAM contention, filesystem stalls or knob reconfiguration costs. Its CPI=1
percentage is not a hardware CPU measurement. Hardware deadline margin remains
unknown, particularly with seven sampler voices and background SD/MIDI traffic.

## First optimization decisions to investigate

0. **Second pass, 2026-10-05 (DC: keep the audio buffers, "look at other things"):**
   (a) every control-rate / main-loop function compiled for size (`FORGE_COLD` =
   noinline + `-Os`): request/response codecs, `ExecuteRequest`, file transfer,
   preset store, sample-loader control (not `Stream`), LEDs, options/restart
   parsing, `Engine::Init/ApplyPatch/ResetControl/LooperControl`,
   `Synth::Configure`, `Parameters::Valid`, main-loop services; per-sample DSP
   untouched. (b) Engine, panel controller, sample loader/table and Inspector
   buffers built at start-up in zeroed storage (`Construct<T>()` in
   `forge_main.cpp`) instead of `.data` images. Measured: release 240,948 →
   **215,160 B**, development 255,632 → **227,120 B**; `.data` 1,732 B; bench
   unchanged (worst 2,677.9). Total for the day: release 267,876 → 215,160
   (−52,716), development 283,856 → 227,120 (−56,736). Hardware note: control
   paths (knob turns reconfiguring the synth, requests) run `-Os` in the audio
   callback; CPU peaks in TEST_SESSION 6.2 confirm. Left alone: libDaisy (upstream,
   already `-O2`; USB-host IRQ code 1.9 KB is linked by its vector table), whole-
   firmware `-O2/-Os` (would change DSP timing: needs hardware numbers), audio
   buffers (DC).
1. **Done 2026-10-05 (0.13 in progress, DC: "implement our memory savings plan"):**
   `SpscQueue` now keeps raw zeroed storage copied with `memcpy` (entries must be
   trivially copyable, checked at compile time), so every queue is in `.bss`.
   Measured: release 267,876 → **240,948 B (−26,928)**, development 283,856 →
   **255,632 B (−28,224)**; `.data` 33,248 → 6,304 B (release); D1 static use
   unchanged (130,724 B: the bytes moved from `.data` to `.bss`); bench unchanged
   (worst 2,677.9). Remaining `.data`: engine 3,372 B, inspector snapshot/log ~2.5 KB
   (development), sample loader/table/panel ~1.4 KB — candidates for placement
   construction later. Original note:
   **Initialized queues before new memory hardware.** Release `requests` and
   `responses` occupy 11,272 + 14,088 = **25,360 B** in `.data`, charged to both
   the image and D1 RAM. Their entries contain default-initialized full patches.
   Investigate zero-backed/pre-audio initialization or compact event payloads;
   preserve object lifetime, queue ordering, overflow recovery and atomic patch
   handoff. This is measured footprint, **not a promised 25,360 B saving**.
2. **D1 code/data split as a later option.** The current split is a linker choice;
   development has 137,924 B of static data-region headroom. Any repartition needs
   stack/heap evidence, bootloader/layout checks and hardware validation. Do not
   spend that space as if already available. QSPI relocation also needs asset
   provisioning, startup/mapping and update/rollback design first.
3. **Reduce audio reservations with explicit ownership.** Reuse the existing
   loader/handoff and recording/save infrastructure. Buffer sharing changes which
   sample, recording and loop operations can coexist; preserve current behavior
   unless an explicit mode tradeoff is accepted. Streaming needs measured SD
   tail stalls, random/reverse reads, fragmentation, card variation and combined
   recording + playback + MIDI load. Callback misses must have deterministic,
   nonblocking behavior. No filesystem work belongs in the audio callback.
4. **Expansion foundation:** define compact shared musical state, deterministic
   seed and event ownership with the bounded clock/scheduler first; then harmony,
   voice-leading and reusable rhythm/arp/probability transforms. Keep AI planning,
   JSON/history editing and expensive search host-side. Do not duplicate the
   existing note/parameter engine. This is a recommendation, not an approved build.

## Update after each significant implementation

Append a dated evidence snapshot and refresh the tables above. Use the same
compiler/flags or explicitly break comparability. Retain prior snapshots.

| Required field | Record |
| --- | --- |
| Identity | Source SHA, parent SHA, previous measured SHA, firmware/wire version, clean/dirty tree, date |
| Build | Compiler/archive hash, flags, release/dev binary bytes + SHA-256, layout result |
| Memory | Each linker region used/free, deltas, major buffers, runtime stack/heap evidence or unknown |
| CPU | Same workload instruction counts + deltas; separate actual callback peak/deadline measurements |
| QA | Native/Python counts, sanitizers including leak mode, browser/integration, skips/failures |
| Value | Musical behavior gained, added executable bytes/RAM, reuse, host-side alternative |
| Decision | Explained regressions, unresolved risks, next hardware evidence needed |

Review trigger (policy proposed by this ledger, not an automated gate): any
unexplained size increase; >=1 KiB or >=1% image growth; any new large static
buffer; any CPU-gate regression. Smaller changes still require a delta entry.
Do not convert missing measurements into zero deltas or passes. Before publishing
docs, recheck `forge/foundation`; never overwrite a concurrent implementation.

## Snapshot 2026-10-05: firmware 0.13 harmony (Phase 1 complete)

| Field | Record |
| --- | --- |
| Identity | Commit after `77fb5a7` (parent, previous measured); firmware 0.13, patch v6, Inspector page 8; clean tree at commit |
| Build | xPack GCC 10.3.1, unchanged flags. Release 217,104 B (SHA-256 prefix `3c57221613456934`), development 229,768 B (`4bfe08a3d9b825bf`); layout OK both |
| Memory | SRAM_EXEC headroom 71,664 / 59,000 B (release/dev). Release `.text` 214,408, `.data` 1,732, `.bss` 129,700, DTCM 35,480, SRAM1 23,800 B. SDRAM unchanged (393,200 B free). Delta over `77fb5a7`: +1,944 / +2,648 B |
| CPU | Emulator gate unchanged: worst 2,677.9 vs WAVE 2,694.9; informational pitch-below-1× 2,800.8 (6.2f, DC accepted). Harmony resolves at note on/off on the main loop |
| QA | 14 native + 121 Python PASS; 13 ASan/UBSan PASS (`detect_leaks=0`); browser 11 + 7 PASS. No hardware |
| Value | Harmony menu page (keys = tonic), Inspector chord page, patch v6 (presets, webapp, AI) for ~2 KB |
| Decision | Hardware evidence still needed: TEST_SESSION 3.65–3.67 and the 6.2 CPU peaks with the `-Os` control paths |

## Snapshot 2026-10-05: CPU fix A (0.13, not installed)

| Field | Record |
| --- | --- |
| Identity | Commit after `2180da2` (parent, previous measured); firmware 0.13 (unchanged wire format) |
| Build | xPack GCC 10.3.1, unchanged flags. Release 221,712 B, development 234,376 B; layout OK both |
| Memory | +4,608 B both builds (`forge::kCubic` 4,104 B table + code). SRAM_EXEC headroom 67,056 / 54,392 B. No RAM or SDRAM change |
| CPU | Emulator vs WAVE 2,694.9: pitch .75 2,669.2 (was 2,800.8), kit below 1× 2,564.2 (2,755.8), 1× worst 2,603.5 (2,677.9), sliding pitch 2,690.5 (informational). Causes found by per-address profiling: float Hermite 59 instructions per stereo read → table + SMUAD/SMLAD ~30; glides that never ended (11 per voice per sample); tail tests (8 per voice); `table_` and `complete` checks. Pitched-down scenarios are now gated |
| QA | 14 native (new: cubic table vs Hermite, glide end) + 121 Python PASS; 13 ASan/UBSan PASS; browser 11 + 7 PASS |
| Value | Every pitched-down voice within the WAVE budget; glide pitch now exact |
| Decision | Hardware 6.2f (and 6.2 peaks) still decide; unaligned 32-bit reads rely on SDRAM being normal memory (libDaisy MPU region 1) and the default no-trap setting |

## Snapshot 2026-10-05: firmware 0.14 clock, arp and bass (not installed)

| Field | Record |
| --- | --- |
| Identity | Commit after `dd06649` (parent, previous measured); firmware 0.14, patch v7, Inspector page 9 |
| Build | xPack GCC 10.3.1, unchanged flags. Release 229,168 B (SHA-256 prefix `d2c48206bb3be6ff`), development 242,896 B (`04d3a67e383016b9`); layout OK both |
| Memory | +7,456 / +8,520 B. SRAM_EXEC headroom 59,600 / 45,872 B. `parts` state 700 B (start-up construction, `.bss`); `.data` unchanged 1,732 B; requests/responses grow with kMaxRequest 97 / kMaxReply 109. No SDRAM change. Control-rate parts code `FORGE_COLD` (first build without it: +23.5 KB, `Parts::Advance` alone 5.6 KB) |
| CPU | Emulator gate unchanged (pitch .75 2,669.2, 1x worst 2,603.5 vs WAVE 2,694.9): the parts run in `Engine::Block`, once per audio block, outside the per-sample path |
| QA | 15 native (new `parts_test`) + 123 Python PASS; 14 ASan/UBSan PASS; browser 11 + 7 PASS |
| Value | Arpeggiator, bass part, tempo / tap / MIDI clock in and out, all from the panel and patches |
| Decision | Hardware evidence needed: TEST_SESSION 3.68–3.72 (timing feel, MIDI clock with a DAW) |

## Snapshot 2026-10-05: firmware 0.15 event recorder and projects (not installed)

| Field | Record |
| --- | --- |
| Identity | Commit after `0b09d45` (parent, previous measured); firmware 0.15, Inspector page 10 |
| Build | xPack GCC 10.3.1, unchanged flags. Release 234,448 B, development 248,256 B; layout OK both |
| Memory | +5,280 / +5,360 B (headroom 54,320 / 40,512 B). SRAM `.bss` 144,588 B (+12,888: `Sequencer` 6,548, `sequence_file` 6,176); `.data` 1,732 B; 89,200 B of the 230 KiB SRAM left. SDRAM `.sdram_bss` 66,721,816 B (+6,152 mailbox), 387,048 B free |
| CPU | Emulator gate PASS: pitch .75 2,676.2 (+7.0, the fourth note source's bend smoothing), 1x worst 2,610.5 vs WAVE 2,694.9. Recorder playback runs in `Engine::Block`, once per block |
| QA | 16 native (new `sequencer_test`) + 124 Python PASS; 15 ASan/UBSan PASS; browser 11 + 7 PASS |
| Value | Loops of played notes and controls, overdub, projects on the card |
| Decision | Hardware evidence needed: TEST_SESSION 3.73–3.76 (feel of the bar start/close, FatFS writes of the loop files on DC's card) |
