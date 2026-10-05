import copy
import io
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_host as host


def envelope(payload):
    return bytes([0xF0, *payload, 0xF7]).hex(" ")


def probe(*payloads, render=None):
    args = [str(ROOT / "build" / "forge_probe")]
    if render:
        args += ["--render", str(render)]
    result = subprocess.run(args, input=" ".join(envelope(p) for p in payloads),
                            capture_output=True, text=True, check=True, timeout=10)
    return [list(bytes.fromhex(line)) for line in result.stdout.splitlines()]


class PatchTests(unittest.TestCase):
    def setUp(self):
        self.patch = host.load_patch(ROOT / "presets" / "03-long-echo.json")

    def test_presets_round_trip_cpp(self):
        for path in (ROOT / "presets").glob("*.json"):
            patch = host.load_patch(path)
            packet = host.encode_patch(patch, 129)
            response = probe(packet)[0]
            self.assertEqual(response[8:len(packet)], packet[7:-1])
            result = host.decode_response(response, 129)
            self.assertEqual(result["firmware"], "0.15")
            self.assertAlmostEqual(host.effect_patch(result["patch"])["parameters"]["time_ms"],
                                   host.effect_patch(patch)["parameters"]["time_ms"], delta=990 / 16383)

    def test_v6_harmony_round_trip_cpp(self):
        patch = host.upgrade_patch(host.load_patch(ROOT / "presets" / "07-warm-pad.json"), 6)
        self.assertEqual(patch["harmony"], host.HARMONY_DEFAULTS)
        cases = (dict(host.HARMONY_DEFAULTS),
                 {"enabled": True, "tonic": "A", "mode": "natural_minor", "extension": "7th", "inversion": 1,
                  "open": True, "voice_leading": False, "layout": "real"},
                 {"enabled": True, "tonic": "B", "mode": host.HARMONY_MODES[-1], "extension": "fifth", "inversion": 3,
                  "open": False, "voice_leading": True, "layout": "static"})
        for harmony in cases:
            patch["harmony"] = harmony
            packet = host.encode_patch(patch, 91)
            self.assertEqual(len(packet), 91)
            response = probe(packet)[0]
            self.assertEqual(len(response), 103)
            self.assertEqual(response[8:len(packet)], packet[7:-1])
            self.assertEqual(host.decode_response(response, 91)["patch"]["harmony"], harmony)
        for bad in ({**host.HARMONY_DEFAULTS, "tonic": "H"}, {**host.HARMONY_DEFAULTS, "inversion": 4},
                    {**host.HARMONY_DEFAULTS, "mode": "ionian"}, {k: v for k, v in host.HARMONY_DEFAULTS.items() if k != "open"}):
            with self.assertRaises(ValueError): host.validate_patch({**patch, "harmony": bad})
        with self.assertRaises(ValueError): host.validate_patch({**patch, "version": 5})
        # The device rejects an out-of-range harmony word (tonic 12) even with a valid checksum.
        broken = host.encode_patch(patch, 92); broken[87] = 12; broken[-1] = host.checksum(broken[:-1])
        with self.assertRaisesRegex(RuntimeError, "invalid patch"): host.decode_response(probe(broken)[0], 92)

    def test_v7_parts_round_trip_cpp(self):
        patch = host.upgrade_patch(host.load_patch(ROOT / "presets" / "07-warm-pad.json"), 7)
        self.assertEqual(patch["parts"], host.PARTS_DEFAULTS)
        cases = (host.PARTS_DEFAULTS,
                 {"arp": {"pattern": "random", "rate": "1/16t", "octaves": 4, "gate": 100, "latch": False},
                  "bass": {"mode": "alternate", "rate": "chord", "octave": 1},
                  "clock": {"bpm": 300, "seed": 2047, "send_clock": False}},
                 {"arp": {"pattern": "updown", "rate": "1/32", "octaves": 2, "gate": 5, "latch": True},
                  "bass": {"mode": "fifth", "rate": "1/8", "octave": 3},
                  "clock": {"bpm": 40, "seed": 0, "send_clock": True}})
        for parts in cases:
            patch["parts"] = parts
            patch["harmony"] = {**host.HARMONY_DEFAULTS, "enabled": True, "tonic": "D"}
            packet = host.encode_patch(patch, 93)
            self.assertEqual(len(packet), 97)
            response = probe(packet)[0]
            self.assertEqual(len(response), 109)
            self.assertEqual(response[8:len(packet)], packet[7:-1])
            decoded = host.decode_response(response, 93)["patch"]
            self.assertEqual((decoded["parts"], decoded["harmony"]), (parts, patch["harmony"]))
        bad_parts = (
            {**host.PARTS_DEFAULTS, "arp": {**host.PARTS_DEFAULTS["arp"], "gate": 37}},
            {**host.PARTS_DEFAULTS, "arp": {**host.PARTS_DEFAULTS["arp"], "pattern": "sideways"}},
            {**host.PARTS_DEFAULTS, "bass": {**host.PARTS_DEFAULTS["bass"], "octave": 4}},
            {**host.PARTS_DEFAULTS, "clock": {**host.PARTS_DEFAULTS["clock"], "bpm": 301}},
            {**host.PARTS_DEFAULTS, "clock": {**host.PARTS_DEFAULTS["clock"], "seed": True}},
            {k: v for k, v in host.PARTS_DEFAULTS.items() if k != "bass"})
        for bad in bad_parts:
            with self.assertRaises(ValueError): host.validate_patch({**patch, "parts": bad})
        with self.assertRaises(ValueError): host.validate_patch({k: v for k, v in patch.items() if k != "harmony"})
        with self.assertRaises(ValueError): host.validate_patch({**patch, "version": 6})
        # The device refuses a tempo below 40 even with a valid checksum (request 93-95 = clock word).
        broken = host.encode_patch(patch, 94); broken[93] = 39; broken[94] = 0; broken[-1] = host.checksum(broken[:-1])
        with self.assertRaisesRegex(RuntimeError, "invalid patch"): host.decode_response(probe(broken)[0], 94)
        # A v6 patch stays v6 in status while the parts are off; turning the arp on reports v7.
        v6 = host.upgrade_patch(host.load_patch(ROOT / "presets" / "07-warm-pad.json"), 6)
        self.assertEqual(host.decode_response(probe(host.encode_patch(v6, 95), host.message(2, 96, []))[1], 96)["patch"]["version"], 6)

    def test_v5_knob_assignments_round_trip_cpp(self):
        patch = host.upgrade_patch(host.load_patch(ROOT / "presets" / "07-warm-pad.json"), 5)
        for knobs in (["default"] * 4, ["filter.cutoff_hz", "synth.attack_ms", "sampler.crossfade_ms", "output.level"],
                      [*list(host.KNOB_TARGETS)[-4:]]):
            patch["knobs"] = knobs
            packet = host.encode_patch(patch, 77)
            self.assertEqual(len(packet), 88)
            response = probe(packet)[0]
            self.assertEqual(len(response), 100)
            self.assertEqual(response[8:len(packet)], packet[7:-1])
            self.assertEqual(host.decode_response(response, 77)["patch"]["knobs"], knobs)
        for bad in (["delay.bypass"] + ["default"] * 3, ["default"] * 3, "default", [None] * 4):
            with self.assertRaises(ValueError): host.validate_patch({**patch, "knobs": bad})
        v4 = {k: v for k, v in patch.items() if k != "knobs"}
        with self.assertRaises(ValueError): host.validate_patch({**v4, "version": 4, "knobs": ["default"] * 4})
        with self.assertRaises(ValueError): host.validate_patch({**v4, "version": 5})
        # Every host name maps to the firmware id the device accepts; the probe rejects an unknown byte.
        broken = host.encode_patch(patch, 78); broken[84] = 5; broken[-1] = host.checksum(broken[:-1])
        with self.assertRaisesRegex(RuntimeError, "invalid patch"): host.decode_response(probe(broken)[0], 78)
        self.assertEqual(host.knob_control(patch, 1), "delay.mix" if patch["knobs"][0] == "default" else patch["knobs"][0])
        sampler = {**patch, "routing": "sampler>delay>reverb>output", "knobs": ["default"] * 4}
        self.assertEqual([host.knob_control(sampler, k) for k in range(1, 5)], list(host.DEFAULT_KNOBS["sampler"]))
        self.assertEqual(host.knob_control(sampler, 3, 4), "sampler.crossfade_ms")
        self.assertEqual(host.knob_control(patch, 3, 4), "synth.osc2_detune_cents")
        self.assertEqual([host.knob_control(sampler, k, 1) for k in range(1, 5)],
                         ["tape.speed", "sampler.start", "sampler.end", "tape.space"])          # TAPE's page 1
        self.assertEqual(host.knob_pages(sampler, 3), 4); self.assertEqual(host.knob_pages(sampler, 4), 3)

    def test_random_patch_round_trips(self):
        rng = random.Random(481)
        packets = []
        for i in range(250):
            patch = copy.deepcopy(self.patch)
            patch["parameters"] = {key: rng.uniform(low, high) for key, (low, high) in host.LIMITS.items()}
            patch["parameters"]["bypass"] = bool(i % 2)
            packets.append(host.encode_patch(patch, i))
        responses = probe(*packets)
        self.assertEqual(len(responses), len(packets))
        for packet, response in zip(packets, responses):
            self.assertEqual(response[8:len(packet)], packet[7:-1])

    def test_invalid_patch_does_not_replace_active_state(self):
        valid = host.encode_patch(self.patch, 1)
        broken = host.encode_patch(self.patch, 2)
        broken[16] = 2  # checksum-valid malformed bypass
        broken[-1] = host.checksum(broken[:-1])
        first, rejected, after = probe(valid, broken, host.message(2, 3))
        with self.assertRaisesRegex(RuntimeError, "invalid patch"):
            host.decode_response(rejected, 2)
        self.assertEqual(first[8:18], after[8:18])

    def test_schema_strictness(self):
        cases = [True, "0.5", float("nan"), float("inf"), 10**1000, -1, 1.1]
        for value in cases:
            patch = copy.deepcopy(self.patch)
            patch["parameters"]["mix"] = value
            with self.subTest(value=str(value)[:30]), self.assertRaises(ValueError):
                host.validate_patch(patch)
        for field, value in [("version", True), ("version", 2), ("engine", "reverb"), ("name", " ")]:
            patch = copy.deepcopy(self.patch); patch[field] = value
            with self.assertRaises(ValueError): host.validate_patch(patch)
        for text in ['{"x":1,"x":2}', '{"x":NaN}', '{"x":Infinity}', " " * 65537]:
            with self.assertRaises(ValueError): host.parse_json(text)

    def test_save_capture_and_overwrite_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "patch.json"
            host.save_patch(self.patch, path)
            self.assertEqual(host.load_patch(path), self.patch)
            with self.assertRaises(FileExistsError): host.save_patch(self.patch, path)
            captured = host.decode_response(probe(host.encode_patch(self.patch, 3))[0], 3)["patch"]
            host.save_patch(captured, path, overwrite=True)
            self.assertEqual(host.load_patch(path), captured)

    def test_ollama_request_and_validation(self):
        def fake_open(request, timeout):
            body = json.loads(request.data)
            self.assertEqual(body["format"], host.SCHEMA)
            self.assertFalse(body["stream"])
            self.assertEqual(timeout, 90)
            return io.BytesIO(json.dumps({"message": {"content": json.dumps(self.patch)}}).encode())
        self.assertEqual(host.generate_patch("Long slow echoes", "test-model", opener=fake_open), self.patch)
        def malformed(request, timeout):
            return io.BytesIO(b'{"message":{"content":"{\\"engine\\":\\"reverb\\"}"}}')
        with self.assertRaises(ValueError): host.generate_patch("reverb", "test", opener=malformed)
        with self.assertRaises(ValueError): host.generate_patch("echo", "test", "http://example.com/api/chat")

    def test_offline_audio_render(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "echo.wav"
            probe(host.encode_patch(self.patch, 1), render=path)
            with wave.open(str(path)) as audio:
                self.assertEqual((audio.getframerate(), audio.getnchannels(), audio.getsampwidth(), audio.getnframes()),
                                 (48000, 2, 2, 192000))
                self.assertNotEqual(set(audio.readframes(192000)), {0})

    def test_response_rejection(self):
        good = probe(host.encode_patch(self.patch, 17))[0]
        with self.assertRaisesRegex(ValueError, "sequence"): host.decode_response(good, 18)
        broken = good.copy(); broken[10] ^= 1
        with self.assertRaisesRegex(ValueError, "checksum"): host.decode_response(broken, 17)
        with self.assertRaises(ValueError): host.decode_response(good[:-1], 17)


class FakePort:
    def __init__(self, midi, is_input): self.midi, self.is_input = midi, is_input
    def __enter__(self): return self
    def __exit__(self, *args): pass
    def send(self, message):
        self.midi.sent += 1
        if self.midi.respond:
            data = probe(message.data)[0]
            if self.midi.mismatch:
                data[9] ^= 1; data[-1] = host.checksum(data[:-1])
            self.midi.replies.append(self.midi.Message("sysex", data))
    def receive(self, block=False):
        return self.midi.replies.pop(0) if self.midi.replies else None


class FakeMidi:
    class Message:
        def __init__(self, kind, data): self.type, self.data = kind, data
    def __init__(self, respond=True, mismatch=False):
        self.respond, self.mismatch = respond, mismatch
        self.replies, self.sent = [], 0
    def get_input_names(self): return ["Forge input"]
    def get_output_names(self): return ["Forge output"]
    def open_input(self, name): return FakePort(self, True)
    def open_output(self, name): return FakePort(self, False)


class TransportTests(unittest.TestCase):
    def test_acknowledged_exchange(self):
        midi = FakeMidi()
        patch = host.load_patch(ROOT / "presets" / "02-slap.json")
        result = host.exchange(host.encode_patch(patch, 9), "Forge input", "Forge output", midi=midi)
        self.assertEqual(result["sequence"], 9)
        self.assertEqual(midi.sent, 1)

    def test_no_ack_is_not_reported_as_success(self):
        with self.assertRaisesRegex(TimeoutError, "may have applied"):
            host.exchange(host.message(2, 9), "Forge input", "Forge output", timeout=0.01, midi=FakeMidi(False))

    def test_mismatched_ack_is_not_success(self):
        patch = host.load_patch(ROOT / "presets" / "02-slap.json")
        with self.assertRaisesRegex(RuntimeError, "does not match"):
            host.exchange(host.encode_patch(patch, 9), "Forge input", "Forge output", midi=FakeMidi(mismatch=True))

    def test_explicit_port_selection(self):
        midi = FakeMidi()
        with self.assertRaisesRegex(ValueError, "Port not found"):
            host.exchange(host.message(2, 1), "Other synth", "Forge output", midi=midi)
        self.assertEqual(midi.sent, 0)


if __name__ == "__main__": unittest.main()


class FakeMido:
    """Records channel messages; enough of mido for play()."""
    def __init__(self): self.sent = []
    def get_output_names(self): return ["out"]
    def Message(self, kind, **fields): return (kind, fields)
    def open_output(self, name):
        fake = self
        class Port:
            def __enter__(self): return self
            def __exit__(self, *exc): return False
            def send(self, message): fake.sent.append(message)
        return Port()


class TestTrafficCommands(unittest.TestCase):
    def test_cc_and_chord_with_both_release_styles(self):
        midi = FakeMido()
        host.play("out", cc=(24, 127), midi=midi)
        self.assertEqual(midi.sent, [("control_change", {"channel": 0, "control": 24, "value": 127})])
        for zero, kind in ((False, "note_off"), (True, "note_on")):
            midi = FakeMido()
            host.play("out", notes=[60, 64, 67, 71, 74], hold=0, zero_velocity_off=zero, midi=midi, sleep=lambda s: None)
            self.assertEqual([m[1]["note"] for m in midi.sent[:5]], [60, 64, 67, 71, 74])
            self.assertTrue(all(m[0] == "note_on" and m[1]["velocity"] == 100 for m in midi.sent[:5]))
            self.assertTrue(all(m[0] == kind and m[1]["velocity"] == 0 for m in midi.sent[5:]))
            self.assertEqual(len(midi.sent), 10)

    def test_notes_are_released_even_when_interrupted(self):
        midi = FakeMido()
        def interrupt(seconds): raise KeyboardInterrupt
        with self.assertRaises(KeyboardInterrupt):
            host.play("out", notes=[60, 62], midi=midi, sleep=interrupt)
        self.assertEqual([m[0] for m in midi.sent], ["note_on", "note_on", "note_off", "note_off"])

    def test_bend_and_sustain_are_always_reset(self):
        midi = FakeMido()
        host.play("out", notes=[69], bend=8191, hold=0, midi=midi, sleep=lambda s: None)
        self.assertEqual([m[0] for m in midi.sent], ["note_on", "pitchwheel", "note_off", "pitchwheel"])
        self.assertEqual([midi.sent[1][1]["pitch"], midi.sent[3][1]["pitch"]], [8191, 0])
        midi = FakeMido()
        def interrupt(seconds): raise KeyboardInterrupt
        with self.assertRaises(KeyboardInterrupt):
            host.play("out", notes=[60, 64], sustain=True, midi=midi, sleep=interrupt)
        self.assertEqual([(m[0], m[1].get("control", m[1].get("note")), m[1].get("value", m[1].get("velocity")))
                          for m in midi.sent],
                         [("control_change", 64, 127), ("note_on", 60, 100), ("note_on", 64, 100),
                          ("note_off", 60, 0), ("note_off", 64, 0), ("control_change", 64, 0)])

    def test_invalid_traffic_sends_nothing(self):
        for kwargs in ({"cc": (128, 0)}, {"cc": (24, -1)}, {"notes": [128]}, {"notes": [60], "velocity": 0},
                       {"notes": [60], "hold": float("nan")}, {"notes": [60], "hold": 31}, {"notes": [True]},
                       {"notes": [60], "bend": 8192}, {"notes": [60], "bend": True}, {"bend": 100},
                       {"sustain": True}):
            midi = FakeMido()
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                host.play("out", midi=midi, **kwargs)
            self.assertEqual(midi.sent, [])
        with self.assertRaises(ValueError):
            host.play("missing", cc=(24, 0), midi=FakeMido())
