"""Battery reading log (development firmware 0.19.2 or newer, `make firmware-dev`): asks CHOMPI for
Inspector page 11 about ten times a second with the MIDI ports held open, writes every reading to a
CSV file and prints a line whenever something other than the clock and counters changes.

Page 11 holds the raw MP2722 registers 0x11-0x16, the stock 8-reading histories, the level check's
state, read-backs of the BATT_LOW threshold (register 0x0C) and how many 20 ms readings were low
against each threshold. Fields: docs/forge/BATTERY_READINGS.md, "Instrumented build"."""
import argparse
import csv
import math
from pathlib import Path
import sys
import time

import forge_host as host
import forge_inspector as inspector

THRESHOLDS = {0b01010001: "3.0V", 0b01011101: "3.3V"}   # the values the stock code writes to 0x0C
COUNTERS = ("checks", "not_ready", "samples_30", "low_30", "samples_33", "low_33", "low_run_30", "low_run_30_max",
            "level_checks", "mismatch_33", "mismatch_30", "mismatch_periodic", "readback_errors")


def charge_name(stat):
    """CHG_STAT as Forge reads it (core/power.h): 0 not charging, 1-4 charging, 5 charge done."""
    return "not charging" if stat == 0 else f"charging ({stat})" if stat <= 4 else "done" if stat == 5 else f"code {stat}"


def decode_registers(regs):
    """The bits of registers 0x11-0x16 that the stock code and Forge use (others only as raw hex)."""
    if len(regs) != 6 or any(type(v) is not int or not 0 <= v <= 255 for v in regs):
        raise ValueError("Six register bytes (0x11-0x16) expected")
    r11, r12, r13, r14, r15, r16 = regs
    return {"dpdm_stat": r11 >> 4,            # 0x11 bits 7-4: 0 = USB source not identified yet
            "iindpm": r11 & 1,                # 0x11 bit 0: input at its current limit
            "vin_gd": r12 >> 6 & 1,           # 0x12 bit 6: USB power present
            "legacy": r12 >> 4 & 1,           # 0x12 bit 4: legacy (non USB-C) source
            "chg_stat": r13 >> 5 & 7,         # 0x13 bits 7-5
            "charge": charge_name(r13 >> 5 & 7),
            "fault": r14,                     # 0x14: fault byte, 0 = none
            "batt_low": r16 >> 4 & 1,         # 0x16 bit 4: battery below the threshold set in 0x0C
            "raw": " ".join(f"{v:02x}" for v in regs)}


def threshold_name(value):
    return "-" if value is None else THRESHOLDS.get(value, f"0x{value:02x}?")


def flatten(page):
    """One CSV row's fields from a decoded page 11 (forge_inspector.decode)."""
    regs = decode_registers(page["registers"])
    row = {"uptime_ms": page["uptime_ms"]}
    row.update({f"reg_{0x11 + i:02x}": f"{v:02x}" for i, v in enumerate(page["registers"])})
    row.update({k: regs[k] for k in ("dpdm_stat", "iindpm", "vin_gd", "legacy", "chg_stat", "fault", "batt_low")})
    row.update({f"hist_{k}": f"{v:08b}" for k, v in page["histories"].items()})
    row.update({"level": page["level"], "check_state": page["check_state"], "expect": "3.3V" if page["expect_33"] else "3.0V",
                "read_ready": int(page["read_ready"])})
    row.update({f"r0c_{k}": threshold_name(v) for k, v in page["readback"].items()})
    row.update({k: page[k] for k in COUNTERS})
    return row


FIELDS = tuple(flatten({"uptime_ms": 0, "registers": [0] * 6, "histories": dict.fromkeys(("battery_low", "usb_good", "legacy", "input_limit"), 0),
                        "level": "unknown", "check_state": 0, "expect_33": False, "read_ready": False,
                        "readback": dict.fromkeys(("after_33", "after_30", "periodic")), **dict.fromkeys(COUNTERS, 0)}))
# What a change line is about: everything except the clock and the counters that grow every check.
WATCHED = tuple(f for f in FIELDS if f not in ("uptime_ms", "checks", "samples_30", "samples_33", "not_ready", "low_run_30"))


