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
        expect(self.page.locator("#notice")).to_contain_text("Forge is lit")

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
        self.assertEqual(p.locator("#preset option").count(), len(list((ROOT / "presets").glob("*.json"))) + 1)
        patch_json = self.json()
        self.assertEqual(patch_json["version"], 3)                  # starts on the first v3 preset
        self.assertFalse(p.is_disabled("#upgrade"))                 # v3 can convert to v6
        self.assertTrue(p.is_disabled("#sampler-mode"))             # sampler controls need v4+
        self.assertTrue(p.is_disabled("#knobs-0"))                  # knob choices need v5
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

    def test_v2_controls_map_to_v2_fields_and_convert_to_v7(self):
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
        self.assertIn("Converted to v7", self.notice())
        upgraded = self.json()
        self.assertEqual(upgraded, forge_host.upgrade_patch(before, 7))
        self.assertEqual((upgraded["version"], upgraded["routing"], upgraded["knobs"], upgraded["harmony"], upgraded["parts"]),
                         (7, "synth>delay>reverb>output", ["default"] * 4, forge_host.HARMONY_DEFAULTS, forge_host.PARTS_DEFAULTS))
        self.assertEqual(upgraded["modules"]["filter"]["cutoff_hz"], forge_host.v2_cutoff(before["modules"]["synth"]))   # 0.17.1 conversion
        self.assertFalse(p.is_disabled("#reverb-mix")); self.assertTrue(p.is_disabled("#upgrade"))
        # 0.15.1: a v7 patch lists its three routes (the routing menu was empty for v7).
        self.assertEqual(len(p.locator("#routing option").all_inner_texts()), 3)
        p.select_option("#routing", "sampler>delay>reverb>output")
        self.assertEqual(self.json()["routing"], "sampler>delay>reverb>output")
        p.select_option("#routing", "synth>delay>reverb>output")
        # Knob choices (page 1 of the panel knobs) edit the v5 "knobs" list.
        self.assertFalse(p.is_disabled("#knobs-0"))
        self.assertIn("Filter: Cutoff", p.locator("#knobs-0 option").all_inner_texts())
        p.select_option("#knobs-0", "filter.cutoff_hz"); p.select_option("#knobs-3", "reverb.mix")
        self.assertEqual(self.json()["knobs"], ["filter.cutoff_hz", "default", "default", "reverb.mix"])
        self.assertEqual(forge_host.validate_patch(self.json())["knobs"][3], "reverb.mix")
        # Harmony controls (v6) edit the "harmony" object.
        p.check("#harmony-enabled"); p.select_option("#harmony-tonic", "A"); p.select_option("#harmony-mode", "dorian")
        p.select_option("#harmony-layout", "real"); p.uncheck("#harmony-voice_leading")
        self.assertEqual(forge_host.validate_patch(self.json())["harmony"],
                         {**forge_host.HARMONY_DEFAULTS, "enabled": True, "tonic": "A", "mode": "dorian", "layout": "real",
                          "voice_leading": False})
        # Arp, bass and tempo controls (v7) edit the "parts" object.
        p.select_option("#parts-arp-pattern", "updown"); p.select_option("#parts-arp-rate", "1/16t"); p.fill("#parts-arp-gate", "75")
        p.select_option("#parts-bass-mode", "octave"); p.select_option("#parts-bass-rate", "chord"); p.fill("#parts-clock-bpm", "96")
        p.uncheck("#parts-arp-latch")
        parts = forge_host.validate_patch(self.json())["parts"]
        self.assertEqual((parts["arp"], parts["bass"]["mode"], parts["bass"]["rate"], parts["clock"]["bpm"]),
                         ({"pattern": "updown", "rate": "1/16t", "octaves": 1, "gate": 75, "latch": False}, "octave", "chord", 96))

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

    def test_chat_copy_and_paste_needs_no_key_or_device(self):
        # The owner, 2026-10-12: no API keys and no hardware in the webapp; any chatbot writes the patch.
        p = self.page
        for gone in ("#provider", "#api-key", "#model", "#send", "#ports", "#slots", "#sample-slots"):
            self.assertEqual(p.locator(gone).count(), 0, gone)
        p.fill("#prompt", "Gritty saw bass with a short slap echo")
        p.click("#generate"); self.wait_idle()
        self.assertIn("paste it into any chatbot", self.notice())
        box = p.locator("#chat-prompt-text")
        if box.is_visible():                                   # clipboard refused: the prompt is shown to copy
            self.assertIn("Gritty saw bass", box.input_value())
        bass = forge_host.upgrade_patch(forge_host.load_patch(ROOT / "presets" / "08-acid-bass.json"), 7)
        p.fill("#reply", "Here you go!\n```json\n" + json.dumps(bass) + "\n```\nHave fun.")
        p.click("#use-reply"); self.wait_idle()
        self.assertIn("valid patch", self.notice()); self.assertEqual(self.json()["name"], bass["name"])
        before = self.json()
        for bad in ("sorry, I can't", "```json\n{\"version\": 7}\n```", json.dumps(forge_host.load_patch(ROOT / "presets" / "02-slap.json"))):
            p.fill("#reply", bad); p.click("#use-reply"); self.wait_idle()
            self.assertIn("error", p.get_attribute("#notice", "class")); self.assertEqual(self.json(), before)
        p.screenshot(path=str(SHOTS / "desktop-chat.png"), full_page=True)

    def test_sampler_controls(self):
        p = self.page
        self.choose_preset("Recorded Keys")
        self.assertEqual(self.json()["version"], 4); self.assertFalse(p.is_disabled("#upgrade"))
        p.select_option("#sampler-mode", "kit"); p.select_option("#sampler-bank", "c"); p.check("#sampler-reverse")
        p.uncheck("#sampler-hold"); p.fill("#sampler-crossfade_ms", "80"); p.fill("#synth-voices", "7")
        s = self.json()["modules"]["sampler"]
        self.assertEqual((s["mode"], s["bank"], s["reverse"], s["hold"], s["crossfade_ms"]), ("kit", "c", True, False, 80))
        self.assertEqual(forge_host.validate_patch(self.json())["modules"]["synth"]["voices"], 7)
        p.fill("#sampler-start", "0.8"); p.fill("#sampler-end", "0.2"); p.click("#download")     # start must stay below end
        expect(p.locator("#notice")).to_have_class("error"); self.assertIn("sampler.start", self.notice())
        p.fill("#sampler-start", "0.1"); p.fill("#sampler-end", "0.9")
        p.screenshot(path=str(SHOTS / "desktop-samples.png"), full_page=True)

    def test_mobile_layout_has_no_horizontal_overflow(self):
        # Layout measurement needs page scripts; CSP bypass affects only this measuring context.
        context = self.browser.new_context(bypass_csp=True)
        page = context.new_page()
        page.goto(self.server.origin + "/")
        expect(page.locator("#notice")).to_contain_text("Forge is lit")
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
        expect(page.locator("#notice")).to_contain_text("Forge is lit", timeout=5000)
        page.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
