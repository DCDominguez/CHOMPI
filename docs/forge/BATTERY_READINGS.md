# CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12)

Open question from the owner: CHOMPI shut off while playing, reads yellow then green minutes
apart, and a "charged" battery behaved like an empty one. Is it the battery, or the way the
battery is read? This page lays out exactly how the readings are made (stock CHOMPI code, which
Forge reuses), what we measured, and the hypotheses with what would prove each. It is written so
a second opinion (a person or another AI) can check it without the rest of the repo.

Status: analysis; an instrumented development build exists (section "Instrumented build",
not yet run on CHOMPI). Nothing below is fixed yet. Claims are marked **code** (read in the
source), **measured** (logged on the owner's CHOMPI) or **hypothesis**.

## The hardware

- Charger / power-path IC: **MPS MP2722** on I2C address 0x3F, read by the Daisy Seed (STM32H750).
- It gives **no battery voltage or percentage**. CHOMPI's firmware only sees status bits; the
  battery level comes from **one comparator bit, BATT_LOW**, whose threshold the firmware sets
  through register **0x0C**.
- Values the stock code writes to 0x0C: `0b01010001` = "BATT_LOW at 3.0 V", `0b01011101` = "3.3 V"
  (comments in the stock code; the other bits of 0x0C are written at the same time. **Not checked
  against the MP2722 datasheet**: worth confirming what else those bits set).

## How the readings are made (stock code: `chompi-wave/code/src/hardware.h`)

**code**, every 20 ms in the main loop (Forge 0.19.2: `forge_main.cpp`, same calls):

1. `BMCPerformCheck()` = `MpReadAll()`: write the register pointer 0x11 (blocking), then start a
   **DMA read** of 6 bytes (registers 0x11-0x16) into `mp_buff_`. When it completes, the callback:
   - shifts `BATT_LOW` (reg 0x16 bit 4) into `batt_low_bounce` (an 8-reading history),
   - shifts VIN_GD (USB power), legacy cable and IINDPM (input at its current limit) into
     their own 8-reading histories,
   - CHG_STAT (reg 0x13 bits 7-5) = 0b101 ("charge done") sets the level to FULL (kept for 20 min).
2. `BMCMediumBattCheck()`, the green / yellow level, a small state machine (skipped while FULL):
   - state 0, every 30 s: write 0x0C = **3.3 V** threshold; state 1.
   - state 1, after more than 30 ms: start a read (`MpReadAll`), then write 0x0C = **3.0 V**; state 2.
   - state 2, after more than 30 ms: `batt_level = (mp_buff_[5] bit 4) ? MEDIUM : HIGH`; state 0.
3. The low-battery protection (stock `LowBatteryLockoutCheck`, called every 20 ms; Forge 0.19.2
   only after 150 checks = 3 s, `core/power.h LockoutGuard`):
   - `batt_low_bounce == 0xFF` (8 low readings in a row) and no USB: 15 s amber flashing, then
     the charger's shipping mode (off).
   - `batt_low_bounce == 0xFF` and (legacy cable or IINDPM for 8 readings): every light off and the
     processor stopped (STOP mode) until the power switch is cycled. **This is what CHOMPI does
     when it "shuts off" on a computer's USB port.**

What Forge reports over USB (`forge_host.py battery`, `core/power.h SupplyFlags`): "battery" =
`batt_level` (FULL white, HIGH green, MEDIUM yellow); "battery_low_reading" =
`batt_low_bounce != 0` (any of the last 8 readings low); "weak_supply" = legacy or IINDPM seen;
"charging" = CHG_STAT 1-4.

## Forge 0.19.2 level check (after release candidate 2)

Implemented and software-tested (`tests/power_test.cpp` LevelCheckBands / LevelCheckFailures),
**not yet hardware-verified**. `core/power.h LevelCheck` replaces `BMCMediumBattCheck` in
`forge_main.cpp` (the stock file is untouched; its 20 ms read, histories, FULL and protection stay):

- Problem 1: the first check runs at switch-on, then every 30 s (1 s after a check that gave no level).
- Problem 2: while 3.3 or 3.1 V is set the stock 20 ms reads are not started, so no reading
  against them reaches `batt_low_bounce`. The charger hand-over's read aborts a check first.
- Problem 3: the comparator (0x16 bit 4) is read by a blocking read 40 ms after the threshold
  write, only when no stock DMA read is in flight.
- Problem 4: every 0x0C write is read back; the 3.0 V restore is retried each 20 ms; a check that
  can't finish within 1 s gives up, the reads resume, and 3.0 V keeps being retried.
- Bands: green >= 3.3 V, yellow 3.1-3.3 V, red < 3.1 V (warning blinks fast), white charge done.
  While charging (CHG_STAT 1-4) a result above the current level is ignored.
- What the hardware can't do: register 0x0C offers only 3.0 / 3.1 / 3.2 / 3.3 V (bits 3-2), so
  nothing distinguishes 3.3 V from full. VSYS_MIN (up to 3.76 V) has a status bit too, but it
  sets the power path; not used.
- Inspector page 11 (development): `check_state` is now LevelCheck's state (0 idle, 1 at 3.3 V,
  2 at 3.1 V, 3 restoring); `samples_33` should stay near 0; `level_checks` counts finished checks.

