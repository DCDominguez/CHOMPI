"""Automatic checks (host/forge_audio.py) and the bridge's discover/audio/autorun operations,
against the C++ simulation (forge_probe) and fake audio. No interface or hardware involved."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock
import zlib

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "build" / ("forge_probe.exe" if sys.platform == "win32" else "forge_probe")
sys.path.insert(0, str(ROOT / "host"))
import forge_bridge as bridge
try:
    import numpy as np
    import forge_audio as audio
except ModuleNotFoundError:          # numpy is a bridge-only dependency
    np = audio = None

RATE = 48000


def sine(hz, seconds=1.0, amplitude=0.25, channels=2, rate=RATE):
    t = np.arange(int(seconds * rate)) / rate
    return np.stack([amplitude * np.sin(2 * np.pi * hz * t)] * channels, axis=1).astype(np.float32)


@unittest.skipIf(np is None, "numpy not installed (pip install -r host/bridge-requirements.txt)")
class AnalysisTests(unittest.TestCase):
    def test_pitch_level_and_silence(self):
        for hz in (55.0, 261.63, 440.0, 3520.0):
            self.assertAlmostEqual(audio.analyze(sine(hz))["pitch_hz"], hz, delta=hz * 0.002)
        self.assertAlmostEqual(audio.analyze(sine(440, rate=44100), 44100)["pitch_hz"], 440, delta=1)
        a = audio.analyze(sine(440, amplitude=0.5))
        self.assertAlmostEqual(a["peak_db"], -6.0, delta=0.1); self.assertAlmostEqual(a["rms_db"], -9.0, delta=0.1)
        self.assertFalse(a["silent"]); self.assertEqual(a["stereo_correlation"], 1.0)
        quiet = audio.analyze(np.zeros((RATE, 2), np.float32))
        self.assertTrue(quiet["silent"]); self.assertIsNone(quiet["pitch_hz"]); self.assertIsNone(quiet["onset_s"])
        self.assertIsNone(audio.analyze(audio.FakeAudio().source(RATE, None, RATE))["pitch_hz"])   # -80 dB noise
        json.dumps(a)                                                  # plain Python types for reports

    def test_audible_change(self):
        # Knob audio checks (3.57): the same phrase twice is no change; pitch, level, decay or pan are.
        t = np.arange(48000) / 48000
        def note(hz=220, level=.3, decay=3.0, pan=.5):
            mono = level * np.sin(2 * np.pi * hz * t) * np.exp(-t * decay)
            return np.stack([mono * (1 - pan) * 2, mono * pan * 2], axis=1) * .5
        base = note()
        self.assertLess(audio.audible_change(base, base.copy())[0], .01)
        self.assertGreater(audio.audible_change(base, note(hz=233))[0], 20)                 # a semitone
        change, what = audio.audible_change(base, note(level=.15)); self.assertGreater(change, 5); self.assertEqual(what, "loudness")
        self.assertGreater(audio.audible_change(base, note(decay=8))[0], 5)
        change, what = audio.audible_change(base, note(pan=.8)); self.assertGreater(change, 5)

    def test_onset_clip_and_clicks(self):
        x = np.zeros((RATE, 2), np.float32); x[RATE // 2:] = np.roll(sine(220, 0.5), -RATE // 880, axis=0)
        a = audio.analyze(x)                                           # starts at a peak: a step, i.e. a click
        self.assertAlmostEqual(a["onset_s"], 0.5, delta=0.011); self.assertEqual(a["clicks"], [0.5])
        self.assertEqual(audio.analyze(sine(220))["clicks"], [])
        clicked = sine(220, amplitude=0.1).copy(); clicked[12000:] += 0.2
        self.assertEqual(audio.analyze(clicked)["clicks"], [0.25])
        self.assertGreater(audio.analyze(sine(100, amplitude=1.2))["clipped"], 0)

    def test_tone_is_bounded_and_faded(self):
        t = audio.tone(440, 0.5)
        self.assertAlmostEqual(20 * np.log10(np.max(np.abs(t))), -18, delta=0.1)
        self.assertEqual(t[0, 0], 0); self.assertEqual(audio.analyze(t)["clicks"], [])
        with self.assertRaises(ValueError): audio.tone(440, 0.5, db=0)

    def test_wav_and_png(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "a.wav"
            audio.write_wav(path, sine(440, 0.2))
            x, rate = audio.read_wav(path)
            self.assertEqual((rate, x.shape), (RATE, (9600, 2)))
            self.assertLess(np.max(np.abs(x - sine(440, 0.2))), 1e-4)
        data = audio.spectrogram(x)
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        width, height = struct.unpack(">II", data[16:24])
        self.assertEqual(height, 192); self.assertGreater(width, 0)
        idat = data.index(b"IDAT"); size = struct.unpack(">I", data[idat - 4:idat])[0]
        self.assertEqual(len(zlib.decompress(data[idat + 4:idat + 4 + size])), height * (width + 1))
        self.assertTrue(audio.data_url(data).startswith("data:image/png;base64,iVBOR"))

    def test_gestures_and_leds(self):
        g = audio.panel_gesture
        self.assertEqual(g("toggle up"), [{"kind": 3, "id": 0, "value": 1}])
        self.assertEqual(g("jack hw"), [{"kind": 4, "id": 0, "value": -1}])
        self.assertEqual(g("tap KEY_22"), [{"kind": 0, "id": 23, "value": 1}, {"kind": 0, "id": 23, "value": 0}])
        self.assertEqual(g("hold CHOMPI"), [{"kind": 0, "id": 5, "value": 1}])
        self.assertEqual(g("turn KNOB_1 -5"), [{"kind": 1, "id": 3, "value": -5}])
        self.assertEqual(g("press SW5"), [{"kind": 2, "id": 4, "value": 0}])
        self.assertEqual(g("release all"), [{"kind": 5, "id": 0, "value": 0}])
        for bad in ("toggle sideways", "spin KNOB_1", "turn KNOB_9 1", "hold KEY_99", ""):
            with self.assertRaises(ValueError): g(bad)
        for event in g("hold KEY_8") + g("turn SW6 63") + g("toggle hw"):          # the bridge's own validation
            bridge.panel_message(event, 1)
        self.assertEqual((audio.key_led("KEY_1"), audio.key_led("KEY_15"), audio.key_led("KEY_16"), audio.key_led("CHOMPI")), (24, 10, 0, 25))

    def test_plan_is_valid(self):
        plan = json.loads(audio.PLAN.read_text())
        actions = {"wait", "send", "status", "panic", "panel", "probe", "samples", "cc", "ensure", *audio.AUDIO_ACTIONS}
        ids = [s["id"] for s in plan["steps"]]
        self.assertEqual(len(ids), len(set(ids)))
        def walk(action):
            (kind, arg), = action.items(); self.assertIn(kind, actions)
            if kind == "panel":
                for gesture in (arg if isinstance(arg, list) else [arg]): audio.panel_gesture(gesture)
            if kind == "ensure": self.assertIn(arg, ("knobs_page1", "menu_presets", "looper_empty"))
            if kind == "send":
                bridge.host.validate_patch(bridge.host.load_patch(ROOT / (arg["file"] if isinstance(arg, dict) else arg)))
            if kind == "tone": self.assertLessEqual(arg.get("db", audio.TONE_DB), -18)
            for inner in arg.get("during", []) if isinstance(arg, dict) else []: walk(inner)
        for step in plan["steps"]:
            for action in step.get("do", []): walk(action)


class WindowsRuntimeTests(unittest.TestCase):
    def test_lock_matches_requirements_and_python(self):
        lock = json.loads((ROOT / "host/windows-runtime.json").read_text())
        files = [lock["python"], *lock["wheels"], lock["msvc"]]
        for entry in files:
            self.assertRegex(entry["sha256"], "^[0-9a-f]{64}$")
            self.assertTrue(entry["url"].startswith(("https://files.pythonhosted.org/", "https://api.nuget.org/")), entry)
            self.assertTrue(entry["url"].endswith(entry["file"]))
        self.assertEqual(lock["python"]["file"], "python.3.12.10.nupkg")
        wheels = {w["file"].split("-")[0].replace("_", "-").lower(): w["file"].split("-")[1] for w in lock["wheels"]}
        for line in (ROOT / "host/requirements.txt").read_text().split():
            name, version = line.split("==")
            self.assertEqual(wheels[name.lower()], version, name)          # the kit runs what the repo pins
        for wheel in lock["wheels"]:
            self.assertRegex(wheel["file"], r"-(cp312-cp312-win_amd64|py3-none-(any|win_amd64))\.whl$")
        self.assertIn("msvcp140.dll", json.dumps(lock["msvc"]["extract"]))


class FakeChompi:
    """Audio world for detection: input 'Scarlett' hears CHOMPI's note; output 'Scarlett Out' feeds its line in."""
    def __init__(self):
        self.held, self.played = set(), []

    # device side
    def send_patch(self, patch): self.patch = patch["name"]; return {"ok": True}
    def note(self, note, velocity): (self.held.add if velocity else self.held.discard)(note)

    # audio side
    def audio(self):
        world = self
        class Interface(audio.FakeAudio):
            def devices(self):
                return [{"index": 0, "name": "Microphone (Webcam)", "api": "WASAPI", "inputs": 1, "outputs": 0},
                        {"index": 1, "name": "Speakers (Realtek)", "api": "WASAPI", "inputs": 0, "outputs": 2},
                        {"index": 2, "name": "Analogue 1 + 2 (Scarlett 2i2)", "api": "WASAPI", "inputs": 2, "outputs": 0},
                        {"index": 3, "name": "Speakers (Scarlett 2i2)", "api": "WASAPI", "inputs": 0, "outputs": 2}]
            def configure(self, input, output=None): self.input, self.output = input, output
            def start(self, seconds, play=None):
                self.seconds, self.play = seconds, play
                if play is not None: world.played.append(self.output)
            def wait(self):
                frames = int(self.seconds * RATE)
                if self.input == 2 and self.play is not None and self.output == 3: return self.play[:frames] if len(self.play) >= frames else np.pad(self.play, ((0, frames - len(self.play)), (0, 0)))
                if self.input == 2 and self.play is None and world.saw_note: return sine(261.63, self.seconds, 0.2)
                return np.zeros((frames, 2), np.float32)
        return Interface()

    @property
    def saw_note(self): return True          # notes are released before wait(); the take contains them


