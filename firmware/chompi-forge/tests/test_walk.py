"""Panel walk and setup check: a scripted hand on a fake CHOMPI, the setup check against DC's
2026-10-04 rig faults, and the bridge's walk/setup/re-run operations in the simulation."""
import contextlib
import io
from pathlib import Path
import queue
import sys
import tempfile
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
try:
    import numpy as np
    import forge_audio as audio
    import forge_walk as walk
except ImportError:
    np = None
import forge_bridge as bridge
import forge_host as host
import test_audio


class FakeChompi:
    """Just enough state for the walk: physical switch events, encoder counters, toggle/jack,
    knob pages and looper state driven by virtual panel taps, active voices."""
    def __init__(self):
        self.events, self.raw = [], [0] * 6
        self.toggle_up, self.jack, self.voices = False, True, 0
        self.pages, self.looper, self.menu_page, self.held = [1, 1, 1, 1], "empty", "samples", set()
        self.patches, self.notes = [], []
        self.patch = host.load_patch(ROOT / "presets" / "01-dry.json")

    def snapshot(self):
        events, self.events = self.events, []
        return {"events": events, "panel": {"physical": {"toggle_up": self.toggle_up, "line_jack": self.jack},
                                            "raw_encoder_turns": list(self.raw), "knob_pages": list(self.pages),
                                            "menu": {"page": self.menu_page}, "leds": [[0, 0, 0]] * 26},
                "engine": {"active_voices": self.voices, "patch": self.patch}, "storage": {"looper": {"state": self.looper}}}

    def panel(self, event):
        kind, ident, value = event["kind"], event["id"], event["value"]
        if kind == 5: self.held.clear(); return
        if kind != 0: return
        if value: self.held.add(ident)
        else: self.held.discard(ident)
        if not value: return
        if ident in (3, 0, 1, 2): k = (3, 0, 1, 2).index(ident); self.pages[k] = self.pages[k] % host.knob_pages(self.patch, k + 1) + 1
        if ident == 34 and self.looper == "empty": self.looper = "first_take"
        if {33, 34} <= self.held: self.looper = "empty"          # hold both: cleared (the walk waits 2.3 s)
        if ident == 23: self.menu_page = "presets" if self.menu_page == "samples" else "samples"

    def send_patch(self, patch):
        self.patches.append(patch["name"]); self.patch = patch
        self.pages = [p if p <= host.knob_pages(patch, k + 1) else 1 for k, p in enumerate(self.pages)]
    def note(self, note, velocity):
        self.notes.append((note, velocity))
        if velocity: self.voices += 1

    # the hand
    def press(self, ident): self.events.append({"kind": "key_down", "id": ident, "value": 1})


