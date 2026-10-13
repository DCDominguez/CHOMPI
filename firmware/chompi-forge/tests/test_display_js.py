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
        self.assertEqual((got[1]["name"], got[1]["numeral"], got[1]["notes"]), ("Am7", "vi", ["A3", "C4", "E4", "G4"]))

    def test_harmony_and_parts_page_knobs(self):
        # The harmony and parts pages show their own settings (harmony::Pack / parts::PackArp = forge_host's words).
        h = dict(host.HARMONY_DEFAULTS, enabled=True, mode="dorian", extension="9th", inversion=2, open=True, voice_leading=False, layout="real")
        parts = {"arp": {"pattern": "up", "rate": "1/16", "octaves": 3, "gate": 45, "latch": False},
                 "bass": {"mode": "root", "rate": "1/4", "octave": 2}, "clock": {"bpm": 128, "seed": 0, "send_clock": False}}
        arp = host.parts_words(parts)[0]
        words = {"menu": 1 | 7 << 1, "fx_lights": 0, "flags": 0, "harmony_lights": host.harmony_word(h), "parts_lights": arp,
                 "parts_clock": 1 | 128 << 8}
        got = model("console.log(JSON.stringify([d.knobs(input, 'harmony'), d.knobs(input, 'parts')]));", words)
        self.assertEqual([(k["label"], k["text"], k["hint"]) for k in got[0]],
                         [("Mode", "dorian", "harmony on"), ("Chord size", "9th", "real layout"),
                          ("Inversion", "2", "voice leading off"), ("Spread", "open", "")])
        self.assertEqual(got[1][0]["hint"], "tap tempo")
        self.assertEqual([(k["label"], k["text"]) for k in got[1]],
                         [("Tempo", "128 BPM"), ("Arp octaves", "3"), ("Arp gate", "45 %"), ("Bass rate", "1/4")])
        self.assertEqual(got[1][3]["hint"], "bass in C2")

    def test_page_key_labels_follow_the_firmware(self):
        # Every key the firmware gives a job on the parts, synth and FX pages is labelled, and only those (+ C#4 / D#4).
        panel = (ROOT / "core" / "panel_controller.h").read_text(encoding="utf-8")
        fx = (ROOT / "core" / "fx.h").read_text(encoding="utf-8")
        def body(name):
            start = panel.index(f"void {name}(")
            return panel[start:panel.index("\n    }\n", start)]
        parts = {int(n) for n in re.findall(r"case (\d+):", body("PartsKey"))}
        synth = set(range(48, 54)) - {49, 51} | {int(n) for n in re.findall(r"\b(\d{2})\b", body("SynthKey")) if 48 <= int(n) <= 72}
        keys = re.search(r"kKeys\[kModules\]\[kChoices\] = \{(.*?)\};", fx, re.S)
        fx_notes = {int(n) for n in re.findall(r"\d+", keys.group(1))} | {int(re.search(rf"{k} = (\d+)", fx).group(1))
                    for k in ("kRouteKey", "kBeforeLoopKey", "kAfterLoopKey", "kSideKey")}
        got = model("console.log(JSON.stringify(['parts', 'synth', 'fx'].map(p => Object.keys(d.guide(p, {menu: 1, harmony_lights: 0}).keys).map(Number))));", None)
        for page, firmware, js in zip(("parts", "synth", "fx"), (parts, synth, fx_notes), got):
            self.assertEqual(set(js), firmware | {61, 63}, page)

    def test_chords_are_spelled_from_the_key(self):
        # Real chords from CHOMPI (2026-10-13, C major, static layout) and the same moved to F, D and F# major:
        # letters from the key, flats for borrowed roots, a chord's notes counted from its root, lower-case minor.
        word = lambda root, kind, degree, count: root | kind << 7 | degree << 10 | count << 14
        cases = [(0, word(9, 2, 1, 4), [57, 61, 64, 67], "A7", "V/ii", ["A3", "C♯4", "E4", "G4"]),
                 (0, word(10, 5, 6, 3), [58, 62, 65], "B♭", "♭VII", ["B♭3", "D4", "F4"]),
                 (0, word(5, 5, 3, 3), [68, 72, 77], "Fm", "iv", ["A♭4", "C5", "F5"]),
                 (0, word(1, 3, 0, 3), [68, 73, 77], "D♭", "♭II", ["A♭4", "D♭5", "F5"]),
                 (0, word(11, 1, 6, 3), [59, 62, 65], "Bdim", "vii°", ["B3", "D4", "F4"]),
                 (5, word(2, 2, 1, 4), [62, 66, 69, 72], "D7", "V/ii", ["D4", "F♯4", "A4", "C5"]),
                 (2, word(1, 1, 6, 3), [61, 64, 67], "C♯dim", "vii°", ["C♯4", "E4", "G4"]),
                 (6, word(8, 2, 4, 4), [63, 66, 68, 72], "G♯7", "V/V", ["D♯4", "F♯4", "G♯4", "B♯4"])]
        payload = [{"words": {"chord": w, "harmony_lights": tonic | 1 << 16}, "notes": notes + [0] * (5 - len(notes))}
                   for tonic, w, notes, *_ in cases]
        got = model("console.log(JSON.stringify(input.map(c => d.chord(c.words, c.notes))));", payload)
        for case, js in zip(cases, got):
            self.assertEqual((js["name"], js["numeral"], js["notes"]), tuple(case[3:]), case)

    def test_sw5_cutoff_and_loop_speed(self):
        # View flags (src/forge_main.cpp ViewNow): bit 5 SW5 turned, 8-15 cutoff, 16-25 (speed + 2) x 255.
        cases = [1 << 5 | 0 << 8 | 765 << 16, 255 << 8 | 0 << 16, 128 << 8 | 510 << 16, 64 << 8 | 1020 << 16]
        got = model("console.log(JSON.stringify(input.map(f => d.sw5({flags: f}))));", cases)
        self.assertEqual([(g["turned"], g["speed"]) for g in got], [(True, 1), (False, -2), (False, 0), (False, 2)])
        self.assertEqual((got[0]["hz"], got[1]["hz"]), ("≈80 Hz", "≈8.0 kHz"))


if __name__ == "__main__":
    unittest.main()