## Problems in the reading itself (code)

1. **Yellow for the first 30 s after every power-on.** `batt_level` starts as `MEDIUM`
   (hardware.h:198) and the first level check runs only once uptime passes 30 s
   (`batt_level_checkt` starts at 0). Whatever the battery, CHOMPI shows yellow on SW6, reports
   "medium", and Forge refuses a USB install for the first half minute.
   *Measured:* after the 0.19.2 install, "medium" then "high" about 25 s later; on 2026-10-12, yellow
   on the first read after power-on, green a minute later.
2. **The 3.3 V readings land in the 3.0 V history.** Between the "3.3 V" write (state 0) and the
   "3.0 V" write (state 1) the regular 20 ms reads keep running, so for about 30-60 ms every 30 s
   one to three readings compare against 3.3 V and are shifted into `batt_low_bounce`. A battery
   between 3.0 and 3.3 V therefore produces "low" bits, and Forge's "battery_low_reading" (and our
   power log) shows "below 3.0 V" when it was only below 3.3 V.
3. **The green / yellow decision probably reads the wrong sample.** State 2 reads `mp_buff_[5]`
   30 ms after the 3.0 V write. By then the regular 20 ms reads (now at 3.0 V) have usually
   overwritten `mp_buff_`, so yellow likely means "below 3.0 V at that moment", not "below 3.3 V"
   (depends on DMA timing). The colour is also one sample every 30 s, taken under whatever load or
   charge current is flowing (charging lifts the voltage, playing sags it).
4. **I2C write results are ignored.** `MpWrite` ignores `TransmitBlocking`'s result. **Hypothesis:**
   if the state-1 write (back to 3.0 V) fails, every reading for the next ~30 s compares against
   3.3 V; under load a healthy battery at 3.1-3.3 V reads "low" continuously and the protection
   fires (stock after 160 ms, Forge 0.19.2 after 3 s).
5. **Concurrent DMA reads share one buffer.** The 20 ms read and the state-1 read both target
   `mp_buff_` with the same callback; libDaisy queues a second DMA job, but the callback order and
   which bytes the level check sees are not controlled.

## What we measured (power log, `D:\tools\chompi-session\power_log.py`, 10 readings a second)

CHOMPI on a computer's USB port (weak supply: IINDPM set, charging), the owner playing fast.

| When | Firmware | What the log shows |
| --- | --- | --- |
| 21:46:13.539 → .647 | 0.19.1 | level high → **medium and low flag together** (a level check), then no answer at 21:46:14.234 (off ~0.6 s later) |
| 21:58 - 22:02 | 0.19.2 | ~2,600 readings, never low, no shut-off |
| 22:03:51.226 → 22:03:54.842 | 0.19.2 | level stays **high**, low flag set **continuously for ~3.4 s** (29 readings), owner sees the yellow warning blink, then off (the 3 s guard ran out) |

Other observations (the owner): on a power bank (USB-C) CHOMPI draws 9.4 W switched off but about
0 W switched on; plugging in while on does not start charging until a power cycle (0.19.2 adds a
hand-over retry, not confirmed to help); after ~2 h "charging" switched off on a power bank that
drained, SW6 showed yellow at power-on (see problem 1) and green a minute later.

## Hypotheses and how to tell them apart

