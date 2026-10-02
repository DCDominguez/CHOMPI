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
        for provider in ("openai","gemini"):
            content=json.dumps(self.patch)
            envelope=({"status":"completed","output":[{"type":"message","content":[{"type":"output_text","text":content}]}]}
                if provider == "openai" else {"candidates":[{"finishReason":"STOP","content":{"parts":[{"text":content}]}}]})
            def opener(request,timeout):
                body=json.loads(request.data)
                schema=body["text"]["format"]["schema"] if provider == "openai" else body["generationConfig"]["responseFormat"]["text"]["schema"]
                self.assertEqual(schema["properties"]["version"]["enum"],[2])
                self.assertIn("modules",schema["properties"])
                return io.BytesIO(json.dumps(envelope).encode())
            self.assertEqual(forge_ai.generate_patch(provider,"fake-key","model","Soft keys",opener,kind="instrument"), self.patch)

if __name__ == "__main__": unittest.main()
