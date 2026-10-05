"""Sampler (v4 patches, TAPE sample slots) through the host tools and the C++ codec (forge_probe)."""
import copy
import io
import json
from pathlib import Path
import random
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
sys.path.insert(0, str(ROOT / "tests"))
import forge_ai
import forge_host as host
import forge_web
import sim_device
from test_host import probe

WARM = host.load_patch(ROOT / "presets/07-warm-pad.json")


def random_v4(rng, index):
    p = host.upgrade_patch(WARM, 4)
    p["routing"] = host.ROUTES4[index % 3]
    for module, fields in host.V4_MODULES.items():
        for key, codec in fields.items():
            if codec[0] in ("lin", "log"): p["modules"][module][key] = rng.choice((codec[1], codec[2], rng.uniform(codec[1], codec[2])))
            elif codec[0] == "enum": p["modules"][module][key] = rng.choice(codec[1])
            elif codec[0] == "int": p["modules"][module][key] = rng.randint(codec[1], codec[2])
            else: p["modules"][module][key] = rng.random() < 0.5
    s = p["modules"]["sampler"]
    s["start"], s["end"] = sorted(rng.sample(range(0, 1001), 2))
    s["start"], s["end"] = s["start"] / 1000, s["end"] / 1000
    return host.validate_patch(p)


class V4PatchTests(unittest.TestCase):
    def test_random_v4_patches_round_trip_through_the_firmware_codec(self):
        rng = random.Random(4404)
        patches = [random_v4(rng, i) for i in range(150)]
        packets = [host.encode_patch(p, i) for i, p in enumerate(patches)]
        self.assertTrue(all(len(packet) == 84 for packet in packets))
        replies = probe(*packets)
        for i, (patch_, packet, reply) in enumerate(zip(patches, packets, replies)):
            self.assertEqual(reply[8:len(packet)], packet[7:-1])            # device echoes every byte
            decoded = host.decode_response(reply, i)["patch"]
            self.assertEqual(decoded["routing"], patch_["routing"])
            for key, codec in host.V4_SAMPLER:
                a, b = decoded["modules"]["sampler"][key], patch_["modules"]["sampler"][key]
                if codec[0] == "lin": self.assertAlmostEqual(a, b, delta=(codec[2] - codec[1]) / 16383)
                else: self.assertEqual(a, b)
            self.assertEqual(decoded["modules"]["synth"]["voices"], patch_["modules"]["synth"]["voices"])

    def test_validation_and_upgrades(self):
        v4 = host.upgrade_patch(WARM, 4)
        self.assertEqual((v4["version"], v4["modules"]["sampler"], v4["routing"]),
                         (4, host.SAMPLER_DEFAULTS, WARM["routing"]))
        self.assertEqual(host.upgrade_patch(host.load_patch(ROOT / "presets/02-slap.json"), 4)["routing"], "aux>delay>reverb>output")
        self.assertEqual(host.upgrade_patch(host.load_patch(ROOT / "presets/05-soft-pad.json"), 4)["version"], 4)
        with self.assertRaises(ValueError): host.upgrade_patch(v4, 3)
        for change in (("start", 0.5, "end", 0.5), ("slot", 16), ("slot", 0), ("bank", "f"), ("mode", "loop"),
                       ("crossfade_ms", 251), ("hold", 1)):
            bad = copy.deepcopy(v4)
            for key, value in zip(change[::2], change[1::2]): bad["modules"]["sampler"][key] = value
            with self.subTest(change=change), self.assertRaises(ValueError): host.validate_patch(bad)
        bad = copy.deepcopy(v4); bad["modules"]["synth"]["voices"] = 8
        with self.assertRaises(ValueError): host.validate_patch(bad)
        v3 = copy.deepcopy(WARM); v3["modules"]["synth"]["voices"] = 5
        with self.assertRaises(ValueError): host.validate_patch(v3)                  # v3 keeps its 4-voice limit
        v3["modules"]["synth"]["voices"] = 4; v3["routing"] = "sampler>delay>reverb>output"
        with self.assertRaises(ValueError): host.validate_patch(v3)                  # no sampler before v4
        # The CLI writes a v4 file on request.
        with tempfile.TemporaryDirectory() as folder, patch("builtins.print"):
            out = Path(folder) / "warm4.json"
            host.cli(["upgrade", str(ROOT / "presets/07-warm-pad.json"), str(out), "--to", "4"])
            self.assertEqual(host.load_patch(out)["version"], 4)

    def test_sampler_presets_are_v4_and_sound_in_the_harness(self):
        for name in ("11-recorded-keys", "12-tape-kit-a", "13-sampler-stress"):
            p = host.load_patch(ROOT / f"presets/{name}.json")
            self.assertEqual((p["version"], p["routing"]), (4, "sampler>delay>reverb>output"), name)