| Hypothesis | Predicts | Test |
| --- | --- | --- |
| A. The battery (or its connector) sags hard under load: worn cell, high resistance | Real < 3.0 V under load even when charged | Measure the battery with a multimeter at the cell while playing fast; full charge (white) then a runtime test on battery only |
| B. The threshold sometimes stays at 3.3 V (problem 4) | Long runs of "low" right after a level check (every ~30 s), while the level reads high | Read back 0x0C after each write; log raw 0x16 bit 4 together with the medium-check state and write results |
| C. The low flag is polluted by the 3.3 V window (problem 2) | Short low bursts (1-3 readings) every ~30 s, aligned with the level check | Same log as B; already consistent with the first event |
| D. The yellow at power-on is the starting value (problem 1) | Yellow for 30 s after every power-on, then the real value | Certain from the code; confirm by logging from power-on |
| E. Weak supply plus load: the charger supplements from the battery | Low readings only on a computer port while playing; never on a 2 A USB-C charger | Repeat the fast-playing test on a 2 A USB-C charger with C-to-C cable |

## Proposed next steps (not done)

1. **Instrument first** (built 2026-10-12, see "Instrumented build"; not yet run on CHOMPI): a development build that reports the raw registers 0x11-0x16, a read-back
   of 0x0C, the level-check state and each I2C write result; log at 10 Hz through the shut-off
   tests (computer port vs. USB-C charger, synth vs. sampler, idle vs. fast playing).
2. **Then a Forge-owned battery reader** (only if B / C / D are confirmed): one state machine that
   never mixes thresholds (pause the 20 ms reads while the threshold is 3.3 V, or keep separate
   histories per threshold), reads back 0x0C and retries failed writes, starts the level from a
   real check at power-on instead of MEDIUM, and keeps the stock protection's behaviour.
3. **Hardware check for A:** a multimeter on the battery while playing, and the cell's age and
   rating. The MP2722 datasheet's register map (0x0C fields, BATT_LOW deglitch and hysteresis)
   should be checked before changing any write.

## Instrumented build (development only, 2026-10-12; software-tested, not run on CHOMPI yet)

Step 1 of the next steps is built: a development firmware (`make firmware-dev`, never a
release) that reports what the stock reader sees, without changing what it does. The release
build is unchanged (same size, 252,416 B; the instrumentation is compiled only with
`FORGE_TEST_HOOKS`).

**What the firmware does** (`core/battery_diag.h` `BatteryMonitor`, `forge_main.cpp`):

- Every 20 ms check, before the stock read starts, it counts the latest completed reading's
  BATT_LOW bit under the threshold that applied **when that read started**, tracking the stock
  level check from Forge's side (state 0 → 1 = the 3.3 V write, 1 → 2 = the 3.0 V write). So the
  1-3 readings taken in the 3.3 V window (problem 2) are counted apart from the real 3.0 V ones.
  Reads started by the charger hand-over are attributed the same way.
- Right after each stock 0x0C write it reads register 0x0C back (blocking, main loop): the stock
  write's own blocking transmit has already waited for the DMA read started before it, so no
  read is in flight; libDaisy's blocking calls wait for the peripheral to be idle and then time
  out after 5 ms each. It also reads 0x0C back once a second, only when `read_ready` shows the
  last stock read completed. The stock reads re-send the register pointer (0x11) every time, so
  the read-back does not disturb them. Forge does not write 0x0C itself (instrumentation only):
  a failed stock write shows up as a read-back that does not match.
- No audio-callback work, no allocation; the stock lockout and `LockoutGuard` are untouched.

**How to log.** Install the development `FORGE.bin` (`firmware/chompi-forge/src/build-dev/`),
connect CHOMPI by USB data cable, then from `firmware/chompi-forge/host`:

```
D:\tools\forge-venv\Scripts\python.exe forge_battery_log.py --csv battery_log.csv
```

It finds CHOMPI's MIDI ports itself (or pass `--input` / `--output` exact names), asks for
Inspector page 11 ten times a second (`--interval 0.1`) with the ports held open, appends every
answer to the CSV (one row per answer, "no answer" rows when CHOMPI is silent) and prints a line
whenever anything other than the clock and the per-check counters changes. Stop with Ctrl+C
(or `--duration` seconds). Older or release firmware answers with an error, and the tool says so.
Run it through the shut-off tests: computer port vs. USB-C charger, synth vs. sampler, idle vs.
fast playing, and from power-on (problem 1).

