import copy
import io
import json
from pathlib import Path
import random
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_host as host
import forge_ai
from test_host import probe

class InstrumentTests(unittest.TestCase):
    def setUp(self):
        self.patch = host.load_patch(ROOT / "presets/04-glass-keys.json")

    def test_v2_random_roundtrips(self):
        rng = random.Random(5303); packets = []
        for index in range(200):
            p = copy.deepcopy(self.patch)
            p["routing"] = host.ROUTES[index % 2]
            p["modules"]["synth"] = {"waveform": host.WAVEFORMS[index % 4],
                **{k: rng.uniform(*bounds) for k,bounds in host.SYNTH_LIMITS.items()}}
            packets.append(host.encode_patch(p, index))
        for packet, reply in zip(packets, probe(*packets)):
            self.assertEqual(len(reply), 42)
            self.assertEqual(reply[8:30], packet[7:29])
            captured = host.decode_response(reply, host.read14(packet,5))["patch"]
            self.assertEqual(host.encode_patch(captured, host.read14(packet,5)), packet)

    def test_v2_rejection_is_atomic_and_v1_recall_is_supported(self):
        valid = host.encode_patch(self.patch, 1)
        for offset,value in ((7,3),(17,2),(18,4),(16,2)):
            broken = valid[:]; broken[offset] = value; broken[-1] = host.checksum(broken[:-1])
            before, rejected, after = probe(valid, broken, host.message(2,2))
            self.assertEqual(rejected[4], 0x41)
            self.assertEqual(before[8:30], after[8:30])
        legacy = host.encode_patch(host.load_patch(ROOT / "presets/01-dry.json"),3)
        replies = probe(valid,host.message(3,2),legacy)
        self.assertEqual(replies[0][8:30],replies[1][8:30])
        self.assertEqual(host.decode_response(replies[2],3)["patch"]["version"],1)

    def test_v2_schema_rejects_unsupported_modules_and_nonfinite_values(self):
        for key,value in (("routing","synth>reverb>output"),("version",2.0),("engine","sampler")):
            p=copy.deepcopy(self.patch);p[key]=value
            with self.assertRaises(ValueError): host.validate_patch(p)
        for value in (True, float("nan"),float("inf"),10**1000,0,2001):
            p=copy.deepcopy(self.patch);p["modules"]["synth"]["attack_ms"]=value
            with self.assertRaises(ValueError): host.validate_patch(p)
        p=copy.deepcopy(self.patch);p["modules"]["sampler"]={}
        with self.assertRaises(ValueError): host.validate_patch(p)

    def test_cloud_instrument_schema_and_response(self):
        v3 = host.load_patch(ROOT / "presets/07-warm-pad.json")
        v4 = host.upgrade_patch(v3, 5)
        v4["knobs"] = ["filter.cutoff_hz", "synth.release_ms", "default", "reverb.mix"]
        for provider in ("openai","gemini"):
            content=json.dumps(v4)
            envelope=({"status":"completed","output":[{"type":"message","content":[{"type":"output_text","text":content}]}]}
                if provider == "openai" else {"candidates":[{"finishReason":"STOP","content":{"parts":[{"text":content}]}}]})
            def opener(request,timeout):
                body=json.loads(request.data)
                schema=body["text"]["format"]["schema"] if provider == "openai" else body["generationConfig"]["responseFormat"]["text"]["schema"]
                self.assertEqual(schema["properties"]["version"]["enum"],[5])
                knobs = schema["properties"]["knobs"]
                self.assertEqual((knobs["minItems"], knobs["maxItems"], knobs["items"]["enum"][0]), (4, 4, "default"))
                self.assertIn("filter.cutoff_hz", knobs["items"]["enum"]); self.assertNotIn("delay.bypass", knobs["items"]["enum"])
                self.assertIn("knobs", body["instructions"] if provider == "openai" else body["systemInstruction"]["parts"][0]["text"])
                modules = schema["properties"]["modules"]["properties"]
                self.assertEqual(list(modules), ["synth","filter","lfo","sampler","delay","reverb","output"])
                voices = modules["synth"]["properties"]["voices"]
                self.assertEqual((voices["type"], voices["minimum"], voices["maximum"]), ("integer", 1, 7))
                self.assertIn("between 1 and 7", voices["description"])   # ranges mirrored for strict modes
                return io.BytesIO(json.dumps(envelope).encode())
            self.assertEqual(forge_ai.generate_patch(provider,"fake-key","model","Soft keys",opener,kind="instrument"), v4)
            # v2, v3 and v4 replies are the wrong format for instrument mode now and are refused.
            for old in (self.patch, v3, host.upgrade_patch(v3, 4)):
              content = json.dumps(old)
              envelope=({"status":"completed","output":[{"type":"message","content":[{"type":"output_text","text":content}]}]}
                  if provider == "openai" else {"candidates":[{"finishReason":"STOP","content":{"parts":[{"text":content}]}}]})
              with self.assertRaises(forge_ai.ProviderError):
                  forge_ai.generate_patch(provider,"fake-key","model","Soft keys",lambda r,timeout: io.BytesIO(json.dumps(envelope).encode()),kind="instrument")

    def random_v3(self, rng, index):
        p = host.load_patch(ROOT / "presets/07-warm-pad.json")
        p["routing"] = host.ROUTES3[index % 2]
        for module, fields in host.V3_MODULES.items():
            for key, codec in fields.items():
                if codec[0] in ("lin", "log"):
                    p["modules"][module][key] = rng.choice((codec[1], codec[2], rng.uniform(codec[1], codec[2])))
                elif codec[0] == "enum": p["modules"][module][key] = rng.choice(codec[1])
                elif codec[0] == "int": p["modules"][module][key] = rng.randint(codec[1], codec[2])
                else: p["modules"][module][key] = rng.random() < 0.5
        return host.validate_patch(p)

    def test_v3_random_roundtrips_through_the_firmware_codec(self):
        rng = random.Random(3303); packets = []
        for index in range(200):
            packets.append(host.encode_patch(self.random_v3(rng, index), index))
        self.assertTrue(all(len(packet) == 69 for packet in packets))
        for packet, reply in zip(packets, probe(*packets)):
            self.assertEqual(len(reply), 81)
            self.assertEqual(reply[8:69], packet[7:68])                  # device echoes exactly what was sent
            captured = host.decode_response(reply, host.read14(packet, 5))
            self.assertEqual(captured["firmware"], "0.11")
            self.assertEqual(host.encode_patch(captured["patch"], host.read14(packet, 5)), packet)

    def test_v3_rejection_is_atomic(self):
        valid = host.encode_patch(host.load_patch(ROOT / "presets/08-acid-bass.json"), 1)
        # osc2 waveform, semitone byte, lfo waveform, wheel flag, voices 0 and 5
        for offset, value in ((29, 4), (32, 49), (49, 4), (58, 2), (59, 0), (59, 5)):
            broken = valid[:]; broken[offset] = value; broken[-1] = host.checksum(broken[:-1])
            before, rejected, after = probe(valid, broken, host.message(2, 2))
            self.assertEqual((rejected[4], rejected[7]), (0x41, 4))
            self.assertEqual(before[8:69], after[8:69])
        # v1 and v2 recall still work after a v3 patch, and v3 after them.
        v2 = host.encode_patch(self.patch, 3); v1 = host.encode_patch(host.load_patch(ROOT / "presets/01-dry.json"), 4)
        replies = probe(valid, v2, v1, valid)
        self.assertEqual([len(r) for r in replies], [81, 42, 30, 81])

    def test_v3_schema_is_strict(self):
        good = host.load_patch(ROOT / "presets/09-bell-keys.json")
        cases = [("routing", None, "synth>delay>output"), ("modules", "chorus", {}),
                 (("synth", "voices"), None, 0), (("synth", "voices"), None, 5), (("synth", "voices"), None, 2.0),
                 (("synth", "voices"), None, True), (("synth", "osc2_semitones"), None, 25),
                 (("lfo", "mod_wheel"), None, 1), (("lfo", "waveform"), None, "random"),
                 (("lfo", "rate_hz"), None, 0.01), (("filter", "env_octaves"), None, -6.5),
                 (("reverb", "size"), None, float("nan")), (("filter", "cutoff_hz"), None, 20000)]
        for target, extra, value in cases:
            p = copy.deepcopy(good)
            if isinstance(target, tuple): p["modules"][target[0]][target[1]] = value
            elif extra: p[target][extra] = value
            else: p[target] = value
            with self.subTest(target=target, value=value), self.assertRaises(ValueError):
                host.validate_patch(p)
        p = copy.deepcopy(good); del p["modules"]["reverb"]["damping"]
        with self.assertRaises(ValueError): host.validate_patch(p)
        self.assertEqual(json.loads(json.dumps(host.SCHEMA3))["properties"]["version"]["enum"], [3])

    def test_upgrade_keeps_settings_and_starts_new_modules_neutral(self):
        for name in ("01-dry", "02-slap", "04-glass-keys", "05-soft-pad", "06-saw-bass"):
            old = host.load_patch(ROOT / f"presets/{name}.json")
            new = host.upgrade_patch(old)
            effect = host.effect_patch(old)["parameters"]
            m = new["modules"]
            self.assertEqual(new["version"], 3)
            self.assertEqual({**m["delay"], **m["output"]}, effect)
            self.assertEqual((m["synth"]["osc2_level"], m["synth"]["noise"], m["filter"]["env_octaves"],
                              m["lfo"]["pitch_cents"], m["lfo"]["filter_octaves"], m["lfo"]["amp_depth"],
                              m["reverb"]["mix"], m["synth"]["glide_ms"], m["synth"]["voices"]), (0, 0, 0, 0, 0, 0, 0, 0, 4))
            if old["version"] == 2:
                self.assertEqual(new["routing"], old["routing"].replace(">delay>", ">delay>reverb>"))
                for key in ("waveform", "attack_ms", "decay_ms", "sustain", "release_ms"):
                    self.assertEqual(m["synth"][key], old["modules"]["synth"][key])
                self.assertEqual(m["filter"]["cutoff_hz"], old["modules"]["synth"]["cutoff_hz"])
            else:
                self.assertEqual(new["routing"], "aux>delay>reverb>output")
            self.assertEqual(host.upgrade_patch(new), new)                          # idempotent
            reply, = probe(host.encode_patch(new, 9))                               # the device accepts it
            self.assertEqual(reply[4], 0x40)
        with self.assertRaises(ValueError): host.upgrade_patch({"version": 2})

    def test_upgrade_cli_writes_a_new_file_only(self):
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder) / "up.json"
            host.cli(["upgrade", str(ROOT / "presets/05-soft-pad.json"), str(out)])
            self.assertEqual(host.load_patch(out)["version"], 3)
            with self.assertRaises(FileExistsError):
                host.cli(["upgrade", str(ROOT / "presets/05-soft-pad.json"), str(out)])

if __name__ == "__main__": unittest.main()