@unittest.skipIf(np is None, "numpy not installed")
class WalkTests(unittest.TestCase):
    def run_walk(self, act, parts=("controls", "lights")):
        device, answers = FakeChompi(), queue.Queue()
        shown = []
        def show(prompt):
            shown.append(prompt)
            if prompt: act(device, prompt, answers)
        w = walk.Walk(device, show, answers, threading.Event(), log=lambda line: None, poll=0.001)
        walk.time = FastTime()                                    # the 2.3 s looper clears take no real time
        try: return w.run(parts), device, shown
        finally: walk.time = time

    def test_every_control_confirmed_and_one_miswired_key_reported(self):
        def hand(device, prompt, answers):
            ident = prompt["id"]
            if ident == "toggle": device.toggle_up = not device.toggle_up
            elif ident.startswith(("white.", "black.", "top.", "knobpress.")):
                group, n = ident.split("."); n = int(n) - 1
                names = {"white": walk.WHITE, "black": walk.BLACK, "top": ["CHOMPI", "KEY_27", "KEY_28"],
                         "knobpress": ["ENC_4_SW", "ENC_1_SW", "ENC_2_SW", "ENC_3_SW", "ENC_6_SW"]}[group]
                target = audio.PANEL_BUTTONS[names[n]]
                if ident == "white.3": target = audio.PANEL_BUTTONS["KEY_4"]   # a swapped switch
                device.press(target)
            elif ident.startswith("turn."):
                index = walk.ENCODER[prompt["title"].split()[1]]
                device.raw[index] += 3 if "RIGHT" in prompt["text"] else -3
            elif ident == "press.SW5": device.voices = 0
            elif ident == "jack": device.jack = not device.jack
            elif ident.startswith("light."):
                colour = {"light.SW4": "Red", "light.SW1": "Purple", "light.SW2": "Green", "light.SW3": "Pink",
                          "light.CHOMPI": "Dim white", "light.PLAY": "Teal", "light.LOOP": "Off", "light.menu": "Yes"}[ident]
                answers.put({"value": colour, "image": "data:image/jpeg;base64,AAAA"})
        result, device, shown = self.run_walk(hand)
        bad = {r["id"]: r for r in result["results"] if r["result"] != "pass"}
        self.assertEqual(set(bad), {"white.3", "light.LOOP"}, bad)
        self.assertIn("expected switch 9 (KEY_3), got 10 (KEY_4)", bad["white.3"]["detail"])
        self.assertIn("answered Off", bad["light.LOOP"]["detail"])
        self.assertEqual(bad["light.LOOP"]["photo"], "data:image/jpeg;base64,AAAA")
        self.assertEqual(result["counts"]["pass"], 15 + 10 + 3 + 5 + 3 + 12 + 1 + 2 + 8 - 2)
        self.assertEqual(device.pages, [1, 1, 1, 1]); self.assertEqual(device.looper, "empty")   # cleaned up
        self.assertEqual(device.patches[-1], "Dry routing check")
        self.assertEqual([v for n, v in device.notes if n in (57, 64)][-2:], [0, 0])            # pad released
        self.assertIsNone(shown[-1])

    def test_wrong_knob_reversed_direction_skip_and_stop(self):
        def hand(device, prompt, answers):
            ident = prompt["id"]
            if ident == "toggle": answers.put({"value": "skip"})
            elif ident.startswith(("white.", "black.", "top.", "knobpress.")): answers.put({"value": "skip"})
            elif ident == "turn.SW1": device.raw[walk.ENCODER["SW2"]] += 3                   # turned the wrong knob
            elif ident == "turn.SW2": device.raw[walk.ENCODER["SW2"]] += 3                   # same way both times
            elif ident.startswith("turn."):
                index = walk.ENCODER[prompt["title"].split()[1]]
                device.raw[index] += 3 if "RIGHT" in prompt["text"] else -3
            elif ident == "press.SW5": answers.put({"value": "stop"})
        result, device, _ = self.run_walk(hand)
        by = {}
        for r in result["results"]: by.setdefault(r["id"], []).append(r)
        self.assertTrue(result["stopped"])
        self.assertIn("SW2 moved most (SW2 +3)", by["turn.SW1"][0]["detail"])
        self.assertEqual([r["result"] for r in by["turn.SW2"]], ["pass", "fail"])
        self.assertTrue(all(r["result"] == "skipped" for r in by["toggle.menu"]))
        self.assertNotIn("light.SW4", by)                                                    # stopped before lights


    def test_bumped_neighbour_and_reversed_knob(self):
        """DC's 2026-10-04 walk: a neighbour that also moves must not be blamed, and a left turn that
        counts like a right turn fails even when that knob's right turn did not pass."""
        def hand(device, prompt, answers):
            ident = prompt["id"]
            if not ident.startswith("turn."): answers.put({"value": "skip"}); return
            index = walk.ENCODER[prompt["title"].split()[1]]
            right = "RIGHT" in prompt["text"]
            if ident == "turn.SW3" and right:
                device.raw[walk.ENCODER["SW2"]] += 3                    # turned the wrong knob
            elif ident == "turn.SW3":
                device.raw[index] += 3                                  # counts like a right turn
            elif ident == "turn.SW5":
                device.raw[walk.ENCODER["SW1"]] -= 2; device.raw[index] += 4 if right else -4   # SW1 bumped too
            else: device.raw[index] += 3 if right else -3
        result, device, _ = self.run_walk(hand, parts=("knobs",))
        by = {}
        for r in result["results"]: by.setdefault(r["id"], []).append(r)
        self.assertEqual([r["result"] for r in by["turn.SW3"]], ["fail", "fail"])
        self.assertIn("same direction as a right turn", by["turn.SW3"][1]["detail"])
        self.assertEqual([r["result"] for r in by["turn.SW5"]], ["pass", "pass"])
        self.assertIn("also SW1 -2, SW5 +4", by["turn.SW5"][0]["detail"])
        self.assertNotIn("white.1", by)                                  # knobs only: no keys asked