def describe(row):
    """One line for people, e.g. `level high, check 0 (expects 3.0V), BATT_LOW 1, ...`."""
    return (f'level {row["level"]}, check {row["check_state"]} (expects {row["expect"]}), BATT_LOW {row["batt_low"]} '
            f'hist {row["hist_battery_low"]}; USB {row["vin_gd"]} legacy {row["legacy"]} IINDPM {row["iindpm"]} '
            f'DPDM {row["dpdm_stat"]} CHG {row["chg_stat"]} fault {row["fault"]}; '
            f'0x0C after 3.3/3.0/now {row["r0c_after_33"]}/{row["r0c_after_30"]}/{row["r0c_periodic"]}; '
            f'low@3.0 {row["low_30"]}/{row["samples_30"]} (run {row["low_run_30"]}, max {row["low_run_30_max"]}), '
            f'low@3.3 {row["low_33"]}/{row["samples_33"]}; mismatches {row["mismatch_33"]}/{row["mismatch_30"]}/{row["mismatch_periodic"]}, '
            f'read-back errors {row["readback_errors"]}; regs {" ".join(row[f"reg_{r:02x}"] for r in range(0x11, 0x17))}')


class Logger:
    """Writes rows and reports changes; `say` prints a line (tests pass a list's append)."""
    def __init__(self, out, say):
        self.writer = csv.writer(out); self.out = out; self.say = say
        self.writer.writerow(("time", *FIELDS)); self.last = None; self.silent = False

    def reading(self, stamp, page):
        row = flatten(page)
        self.writer.writerow((stamp, *(row[f] for f in FIELDS))); self.out.flush()
        if self.silent: self.say(f"{stamp} answering again"); self.silent = False
        watched = tuple(row[f] for f in WATCHED)
        if watched != self.last:
            self.say(f"{stamp} {describe(row)}"); self.last = watched
        return row

    def no_answer(self, stamp):
        self.writer.writerow((stamp, "no answer")); self.out.flush()
        if not self.silent: self.say(f"{stamp} NO ANSWER from CHOMPI"); self.silent = True


def stamp():
    now = time.time()
    return time.strftime("%H:%M:%S", time.localtime(now)) + f".{int(now * 1000) % 1000:03d}"


def ask(source, destination, midi, sequence, timeout):
    """One page-11 request on open ports; None when CHOMPI does not answer in time."""
    destination.send(midi.Message("sysex", data=inspector.request(11, sequence)))
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for reply in source.iter_pending():
            data = list(reply.data) if reply.type == "sysex" else []
            if len(data) >= 7 and data[:3] == host.PREFIX[:3] and host.read14(data, 5) == sequence:
                try: return inspector.decode(data, sequence, 11)
                except RuntimeError as error:           # an error reply: release build or older firmware
                    raise RuntimeError(f"{error}. The battery page needs a development build of Forge 0.19.2 "
                                       "or newer (make firmware-dev)") from None
        time.sleep(0.002)
    return None


def cli(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input"); parser.add_argument("--output")
    parser.add_argument("--csv", type=Path, help="CSV file to append to (default battery_log_<date-time>.csv here)")
    parser.add_argument("--interval", type=float, default=0.1, help="Seconds between requests (0.05-5, default 0.1)")
    parser.add_argument("--timeout", type=float, default=0.5, help="Seconds to wait for each answer")
    parser.add_argument("--duration", type=float, help="Stop after this many seconds (default: until Ctrl+C)")
    args = parser.parse_args(argv)
    if not math.isfinite(args.interval) or not .05 <= args.interval <= 5: parser.error("Interval must be 0.05-5 seconds")
    if not math.isfinite(args.timeout) or not 0 < args.timeout <= 5: parser.error("Timeout must be 0-5 seconds")
    midi = host.midi_module()
    names = (args.input, args.output) if args.input and args.output else host.find_chompi_ports(midi)
    if names[0] not in midi.get_input_names() or names[1] not in midi.get_output_names():
        raise ValueError("Port not found; run forge_host.py ports and copy the exact input/output names")
    path = args.csv or Path(time.strftime("battery_log_%Y%m%d_%H%M%S.csv"))
    started = time.monotonic(); sequence = 0
    with midi.open_input(names[0]) as source, midi.open_output(names[1]) as destination, \
            path.open("a", newline="", encoding="utf-8") as out:
        log = Logger(out, lambda line: print(line, flush=True))
        print(f"logging to {path}; Ctrl+C to stop", flush=True)
        while args.duration is None or time.monotonic() - started < args.duration:
            begun = time.monotonic(); sequence = (sequence + 1) & 16383
            page = ask(source, destination, midi, sequence, args.timeout)
            if page is None: log.no_answer(stamp())
            else: log.reading(stamp(), page)
            time.sleep(max(0, args.interval - (time.monotonic() - begun)))
    return 0


if __name__ == "__main__":
    try: sys.exit(cli())
    except KeyboardInterrupt: sys.exit(0)
    except (ValueError, RuntimeError, OSError) as e:
        print(f"Forge battery log: {e}", file=sys.stderr); sys.exit(1)
