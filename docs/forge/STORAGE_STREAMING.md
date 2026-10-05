# Storage virtualization / SDRAM reclamation — feasibility and design

Status: **parked for Forge v2 (DC, 2026-10-05).** Design only; nothing
implemented; no production memory allocation changed. Every SDRAM, buffer and
latency figure below is an estimate until the Phase 0 hardware measurements exist.

**Revalidate before v2 work starts.** This study describes v1 as of `753078b`.
Re-check (and update sections 1, 4, 6, 7) if v1 changes any of:
- SDRAM allocations in `src/forge_main.cpp` (`kPoolSamples`, `kRecordFrames`,
  `kLoopFrames`, `kDelayCapacity`) or anything new placed in SDRAM;
- the sampler read path (`Synth::SampleFrame`/`Read`/`ReadEdge`: interpolation,
  crossfade, reverse, start/end window) or the voice count;
- `SampleLoader` / `SampleSlot` / `SampleHandoff`, the recorder, or the looper's
  memory use;
- USB file transfer (anything that replaces files on the card);
- FatFS or SDMMC configuration (vendored libDaisy `ffconf.h`, bus speed);
- executable headroom (release/development image size).

Principle: storage capacity belongs on the SD card; SDRAM holds only the working
set that real-time playback needs; internal SRAM/DTCM holds hard-real-time state.

## 1. Baseline (verified from source and the ELF)

Branch head `753078b` (the brief named `b7d098b`; 23 commits later: knob pages,
0.7 USB loader, bridge). Release build `src/build/FORGE.elf` (xPack GCC 10.3.1),
`.sdram_bss` = **66,715,664 B** of 67,108,864 B:

| SDRAM region | Source | Bytes |
| --- | --- | --- |
| Sample pool | `forge_main.cpp` `kPoolSamples` = 32 MiB / 2 | 33,554,432 (32 MiB) |
| Recorder | `kRecordFrames` = 16 MiB / 4, stereo int16 | 16,777,216 (16 MiB, 87.4 s) |
| Looper | `kLoopFrames` = 4,000,000 stereo int16 | 16,000,000 (15.26 MiB, 83.3 s) |
| Delay lines | `kDelayCapacity` 48,002 floats × 2 (not in the brief) | 384,016 |
| **Free** | | **393,200 (0.37 MiB)** |

