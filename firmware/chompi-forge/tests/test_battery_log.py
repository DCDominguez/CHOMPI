import io
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_host as host
import forge_inspector as inspector
import forge_battery_log as battery
from test_host import probe


class BatteryLogTests(unittest.TestCase):
    def test_register_bits(self):
        r = battery.decode_registers([0x31, 0x50, 0xa0, 0x08, 0xab, 0x10])
        self.assertEqual((r["dpdm_stat"], r["iindpm"], r["vin_gd"], r["legacy"]), (3, 1, 1, 1))
        self.assertEqual((r["chg_stat"], r["charge"], r["fault"], r["batt_low"]), (5, "done", 8, 1))
        self.assertEqual(r["raw"], "31 50 a0 08 ab 10")
        r = battery.decode_registers([0x0e, 0xaf, 0x5f, 0, 0xff, 0xef])     # every other bit set: none of ours
        self.assertEqual((r["dpdm_stat"], r["iindpm"], r["vin_gd"], r["legacy"], r["batt_low"]), (0, 0, 0, 0, 0))
        self.assertEqual((r["chg_stat"], r["charge"]), (2, "charging (2)"))
        self.assertEqual(battery.decode_registers([0, 0, 0xe0, 0, 0, 0])["charge"], "code 7")
        for bad in ([0] * 5, [0, 0, 0, 0, 0, 256], [0, 0, 0, 0, 0, -1]):
            with self.assertRaises(ValueError): battery.decode_registers(bad)
        self.assertEqual([battery.threshold_name(v) for v in (0x51, 0x5d, 0x55, None)], ["3.0V", "3.3V", "0x55?", "-"])

    def test_page_11_from_the_simulated_device(self):
        replies = probe(inspector.request(11, 7), inspector.request(10, 8))
        page = inspector.decode(replies[0], 7, 11)
        self.assertEqual(page["registers"], [0x31, 0x50, 0x20, 0x00, 0xab, 0x10])
        self.assertEqual(page["histories"], {"battery_low": 0xff, "usb_good": 0xff, "legacy": 0xff, "input_limit": 0x0f})
        self.assertEqual((page["level"], page["check_state"], page["read_ready"], page["expect_33"]), ("high", 0, True, False))
        self.assertEqual(page["readback"], {"after_33": 0x5d, "after_30": 0x5d, "periodic": None})
        self.assertEqual({k: page[k] for k in battery.COUNTERS},
                         {"checks": 4, "not_ready": 1, "samples_30": 2, "low_30": 1, "samples_33": 1, "low_33": 1,
                          "low_run_30": 1, "low_run_30_max": 1, "level_checks": 1, "mismatch_33": 0, "mismatch_30": 1,
                          "mismatch_periodic": 0, "readback_errors": 1})
        row = battery.flatten(page)
        self.assertEqual(tuple(row), battery.FIELDS)
        self.assertEqual((row["reg_11"], row["reg_16"], row["batt_low"], row["hist_input_limit"], row["r0c_after_30"], row["expect"]),
                         ("31", "10", 1, "00001111", "3.3V", "3.0V"))
        bad = replies[0].copy(); bad[15] = 2; bad[-1] = host.checksum(bad[:-1])
        with self.assertRaisesRegex(ValueError, "layout"): inspector.decode(bad, 7, 11)
        bad = replies[0].copy(); bad[22] = 2; bad[-1] = host.checksum(bad[:-1])        # register 0x11 = 0x17f
        with self.assertRaisesRegex(ValueError, "register"): inspector.decode(bad, 7, 11)
        with self.assertRaisesRegex(ValueError, "length"): inspector.decode(replies[0][:-2] + [host.checksum(replies[0][:-2])], 7, 11)
        with self.assertRaises(ValueError): inspector.request(12, 1)

    def test_logger_writes_every_reading_and_reports_changes(self):
        page = inspector.decode(probe(inspector.request(11, 3))[0], 3, 11)
        out, lines = io.StringIO(), []
        log = battery.Logger(out, lines.append)
        log.reading("t1", page)
        later = dict(page, uptime_ms=page["uptime_ms"] + 100, checks=page["checks"] + 5, samples_30=9)
        log.reading("t2", later)                                    # only clock and counters: no line
        log.no_answer("t3"); log.no_answer("t4")
        low = dict(later, registers=[0x31, 0x50, 0x20, 0, 0xab, 0x00], low_30=2)
        log.reading("t5", low)
        rows = out.getvalue().splitlines()
        self.assertEqual(len(rows), 6)
        self.assertEqual(rows[0].split(",")[:3], ["time", "uptime_ms", "reg_11"])
        self.assertEqual(rows[3], "t3,no answer")
        self.assertEqual([l.split()[0] for l in lines], ["t1", "t3", "t5", "t5"])
        self.assertIn("NO ANSWER", lines[1]); self.assertIn("answering again", lines[2])
        self.assertIn("BATT_LOW 0", lines[3]); self.assertIn("low@3.0 2/9", lines[3]); self.assertIn("0x0C after 3.3/3.0/now 3.3V/3.3V/-", lines[0])


if __name__ == "__main__":
    unittest.main()