@unittest.skipIf(np is None, "numpy not installed")
class DetectionTests(unittest.TestCase):
    def test_finds_input_then_the_output_wired_to_line_in(self):
        world = FakeChompi(); interface = world.audio()
        info = audio.detect(world, interface, log=lambda line: None)
        self.assertEqual(info["input"]["name"], "Analogue 1 + 2 (Scarlett 2i2)")
        self.assertEqual(info["output"]["name"], "Speakers (Scarlett 2i2)")
        self.assertEqual(world.played, [3])                    # the interface's own output first: one beep only
        self.assertEqual((interface.input, interface.output), (2, 3))
        self.assertEqual(world.patch, "Dry routing check")                 # leaves CHOMPI on the dry aux patch
        self.assertEqual(world.held, set())

    def test_ensure_clears_a_leftover_loop(self):
        events = []
        class Device:
            def snapshot(self): return {"storage": {"looper": {"state": "playing"}}}
            def panel(self, event): events.append((event["kind"], event["id"], event["value"]))
        runner = audio.Runner(Device(), None)
        start = time.monotonic(); runner.ensure("looper_empty")
        self.assertGreaterEqual(time.monotonic() - start, 2.3)                      # TAPE's 2 s hold
        self.assertEqual(events[:4], [(0, 33, 1), (0, 34, 1), (0, 33, 0), (0, 34, 0)])
        events.clear()
        Device.snapshot = lambda self: {"storage": {"looper": {"state": "empty"}}}
        runner.ensure("looper_empty"); self.assertEqual(events, [])

    def test_cpu_peak_is_per_step_when_the_firmware_can_reset_it(self):
        plan = {"steps": [{"id": "a", "do": [{"status": True}], "check": [{"status": {"cpu_max_percent": [0, 50]}}]},
                          {"id": "b", "do": [{"status": True}], "check": []}, {"id": "c", "do": [], "check": []}]}
        class Device:
            peak, resets = 0.0, []
            def status(self, reset_cpu=False):
                result = {"cpu_max_percent": self.peak, "cpu_average_percent": 1.0}
                if reset_cpu: self.resets.append(self.peak); self.peak = 0.0
                else: self.peak = 20.0                         # this step's load
                return result
        device = Device(); device.peak = 90.0                   # a spike before the run
        result = audio.Runner(device, None).run(plan)
        self.assertEqual([s.get("cpu_peak") for s in result["steps"]], ["this step", "this step", None])
        self.assertEqual(result["steps"][0]["result"], "pass"); self.assertEqual(device.resets, [90.0, 20.0])
        class OldFirmware(Device):                              # 0.6: a flags byte is a length error
            def status(self, reset_cpu=False):
                if reset_cpu: raise RuntimeError("Device rejected request: length")
                return {"cpu_max_percent": 90.0}
        old = OldFirmware(); result = audio.Runner(old, None).run(plan)
        self.assertEqual([s.get("cpu_peak") for s in result["steps"]], ["since boot", "since boot", None])
        self.assertEqual(result["steps"][0]["result"], "fail")

    def test_no_line_in_plug_means_no_output_and_no_beeps(self):
        # The dry path carries CHOMPI's mic without a plug: a speaker beep must not count as line in.
        world = FakeChompi(); interface = world.audio()
        world.snapshot = lambda: {"panel": {"physical": {"line_jack": False}}}
        info = audio.detect(world, interface, log=lambda line: None)
        self.assertEqual(info["input"]["name"], "Analogue 1 + 2 (Scarlett 2i2)")
        self.assertIsNone(info["output"]); self.assertIs(info["line_jack"], False); self.assertEqual(world.played, [])
        world.snapshot = lambda: {"panel": {"physical": {"line_jack": True}}}
        self.assertEqual(audio.detect(world, world.audio(), log=lambda line: None)["output"]["name"], "Speakers (Scarlett 2i2)")

    def test_nothing_heard_means_no_audio(self):
        world = FakeChompi(); interface = world.audio()
        interface.wait = lambda: np.zeros((RATE, 2), np.float32)
        info = audio.detect(world, interface, log=lambda line: None)
        self.assertIsNone(info["input"]); self.assertIsNone(info["output"]); self.assertEqual(world.played, [])


