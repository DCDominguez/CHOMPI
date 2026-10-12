"""The static forge page (docs/workshop): its generated files are current (host/build_site.py), and the
chatbot prompt it builds is exactly forge_ai.chat_prompt's. Node.js for the prompt check (skips without)."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import build_site
import forge_ai

NODE = os.environ.get("FORGE_NODE") or shutil.which("node") or next(
    (str(p) for p in (Path.home() / "AppData/Local/hermes/node/node.exe",) if p.exists()), None)


class SiteTests(unittest.TestCase):
    def test_generated_files_are_current(self):
        for path, text in build_site.outputs().items():
            self.assertTrue(path.exists(), f"{path.name} missing: run python host/build_site.py")
            on_disk = path.read_text(encoding="utf-8").replace("\r\n", "\n")
            self.assertEqual(on_disk, text, f"{path.name} is stale: run python host/build_site.py")

    @unittest.skipIf(NODE is None, "Node.js not found (set FORGE_NODE)")
    def test_prompt_matches_python(self):
        ideas = ["warm pad with a slow swell", "  glassy bell, long echo\n", "8-bit bass 🎮 with \"quotes\""]
        with tempfile.TemporaryDirectory() as folder:
            runner = Path(folder) / "run.js"
            runner.write_text("global.window = {}; require(%s); const D = window.CHOOWI_DATA;\n"
                              "console.log(JSON.stringify(%s.map(i => D.promptPrefix + i.trim() + D.promptSuffix)));"
                              % (json.dumps(str(build_site.SITE / "data.js")), json.dumps(ideas)), encoding="utf-8")
            out = subprocess.run([NODE, str(runner)], capture_output=True, text=True, encoding="utf-8", timeout=60)
            self.assertEqual(out.returncode, 0, out.stderr)
            for idea, js in zip(ideas, json.loads(out.stdout)):
                self.assertEqual(js, forge_ai.chat_prompt(idea, "instrument"))

    def test_presets_are_v8_and_valid(self):
        import forge_host
        for patch in json.loads(build_site.data_js().split("window.CHOOWI_DATA = ", 1)[1].rstrip().rstrip(";"))["presets"]:
            self.assertEqual(patch["version"], 8)
            self.assertEqual(forge_host.validate_patch(patch), patch)


if __name__ == "__main__":
    unittest.main()
