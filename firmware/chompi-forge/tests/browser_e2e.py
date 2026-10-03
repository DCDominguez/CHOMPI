"""Real Chromium end-to-end test of the Forge webapp (run: make browser-test).

Drives the actual page served by forge_web.py. MIDI goes to tests/sim_device.py
(the C++ runtime via build/forge_probe); cloud providers are replaced with canned
HTTP responses so no API key or network is used. Screenshots: build/browser/.
This verifies browser behaviour and layout, not hardware, audio or live providers.
"""
import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
import urllib.error

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
sys.path.insert(0, str(ROOT / "tests"))
import forge_ai
import forge_host
import forge_web
import sim_device

try:
    from playwright.sync_api import expect, sync_playwright
except ImportError:  # pragma: no cover
    sync_playwright = None

SHOTS = ROOT / "build" / "browser"
FAKE_KEY = "sk-test-not-a-real-key-123"


class FakeProvider:
    """Replaces urllib's opener inside forge_ai; records requests, never touches the network."""
    def __init__(self):
        self.requests, self.patch, self.status = [], None, 200

    def build_opener(self, *handlers):
        return self

    def open(self, request, timeout):
        self.requests.append(request)
        if self.status != 200:
            raise urllib.error.HTTPError(request.full_url, self.status, "denied", {}, io.BytesIO(b"secret body"))
        text = json.dumps(self.patch)
        if "openai" in request.full_url:
            envelope = {"status": "completed", "output": [{"type": "message", "content": [{"type": "output_text", "text": text}]}]}
        else:
            envelope = {"candidates": [{"finishReason": "STOP", "content": {"parts": [{"text": text}]}}]}
        return io.BytesIO(json.dumps(envelope).encode())