class SampleMessageTests(unittest.TestCase):
    def test_requests_and_replies(self):
        self.assertEqual(host.sample_message(8, 3), host.message(8, 3))
        self.assertEqual(host.sample_message(9, 3, "copy", "kit", "e", 14, "chromatic", "a", 1)[7:14], [2, 1, 4, 13, 0, 0, 0])
        self.assertEqual(host.sample_message(9, 3, "erase", "chromatic", "b", 2)[7:14], [1, 0, 1, 1, 0, 0, 0])
        for args in (("move", "kit", "a", 1), ("save", "kit", "a", 15), ("save", "kit", "a", 0), ("save", "jam", "a", 1),
                     ("save", "kit", "f", 1), ("save", "kit", "a", 1.0), ("copy", "kit", "a", 1)):
            with self.subTest(args=args), self.assertRaises(ValueError): host.sample_message(9, 1, *args)
        replies = probe(host.sample_message(8, 1), host.sample_message(9, 2, "save", "chromatic", "d", 4),
                        host.sample_message(9, 3, "copy", "kit", "a", 1, "chromatic", "d", 5),
                        host.sample_message(9, 4, "erase", "chromatic", "d", 4), host.sample_message(8, 5),
                        host.sample_message(9, 6, "copy", "kit", "c", 9, "kit", "c", 10))
        listing = host.decode_response(replies[0], 1)
        self.assertEqual(listing["samples"]["kit"]["a"], [1, 2])                     # the harness's simulated card
        self.assertTrue(listing["card"] and listing["recording"] and not listing["busy"])
        self.assertEqual((listing["recording_seconds"], listing["capacity_seconds"]), (1.0, 4.0))
        self.assertEqual(host.decode_response(replies[1], 2), {"sequence": 2, "action": "saved", "mode": "chromatic", "bank": "d", "slot": 4})
        self.assertEqual(host.decode_response(replies[2], 3)["action"], "copied")
        self.assertEqual(host.decode_response(replies[3], 4)["action"], "erased")
        self.assertEqual(host.decode_response(replies[4], 5)["samples"]["chromatic"]["d"], [5])
        with self.assertRaisesRegex(RuntimeError, "empty"): host.decode_response(replies[5], 6)
        # Malformed replies are refused.
        for bad in (replies[0][:-2] + [replies[0][-1]], replies[1][:8] + [3] + replies[1][9:]):
            bad = bad[:-1] + [host.checksum(bad[:-1])]
            with self.assertRaises(ValueError): host.decode_response(bad, bad[5] | bad[6] << 7)

    def test_cli_with_the_simulated_device(self):
        device = sim_device.SimulatedMido()
        try:
            ports = ["--input", sim_device.INPUT, "--output", sim_device.OUTPUT]
            with patch.object(host, "midi_module", lambda: device), patch("builtins.print") as printed:
                host.cli(["samples", *ports]); self.assertTrue(json.loads(printed.call_args.args[0])["recording"])
                host.cli(["sample-save", "kit", "b", "3", *ports])
                host.cli(["sample-copy", "kit", "b", "3", "kit", "b", "4", *ports])
                host.cli(["sample-erase", "kit", "b", "3", *ports])
                host.cli(["samples", *ports]); self.assertEqual(json.loads(printed.call_args.args[0])["samples"]["kit"]["b"], [4])
                # A sampler patch plays from the card in the harness: send it, then status echoes it.
                host.cli(["send", str(ROOT / "presets/12-tape-kit-a.json"), *ports])
                self.assertEqual(json.loads(printed.call_args.args[0])["patch"]["modules"]["sampler"]["mode"], "kit")
                with self.assertRaises(ValueError): host.cli(["sample-save", "kit", "b", "15", *ports])
        finally:
            device.close()


class SampleWebAndAiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = forge_web.ForgeServer(0)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True); cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown(); cls.server.server_close(); cls.thread.join()

    def post(self, path, body):
        import http.client
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=10)
        connection.request("POST", path, body=json.dumps(body), headers={
            "Host": self.server.authority, "Origin": self.server.origin, "Content-Type": "application/json",
            "X-Forge-Token": self.server.token})
        reply = connection.getresponse(); data = json.loads(reply.read()); connection.close()
        return reply.status, data

    def test_samples_api_and_upgrade(self):
        device = sim_device.SimulatedMido()
        try:
            ports = {"input": sim_device.INPUT, "output": sim_device.OUTPUT}
            with patch.object(host, "midi_module", lambda: device):
                status, listing = self.post("/api/samples", {**ports, "action": "list"})
                self.assertEqual((status, listing["samples"]["chromatic"]["a"]), (200, [1]))
                self.assertEqual(self.post("/api/samples", {**ports, "action": "save", "mode": "chromatic", "bank": "e", "slot": 2})[1]["action"], "saved")
                self.assertEqual(self.post("/api/samples", {**ports, "action": "erase", "mode": "chromatic", "bank": "e", "slot": 2})[1]["action"], "erased")
                self.assertEqual(self.post("/api/samples", {**ports, "action": "copy", "mode": "kit", "bank": "a", "slot": 1,
                                                            "to_mode": "kit", "to_bank": "a", "to_slot": 9})[1]["slot"], 9)
                self.assertEqual(self.post("/api/samples", {**ports, "action": "explode"})[0], 400)
                self.assertEqual(self.post("/api/samples", {**ports, "action": "save", "mode": "kit", "bank": "a", "slot": 15})[0], 400)
            status, result = self.post("/api/upgrade", {"patch": WARM, "to": 4})
            self.assertEqual((status, result["patch"]["version"]), (200, 4))
            status, result = self.post("/api/upgrade", {"patch": WARM, "to": 5})
            self.assertEqual((status, result["patch"]["version"], result["patch"]["knobs"]), (200, 5, ["default"] * 4))
            status, result = self.post("/api/upgrade", {"patch": WARM, "to": 6})
            self.assertEqual((status, result["patch"]["version"], result["patch"]["harmony"]), (200, 6, host.HARMONY_DEFAULTS))
            status, result = self.post("/api/upgrade", {"patch": WARM, "to": 7})
            self.assertEqual((status, result["patch"]["version"], result["patch"]["parts"]), (200, 7, host.PARTS_DEFAULTS))
            self.assertEqual(self.post("/api/upgrade", {"patch": WARM, "to": 8})[0], 400)
        finally:
            device.close()

    def test_ai_uses_only_reported_samples(self):
        listing = {"samples": {m: {b: [] for b in host.SAMPLE_BANKS} for m in host.SAMPLE_MODES},
                   "recording": True, "recording_seconds": 2.5}
        listing["samples"]["kit"]["c"] = [1, 2, 5]
        summary = forge_ai.sample_summary(listing)
        self.assertIn("kit bank c: slots 1, 2, 5", summary); self.assertIn("recording (slot 15, chromatic): 2.5 s", summary)
        self.assertIn("No sample list", forge_ai.sample_summary(None))
        v4 = host.upgrade_patch(WARM, 7); v4["routing"] = "sampler>delay>reverb>output"
        def reply(p):
            text = json.dumps(p)
            return lambda request, timeout: io.BytesIO(json.dumps({"status": "completed", "output": [
                {"type": "message", "content": [{"type": "output_text", "text": text}]}]}).encode())
        captured = {}
        def capture(p):
            inner = reply(p)
            def opener(request, timeout):
                captured["system"] = json.loads(request.data)["instructions"]; return inner(request, timeout)
            return opener
        ok = copy.deepcopy(v4); ok["modules"]["sampler"].update(mode="kit", bank="c")
        self.assertEqual(forge_ai.generate_patch("openai", "k", "m", "drums", capture(ok), kind="instrument", samples=listing), ok)
        self.assertIn("kit bank c: slots 1, 2, 5", captured["system"])
        rec = copy.deepcopy(v4); rec["modules"]["sampler"].update(mode="chromatic", slot=15)
        self.assertEqual(forge_ai.generate_patch("openai", "k", "m", "keys", reply(rec), kind="instrument", samples=listing), rec)
        for bad in ({"mode": "kit", "bank": "a"}, {"mode": "chromatic", "bank": "c", "slot": 3}):
            wrong = copy.deepcopy(v4); wrong["modules"]["sampler"].update(bad)
            with self.subTest(bad=bad), self.assertRaises(forge_ai.ProviderError):
                forge_ai.generate_patch("openai", "k", "m", "x", reply(wrong), kind="instrument", samples=listing)
        # The web bridge checks the echoed list's shape before it reaches a prompt.
        for samples in ({"samples": {}}, {**listing, "recording_seconds": "x"}, {**listing, "samples": {**listing["samples"], "kit": {"a": [99]}}}):
            status, _ = self.post("/api/generate", {"provider": "openai", "api_key": "k", "model": "m", "prompt": "x",
                                                    "kind": "instrument", "samples": samples})
            self.assertEqual(status, 400)


if __name__ == "__main__":
    unittest.main()
