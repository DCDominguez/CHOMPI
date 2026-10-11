"""The browser's wire format (docs/send/forge-wire.js, the GitHub Pages send page) against forge_host:
byte-for-byte patch encoding for every preset and random v7 patches, the same refusals, and replies from
the firmware's own decoder (build/forge_probe via sim_device) decoded alike. Needs Node.js (skips without)."""
import copy
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host")); sys.path.insert(0, str(ROOT / "tests"))
import forge_host as host

WIRE = ROOT.parents[1] / "docs" / "send" / "forge-wire.js"
NODE = os.environ.get("FORGE_NODE") or shutil.which("node") or next(
    (str(p) for p in (Path.home() / "AppData/Local/hermes/node/node.exe",) if p.exists()), None)


def node(script, payload):
    with tempfile.TemporaryDirectory() as folder:
        data = Path(folder) / "in.json"; data.write_text(json.dumps(payload))
        runner = Path(folder) / "run.js"
        runner.write_text(f"const w = require({json.dumps(str(WIRE))}); const input = require({json.dumps(str(data))});\n{script}")
        out = subprocess.run([NODE, str(runner)], capture_output=True, text=True, timeout=60)
        if out.returncode: raise AssertionError(out.stderr)
        return json.loads(out.stdout)


def random_patch(r, base):
    p = copy.deepcopy(base); m = p["modules"]
    def pick(codec, edge):
        if codec[0] == "enum": return r.choice(codec[1])
        if codec[0] == "bool": return r.random() < .5
        if codec[0] == "int": return r.choice((codec[1], codec[2])) if edge else r.randint(codec[1], codec[2])
        lo, hi = codec[1], codec[2]
        return r.choice((lo, hi)) if edge else (lo * (hi / lo) ** r.random() if codec[0] == "log" else r.uniform(lo, hi))
    for module, fields in host.V4_MODULES.items():
        for key, codec in fields.items():
            if module == "synth" and key == "voices": codec = ("int", 1, 7, 0)
            m[module][key] = pick(codec, r.random() < .15)
    m["sampler"]["start"], m["sampler"]["end"] = sorted(r.sample([0, .1, .33, .5, .77, 1], 2))
    p["routing"] = r.choice(host.ROUTES4)
    p["knobs"] = [r.choice(host.KNOB_CHOICES) for _ in range(4)]
    h = p["harmony"]
    h.update(enabled=r.random() < .5, tonic=r.choice(host.HARMONY_TONICS), mode=r.choice(host.HARMONY_MODES),
             extension=r.choice(host.HARMONY_EXTENSIONS), inversion=r.randint(0, 3), open=r.random() < .5,
             voice_leading=r.random() < .5, layout=r.choice(("static", "real")))
    a, b, c = p["parts"]["arp"], p["parts"]["bass"], p["parts"]["clock"]
    a.update(pattern=r.choice(host.PARTS_PATTERNS), rate=r.choice(host.PARTS_RATES), octaves=r.randint(1, 4), gate=5 * r.randint(1, 20), latch=r.random() < .5)
    b.update(mode=r.choice(host.BASS_MODES), rate=r.choice(host.BASS_RATES), octave=r.randint(1, 3))
    c.update(bpm=r.randint(40, 300), seed=r.randint(0, 2047), send_clock=r.random() < .5)
    return host.validate_patch(p)


@unittest.skipIf(NODE is None, "Node.js not found (set FORGE_NODE)")
class WireJsTests(unittest.TestCase):
    def test_encoding_matches_python_byte_for_byte(self):
        presets = [host.upgrade_patch(host.load_patch(path), 7) for path in sorted((ROOT / "presets").glob("*.json"))]
        r = random.Random(20261012)
        patches = presets + [random_patch(r, presets[0]) for _ in range(300)]
        cases = [{"patch": p, "seq": r.randint(0, 16383)} for p in patches]
        got = node("console.log(JSON.stringify(input.map(c => w.encodePatch(c.patch, c.seq))));", cases)
        for case, js in zip(cases, got):
            self.assertEqual(js, host.encode_patch(case["patch"], case["seq"]), case["patch"]["name"])
        self.assertEqual(len(got), len(presets) + 300)

    def test_the_same_sounds_are_refused(self):
        base = host.upgrade_patch(host.load_patch(ROOT / "presets" / "08-acid-bass.json"), 7)
        bad = []
        for path, value in ((("modules", "synth", "attack_ms"), 5000), (("modules", "sampler", "start"), 1),
                            (("parts", "arp", "gate"), 7), (("knobs",), ["nope"] * 4), (("version",), 6),
                            (("modules", "filter", "cutoff_hz"), 10), (("routing",), "synth>output")):
            p = copy.deepcopy(base); node_ = p
            for key in path[:-1]: node_ = node_[key]
            node_[path[-1]] = value
            with self.assertRaises((ValueError, KeyError, TypeError)): host.validate_patch(p) if path != ("version",) else host.encode_patch({**p, "harmony": None}, 1)
            bad.append(p)
        extra = copy.deepcopy(base); extra["modules"]["synth"]["extra"] = 1; bad.append(extra)
        refused = node("console.log(JSON.stringify(input.map(p => { try { w.encodePatch(p, 1); return false; } catch (e) { return true; } })));", bad)
        self.assertEqual(refused, [True] * len(bad))

    def test_chatbot_replies_and_files_are_read(self):
        base = host.upgrade_patch(host.load_patch(ROOT / "presets" / "08-acid-bass.json"), 7)
        texts = ["Here!\n```json\n" + json.dumps(base) + "\n```\nenjoy", json.dumps(base), "nope"]
        got = node("console.log(JSON.stringify(input.map(t => { try { return w.readSound(t).name; } catch (e) { return null; } })));", texts)
        self.assertEqual(got, [base["name"], base["name"], None])

    @unittest.skipUnless((ROOT / "build" / ("forge_probe.exe" if os.name == "nt" else "forge_probe")).exists(), "build/forge_probe not built")
    def test_replies_from_the_firmware_decoder(self):
        import sim_device
        device = sim_device.SimulatedMido()
        try:
            patch = host.upgrade_patch(host.load_patch(ROOT / "presets" / "08-acid-bass.json"), 7)
            cases = []
            for sent in (host.encode_patch(patch, 77), host.message(2, 78), host.message(3, 79)):
                device.handle(sim_device.Message("sysex", sent))
                reply = list(device.replies.popleft().data)
                cases.append({"sent": sent, "reply": reply, "python": host.decode_response(reply, host.read14(sent, 5))})
            got = node("console.log(JSON.stringify(input.map(c => w.decodeReply(c.reply, c.sent))));", cases)
            for case, js in zip(cases, got):
                py = case["python"]
                self.assertEqual(js["firmware"], py["firmware"]); self.assertEqual(js["build"], py["build"])
                self.assertEqual(js["patchVersion"], py["patch"]["version"])
                if py["power"]: self.assertEqual(js["power"]["battery"], py["power"]["battery"])
            wrong = [dict(cases[0], sent=host.encode_patch({**patch, "name": "x", "modules": {**patch["modules"], "delay": {**patch["modules"]["delay"], "mix": .123}}}, 77))]
            self.assertEqual(node("console.log(JSON.stringify(input.map(c => { try { w.decodeReply(c.reply, c.sent); return 'ok'; } catch (e) { return 'refused'; } })));", wrong), ["refused"])
        finally:
            device.close()


if __name__ == "__main__":
    unittest.main()