class FastTime:
    """time without sleeping (the walk's waits are for a real panel)."""
    def sleep(self, seconds): pass
    def time(self): return time.time()


@unittest.skipIf(np is None, "numpy not installed")
class SetupCheckTests(unittest.TestCase):
    class Device:
        def __init__(self, jack): self.jack = jack
        def send_patch(self, patch): pass
        def note(self, note, velocity): pass
        def snapshot(self): return {"panel": {"physical": {"line_jack": self.jack}}}
        def status(self): return {"firmware": host.FIRMWARE_VERSION, "build": "0f5bb18", "development": True,
                                  "dirty": False, "safe_mode": False}

    def rig(self, left_only, hum, hot):
        calls = []
        def source(frames, play, rate):
            calls.append(play is not None)
            t = np.arange(frames) / rate
            x = np.zeros((frames, 2))
            if hum: x[:, 0] += 10 ** (-55 / 20) * np.sqrt(2) * np.sin(2 * np.pi * 144 * t)
            n = len(calls)
            if n == 2: x[:, 0] += 0.35 * np.sin(2 * np.pi * 261.63 * t)                        # one C4
            if n == 3: x[:, 0] += (1.4 if hot else 0.3) * np.sin(2 * np.pi * 261.63 * t)       # loud chord
            if n == 4 and play is not None: x += play[:frames] if len(play) >= frames else np.pad(play, ((0, frames - len(play)), (0, 0)))
            if n in (2, 3): x[:, 1] += x[:, 0] * (0.045 if left_only else 1.0)
            return np.clip(x, -1, 1).astype(np.float32)
        return audio.FakeAudio(source)

    def test_dc_rig_problems_each_get_a_fix(self):
        result = audio.setup_check(self.Device(False), self.rig(True, True, True), False, log=lambda line: None)
        status = {f["what"]: f for f in result["findings"]}
        self.assertFalse(result["ok"])
        self.assertEqual(status["Noise on Input 1 (left)"]["status"], "fail")
        self.assertIn("144 Hz", status["Noise on Input 1 (left)"]["detail"])
        self.assertEqual(status["Noise on Input 2 (right)"]["status"], "ok")
        self.assertEqual(status["Both outputs"]["status"], "fail"); self.assertIn("right main output", status["Both outputs"]["fix"])
        self.assertEqual(status["Input gain"]["status"], "fail"); self.assertIn("10 dB", status["Input gain"]["fix"])
        self.assertEqual(status["Line input"]["status"], "fail"); self.assertIn("line input", status["Line input"]["fix"])

    def test_power_state_is_reported(self):
        device = self.Device(True)
        power = {"battery": "medium", "usb_power": True, "charge_done": False, "charge_state": 3, "charger_fault": False}
        device.snapshot = lambda: {"panel": {"physical": {"line_jack": True}}, "system": {"power": power}}
        result = audio.setup_check(device, self.rig(False, False, False), True, log=lambda line: None)
        found = {f["what"]: f for f in result["findings"]}
        self.assertEqual(found["Power"]["status"], "ok"); self.assertIn("Battery medium; on USB power, charge state 3", found["Power"]["detail"])
        power.update(charger_fault=True, charge_done=True)
        result = audio.setup_check(device, self.rig(False, False, False), True, log=lambda line: None)
        found = {f["what"]: f for f in result["findings"]}
        self.assertEqual(found["Power"]["status"], "warn"); self.assertIn("charged", found["Power"]["detail"])
        self.assertTrue(result["ok"])                                    # a warning does not fail the rig
        # 0.12: a weak supply alone is fine; with a low battery it warns (installs refused, may switch off).
        power.update(charger_fault=False, charge_done=False, weak_supply=True, battery_low_reading=False, install_blocked=False)
        found = {f["what"]: f for f in audio.setup_check(device, self.rig(False, False, False), True, log=lambda l: None)["findings"]}
        self.assertEqual(found["Power"]["status"], "ok"); self.assertIn("weak", found["Power"]["detail"])
        power.update(install_blocked=True)
        found = {f["what"]: f for f in audio.setup_check(device, self.rig(False, False, False), True, log=lambda l: None)["findings"]}
        self.assertEqual(found["Power"]["status"], "warn"); self.assertIn("USB-C", found["Power"]["fix"])

    def test_last_start_is_reported(self):
        device = self.Device(True)
        restart = {"causes": ["power-on", "brown-out"], "crashed": False, "crash_pc": None}
        device.snapshot = lambda: {"panel": {"physical": {"line_jack": True}}, "system": {"restart": restart}}
        found = {f["what"]: f for f in audio.setup_check(device, self.rig(False, False, False), True, log=lambda l: None)["findings"]}
        self.assertEqual(found["Last start"]["status"], "ok"); self.assertIn("power-on, brown-out", found["Last start"]["detail"])
        restart.update(causes=["brown-out"])                             # a dip without a power-on: the supply sagged
        found = {f["what"]: f for f in audio.setup_check(device, self.rig(False, False, False), True, log=lambda l: None)["findings"]}
        self.assertEqual(found["Last start"]["status"], "warn"); self.assertIn("supply dipped", found["Last start"]["detail"])
        restart.update(causes=["software"], crashed=True, crash_pc=0x24012345)
        found = {f["what"]: f for f in audio.setup_check(device, self.rig(False, False, False), True, log=lambda l: None)["findings"]}
        self.assertEqual(found["Last start"]["status"], "warn"); self.assertIn("pc 0x24012345", found["Last start"]["detail"])
        self.assertIn("RESTARTS.TXT", found["Last start"]["fix"])

    def test_good_rig_passes(self):
        result = audio.setup_check(self.Device(True), self.rig(False, False, False), True, log=lambda line: None)
        self.assertTrue(result["ok"], result)
        self.assertEqual([f["status"] for f in result["findings"]], ["ok"] * 6)
        self.assertEqual(result["findings"][0]["what"], "Firmware")

    def test_power_gate_before_an_unattended_run(self):
        ports = {"input": "CHOMPI 1", "output": "CHOMPI 2"}
        def status(power):
            return lambda payload, i, o: {"firmware": host.FIRMWARE_VERSION, "build": "0f5bb18", "development": True,
                                          "dirty": False, "safe_mode": False, "power": power}
        low = host.decode_power([2, 8 | 32, 0]); good = host.decode_power([1, 1, 2])
        quiet = io.StringIO()
        with contextlib.redirect_stdout(quiet):
            self.assertTrue(audio.power_ok(ports, exchange=status(good)))
            self.assertFalse(audio.power_ok(ports, exchange=status(low)))
            self.assertTrue(audio.power_ok(ports, ignore=True, exchange=status(low)))
            self.assertTrue(audio.power_ok(ports, exchange=status(None)))      # before 0.15.2: not reported
        self.assertIn("install refused", quiet.getvalue()); self.assertIn("charge CHOMPI first", quiet.getvalue())

    def test_firmware_line(self):
        # 0.15.2: Check setup names the build; another version, an unknown or uncommitted build warn; safe mode fails.
        good = {"firmware": host.FIRMWARE_VERSION, "build": "0f5bb18", "development": False, "dirty": False, "safe_mode": False}
        self.assertEqual(audio.firmware_finding(good)[1:3], ("ok", f"Forge {host.FIRMWARE_VERSION} (build 0f5bb18)."))
        self.assertEqual(audio.firmware_finding({**good, "firmware": "0.15", "build": None, "development": None,
                                                  "dirty": None, "safe_mode": None})[1], "warn")
        self.assertEqual(audio.firmware_finding({**good, "dirty": True})[1], "warn")
        self.assertEqual(audio.firmware_finding({**good, "build": None})[1], "warn")
        failed = audio.firmware_finding({**good, "safe_mode": True})
        self.assertEqual(failed[1], "fail"); self.assertIn("RESTARTS.TXT", failed[3])