Executable space (SRAM_EXEC 282 KiB = 288,768 B): release **221,712 B, headroom
67,056 B**; development 234,376 B, headroom 54,392 B (0.13: CPU fix A cubic table
+4.6 KB; harmony menu page, Inspector page 8 and patch v6 +1.9 / +2.6 KB after the savings; harmony
core +6.6 KB, queues moved to `.bss` −26.9 KB, cold code `-Os` and start-up
construction −25.8 KB; 0.12 was 261,224 / 277,268 B. 0.12: per-slot
settings, +6.3 KB after moving the cold paths out of line, then the safety extras
+152 B; 0.11 was 254,756 /
269,808 B, 0.10 253,636 / 268,728 B, 0.9
was 238,092 / 253,696 B, 0.7 235,980 / 251,684 B; the brief's 223,444 B / 63.8 KiB predates 0.6–0.7). D1 SRAM (`SRAM`, .data + .bss) 130,148 / 235,520 B used (0.12: + the 8 KB `presets.json` buffer and the 2.9 KB settings table; 0.11 118,236 B); DTCM .dtcmram_bss 35,480 B (reverb 34,816 + the 664-byte vector table copy from 0.11).

Calibration of what storage code costs today (release ELF, `nm -S`):
`SampleLoader` ≈ 7.1 KB (Poll 1,568, StartJob 1,544, Stream 1,404, StepJob
1,360, Header 1,240, Scan …); USB file transfer ≈ 4.0 KB + 1.5 KB FatFS glue;
`Synth::ProcessSampler` (voice loop, `SampleFrame` inlined) 3,712 B,
`ReadEdge` 532 B; FatFS `f_read` 622, `f_write` 684, `f_lseek` 916.

## 2. What the code does now

- **SampleLoader** (main loop): scan → on a selection change, `SampleHandoff`
  detaches file voices → headers → `Stream` reads ≤ 16 KiB per `Poll` (scratch
  in D1 SRAM, DMA-reachable), converts any WAV (8/16/24-bit, float, mono/stereo,
  8–96 kHz) to int16 interleaved into the pool and publishes `SampleSlot::loaded`
  (release). It already plays **progressively**: voices may start before a file
  has finished loading; frames at or beyond `loaded` read as silence.
- **Whole-bank residency**: the chromatic file, or all 14 kit files, live in
  the pool until the selection changes; a file longer than the free pool is
  `partial` (cut). The pool caps a kit bank at ~174 s of stereo 48 kHz.
- **Voices** (`Synth::SampleFrame`/`Read`): `s.data + pos × channels`, contiguous
  addressing over the whole file; 4-tap Hermite below 1× (linear above); the loop
  crossfade reads a **second, distant position** (`pos − length`); reverse,
  live start/end window (`Window`), `rate_ratio` = file rate / 48 kHz, glide,
  ±2-semitone bend, LFO pitch. Up to 7 voices.
- **Recorder** (audio owner): writes stereo int16 straight into slot 14's
  memory, 5 ms fades (the tail fade rewrites the last 240 frames at `Stop`),
  normalisation gain applied at playback and at save; the take is playable as
  slot 14 immediately after `Stop`. Saving copies RAM → `FORGE_TMP.WAV` →
  rename (main loop, `StepJob`).
- **Looper**: RAM only; reads and writes around the head, varispeed ±2, scrub,
  reverse, overdub. No Forge code copies a sample into the looper (LOOPING.md
  describes TAPE's menu for it).
- **FatFS** (vendored libDaisy `ffconf.h`): `_USE_FASTSEEK 1` (cluster link map
  available: seeks in large files need not walk the FAT chain), `_USE_EXPAND 0`
  (no `f_expand`; pre-allocate by seeking past EOF in write mode, then truncate),
  `_FS_EXFAT 0` (FAT32 only, 4 GiB file limit), `_FS_TINY 0` (per-FIL 512 B
  buffer). SDMMC 4-bit at 50 MHz (`Speed::FAST`). Audio blocks are 24 frames
  (0.5 ms).

## 3. Precedent: stock TAPE already streams from SD on this hardware

`firmware/chompi-tape/code/src/FileStreamingManager.*`, `SampleReader.h`,
`DSPEngine.h`, `RamBuffer.h` (read-only, upstream):
- **All sample files stream**; `kMaxPoly = 7` voices, each with its own `FIL`
  and a FIFO of `kMaxFileStreamingSamps = 8192` int16 (4,096 stereo frames ≈
  85 ms). Total stream buffering ≈ 112 KiB. File I/O is a request FIFO (64)
  served from the main loop (`ProcessRequests`); reverse uses `REV_READ`.
- **No head preload**: a note opens/seeks and the voice outputs its last value
  × envelope while the FIFO holds < 16 samples (start latency and starvation
  are absorbed this way).
- **`_double.wav`** files are half-length 2× copies: above 1.5× speed the voice
  switches to them, halving SD bandwidth when pitched up. TAPE regenerates them
  at boot (Forge deletes them on save).
- SDRAM goes to the looper (`loop_mem`) and the CHOMPI recording
  (`chompi_mem`), `kMaxRamBuffSize` = 31,694,848 B each.

So 7-voice SD streaming with sub-100 ms buffers ships on CHOMPI. It does not
prove Forge's harder cases (Hermite taps, loop crossfade, live start/end,
24-bit/96 kHz files, kit attacks with zero latency), and TAPE's real stall
behaviour is unmeasured by us.

## 4. Architectural blockers and couplings

1. **Contiguous addressing in the hottest DSP loop.** `Read`/`ReadEdge` assume
   the whole file at `s.data`; the crossfade reads `pos − length`. Streaming
   needs a page translation in `SampleFrame` (CPU-sensitive, must be benched and
   proven bit-exact when everything is resident).
2. **`loaded` is a prefix model** (frames `[0, loaded)` valid). Streaming needs
   per-page validity with generations (a page can be evicted and refilled).
3. **One read handle** (`SampleFiles` / one `FIL`). Kit streaming needs a FIL
   per streamed file (~560 B each in D1 SRAM) plus a fast-seek table.
4. **Single-threaded main loop, blocking FatFS.** An SD stall blocks MIDI
   handling, USB transfer and LEDs for its duration (the audio interrupt is
   unaffected). Acceptable during today's bank loads; continuous streaming makes
   it continuous. Bounded per-`Poll` work stays mandatory; MIDI RX overflow
   under long stalls must be measured.
5. **File replacement while streaming.** A USB upload renames over a file a
   voice may be reading (today `End` is refused only while the loader is busy).
   Must detach that slot before the rename.
6. **Bandwidth depends on the file, not the cache.** Cache holds int16; the
   card delivers 2–6 B/sample frame-channel: a 96 kHz 24-bit stereo file pitched
   up an octave needs ~1.15 MB/s per voice; 7 such voices ~8 MB/s. Needs a
   per-file bandwidth class (resident fallback for heavy files, or a voice cap).
7. **Recording slot semantics.** The recorder writes the playable slot directly
   in RAM, rewrites its last 5 ms at `Stop`, and voices read it at once. A
   streaming recorder must hold back the fade region and make the take playable
   through the cache.
8. **Inspector schema** reports pool bytes; new diagnostics need a schema bump.

Not blockers: no allocation exists or is needed (fixed pools); handoff/detach
already exists; conversion already runs in the main loop; card insert/remove
is already watched (`WatchCard`); temp-file + rename is already the write rule.

## 5. Proposed design

### 5.1 Sample streaming: shared page cache + pinned heads (one pool)

- **Page pool** in SDRAM: N fixed pages (proposal 32 KiB = 8,192 int16 stereo
  frames ≈ 171 ms at 1×; mono files pack 2× frames). Page descriptor in D1/DTCM:
  `{slot, file_page, generation, state, pins}`; state published with release,
  read with acquire. No allocation, fixed arrays, O(1) lookup through a per-slot
  page map (`slot × pages_per_file_window`).
- **Small files are simply resident**: a file that fits its head budget loads
  whole (today's behaviour, minus the 32 MiB pool). Typical kit hits never stream.
- **Pinned heads** per slot: pages from window start − crossfade span forward
  (forward patches), or from window end backward (reverse patches); loaded at
  bank selection, re-made when start/end move (main loop, like today's reload).
  They give zero-latency note starts and cover the loop wrap/crossfade region.
- **Streaming voices**: each sampling voice owns a small read-ahead window of
  page slots in the direction of travel; the main-loop **scheduler** refills
  earliest-deadline-first: deadline = buffered frames ÷ (speed × 48 kHz). Seven
  chromatic voices on one file share pages (same `slot, file_page`).
- **Audio side**: per voice per block, translate the playhead page once; the
  common case (all 4 taps in one valid page) costs one range compare, as the
  current `readable` check does. Taps on a missing page take the edge path.
- **Eviction**: unpinned, refcount 0, least recently used; never a page a voice
  window references (the audio side publishes window positions each block).
- **Hybrid fallback**: a file whose bandwidth class exceeds the measured budget,
  or any slot when streaming is disabled, loads resident as today.

### 5.2 Underrun and failure semantics

| Event | Behaviour |
| --- | --- |
| Voice reaches a non-resident page | 2 ms fade to silence, playhead keeps advancing (stays in time), fade back in when the page arrives; `underrun` counter + per-voice starved flag. Never stale memory: a page is valid only while its generation matches |
| Note start without a head (head being rebuilt after a start/end move) | Start deferred up to a bound (proposal 20 ms), then start silent-advancing; counter |
| Card removed | Resident heads and pages keep playing; streaming voices fade at their next miss; new loads/streams fail with `Storage`; FILs dropped; remount + rescan stay in `WatchCard` (main loop) |
| File replaced (USB) / erased / copied over | Slot detached (handoff) before the rename; its pages invalidated (generation++), reloaded after |
| FatFS error | Bounded retries in the main loop only (proposal 2), then the slot goes `partial`/error; Inspector shows last error |

### 5.3 Streaming recorder

- Audio writes stereo int16 into an SDRAM **ring** (single producer/consumer,
  release/acquire indices); the main loop drains to `FORGE/TAKE.TMP` in 32 KiB
  chunks, holding back the last 240 frames (the `Stop` fade rewrites them in RAM).
- File pre-allocated at `Start` by seeking past EOF (no `f_expand`), extended in
  large steps; truncated and header-patched at finalize; renamed to its final
  name only when complete. A partial or aborted take never carries the final
  name (crash-safe: only `TAKE.TMP` can be partial).
- **Overrun** (ring full): recording stops at that frame; everything before it
  is kept and finalized as a valid, shorter take flagged `overrun`; counter +
  Inspector flag. Nothing after the overrun is half-written.
- Take length bounded by the card (FAT32: 4 GiB ≈ 6.2 h stereo 16-bit 48 kHz),
  not SDRAM. Playback of the take goes through the page cache (its head pages
  are filled from the ring before it drains, so it plays immediately after Stop).
- Save to a slot = SD→SD copy with gain (`StepJob` Copy path + gain), as today's
  normalised save.
- Monitoring and record-source selection are untouched (they are before
  `Recorder::Write`).

### 5.4 Looper

Stays RAM-resident (15.26 MiB) in every scenario. A later, separate option:
asynchronous **archival** of the performance loop or of a long capture through
the same recorder ring (RAM loop stays authoritative for playback/overdub).

### 5.5 QSPI (Tier 2)

The bootloader lives in the STM32's internal flash; it writes Forge's BOOT_SRAM
image into QSPI (at its program offset), and the image is copied to SRAM at boot. It is memory-mapped read-only during normal operation; erase/program
needs indirect mode and stalls every QSPI access, so it is only writable at update
time. Usable later for immutable assets/tables; **not** for sample streaming or
anything written at run time. Forge has no such asset need today: no QSPI work
in this project.

## 6. SDRAM scenarios (estimates — not production values)

Assumptions: int16 cache; worst playback speed 4.5× (2× key range × 1.12 bend ×
2× for 96 kHz files); TAPE's 85 ms per-voice FIFO as the shipped lower bound;
SD write busy up to 250–500 ms (SD spec timeouts) before measurement.

| Region | Current | Conservative | Balanced | Aggressive |
| --- | --- | --- | --- | --- |
| Sample working set (heads + voice windows + spare) | 32 MiB | 8 MiB: 256 × 32 KiB; heads 14 × 2 directions × 4 pages; 7 voices × 16 pages (2.7 s at 1×, 0.6 s at 4.5×) | 4 MiB: 128 × 32 KiB; heads 14 × 2 pages (current direction only); 7 voices × 8 pages (1.4 s at 1×, 0.3 s at 4.5×) | 1 MiB: 128 × 8 KiB; heads 14 × 1; 7 voices × 8 pages (0.34 s at 1×, 76 ms at 4.5× ≈ TAPE) |
| Recorder | 16 MiB | 4 MiB ring (21.8 s of stall) | 2 MiB (10.9 s) | 0.5 MiB (2.7 s) |
| Looper | 15.26 MiB | 15.26 MiB | 15.26 MiB | 15.26 MiB |
| Delay lines | 0.37 MiB | 0.37 MiB | 0.37 MiB | 0.37 MiB |
| **Free SDRAM** | **0.37 MiB** | **36.4 MiB** | **42.4 MiB** | **46.9 MiB** |
| Reclaimed vs today | — | +36.0 MiB | +42.0 MiB | +46.5 MiB |

Reading the table: **eliminating whole-file residency gives ~80 % of the gain
already in the conservative scenario**; going aggressive adds only ~10 MiB more
for most of the added risk (TAPE-sized buffers, no pitch-up margin, short write
stall tolerance). Recommendation: target conservative first, move toward
balanced only with measured worst-case stalls; aggressive only with `_double`
use or a lower speed cap, and never without stress data.

Aggressive-only risks: underruns on pitched-up or 96 kHz/24-bit files; kit
starts dependent on one-page heads; recorder failing on cards with long GC
stalls; no slack for simultaneous record + 7-voice streaming.

## 7. Code, CPU and main-loop cost (estimates)

| Part | Executable (release) |
| --- | --- |
| Page cache + scheduler + head management (main loop) | 4–6 KiB (cf. loader 7.1 KB) |
| Page translation in the voice loop | 0.5–1.5 KiB (inlined into `ProcessSampler`) |
| Streaming recorder + finalize | 2–3 KiB |
| Diagnostics / Inspector fields | 0.5–1 KiB (mostly development builds) |
| **Total** | **≈ 7–12 KiB = 20–35 % of the 34.3 KiB release headroom** (35–61 % of the 19.6 KiB development headroom; firmware 0.10 stage 2b) |

D1 SRAM: FIL per streamed file (~560 B) × up to 15 + fast-seek tables (e.g. 64
words each) + page descriptors (~16 B × 256) ≈ 14 KiB, inside the ~124 KiB free.

CPU, audio side: one page check per voice-frame (same order as today's
`readable` compare) — must be verified with `make bench` (gate: worst ≤ WAVE's
2,695 instructions/sample) and on hardware (per-step CPU now exists).
Main loop: at 1× and 7 voices ≈ 1.3 MB/s (42 pages/s); at the 4.5× worst case
≈ 6 MB/s. With an assumed (unmeasured) 8–12 MB/s effective FatFS read rate that
is ~15 % main-loop duty at 1× and 50–75 % at worst — the reason heavy files need
the bandwidth class / resident fallback.

**Per reclaimed byte**: conservative ≈ 36 MiB for ≈ 10 KiB of code — about
3,700 bytes of SDRAM per byte of executable.

## 8. Hardware benchmark (Phase 0) — required before any buffer size is fixed

Development-build opcode (proposal 0D, development only) that runs a scripted
storage workload from the main loop **with audio running**, timing every FatFS
call with the DWT cycle counter, and replies with log2 latency histograms
(64 µs … 2 s, 16 buckets), max, count, bytes. The bridge gets a *Storage
benchmark* job that writes the report like the automatic checks.

| Workload | Measures |
| --- | --- |
| Create 64 MiB test file (sequential 32 KiB writes) | write throughput, **write stall distribution and max** |
| Sequential 32 KiB reads, one file | read throughput, read latency |
| 1/4/7 interleaved files, 32 KiB reads round-robin | seek + multi-file cost (the 7-voice kit pattern) |
| Random 32 KiB reads in a 64 MiB file, with and without fast seek | seek latency, CLMT benefit |
| Same while `13-sampler-stress` / `10-cpu-stress` play | contention with audio, per-step CPU |
| Write 192 KB/s stream + 7-file reads together | recorder + playback |
| Reads while MIDI floods (bridge sends notes/CCs) | main-loop blocking vs MIDI RX (lost or late messages) |
| Fragmented file (written interleaved with another) | fragmentation penalty |
| Pull the card mid-workload, reinsert | error paths, remount time |
| DC's cards (at least the card in use + one other) | card-to-card spread |

Report worst case first (max, p99.9), then averages. Also log main-loop
iteration time max, which bounds MIDI latency.

## 9. Staged plan (each phase separately testable and revertible)

| Phase | Content | Memory change | Gate to continue |
| --- | --- | --- | --- |
| 0 | Benchmark opcode + bridge job (development only); this document | none | DC runs it once (part of the next consolidated hardware session) |
| 1 | `PageCache` + scheduler as a native library with a simulated slow card (latency/stall injection), then one chromatic voice streaming in development builds behind `FORGE_STREAMING`, pool untouched | none (cache carved from the existing pool) | bit-exact vs resident playback when no stall; underrun semantics tested; bench ≤ gate |
| 2 | 7 chromatic voices, loop crossfade, reverse, live start/end, glide/bend | none | stress + stall tests, hardware playback test |
| 3 | Kit (14 files, FIL per file, fast seek), bandwidth classes, USB-upload detach | none | kit stress on hardware, card pull |
| 4 | Streaming recorder (ring, pre-allocation, finalize/rename, overrun) | none (ring carved from the recorder buffer) | long take, stall injection, card pull |
| 5 | Shrink `kPoolSamples` / `kRecordFrames` to the measured scenario; report freed SDRAM as headroom (not allocated) | one constant per region, own commit | full regression + hardware session |

Rollback at every phase: the resident path stays (build flag and per-file
fallback); Phase 5 is a single revertible commit.

QA per phase (from the brief, all apply): native + sanitizers + full regression;
sampler and recorder regression (resident vs streamed outputs bit-exact without
stalls); cache boundary, page-crossing taps, wraparound, reverse/loop/crossfade
at page edges; malformed/truncated WAVs; injected slow storage and long stalls;
queue saturation; underrun; card removal; deterministic randomized stress; ARM
release/dev build, size diff and memory-layout check; `make bench`.

Diagnostics (Inspector development telemetry): underruns, starved
voice-blocks, min buffered ms per voice, page hits/misses, pages in use/pinned,
scheduler queue depth, recorder ring high-water and overruns, SD max latency
since reset (resettable like the CPU peak).

## 10. Answers to the brief's questions

1. Reclaimable SDRAM: ~36 MiB (conservative) to ~42 MiB (balanced); ~46.5 MiB
   aggressive. Estimates.
2. Minimum plausible sample cache: TAPE ships ~112 KiB of per-voice FIFOs;
   for Forge's features (instant kit starts, crossfade, 96 kHz/24-bit) ~1 MiB is
   the floor and 4–8 MiB the realistic target, pending Phase 0.
3. Minimum plausible recording ring: 0.5 MiB covers 2.7 s; 2–4 MiB recommended
   until measured write stalls are known.
4. Hard features: contiguous Hermite/crossfade reads, live start/end, reverse,
   kit attacks at zero latency, high-bandwidth files, file replacement while
   streaming, the recorder rewriting its tail.
5. New failure modes: underrun (dropouts), start deferral, MIDI lateness during
   SD stalls, recorder overrun, partial takes, stale pages after file changes.
6. Code: ≈ 7–12 KiB release.
7. CPU: audio side small (bench); main loop 15 % at 1× up to 50–75 % at the
   worst speed/format, unmeasured.
8. Hardware measurements: section 8, before any size is fixed.
9. Incremental: yes, phases 0–5 with the resident path kept throughout.
10. Fallback: resident mode per build and per file; Phase 5 revert restores
    today's allocation.

**Smallest safe proof of concept:** Phase 0 (benchmark only, development
build, no audio path change), then the Phase 1 native `PageCache` with a
simulated slow card. Stop for architecture review before Phase 1 code lands.
