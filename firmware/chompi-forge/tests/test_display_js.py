"""The phone display (docs/workshop/display-model.js, forge-wire.js's view decoding) against the firmware and
forge_host: view replies decoded alike, the knob names in core/parameters.h's order, chord names as the
Inspector's chord_label, the menu page from PresetMenu::Packed. Needs Node.js (skips without)."""
import json
from pathlib import Path
import random
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host")); sys.path.insert(0, str(ROOT / "tests"))
import forge_host as host
import forge_inspector
from test_wire_js import NODE, node as wire_node

MODEL = ROOT.parents[1] / "docs" / "workshop" / "display-model.js"


def model(script, payload):
    return wire_node(f"const d = require({json.dumps(str(MODEL))});\n{script}", payload)


def view_reply(sequence, page, r):
    data = [0, page]
    if page == 0: data += [r.randint(0, 127) for _ in range(3 * host.VIEW_LEDS)]
    else: data += [r.randint(0, 127) for _ in range(5 * len(host.VIEW_WORDS) + 8)]
    return host.message(0x49, sequence, data)


@unittest.skipIf(NODE is None, "Node.js not found (set FORGE_NODE)")
class DisplayJsTests(unittest.TestCase):
    def test_view_replies_decode_alike(self):
        r = random.Random(13)
        cases = []
        for i in range(40):
            page = i % 2; sent = host.view_message(i, page)
            cases.append({"sent": sent, "reply": view_reply(i, page, r)})
        got = wire_node("console.log(JSON.stringify(input.map(c => w.decodeReply(c.reply, c.sent))));", cases)
        for case, js in zip(cases, got):
            py = host.decode_response(case["reply"], host.read14(case["sent"], 5))
            if py["page"] == 0: self.assertEqual(js["leds"], py["leds"])
            else:
                self.assertEqual(js["words"], py["words"]); self.assertEqual(js["chordNotes"], py["chord_notes"])
                self.assertEqual((js["power"]["battery"], js["power"]["chargeState"]), (py["power"]["battery"], py["power"]["charge_state"]))
        self.assertEqual(wire_node("console.log(JSON.stringify([w.viewMessage(7, 0), w.viewMessage(8, 1)]));", None),
                         [host.view_message(7, 0), host.view_message(8, 1)])

    def test_knob_names_follow_the_parameter_enum(self):
        source = (ROOT / "core" / "parameters.h").read_text(encoding="utf-8")
        body = re.search(r"enum class Parameter : uint8_t \{(.*?)\};", source, re.S).group(1)
        body = re.sub(r"//[^\n]*", "", body)
        names = [n.strip() for n in body.split(",") if n.strip()]
        self.assertEqual(names[-1], "Count")
        self.assertEqual(len(model("console.log(JSON.stringify(d.PARAMETERS));", None)), len(names) - 1)

    def test_chord_names_match_the_inspector(self):
        r = random.Random(20261013); cases = []
        for _ in range(400):
            root = r.randrange(12); count = r.randint(1, 5)
            notes = sorted(r.sample(range(36, 90), count))
            cases.append({"root": root, "notes": notes})
        got = model("console.log(JSON.stringify(input.map(c => d.chordName(c.root, c.notes))));", cases)
        for case, js in zip(cases, got):
            py = forge_inspector.chord_label(case["root"], 0, 1, 0, False, case["notes"], {})["name"]
            self.assertEqual(js, py, case)

    def test_pages_from_the_menu_word(self):
        words = lambda menu, fx=0, flags=0: {"menu": menu, "fx_lights": fx, "flags": flags}
        cases = [words(0), words(0, flags=1), words(1), words(1 | 1 << 21), words(1 | 7 << 1), words(1 | 6 << 1),
                 words(1 | 5 << 1), words(1 | 5 << 1, fx=2 ** 31), words(1 | 2 << 1)]
        self.assertEqual(model("console.log(JSON.stringify(input.map(w => d.page(w))));", cases),
                         ["play", "record", "presets", "samples", "harmony", "parts", "synth", "fx", "presets"])

    def test_looper_and_chord_words(self):
        looper = 3 | 8 | 32 | 512 << 6 | 750 << 16                  # playing, overdub, a loop, half way, 7.5 s
        chord = 9 | 2 << 4 | 1 << 7 | 5 << 10 | 4 << 14 | 3 << 17   # A minor, diatonic vi, four notes
        got = model("console.log(JSON.stringify([d.looper({looper: input[0]}), d.chord({chord: input[1]}, [57, 60, 64, 67, 0])]));", [looper, chord])
        self.assertEqual((got[0]["state"], got[0]["overdub"], got[0]["seconds"]), ("playing", True, 7.5))
        self.assertAlmostEqual(got[0]["position"], 512 / 1023, places=4)
        self.assertEqual((got[1]["name"], got[1]["numeral"], got[1]["notes"]), ("Am7", "VI", ["A3", "C4", "E4", "G4"]))


if __name__ == "__main__":
    unittest.main()