**CSV columns** (page 11; wire layout in PROTOCOL.md):

| Column | Meaning |
| --- | --- |
| `time` | the computer's clock when the answer arrived |
| `uptime_ms` | CHOMPI's milliseconds since power-on (restarts = a shut-off or reset) |
| `reg_11` … `reg_16` | registers 0x11-0x16 in hex, as last read by the stock 20 ms read (`mp_buff_`) |
| `dpdm_stat` | 0x11 bits 7-4: USB source type; 0 = not identified yet (the hand-over then runs) |
| `iindpm` | 0x11 bit 0: input at its current limit (weak supply) |
| `vin_gd` | 0x12 bit 6: USB power present |
| `legacy` | 0x12 bit 4: legacy (non USB-C) source |
| `chg_stat` | 0x13 bits 7-5: 0 not charging, 1-4 charging, 5 charge done (as Forge reads it) |
| `fault` | 0x14, the fault byte (0 = none) |
| `batt_low` | 0x16 bit 4: below the threshold currently set in 0x0C |
| `hist_battery_low`, `hist_usb_good`, `hist_legacy`, `hist_input_limit` | the stock 8-reading histories (newest bit on the right); the stock lockout needs `hist_battery_low` = 11111111 |
| `level` | `batt_level`: full / high / medium (SW6 white / green / yellow) |
| `check_state` | the stock level check: 0 idle (3.0 V), 1 3.3 V set, 2 back to 3.0 V, decision due |
| `expect` | the threshold Forge believes is set (3.0V or 3.3V) |
| `read_ready` | 1 = the last stock read completed without an I2C error |
| `r0c_after_33`, `r0c_after_30`, `r0c_periodic` | 0x0C read back right after the 3.3 V write, right after the 3.0 V write, and once a second: `3.0V` (0x51), `3.3V` (0x5d), another value as hex with `?`, `-` = not read yet |
| `checks` | 20 ms checks since power-on |
| `not_ready` | checks with no completed read to count (an I2C error, or a hand-over read just started) |
| `samples_30`, `low_30` | readings taken at 3.0 V, and how many were low |
| `samples_33`, `low_33` | readings taken in the 3.3 V window, and how many were low |
| `low_run_30`, `low_run_30_max` | consecutive low readings at 3.0 V now, and the longest so far (8 = the stock lockout's condition, 150 = Forge's guard) |
| `level_checks` | completed green / yellow decisions (one every ~30 s) |
| `mismatch_33`, `mismatch_30`, `mismatch_periodic` | read-backs that did not show the expected value |
| `readback_errors` | read-backs that failed on I2C |

**Reading the log against the hypotheses:**

- **B (threshold stuck at 3.3 V):** `mismatch_30` or `mismatch_periodic` grows, `r0c_periodic`
  shows `3.3V` while `expect` is 3.0V, and `low_30` climbs in a long run right after a level
  check. Caveat: if 0x0C has read-only or self-clearing bits, every read-back differs from the
  written byte by the same bits (a constant mismatch from power-on, never `3.0V`/`3.3V`): then
  compare the threshold bits against the MP2722 datasheet before concluding.
- **C (3.3 V window pollution):** `low_33` grows by 1-3 every ~30 s while `low_30` stays still;
  the stock `hist_battery_low` shows short bursts aligned with `level_checks`.
- **A (real sag under load):** `low_30` grows with correct read-backs (`3.0V`), in runs while
  playing fast, and more on a weak supply; confirm with a multimeter at the cell.
- **D (yellow at power-on):** `level` = medium with `level_checks` = 0 for the first ~30 s.
- **E (weak supply):** compare `iindpm` / `legacy` / `low_30` between the computer port and a
  2 A USB-C charger.

One more code observation to watch (not yet seen): if the charger reports "charge done" during the
30 ms state-1 window, `batt_level` becomes FULL and the stock check skips itself while FULL (up to
20 min), so 0x0C would stay at 3.3 V that long; the log would show `check_state` 1, `expect`
3.3V and `level` full together.

Related: `docs/forge/TEST_RESULTS.md` (2026-10-11 night entries), `core/power.h`,
`chompi-wave/code/src/hardware.h` (stock, untouched).
