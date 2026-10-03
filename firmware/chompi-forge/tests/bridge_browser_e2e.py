"""Real browser checks against the shared C++ Inspector simulation. No hardware evidence."""
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"host"))
import forge_web
from playwright.sync_api import expect, sync_playwright


class BridgeBrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server=forge_web.ForgeServer(0, ROOT/"build"/("forge_probe.exe" if sys.platform=="win32" else "forge_probe"))
        cls.thread=threading.Thread(target=cls.server.serve_forever,daemon=True); cls.thread.start()
        cls.pw=sync_playwright().start(); cls.browser=cls.pw.chromium.launch()
        (ROOT/"build/browser").mkdir(parents=True,exist_ok=True)

    @classmethod
    def tearDownClass(cls):
        cls.browser.close(); cls.pw.stop()
        cls.server.shutdown(); cls.server.server_close(); cls.thread.join()

    def setUp(self):
        self.context=self.browser.new_context(viewport={"width":1440,"height":1000},accept_downloads=True)
        self.page=self.context.new_page(); self.errors=[]
        self.page.on("pageerror",lambda e:self.errors.append(str(e)))
        self.page.on("console",lambda m:self.errors.append(m.text) if m.type=="error" and not m.text.startswith("Failed to load resource") else None)
        self.page.goto(self.server.origin+"/inspector")
        expect(self.page.locator("#notice")).to_contain_text("Ready")
        self.page.select_option("#mode","simulation"); self.page.fill("#metadata","Browser QA · simulation only")
        self.page.click("#connect"); expect(self.page.locator("#notice")).to_contain_text("SIMULATION")
        self.page.click("#pause"); expect(self.page.locator("#pause")).to_have_text("Resume polling")

    def tearDown(self):
        if self.page.locator("#disconnect").is_enabled(): self.page.click("#disconnect")
        self.context.close(); self.assertEqual(self.errors,[])

    def test_guided_session_controls_events_and_export(self):
        expect(self.page.locator("#send-patch")).to_be_disabled()
        expect(self.page.locator("#system-summary")).to_contain_text("Unavailable")
        self.page.check("#arm")
        self.page.select_option("#preset",label="Glass Keys")
        self.page.click("#send-patch")
        expect(self.page.locator("#action-result")).to_contain_text('"patch"')
        self.page.get_by_text("Virtual panel",exact=True).click()
        self.page.click("#key-down")
        expect(self.page.locator("#notice")).to_contain_text("OVERRIDES ACTIVE")
        expect(self.page.locator("#events")).to_contain_text("key_down [injected]")
        self.assertEqual(self.page.locator(".key.physical").count(),0)
        self.assertEqual(self.page.locator(".key.logical").count(),1)
        self.page.click("#key-up"); self.page.click("#release")
        expect(self.page.locator("#notice")).not_to_contain_text("OVERRIDES ACTIVE")
        self.page.get_by_text("MIDI test signals",exact=True).click()
        self.page.click("#play-note")
        expect(self.page.locator("#action-result")).to_contain_text('"note_off_sent": true')
        self.page.fill("#observation","Injected activity only; physical controls still pending.")
        self.page.select_option("#result","blocked"); self.page.click("#save-check")
        expect(self.page.locator("#check-progress")).to_contain_text("1 / 62")
        self.page.get_by_text("SD presets & samples",exact=True).click()
        self.page.click('[data-preset="store"]')
        expect(self.page.locator("#notice")).to_contain_text("Confirm the SD")
        self.page.check("#confirm-storage"); self.page.click('[data-preset="store"]')
        expect(self.page.locator("#action-result")).to_contain_text('"stored"')
        expect(self.page.locator("#confirm-storage")).not_to_be_checked()
        self.page.locator("#telemetry").scroll_into_view_if_needed()
        self.page.screenshot(path=str(ROOT/"build/browser/bridge-desktop.png"),full_page=True)
        self.page.click("#disconnect")
        with self.page.expect_download() as download: self.page.click("#export")
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"session.json"; download.value.save_as(path); report=json.loads(path.read_text())
        self.assertEqual(report["mode"],"simulation")
        self.assertFalse(report["hardware_verified"])
        self.assertEqual(report["checks"]["1A.1"]["result"],"blocked")
        self.assertTrue(any(r["kind"]=="injection" for r in report["records"]))
        self.assertTrue(any(r["kind"]=="disconnect" for r in report["records"]))
        with self.page.expect_download() as download: self.page.click("#jsonl")
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"trace.jsonl"; download.value.save_as(path)
            self.assertEqual(json.loads(path.read_text().splitlines()[0])["kind"],"session")

    def test_automatic_checks_in_simulation(self):
        expect(self.page.locator("#autorun")).to_be_enabled()
        self.page.click("#detect")                     # the simulation has no audio: explained, session kept
        expect(self.page.locator("#notice")).to_contain_text("simulation has no audio")
        self.page.once("dialog",lambda dialog: dialog.accept())
        self.page.click("#autorun")
        expect(self.page.locator("#autorun")).to_be_disabled()
        expect(self.page.locator("#notice")).to_contain_text("Automatic checks finished",timeout=60000)
        expect(self.page.locator("#auto-results")).to_contain_text("PASS 3.17")
        expect(self.page.locator("#auto-results")).to_contain_text("SKIPPED 2.1")
        expect(self.page.locator("#autorun")).to_be_enabled()
        self.assertEqual(self.page.locator(".auto-step.fail, .auto-step.error").count(),0)
        self.page.locator("#auto").screenshot(path=str(ROOT/"build/browser/bridge-automatic.png"))
        self.page.click("#pause")                      # polling resumes normally after the job
        expect(self.page.locator("#notice")).to_contain_text("snapshot")

    def test_mobile_layout_and_connection_failure(self):
        self.page.set_viewport_size({"width":390,"height":844})
        self.assertLessEqual(self.page.evaluate("document.documentElement.scrollWidth"),390)
        self.page.screenshot(path=str(ROOT/"build/browser/bridge-mobile.png"),full_page=True)
        # Failure from the real server must leave the last state visibly stale and exportable.
        with self.server.bridge.lock:
            self.server.bridge.disconnect("Test transport loss")
        self.page.click("#pause")
        expect(self.page.locator("#notice")).to_contain_text("disconnected")
        # The heartbeat handles loss even while polling remains paused.
        expect(self.page.locator("#connect")).to_be_enabled(timeout=12000)
        expect(self.page.locator("#telemetry")).to_have_class("grid telemetry stale")

    def test_reload_preserves_report_access(self):
        self.page.fill("#observation","Saved before reload")
        self.page.select_option("#result","blocked"); self.page.click("#save-check")
        expect(self.page.locator("#notice")).to_contain_text("Recorded")
        self.page.reload()
        expect(self.page.locator("#export")).to_be_enabled()
        expect(self.page.locator("#observation")).to_have_value("Saved before reload")


if __name__=="__main__": unittest.main()
