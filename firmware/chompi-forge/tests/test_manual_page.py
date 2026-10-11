"""The illustrated manual draws its keyboards, panel and menu map with a script; a syntax
error leaves every figure empty (found 2026-10-11: a raw line break inside a string).
Opens it in headless Chromium and checks every figure and legend is drawn. Skips without
Playwright / Chromium (CI's browser job has them)."""
import unittest
from pathlib import Path

MANUAL = Path(__file__).resolve().parents[3] / "docs" / "forge" / "ILLUSTRATED_MANUAL.html"
FIGURES = ["fig-panel", "fig-knobs", "fig-menu", "fig-tape", "fig-presets", "fig-harmony", "fig-parts",
           "fig-synth", "fig-recorder", "fig-magic", "fig-chain", "fig-countin", "fig-looper"]
LEGENDS = ["lg-steps", "lg-chompi", "lg-sw6", "lg-safe"]


class ManualPageTests(unittest.TestCase):
    def test_every_figure_is_drawn(self):
        try:
            from playwright.sync_api import sync_playwright
        except ImportError:
            self.skipTest("Playwright not installed")
        with sync_playwright() as p:
            try: browser = p.chromium.launch()
            except Exception as error: self.skipTest(f"no Chromium: {error}")
            page = browser.new_page(); errors = []
            page.on("pageerror", lambda e: errors.append(str(e)))
            page.goto(MANUAL.as_uri()); page.wait_for_timeout(500)
            empty = [f for f in FIGURES + LEGENDS if page.eval_on_selector(f"#{f}", "e => e.children.length") == 0]
            browser.close()
        self.assertEqual(errors, []); self.assertEqual(empty, [])


if __name__ == "__main__":
    unittest.main()
