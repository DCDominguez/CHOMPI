import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_ai
import forge_ai_check
import forge_host

KEY = "sk-live-check-secret-999"
PATCH = forge_host.load_patch(ROOT / "presets/04-glass-keys.json")


class AiCheckTests(unittest.TestCase):
    def test_record_never_contains_key_and_captures_outcome(self):
        ticks = iter([10.0, 12.5])
        ok = forge_ai_check.run("gemini", "m", "warm keys", "instrument", KEY,
                                generate=lambda *a, **k: PATCH, clock=lambda: next(ticks))
        self.assertTrue(ok["ok"]); self.assertEqual(ok["seconds"], 2.5); self.assertEqual(ok["patch"], PATCH)
        def fail(*a, **k): raise forge_ai.ProviderError("API key was not accepted.")
        bad = forge_ai_check.run("openai", "m", "x", "instrument", KEY, generate=fail)
        self.assertFalse(bad["ok"]); self.assertIn("not accepted", bad["error"])
        self.assertNotIn(KEY, json.dumps([ok, bad]))

    def test_cli_reads_hidden_key_and_appends_keyless_record(self):
        with tempfile.TemporaryDirectory() as folder, \
                patch.object(forge_ai_check.getpass, "getpass", return_value=KEY) as hidden, \
                patch.object(forge_ai, "generate_patch", return_value=PATCH) as generate, \
                patch.dict(forge_ai_check.os.environ, {}, clear=True), \
                patch("sys.stdout", io.StringIO()) as out:
            record_path = Path(folder) / "r.jsonl"
            code = forge_ai_check.main(["--provider", "openai", "--model", "m", "--record", str(record_path)])
            hidden.assert_called_once()
            self.assertEqual(code, 0)
            text = record_path.read_text()
            self.assertNotIn(KEY, text); self.assertNotIn(KEY, out.getvalue())
            self.assertEqual(json.loads(text)["patch"], PATCH)
            self.assertEqual(generate.call_args.args[1], KEY)


if __name__ == "__main__":
    unittest.main()
