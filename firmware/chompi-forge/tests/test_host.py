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
            self.assertEqual(result["firmware"], "0.3")
            self.assertAlmostEqual(host.effect_patch(result["patch"])["parameters"]["time_ms"],
                                   host.effect_patch(patch)["parameters"]["time_ms"], delta=990 / 16383)

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
