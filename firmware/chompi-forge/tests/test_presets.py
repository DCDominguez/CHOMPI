"""Device presets (SD card) through the host tools and the C++ runtime (forge_probe)."""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
sys.path.insert(0, str(ROOT / "tests"))
import forge_host as host
import sim_device
from test_host import probe


class PresetProtocolTests(unittest.TestCase):
    def test_messages_use_panel_numbering_and_reject_bad_addresses(self):
        self.assertEqual(host.preset_message(4, 1, 1, 1)[4:10], [4, 1, 0, 0, 0, host.preset_message(4, 1, 1, 1)[-1]])
        self.assertEqual(host.preset_message(5, 1, 8, 15)[7:9], [7, 14])
        self.assertEqual(len(host.preset_message(7, 1)), 8)
        for bank, slot in ((0, 1), (9, 1), (1, 0), (1, 16), (True, 1), (1.0, 1), ("1", 1), (None, None)):
            with self.subTest(bank=bank, slot=slot), self.assertRaises(ValueError):
                host.preset_message(4, 1, bank, slot)

    def test_store_list_recall_erase_through_the_firmware_codec(self):
        warm = host.load_patch(ROOT / "presets/07-warm-pad.json")
        sent = host.encode_patch(warm, 1)
        dry = host.encode_patch(host.load_patch(ROOT / "presets/01-dry.json"), 2)
        requests = [sent, host.preset_message(4, 3, 3, 15), dry, host.preset_message(7, 4),
                    host.preset_message(5, 5, 3, 15), host.preset_message(6, 6, 3, 15),
                    host.preset_message(7, 7), host.preset_message(5, 8, 3, 15)]
        replies = [r for r in probe(*requests)]
        stored = host.decode_response(replies[1], 3)
        self.assertEqual((stored["action"], stored["bank"], stored["slot"]), ("stored", 3, 15))
        self.assertEqual(host.decode_response(replies[3], 4)["occupied"][3], [15])
        recalled = host.decode_response(replies[4], 5)                   # the device is back on Warm Pad
        self.assertEqual(replies[4][8:69], sent[7:68])
        self.assertEqual(recalled["patch"]["version"], 3)
        self.assertEqual(host.decode_response(replies[5], 6)["action"], "erased")
        self.assertEqual(host.decode_response(replies[6], 7)["occupied"], {b: [] for b in range(1, 9)})
        with self.assertRaisesRegex(RuntimeError, "slot is empty"):
            host.decode_response(replies[7], 8)

    def test_every_patch_format_survives_storage(self):
        for name in ("01-dry", "05-soft-pad", "08-acid-bass"):
            payload = host.encode_patch(host.load_patch(ROOT / f"presets/{name}.json"), 1)
            other = host.encode_patch(host.load_patch(ROOT / "presets/09-bell-keys.json"), 2)
            replies = probe(payload, host.preset_message(4, 3, 8, 1), other, host.preset_message(5, 4, 8, 1))
            self.assertEqual(replies[3][8:8 + len(payload) - 8], payload[7:-1], name)

    def test_malformed_preset_replies_are_rejected(self):
        good = probe(host.preset_message(4, 9, 1, 1))[0]
        for broken in (good[:-2] + [good[-1]], good[:8] + [3] + good[9:]):
            broken = broken[:-1] + [host.checksum(broken[:-1])]
            with self.assertRaises(ValueError):
                host.decode_response(broken, 9)


class PresetCliAndWebTests(unittest.TestCase):
    def test_cli_round_trip_with_the_simulated_device(self):
        device = sim_device.SimulatedMido()
        try:
            with patch.object(host, "midi_module", lambda: device), patch("builtins.print") as printed:
                ports = ["--input", sim_device.INPUT, "--output", sim_device.OUTPUT]
                host.cli(["send", str(ROOT / "presets/08-acid-bass.json"), *ports])
                host.cli(["store", "2", "4", *ports])
                host.cli(["slots", *ports])
                listing = json.loads(printed.call_args.args[0])
                self.assertEqual(listing["occupied"]["2"], [4])
                host.cli(["send", str(ROOT / "presets/01-dry.json"), *ports])
                host.cli(["recall", "2", "4", *ports])
                self.assertEqual(json.loads(printed.call_args.args[0])["patch"]["modules"]["synth"]["voices"], 1)
                host.cli(["erase", "2", "4", *ports])
                with self.assertRaisesRegex(RuntimeError, "empty"):
                    host.cli(["recall", "2", "4", *ports])
                with self.assertRaises(ValueError):
                    host.cli(["store", "9", "1", *ports])
        finally:
            device.close()


if __name__ == "__main__":
    unittest.main()
