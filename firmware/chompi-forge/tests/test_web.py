import copy
import http.client
import io
import json
from pathlib import Path
import sys
import threading
import unittest
from unittest.mock import Mock, patch
import urllib.error

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_ai
import forge_host
import forge_web

PRESET = forge_host.load_patch(ROOT / "presets/02-slap.json")


def response(provider, preset=PRESET):
    content = json.dumps(preset)
    if provider == "openai":
        return {"status": "completed", "output": [{"type": "reasoning"}, {"type": "message",
                "content": [{"type": "output_text", "text": content}]}]}
    return {"candidates": [{"finishReason": "STOP", "content": {"parts": [
        {"text": "internal reasoning is not a patch", "thought": True}, {"text": content}]}}]}


class ProviderTests(unittest.TestCase):
    def generate(self, provider, envelope, opener=None):
        return forge_ai.generate_patch(provider, "test-secret", "test-model", "Slap echo",
            opener or (lambda request, timeout: io.BytesIO(json.dumps(envelope).encode())))

    def test_both_provider_requests_and_validated_responses(self):
        for provider in ("openai", "gemini"):
            with self.subTest(provider=provider):
                opener = Mock(return_value=io.BytesIO(json.dumps(response(provider)).encode()))
                self.assertEqual(self.generate(provider, None, opener), PRESET)
                request = opener.call_args.args[0]
                body = json.loads(request.data)
                self.assertNotIn("test-secret", request.full_url)
                self.assertNotIn("test-secret", request.data.decode())
                if provider == "openai":
                    self.assertEqual(request.full_url, "https://api.openai.com/v1/responses")
                    self.assertEqual(request.get_header("Authorization"), "Bearer test-secret")
                    self.assertFalse(body["store"])
                    schema = body["text"]["format"]["schema"]
                    self.assertTrue(body["text"]["format"]["strict"])
                else:
                    self.assertTrue(request.full_url.endswith("/test-model:generateContent"))
                    self.assertEqual(request.get_header("X-goog-api-key"), "test-secret")
                    schema = body["generationConfig"]["responseFormat"]["text"]["schema"]
                self.assertFalse(schema["additionalProperties"])
                self.assertEqual(schema["properties"]["engine"]["enum"], ["stereo_delay"])

    def test_invalid_and_incomplete_outputs_never_become_patches(self):
        invalid = copy.deepcopy(PRESET)
        invalid["parameters"]["feedback"] = 1
        for provider in ("openai", "gemini"):
            for envelope in ({}, [], response(provider, invalid)):
                with self.subTest(provider=provider, envelope=envelope):
                    with self.assertRaises(forge_ai.ProviderError):
                        self.generate(provider, envelope)
        for envelope in ({"status": "incomplete", "output": []},
                {"status": "completed", "output": [{"type": "message", "content": [{"type": "refusal"}]}]}):
            with self.assertRaises(forge_ai.ProviderError):
                self.generate("openai", envelope)
        envelope = response("gemini")
        envelope["candidates"][0]["finishReason"] = "MAX_TOKENS"
        with self.assertRaises(forge_ai.ProviderError):
            self.generate("gemini", envelope)

    def test_provider_error_bodies_are_not_exposed(self):
        for code in (400, 401, 403, 404, 429, 500):
            opener = Mock(side_effect=urllib.error.HTTPError("https://provider", code, "test-secret", {},
                                                           io.BytesIO(b"test-secret")))
            with self.assertRaises(forge_ai.ProviderError) as caught:
                self.generate("openai", None, opener)
            self.assertNotIn("test-secret", str(caught.exception))

    def test_invalid_inputs_make_no_request(self):
        opener = Mock()
        for change in ({"provider": "other"}, {"api_key": "x\r\nInjected: y"},
                       {"model": "../../other"}, {"model": None}, {"prompt": ""}, {"prompt": "a" * 4001}):
            args = dict(provider="openai", api_key="key", model="model", prompt="echo", opener=opener)
            args.update(change)
            with self.assertRaises(ValueError):
                forge_ai.generate_patch(**args)
        opener.assert_not_called()

    def test_duplicate_keys_and_oversize_are_rejected(self):
        envelope = response("openai")
        envelope["output"][1]["content"][0]["text"] = '{"version":1,"version":1}'
        with self.assertRaises(forge_ai.ProviderError):
            self.generate("openai", envelope)
        with self.assertRaises(forge_ai.ProviderError):
            self.generate("gemini", None, lambda request, timeout: io.BytesIO(b"x" * 65537))

    def test_redirect_and_network_errors_are_safe(self):
        with self.assertRaises(forge_ai.ProviderError):
            forge_ai.NoRedirect().redirect_request(None, None, 302, "", {}, "https://other")
        for error in (TimeoutError("secret"), urllib.error.URLError("secret")):
            with self.assertRaises(forge_ai.ProviderError) as caught:
                self.generate("gemini", None, Mock(side_effect=error))
            self.assertNotIn("secret", str(caught.exception))


class WebTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = forge_web.ForgeServer(0)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def request(self, path, body=None, headers=None, raw=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=5)
        fields = {"Host": self.server.authority, "Origin": self.server.origin,
                  "Content-Type": "application/json", "X-Forge-Token": self.server.token}
        fields.update(headers or {})
        connection.request("POST" if body is not None or raw is not None else "GET", path,
                           body=raw if raw is not None else json.dumps(body) if body is not None else None,
                           headers=fields)
        reply = connection.getresponse()
        data = reply.read()
        result = (reply.status, dict(reply.getheaders()), data)
        connection.close()
        return result

    def test_assets_session_and_no_arbitrary_file_access(self):
        for path in ("/", "/app.js", "/style.css", "/api/session"):
            status, headers, data = self.request(path)
            self.assertEqual(status, 200)
            self.assertEqual(headers["Cache-Control"], "no-store")
            self.assertIn("frame-ancestors 'none'", headers["Content-Security-Policy"])
        self.assertEqual(len(json.loads(data)["presets"]), 9)
        for path in ("/../forge_ai.py", "/forge_web.py", "/?api_key=secret"):
            self.assertEqual(self.request(path)[0], 404)

    def test_cross_origin_and_missing_tokens_are_rejected(self):
        for headers in ({"Host": "evil.example"}, {"Origin": "https://evil.example"},
                        {"Sec-Fetch-Site": "cross-site"}, {"X-Forge-Token": ""}):
            self.assertEqual(self.request("/api/send", {}, headers)[0], 403)
        self.assertEqual(self.request("/api/session", headers={"Host": "evil.example"})[0], 403)

    def test_localhost_spelling_is_accepted_but_origins_must_match(self):
        local = f"localhost:{self.server.server_port}"
        self.assertEqual(self.request("/api/session", headers={"Host": local, "Origin": "http://" + local})[0], 200)
        self.assertEqual(self.request("/api/validate", {"patch": PRESET},
                                      {"Host": local, "Origin": "http://" + local})[0], 200)
        for headers in ({"Host": local, "Origin": self.server.origin},
                        {"Host": "localhost:1", "Origin": "http://localhost:1"},
                        {"Host": "127.0.0.1.evil.example", "Origin": "http://127.0.0.1.evil.example"}):
            self.assertEqual(self.request("/api/validate", {"patch": PRESET}, headers)[0], 403)

    def test_validation_errors_name_the_field_and_device_rejections_are_reported(self):
        bad = copy.deepcopy(PRESET); bad["parameters"]["time_ms"] = 5
        status, _, data = self.request("/api/validate", {"patch": bad})
        self.assertEqual(status, 400)
        self.assertIn("time_ms", json.loads(data)["error"])
        with patch.object(forge_host, "exchange", side_effect=RuntimeError("Device rejected request: device queue busy")):
            status, _, data = self.request("/api/status", {"input": "in", "output": "out"})
            self.assertEqual(status, 502)
            self.assertIn("queue busy", json.loads(data)["error"])

    def test_upgrade_endpoint_converts_validates_and_needs_the_token(self):
        status, _, data = self.request("/api/upgrade", {"patch": PRESET})
        self.assertEqual(status, 200)
        upgraded = json.loads(data)["patch"]
        self.assertEqual((upgraded["version"], upgraded["routing"]), (3, "aux>delay>reverb>output"))
        self.assertEqual(self.request("/api/upgrade", {"patch": PRESET}, {"X-Forge-Token": "wrong"})[0], 403)
        bad = copy.deepcopy(PRESET); bad["parameters"]["mix"] = 2
        self.assertEqual(self.request("/api/upgrade", {"patch": bad})[0], 400)

    def test_validation_and_malformed_request_bodies(self):
        self.assertEqual(self.request("/api/validate", {"patch": PRESET})[0], 200)
        for raw in (b'{"patch":{},"patch":{}}', b"[]", b'{"patch":NaN}', b"x" * 65537):
            self.assertEqual(self.request("/api/validate", raw=raw)[0], 400)

    def test_generate_is_separate_from_midi_and_returns_no_key(self):
        with patch.object(forge_ai, "generate_patch", return_value=PRESET) as generate, \
                patch.object(forge_host, "exchange") as exchange:
            status, _, data = self.request("/api/generate", {"provider": "gemini", "model": "model",
                                                            "api_key": "test-secret", "prompt": "echo"})
            self.assertEqual(status, 200)
            self.assertEqual(json.loads(data)["patch"], PRESET)
            self.assertNotIn(b"test-secret", data)
            generate.assert_called_once_with("gemini", "test-secret", "model", "echo", kind="delay")
            exchange.assert_not_called()

    def test_send_uses_existing_validated_protocol_and_explicit_ports(self):
        result = {"firmware": "0.2", "patch": PRESET}
        with patch.object(forge_host, "exchange", return_value=result) as exchange:
            self.assertEqual(self.request("/api/send", {"patch": PRESET, "input": "in", "output": "out"})[0], 200)
            payload, source, destination = exchange.call_args.args
            self.assertEqual(payload, forge_host.encode_patch(PRESET, forge_host.read14(payload, 5)))
            self.assertEqual((source, destination), ("in", "out"))
            exchange.reset_mock()
            bad = copy.deepcopy(PRESET); bad["parameters"]["level"] = 2
            self.assertEqual(self.request("/api/send", {"patch": bad, "input": "in", "output": "out"})[0], 400)
            self.assertEqual(self.request("/api/send", {"patch": PRESET})[0], 400)
            exchange.assert_not_called()

    def test_status_ports_timeout_and_busy(self):
        with patch.object(forge_host, "midi_module", return_value=Mock(
                get_input_names=lambda: ["in"], get_output_names=lambda: ["out"])):
            self.assertEqual(json.loads(self.request("/api/ports", {})[2]), {"inputs": ["in"], "outputs": ["out"]})
        with patch.object(forge_host, "exchange", side_effect=TimeoutError("private data")) as exchange:
            status, _, data = self.request("/api/status", {"input": "in", "output": "out"})
            self.assertEqual(status, 504)
            self.assertIn(b"may have applied", data)
            self.assertEqual(exchange.call_args.args[0][4], 2)
        with patch.object(forge_host, "exchange", return_value={"firmware": "0.3"}) as exchange:
            self.assertEqual(self.request("/api/panic", {"input": "in", "output": "out"})[0], 200)
            self.assertEqual(exchange.call_args.args[0][4], 3)
        for lock, route in ((self.server.midi_lock, "ports"), (self.server.ai_lock, "generate")):
            with lock:
                self.assertEqual(self.request("/api/" + route, {})[0], 409)


if __name__ == "__main__":
    unittest.main()
