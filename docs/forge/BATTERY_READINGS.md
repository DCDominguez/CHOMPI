# CHOMPI battery readings: how they work and why they may mislead (investigation, 2026-10-12)

Open question from the owner: CHOMPI shut off while playing, reads yellow then green minutes
apart, and a "charged" battery behaved like an empty one. Is it the battery, or the way the
battery is read? This page lays out exactly how the readings are made (stock CHOMPI code, which
Forge reuses), what we measured, and the hypotheses with what would prove each. It is written so
a second opinion (a person or another AI) can check it without the rest of the repo.

Status: analysis only. Nothing below is fixed yet. Claims are marked **code** (read in the
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

1. **Instrument first:** a development build that reports the raw registers 0x11-0x16, a read-back
   of 0x0C, the level-check state and each I2C write result; log at 10 Hz through the shut-off
   tests (computer port vs. USB-C charger, synth vs. sampler, idle vs. fast playing).
2. **Then a Forge-owned battery reader** (only if B / C / D are confirmed): one state machine that
   never mixes thresholds (pause the 20 ms reads while the threshold is 3.3 V, or keep separate
   histories per threshold), reads back 0x0C and retries failed writes, starts the level from a
   real check at power-on instead of MEDIUM, and keeps the stock protection's behaviour.
3. **Hardware check for A:** a multimeter on the battery while playing, and the cell's age and
   rating. The MP2722 datasheet's register map (0x0C fields, BATT_LOW deglitch and hysteresis)
   should be checked before changing any write.

Related: `docs/forge/TEST_RESULTS.md` (2026-10-11 night entries), `core/power.h`,
`chompi-wave/code/src/hardware.h` (stock, untouched).