@unittest.skipIf(sync_playwright is None, "playwright not installed")
class BrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        SHOTS.mkdir(parents=True, exist_ok=True)
        cls.device = sim_device.SimulatedMido()
        cls.provider = FakeProvider()
        cls.patches = [patch.object(forge_host, "midi_module", lambda: cls.device),
                       patch.object(forge_ai.urllib.request, "build_opener", cls.provider.build_opener)]
        for item in cls.patches: item.start()
        cls.server = forge_web.ForgeServer(0)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.pw = sync_playwright().start()
        cls.browser = cls.pw.chromium.launch()

    @classmethod
    def tearDownClass(cls):
        cls.browser.close(); cls.pw.stop()
        cls.server.shutdown(); cls.server.server_close(); cls.thread.join()
        for item in cls.patches: item.stop()
        cls.device.close()

    def setUp(self):
        self.context = self.browser.new_context(viewport={"width": 1280, "height": 900}, accept_downloads=True)
        self.page = self.context.new_page()
        self.console = []
        # Deliberate 4xx/5xx API replies are logged by Chromium as resource errors; anything
        # else (JS exceptions, CSP violations) fails the test.
        self.page.on("console", lambda m: m.type in ("error", "warning")
                     and not m.text.startswith("Failed to load resource: the server responded")
                     and self.console.append(m.text))
        self.page.on("pageerror", lambda e: self.console.append(str(e)))
        self.page.goto(self.server.origin + "/")
        expect(self.page.locator("#notice")).to_contain_text("Ready")

    def tearDown(self):
        self.assertEqual(self.console, [], "browser console errors/CSP violations")
        self.context.close()

    def json(self):
        # textContent works while <details> is closed; evaluate() would be blocked by the CSP.
        return json.loads(self.page.text_content("#json"))

    def notice(self):
        return self.page.inner_text("#notice")

    def wait_idle(self):
        expect(self.page.locator("#preset")).to_be_enabled()

    def choose_preset(self, name):
        self.page.select_option("#preset", label=name); self.wait_idle()

    def test_initial_render_and_instrument_editing(self):
        p = self.page
        self.assertEqual(p.locator("#preset option").count(), 11)
        patch_json = self.json()
        self.assertEqual(patch_json["version"], 3)                  # starts on the first v3 preset
        self.assertTrue(p.is_disabled("#upgrade"))
        for control in ("#synth-waveform", "#filter-resonance", "#lfo-rate_hz", "#reverb-mix", "#lfo-mod_wheel"):
            self.assertFalse(p.is_disabled(control), control)
        self.assertEqual(p.input_value("#routing"), patch_json["routing"])
        p.select_option("#synth-waveform", "square"); p.select_option("#routing", "aux>delay>reverb>output")
        p.select_option("#lfo-waveform", "sample_hold"); p.select_option("#synth-osc2_waveform", "triangle")
        p.fill("#synth-attack_ms", "250"); p.fill("#filter-cutoff_hz", "1200"); p.check("#delay-bypass")
        p.fill("#synth-voices", "1"); p.fill("#synth-osc2_semitones", "-12"); p.fill("#filter-env_octaves", "-2.5")
        p.check("#lfo-mod_wheel"); p.fill("#reverb-size", "0.9")
        # Same key in two modules: filter attack must not overwrite synth attack.
        p.fill("#filter-attack_ms", "33")
        p.fill("#patch-name", "Edited in Chromium")
        edited = self.json(); m = edited["modules"]
        self.assertEqual((m["synth"]["waveform"], m["synth"]["osc2_waveform"], m["lfo"]["waveform"]), ("square", "triangle", "sample_hold"))
        self.assertEqual(edited["routing"], "aux>delay>reverb>output")
        self.assertEqual((m["synth"]["attack_ms"], m["filter"]["attack_ms"]), (250, 33))
        self.assertEqual((m["filter"]["cutoff_hz"], m["synth"]["voices"], m["synth"]["osc2_semitones"]), (1200, 1, -12))
        self.assertEqual((m["filter"]["env_octaves"], m["reverb"]["size"]), (-2.5, 0.9))
        self.assertTrue(m["delay"]["bypass"]); self.assertTrue(m["lfo"]["mod_wheel"])
        self.assertEqual(edited["name"], "Edited in Chromium")
        self.assertEqual(forge_host.validate_patch(edited), edited)
        # Range slider drives the number box and the patch.
        p.locator("#synth-release_ms-range").fill("900")
        self.assertEqual(self.json()["modules"]["synth"]["release_ms"], 900)
        self.assertEqual(p.input_value("#synth-release_ms"), "900")
        # Log sliders match the firmware curve: cutoff midpoint 40 * 400**0.5 = 800 Hz, LFO 0.05 * 400**0.5 = 1 Hz.
        p.locator("#filter-cutoff_hz-range").fill("500")
        self.assertEqual(self.json()["modules"]["filter"]["cutoff_hz"], 800)
        p.locator("#lfo-rate_hz-range").fill("500")
        self.assertEqual(self.json()["modules"]["lfo"]["rate_hz"], 1)
        p.fill("#filter-cutoff_hz", "16000")
        self.assertEqual(p.input_value("#filter-cutoff_hz-range"), "1000")
        p.screenshot(path=str(SHOTS / "desktop-instrument.png"), full_page=True)

    def test_v2_controls_map_to_v2_fields_and_convert_to_v3(self):
        p = self.page
        self.choose_preset("Soft Pad")
        self.assertEqual(self.json()["version"], 2)
        self.assertFalse(p.is_disabled("#filter-cutoff_hz"))         # v2 cutoff lives in synth
        for control in ("#filter-resonance", "#synth-osc2_level", "#lfo-rate_hz", "#reverb-mix", "#synth-voices"):
            self.assertTrue(p.is_disabled(control), control)
        p.fill("#filter-cutoff_hz", "1500")
        self.assertEqual(self.json()["modules"]["synth"]["cutoff_hz"], 1500)
        self.assertNotIn("filter", self.json()["modules"])
        before = self.json()
        p.click("#upgrade"); self.wait_idle()
        self.assertIn("Converted to v3", self.notice())
        upgraded = self.json()
        self.assertEqual(upgraded, forge_host.upgrade_patch(before))
        self.assertEqual((upgraded["version"], upgraded["routing"]), (3, "synth>delay>reverb>output"))
        self.assertEqual(upgraded["modules"]["filter"]["cutoff_hz"], 1500)
        self.assertFalse(p.is_disabled("#reverb-mix")); self.assertTrue(p.is_disabled("#upgrade"))

    def test_save_validates_and_reports_field_errors(self):
        p = self.page
        p.fill("#synth-attack_ms", "5000")
        p.click("#download")
        expect(p.locator("#notice")).to_have_class("error")
        self.assertIn("synth.attack_ms", self.notice())
        p.fill("#synth-attack_ms", "20")
        with p.expect_download() as info:
            p.click("#download")
        saved = json.loads(Path(info.value.path()).read_text())
        self.assertEqual(forge_host.validate_patch(saved), saved)
        self.assertEqual(saved["modules"]["synth"]["attack_ms"], 20)
        self.assertNotIn("api_key", json.dumps(saved))

    def test_import_v1_v2_v3_and_rejects_bad_files(self):
        p = self.page
        with tempfile.TemporaryDirectory() as folder:
            v1 = ROOT / "presets" / "02-slap.json"
            p.set_input_files("#import", str(v1)); self.wait_idle()
            self.assertEqual(self.json()["version"], 1)
            self.assertTrue(p.is_disabled("#synth-waveform"), "v1 delay patch must not expose synth controls")
            self.assertTrue(p.is_disabled("#routing")); self.assertFalse(p.is_disabled("#delay-mix"))
            self.assertEqual(p.input_value("#synth-attack_ms"), "")
            dup = Path(folder) / "dup.json"; dup.write_text('{"version":1,"version":1}')
            p.set_input_files("#import", str(dup)); self.wait_idle()
            self.assertIn("error", p.get_attribute("#notice", "class"))
            self.assertEqual(self.json()["version"], 1, "failed import must not replace the editor")
            v2 = ROOT / "presets" / "05-soft-pad.json"
            p.set_input_files("#import", str(v2)); self.wait_idle()
            self.assertEqual(self.json(), forge_host.load_patch(v2))
            self.assertFalse(p.is_disabled("#synth-waveform"))
            v3 = ROOT / "presets" / "08-acid-bass.json"
            p.set_input_files("#import", str(v3)); self.wait_idle()
            self.assertEqual(self.json(), forge_host.load_patch(v3))
            self.assertEqual(p.input_value("#synth-voices"), "1")

    def test_generate_with_mock_provider_keeps_key_ephemeral(self):
        p, provider = self.page, self.provider
        provider.status, provider.patch = 200, forge_host.load_patch(ROOT / "presets" / "08-acid-bass.json")
        provider.requests.clear()
        for name in ("openai", "gemini"):
            p.select_option("#provider", name)
            p.fill("#model", "test-model"); p.fill("#api-key", FAKE_KEY)
            p.fill("#prompt", "Gritty saw bass with a short slap echo")
            p.click("#generate"); self.wait_idle()
            self.assertIn("generated and validated", self.notice())
            self.assertEqual(self.json()["name"], provider.patch["name"])
            request = provider.requests[-1]
            self.assertNotIn(FAKE_KEY, request.data.decode()); self.assertNotIn(FAKE_KEY, request.full_url)
            self.assertIn(FAKE_KEY, json.dumps(dict(request.header_items())))
            self.assertEqual(json.loads(request.data)["text"]["format"]["schema"]["properties"]["version"]["enum"]
                             if name == "openai" else
                             json.loads(request.data)["generationConfig"]["responseFormat"]["text"]["schema"]["properties"]["version"]["enum"], [3])
        # storage_state() reads cookies/localStorage without page eval (blocked by the CSP).
        self.assertNotIn(FAKE_KEY, json.dumps(self.context.storage_state()))
        self.assertNotIn(FAKE_KEY, p.inner_text("#json"))
        p.click("#clear-key"); self.assertEqual(p.input_value("#api-key"), "")
        # Provider rejection: safe message, editor unchanged.
        before = self.json()
        provider.status = 401
        p.fill("#api-key", FAKE_KEY); p.click("#generate"); self.wait_idle()
        self.assertIn("API key was not accepted", self.notice())
        self.assertNotIn("secret body", self.notice())
        self.assertEqual(self.json(), before)
        # Wrong mode output (delay patch for instrument request) is rejected.
        provider.status, provider.patch = 200, forge_host.load_patch(ROOT / "presets" / "02-slap.json")
        p.click("#generate"); self.wait_idle()
        self.assertIn("error", p.get_attribute("#notice", "class"))
        provider.status = 200

    def test_device_round_trip_v2_v1_v3_capture_and_panic(self):
        p = self.page
        p.click("#ports"); self.wait_idle()
        p.select_option("#input-port", sim_device.INPUT); p.select_option("#output-port", sim_device.OUTPUT)
        self.choose_preset("Soft Pad")
        sent = self.json()
        p.click("#send"); self.wait_idle()
        self.assertIn("acknowledged", self.notice())
        self.assertIn("Firmware 0.5", p.inner_text("#device-state"))
        # Legacy v1 delay patch switches device to aux path.
        self.choose_preset("Short slap"); p.click("#send"); self.wait_idle()
        p.click("#capture"); self.wait_idle()
        self.assertEqual(self.json()["version"], 1)
        self.assertEqual(self.json()["name"], "Captured from Forge")
        # Back to an instrument and capture it: values survive 14-bit quantization.
        self.choose_preset("Soft Pad"); p.click("#send"); self.wait_idle()
        p.click("#capture"); self.wait_idle()
        captured = self.json()
        self.assertEqual(captured["version"], 2)
        self.assertEqual(captured["routing"], sent["routing"])
        for key, value in sent["modules"]["synth"].items():
            if key == "waveform": self.assertEqual(captured["modules"]["synth"][key], value)
            else: self.assertAlmostEqual(captured["modules"]["synth"][key], value, delta=max(1, value * 0.001))
        # v3 instrument: every module survives send -> capture within quantization.
        self.choose_preset("Warm Pad"); sent = self.json(); p.click("#send"); self.wait_idle()
        self.assertIn("acknowledged", self.notice())
        p.click("#capture"); self.wait_idle()
        captured = self.json()
        self.assertEqual((captured["version"], captured["routing"]), (3, sent["routing"]))
        for module, fields in sent["modules"].items():
            for key, value in fields.items():
                got = captured["modules"][module][key]
                if isinstance(value, (bool, str)) or isinstance(value, int) and key in ("voices", "osc2_semitones"):
                    self.assertEqual(got, value, f"{module}.{key}")
                else:
                    self.assertAlmostEqual(got, value, delta=max(0.01, abs(value) * 0.002), msg=f"{module}.{key}")
        p.click("#panic"); self.wait_idle()
        self.assertIn("Panic acknowledged", self.notice())
        # Panic must not change targets.
        p.click("#status"); self.wait_idle()
        self.assertIn("editor patch is unchanged", self.notice())

    def test_device_presets_store_recall_erase(self):
        p = self.page
        self.assertEqual(p.locator("#slots button").count(), 15)
        p.click("#slot-recall"); self.wait_idle()
        self.assertIn("Choose a slot", self.notice())
        p.click("#ports"); self.wait_idle()
        p.select_option("#input-port", sim_device.INPUT); p.select_option("#output-port", sim_device.OUTPUT)
        self.choose_preset("Acid Bass"); sent = self.json(); p.click("#send"); self.wait_idle()
        p.select_option("#bank", "2"); p.click("#slot-4")
        self.assertEqual(p.get_attribute("#slot-4", "aria-pressed"), "true")
        p.click("#slot-store"); self.wait_idle()
        self.assertIn("bank 2 slot 4", self.notice())
        expect(p.locator("#slot-4")).to_have_class("filled")
        self.choose_preset("Dry routing check"); p.click("#send"); self.wait_idle()       # device now plays something else
        p.click("#slot-recall"); self.wait_idle()
        recalled = self.json()
        self.assertEqual(recalled["name"], "Bank 2 slot 4")
        self.assertEqual(recalled["modules"]["synth"]["voices"], sent["modules"]["synth"]["voices"])
        self.assertAlmostEqual(recalled["modules"]["filter"]["cutoff_hz"], sent["modules"]["filter"]["cutoff_hz"], delta=1)
        # Erase needs a second click.
        p.click("#slot-erase"); self.wait_idle()
        self.assertIn("again within 4 seconds", self.notice())
        expect(p.locator("#slot-4")).to_have_class("filled")
        p.click("#slot-erase"); self.wait_idle()
        self.assertIn("erased", self.notice())
        expect(p.locator("#slot-4")).not_to_have_class("filled")
        p.click("#slot-recall"); self.wait_idle()
        self.assertIn("slot is empty", self.notice())
        p.select_option("#bank", "1"); self.assertEqual(p.get_attribute("#slot-4", "aria-pressed"), "false")

    def test_missing_ports_and_lost_reply(self):
        p = self.page
        p.click("#send"); self.wait_idle()
        self.assertIn("MIDI input and output", self.notice())
        # The status banner is sticky, so feedback stays visible next to the device buttons.
        box = p.locator("#notice").bounding_box()
        self.assertTrue(0 <= box["y"] < 900, "status message scrolled out of view")
        p.click("#ports"); self.wait_idle()
        p.select_option("#input-port", sim_device.INPUT); p.select_option("#output-port", sim_device.OUTPUT)
        self.device.drop_replies = True
        try:
            p.click("#send"); self.wait_idle()
            self.assertIn("may have applied", self.notice())
        finally:
            self.device.drop_replies = False

    def test_mobile_layout_has_no_horizontal_overflow(self):
        # Layout measurement needs page scripts; CSP bypass affects only this measuring context.
        context = self.browser.new_context(bypass_csp=True)
        page = context.new_page()
        page.goto(self.server.origin + "/")
        expect(page.locator("#notice")).to_contain_text("Ready")
        for width, height, name in ((390, 844, "mobile"), (768, 1024, "tablet")):
            page.set_viewport_size({"width": width, "height": height})
            overflow = page.evaluate("document.documentElement.scrollWidth - window.innerWidth")
            self.assertLessEqual(overflow, 0, f"{name} layout scrolls sideways")
            small = page.evaluate("""[...document.querySelectorAll('button,select,input:not([type=checkbox]):not([type=range]):not([type=file])')]
                .filter(e => e.offsetParent && e.getBoundingClientRect().height < 40).map(e => e.id)""")
            self.assertEqual(small, [], f"{name}: touch targets under 40px")
            page.screenshot(path=str(SHOTS / f"{name}.png"), full_page=True)
        context.close()

    def test_localhost_url_is_accepted(self):
        page = self.context.new_page()
        page.goto(f"http://localhost:{self.server.server_port}/")
        expect(page.locator("#notice")).to_contain_text("Ready", timeout=5000)
        page.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
