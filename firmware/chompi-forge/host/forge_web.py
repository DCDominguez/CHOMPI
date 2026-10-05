#!/usr/bin/env python3
"""Forge local webapp. Run on the computer connected to CHOMPI (Python 3.10+)."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import secrets
import sys
import socket
import threading
import time
import webbrowser

import forge_ai
import forge_host as host
import forge_bridge

ROOT = Path(os.environ.get("FORGE_HOST_DIR") or Path(__file__).resolve().parent)   # FORGE_HOST_DIR: Forge Bridge.exe
ASSETS = {"/": ("index.html", "text/html"), "/app.js": ("app.js", "text/javascript"),
          "/style.css": ("style.css", "text/css"),
          "/inspector": ("inspector.html", "text/html"),
          "/inspector.js": ("inspector.js", "text/javascript"),
          "/inspector.css": ("inspector.css", "text/css")}


def validate_samples(samples):
    """The client echoes a sample list it read; check its shape before it reaches a prompt."""
    try:
        clean = {"samples": {mode: {bank: [s for s in samples["samples"][mode][bank]] for bank in host.SAMPLE_BANKS}
                             for mode in host.SAMPLE_MODES},
                 "recording": samples.get("recording") is True,
                 "recording_seconds": float(samples.get("recording_seconds", 0))}
        if any(type(s) is not int or not 1 <= s <= host.SAMPLE_SLOTS for mode in clean["samples"].values()
               for slots in mode.values() for s in slots) or not 0 <= clean["recording_seconds"] < 10000:
            raise ValueError
        return clean
    except (KeyError, TypeError, AttributeError, ValueError):
        raise ValueError("Invalid sample list") from None


class ForgeServer(ThreadingHTTPServer):
    daemon_threads = True
    # Windows' SO_REUSEADDR lets a second server bind a port another program already serves, and the
    # browser then reaches that other program (seen 2026-10-04). Insist on an exclusive port there.
    allow_reuse_address = sys.platform != "win32"

    def server_bind(self):
        if sys.platform == "win32": self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()

    def __init__(self, port=8765, probe=None):
        super().__init__(("127.0.0.1", port), Handler)
        self.authority = f"127.0.0.1:{self.server_port}"
        self.origin = "http://" + self.authority
        # Both loopback spellings are accepted; any other Host (DNS rebinding) is refused.
        self.authorities = (self.authority, f"localhost:{self.server_port}")
        self.token = secrets.token_urlsafe(32)
        self.midi_lock = threading.Lock()
        self.ai_lock = threading.Lock()
        self.bridge = forge_bridge.Bridge(self.midi_lock, probe)

    def server_close(self):
        if hasattr(self, "bridge"): self.bridge.close()      # absent when binding the port failed
        super().server_close()

    def handle_error(self, request, client_address):
        # Never dump request bodies, provider responses or credentials to stderr.
        pass


class Handler(BaseHTTPRequestHandler):
    server_version = "ForgeLocal"

    def setup(self):
        super().setup()
        self.connection.settimeout(100)

    def log_message(self, format, *args):
        pass

    def reply(self, status, value, mime="application/json"):
        data = json.dumps(value, allow_nan=False).encode() if mime == "application/json" else value
        self.send_response(status)
        self.send_header("Content-Type", mime + "; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; "
                         "style-src 'self'; connect-src 'self'; img-src 'self' data:; "
                         "frame-ancestors 'none'; base-uri 'none'; form-action 'none'")
        self.end_headers()
        self.wfile.write(data)
        # Closing a Windows socket with unread POST bytes can reset the connection
        # before the client receives the rejection. Drain a bounded amount only,
        # after sending the response; never parse rejected bodies or log them.
        if status >= 400 and self.command == "POST" and not getattr(self, "body_read", False):
            try:
                remaining = min(max(0, int(self.headers.get("Content-Length", "0"))), 65537)
                deadline = time.monotonic() + .25
                while remaining > 0 and time.monotonic() < deadline:
                    self.connection.settimeout(max(.001, deadline-time.monotonic()))
                    chunk = self.rfile.read1(min(remaining, 16384))
                    if not chunk: break
                    remaining -= len(chunk)
            except (ValueError, OSError):
                pass

    def allowed(self):
        host = self.headers.get("Host")
        return (host in self.server.authorities
                and self.headers.get("Origin", "http://" + host) == "http://" + host
                and self.headers.get("Sec-Fetch-Site", "same-origin") in ("same-origin", "none"))

    def do_GET(self):
        if not self.allowed():
            return self.reply(403, {"error": "Use the local URL printed by Forge"})
        if self.path in ASSETS:
            filename, mime = ASSETS[self.path]
            return self.reply(200, (ROOT / "web" / filename).read_bytes(), mime)
        if self.path == "/api/session":
            return self.reply(200, {"token": self.server.token,
                "presets": [host.load_patch(p) for p in sorted((ROOT.parent / "presets").glob("*.json"))],
                "checks": forge_bridge.CHECKS, "simulation_available": bool(self.server.bridge.probe)})
        return self.reply(404, {"error": "Not found"})

    def do_POST(self):
        if not self.allowed() or not secrets.compare_digest(
                self.headers.get("X-Forge-Token", "").encode(), self.server.token.encode()):
            return self.reply(403, {"error": "Session expired or request blocked; reload Forge"})
        try:
            if self.headers.get("Content-Type") != "application/json" or self.headers.get("Transfer-Encoding"):
                raise ValueError("Send a JSON request")
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 65536:
                raise ValueError("Request must be 1–65536 bytes")
            raw_body = self.rfile.read(length)
            self.body_read = True
            body = host.parse_json(raw_body.decode("utf-8"))
            if not isinstance(body, dict):
                raise ValueError("Request must be an object")
            if self.path.startswith("/api/bridge/"):
                result = self.server.bridge.request(self.path.removeprefix("/api/bridge/"), body)
            elif self.path == "/api/validate":
                result = {"patch": host.validate_patch(body.get("patch"))}
            elif self.path == "/api/upgrade":
                to = body.get("to", 3)
                if to not in (3, 4, 5, 6): raise ValueError("Upgrade target must be 3, 4, 5 or 6")
                result = {"patch": host.upgrade_patch(body.get("patch"), to)}
            elif self.path == "/api/generate":
                if not self.server.ai_lock.acquire(blocking=False):
                    return self.reply(409, {"error": "A generation is already in progress"})
                try:
                    samples = body.get("samples")
                    if samples is not None: samples = validate_samples(samples)
                    result = {"patch": forge_ai.generate_patch(body.get("provider"), body.get("api_key"),
                                body.get("model"), body.get("prompt"), kind=body.get("kind", "delay"), samples=samples)}
                finally:
                    body.pop("api_key", None)
                    self.server.ai_lock.release()
            elif self.path in ("/api/ports", "/api/status", "/api/send", "/api/panic", "/api/preset", "/api/samples"):
                if not self.server.midi_lock.acquire(blocking=False):
                    return self.reply(409, {"error": "A MIDI request is already in progress"})
                try:
                    if self.path == "/api/ports":
                        midi = host.midi_module()
                        result = {"inputs": midi.get_input_names(), "outputs": midi.get_output_names()}
                    else:
                        for key in ("input", "output"):
                            if not isinstance(body.get(key), str) or not body[key]:
                                raise ValueError("Select both MIDI input and output ports")
                        seq = secrets.randbelow(16384)
                        if self.path == "/api/samples":
                            action = body.get("action")
                            if action not in ("list", "save", "erase", "copy"):
                                raise ValueError("Choose list, save, erase or copy")
                            payload = (host.sample_message(8, seq) if action == "list" else
                                       host.sample_message(9, seq, action, body.get("mode"), body.get("bank"), body.get("slot"),
                                                           body.get("to_mode"), body.get("to_bank"), body.get("to_slot")))
                        elif self.path == "/api/preset":
                            opcodes = {"store": 4, "recall": 5, "erase": 6, "list": 7}
                            if body.get("action") not in opcodes:
                                raise ValueError("Choose store, recall, erase or list")
                            payload = host.preset_message(opcodes[body["action"]], seq, body.get("bank"), body.get("slot"))
                        else:
                            payload = (host.encode_patch(body.get("patch"), seq) if self.path == "/api/send"
                                       else host.message(3 if self.path == "/api/panic" else 2, seq))
                        result = host.exchange(payload, body["input"], body["output"])
                finally:
                    self.server.midi_lock.release()
            else:
                return self.reply(404, {"error": "Not found"})
            return self.reply(200, result)
        except forge_ai.ProviderError as error:
            return self.reply(502, {"error": str(error)})
        except TimeoutError:
            return self.reply(504, {"error": "No acknowledgement. The patch may have applied; read device status before retrying."})
        except UnicodeError:
            return self.reply(400, {"error": "Invalid request: send UTF-8 JSON."})
        except ValueError as error:
            # Validation messages are fixed host strings or JSON positions; they never contain keys.
            return self.reply(400, {"error": f"Invalid request or patch: {error}"})
        except RuntimeError as error:
            # Device rejections/acknowledgement mismatches and missing MIDI dependencies.
            return self.reply(502, {"error": str(error)})
        except Exception:
            return self.reply(503, {"error": "Operation failed. For MIDI, check dependencies, ports and the device; close other MIDI hosts."})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, help="default: 8765, or the next free port up to 8775")
    parser.add_argument("--probe", type=Path, help="Enable labelled simulation using a local forge_probe executable")
    parser.add_argument("--open", action="store_true", help="Open the hardware test bridge in your browser")
    args = parser.parse_args()
    if args.port is not None and not 1 <= args.port <= 65535:
        parser.error("port must be 1–65535")
    if args.probe and not args.probe.is_file(): parser.error("Probe executable does not exist")
    server = None
    for port in ([args.port] if args.port else range(8765, 8776)):
        try: server = ForgeServer(port, args.probe.resolve() if args.probe else None); break
        except OSError:
            if args.port: raise SystemExit(f"Port {port} is in use by another program; choose another with --port")
            print(f"Port {port} is in use by another program; trying {port + 1}", flush=True)
    if server is None: raise SystemExit("Ports 8765–8775 are all in use; choose one with --port")
    with server:
        print(f"Forge: {server.origin} (Ctrl+C to stop)", flush=True)
        print(f"Hardware test bridge: {server.origin}/inspector", flush=True)
        if args.open: webbrowser.open(server.origin + "/inspector")
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