class BridgeWalkTests(unittest.TestCase):
    _base = test_audio.BridgeAutomaticTests
    make, tearDown, connect, call, wait = _base.make, _base.tearDown, _base.connect, _base.call, _base.wait
    @unittest.skipIf(np is None, "numpy not installed")
    def test_light_questions_in_simulation_rerun_and_export(self):
        self.make(); self.connect()
        with self.assertRaisesRegex(RuntimeError, "physical CHOMPI"): self.call("walk")
        with self.assertRaisesRegex(RuntimeError, "no audio"): self.call("setup")
        with self.assertRaisesRegex(ValueError, "Walk parts"): self.call("walk", parts=["dance"])
        self.call("walk", parts=["lights"])
        expected = {"light.SW4": "Red", "light.SW1": "Purple", "light.SW2": "Green", "light.SW3": "Pink",
                    "light.CHOMPI": "Dim white", "light.PLAY": "Teal", "light.LOOP": "Red", "light.menu": "Yes"}
        seen = []
        for _ in range(2000):
            job = self.call("job")
            if job["finished"]: break
            p = job["prompt"]
            if p and (not seen or seen[-1] != p["seq"]):
                seen.append(p["seq"])
                with self.assertRaisesRegex(ValueError, "Unknown answer"): self.call("answer", seq=p["seq"], value="Magenta")
                with self.assertRaisesRegex(ValueError, "no longer open"): self.call("answer", seq=p["seq"] - 1, value="skip")
                self.call("answer", seq=p["seq"], value=expected[p["id"]])
            time.sleep(0.01)
        self.assertIsNone(job["error"]); self.assertEqual(len(seen), 8)
        self.assertEqual(job["result"]["counts"], {"pass": 8, "fail": 0, "skipped": 0, "error": 0})
        # The simulation's LED shadow agrees with what the bridge asked about (menu keys lit).
        menu = next(r for r in job["result"]["results"] if r["id"] == "light.menu")
        self.assertTrue(any(max(c) > 8 for c in menu["commanded"]))
        self.assertEqual(self.call("poll")["panel"]["knob_pages"], [1, 1, 1, 1])
        self.assertEqual(self.call("export")["automatic"]["panel_walks"][0]["counts"]["pass"], 8)
        # Re-run only chosen steps.
        with self.assertRaisesRegex(ValueError, "step ids"): self.call("autorun", confirm=True, only=["9.9"])
        self.call("autorun", confirm=True, only=["3.17", "3.52"], folder="rerun")
        job = self.wait()
        self.assertEqual([s["id"] for s in job["result"]["steps"]], ["3.17", "3.52"])
        self.assertEqual({s["result"] for s in job["result"]["steps"]}, {"pass"})


if __name__ == "__main__":
    unittest.main()