class FakePorts:
    """Port names as Windows shows them; only the CHOMPI pair answers (through the C++ simulation)."""
    def __init__(self, inputs, outputs, answering=("CHOMPI 0", "CHOMPI 1")):
        self.inputs, self.outputs, self.answering, self.opened = inputs, outputs, answering, []

    def ports(self): return self.inputs, self.outputs

    def factory(self, i, o, probe):
        self.opened.append((i, o))
        if probe or (i, o) == self.answering: return bridge.Transport(None, None, probe or PROBE)
        class Silent:
            def exchange(self, payload, decoder=None, timeout=2): raise TimeoutError("no reply")
            def close(self): pass
        return Silent()


class BridgeAutomaticTests(unittest.TestCase):
    def make(self, ports=None, **kwargs):
        self.lock = threading.Lock(); self.tmp = tempfile.TemporaryDirectory()
        self.bridge = bridge.Bridge(self.lock, PROBE, factory=ports.factory if ports else bridge.Transport,
                                    ports=ports.ports if ports else bridge.midi_ports, reports=self.tmp.name, **kwargs)
        return self.bridge

    def tearDown(self):
        self.bridge.close(); self.tmp.cleanup()
        self.assertFalse(self.lock.locked())

    def test_discover_probes_only_chompi_named_ports(self):
        ports = FakePorts(["Microsoft GS Wavetable Synth", "Arturia KeyStep", "CHOMPI 0"],
                          ["Microsoft GS Wavetable Synth", "Arturia KeyStep", "CHOMPI 1"])
        found = self.make(ports).request("discover", {})
        self.assertEqual((found["input"], found["output"], found["firmware"]), ("CHOMPI 0", "CHOMPI 1", "0.11"))
        self.assertEqual(ports.opened, [("CHOMPI 0", "CHOMPI 1")])
        self.assertFalse(self.lock.locked())

    def test_discover_explains_what_is_wrong(self):
        self.make(FakePorts(["Arturia KeyStep"], ["Arturia KeyStep"]))
        with self.assertRaisesRegex(RuntimeError, "No MIDI port named CHOMPI.*Arturia KeyStep"): self.bridge.request("discover", {})
        self.bridge.close(); self.tmp.cleanup()
        ports = FakePorts(["CHOMPI 0"], ["CHOMPI 1"], answering=None)
        self.make(ports)
        with self.assertRaisesRegex(RuntimeError, "did not answer.*development firmware"): self.bridge.request("discover", {})
        self.assertEqual(ports.opened, [("CHOMPI 0", "CHOMPI 1")])

    def connect(self):
        self.owner = self.bridge.request("connect", {"mode": "simulation", "metadata": "software test"})["owner"]

    def call(self, op, **body): return self.bridge.request(op, {"owner": self.owner, **body})

    def wait(self):
        for _ in range(600):
            job = self.call("job")
            if job["finished"]: return job
            time.sleep(0.05)
        self.fail("job did not finish")

    @unittest.skipIf(np is None, "numpy not installed")
    def test_autorun_in_simulation_skips_audio_and_checks_panel_state(self):
        self.make(); self.connect()
        with self.assertRaisesRegex(ValueError, "Confirm"): self.call("autorun")
        with self.assertRaisesRegex(RuntimeError, "simulation has no audio"): self.call("audio_detect")
        self.call("autorun", confirm=True, folder="run1")
        job = self.wait()
        self.assertIsNone(job["error"])
        results = {s["id"]: s["result"] for s in job["result"]["steps"]}
        for step in ("1.3", "3.17", "3.30", "3.30c", "3.28", "3.52", "3.53", "3.55", "6.3"): self.assertEqual(results[step], "pass", job["result"])
        self.assertEqual(results["2.1"], "skipped")
        self.assertEqual(job["result"]["counts"]["fail"] + job["result"]["counts"]["error"], 0)
        self.assertFalse(self.call("poll")["panel"]["logical"]["overridden"])     # virtual panel handed back
        report = self.call("export")
        self.assertEqual(report["automatic"]["runs"][0]["counts"], job["result"]["counts"])
        self.assertFalse(report["hardware_verified"])
        self.assertTrue((Path(self.tmp.name) / "run1" / "summary.md").exists())

    @unittest.skipIf(np is None, "numpy not installed")
    def test_autorun_sets_its_own_starting_state(self):
        # As on DC's unit (2026-10-04): knob pages, the menu page and a loop left from hand testing.
        self.make(); self.connect(); self.call("arm", enabled=True)
        for button in (3, 2, 2):                              # Dry (effects only): SW4 to page 2, SW3 to page 3
            for value in (1, 0): self.call("action", action="panel", kind=0, id=button, value=value)
        for value in (1, 0): self.call("action", action="panel", kind=0, id=34, value=value)   # a first take
        self.assertEqual(self.call("poll")["panel"]["knob_pages"], [2, 1, 1, 3])
        self.call("action", action="release")
        for folder in ("dirty", "again"):                     # and a second run starts where the first ended
            self.call("autorun", confirm=True, folder=folder)
            job = self.wait()
            results = {s["id"]: s["result"] for s in job["result"]["steps"]}
            for step in ("3.17", "3.30", "3.30c", "3.52", "3.53", "3.55"):   # looper steps need real time
                self.assertEqual(results[step], "pass", (folder, step, job["result"]))

    def test_looper_state_reaches_the_bridge(self):
        self.make(); self.connect(); self.call("arm", enabled=True)
        looper = self.call("poll")["storage"]["looper"]
        self.assertEqual((looper["state"], looper["length_frames"], looper["speed"], looper["effects_before_loop"]), ("empty", 0, 1.0, True))
        for value in (1, 0): self.call("action", action="panel", kind=0, id=34, value=value)      # tap LOOP (KEY_28)
        for _ in range(3): looper = self.call("poll")["storage"]["looper"]
        self.assertEqual(looper["state"], "first_take"); self.assertGreater(looper["length_frames"], 0)

    @unittest.skipIf(np is None, "numpy not installed")
    def test_audio_steps_run_with_an_interface_and_images_are_served(self):
        plan = Path(tempfile.mkdtemp()) / "plan.json"
        plan.write_text(json.dumps({"name": "t", "steps": [
            {"id": "a", "title": "tone", "do": [{"send": "presets/01-dry.json"},
                                              {"play": {"notes": [60], "seconds": 0.05, "tail": 0.05, "capture": "c"}}],
             "check": [{"capture": {"name": "c", "pitch_hz": [438, 442], "silent": False, "clicks_max": 0}}]},
            {"id": "b", "title": "wrong", "do": [{"capture": {"name": "d", "seconds": 0.1}}],
             "check": [{"capture": {"name": "d", "pitch_hz": [100, 200]}}]},
            {"id": "c", "title": "leaves keys held", "do": [{"panel": ["toggle up", "hold CHOMPI"]}]}]}))
        self.make(plan=plan); self.connect()
        self.bridge.audio = audio.FakeAudio(lambda frames, play, rate: sine(440, frames / rate))
        with mock.patch.object(self.bridge.transport, "raw", wraps=self.bridge.transport.raw) as raw:
            self.call("autorun", confirm=True)
            job = self.wait()
        self.assertEqual([s["result"] for s in job["result"]["steps"]], ["pass", "fail", "pass"], job)
        self.assertFalse(self.call("poll")["panel"]["logical"]["overridden"])     # the runner hands the panel back
        self.assertIn([0x90, 60, 100], [c.args[0] for c in raw.call_args_list])
        self.assertIn([0x80, 60, 0], [c.args[0] for c in raw.call_args_list])
        image = job["result"]["steps"][0]["captures"]["c"]["image"]
        self.assertTrue(self.call("capture", image=image)["png"].startswith("data:image/png;base64,"))
        with self.assertRaises(ValueError): self.call("capture", image="../../etc/passwd")
        self.assertIn("expected capture.pitch_hz = [100, 200]", (Path(self.tmp.name) / job["result"]["folder"].split("/")[-1] / "summary.md").read_text())

    @unittest.skipIf(np is None, "numpy not installed")
    def test_job_owns_the_transport_and_can_be_cancelled(self):
        plan = Path(tempfile.mkdtemp()) / "plan.json"
        plan.write_text(json.dumps({"name": "slow", "steps": [{"id": str(i), "do": [{"wait": 0.2}, {"status": True}]} for i in range(20)]}))
        self.make(plan=plan); self.connect()
        self.call("arm", enabled=True)
        self.call("autorun", confirm=True)
        for op in ("poll", "disconnect", "autorun"):
            with self.assertRaisesRegex(RuntimeError, "running"): self.call(op, confirm=True)
        with self.assertRaisesRegex(RuntimeError, "running"): self.call("action", action="panic")
        self.assertTrue(self.call("heartbeat")["connected"])
        self.assertTrue(self.call("cancel")["cancelling"])
        job = self.wait()
        self.assertTrue(job["result"]["cancelled"]); self.assertLess(len(job["result"]["steps"]), 20)
        self.assertIn("generation", self.call("poll"))                          # the session continues


if __name__ == "__main__":
    unittest.main()
