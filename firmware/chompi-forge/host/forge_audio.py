#!/usr/bin/env python3
"""Forge automatic checks (development only): the hardware test bridge's ears.

Records CHOMPI's output through an audio interface, plays bounded test tones
into its line in, measures level, pitch, clicks, onset and noise, draws
spectrograms, finds the interface by itself, and runs the automatic check plan
(host/auto_checks.json) through a bridge session. The browser bridge
(forge_web.py --open, /inspector) uses this module; the command line runs the
same session for an agent on the PC:  python host/forge_audio.py run

Needs numpy and sounddevice (host/bridge-requirements.txt; bundled in the
Windows development kit). Without them the bridge still works and audio
steps are reported as skipped. Nothing here is used by the instrument itself.
"""
import argparse
import base64
import json
import math
from pathlib import Path
import struct
import sys
import threading
import time
import wave
import zlib

import numpy as np

import forge_host as host

RATE = 48000                 # preferred; a device that refuses it records at its own rate
MAX_TONE_DB = -6.0           # never drive CHOMPI's input harder than this
TONE_DB = -18.0              # test tones in plans and detection
ROOT = Path(__file__).resolve().parents[1]
PLAN = Path(__file__).resolve().parent / "auto_checks.json"

# Upstream SwId order (chompi hardware.h); CHOMPI = KEY_26. Knobs 1-4 in stock order.
PANEL_BUTTONS = {name: index for index, name in enumerate((
    "ENC_1_SW", "ENC_2_SW", "ENC_3_SW", "ENC_4_SW", "NC_6", "CHOMPI", "SW_TOG", "KEY_16",
    "KEY_2", "KEY_3", "KEY_4", "KEY_5", "KEY_17", "KEY_18", "KEY_19", "KEY_1",
    "KEY_6", "KEY_7", "KEY_8", "KEY_9", "KEY_10", "KEY_20", "KEY_21", "KEY_22",
    "KEY_11", "KEY_12", "KEY_13", "KEY_14", "KEY_15", "KEY_23", "KEY_24", "KEY_25",
    "ENC_6_SW", "KEY_27", "KEY_28"))}
PANEL_ENCODERS = {"KNOB_1": 3, "KNOB_2": 0, "KNOB_3": 1, "KNOB_4": 2, "SW5": 4, "SW6": 5}
AUDIO_ACTIONS = ("play", "capture", "tone")


class Skipped(Exception):
    """A step that cannot run in this setup (no audio interface, simulation)."""


def key_led(name):
    """Renderer LED under a key: white keys 25 - n, black keys KEY_16..25 -> 0..9, CHOMPI 25."""
    if name == "CHOMPI": return 25
    number = int(name.split("_")[1])
    if 1 <= number <= 15: return 25 - number
    if 16 <= number <= 25: return number - 16
    raise ValueError(f"{name} has no key LED")


def panel_gesture(text):
    """'toggle up|down|hw', 'jack in|out|hw', 'hold X', 'let X', 'tap X', 'turn KNOB_1 +10',
    'press SW5', 'release all' -> bridge panel events {kind, id, value}."""
    words = text.split()
    def key(name):
        if name not in PANEL_BUTTONS: raise ValueError(f"Unknown key {name}")
        return PANEL_BUTTONS[name]
    if len(words) == 2 and words[0] in ("toggle", "jack"):
        value = {"up": 1, "in": 1, "down": 0, "out": 0, "hw": -1}.get(words[1])
        if value is None: raise ValueError(f"Bad panel step: {text}")
        return [{"kind": 3 if words[0] == "toggle" else 4, "id": 0, "value": value}]
    if len(words) == 2 and words[0] in ("hold", "let"): return [{"kind": 0, "id": key(words[1]), "value": int(words[0] == "hold")}]
    if len(words) == 2 and words[0] == "tap": return [{"kind": 0, "id": key(words[1]), "value": v} for v in (1, 0)]
    if len(words) == 3 and words[0] == "turn" and words[1] in PANEL_ENCODERS:
        return [{"kind": 1, "id": PANEL_ENCODERS[words[1]], "value": int(words[2])}]
    if text == "press SW5": return [{"kind": 2, "id": 4, "value": 0}]
    if text == "release all": return [{"kind": 5, "id": 0, "value": 0}]
    raise ValueError(f"Bad panel step: {text}")


# ---- audio devices --------------------------------------------------------------------
class SoundDeviceAudio:
    """An audio interface through sounddevice (PortAudio). On Windows only WASAPI devices are used."""
    def __init__(self, sd=None):
        if sd is None:
            import sounddevice as sd
        self.sd = sd
        self.input = self.output = None
        self.rate = RATE

    def devices(self):
        apis = self.sd.query_hostapis()
        found = [{"index": i, "name": d["name"], "api": apis[d["hostapi"]]["name"], "inputs": d["max_input_channels"],
                  "outputs": d["max_output_channels"], "default_rate": d["default_samplerate"]}
                 for i, d in enumerate(self.sd.query_devices())]
        wasapi = [d for d in found if "WASAPI" in d["api"]]
        return wasapi or found

    def configure(self, input, output=None):
        self.input, self.output = input, output
        self.rate = RATE
        try:
            self.sd.check_input_settings(device=input, samplerate=RATE, channels=self.channels())
            if output is not None: self.sd.check_output_settings(device=output, samplerate=RATE, channels=2)
        except Exception:                             # e.g. WASAPI shared mode at 44.1 kHz
            self.rate = int(self.sd.query_devices(input)["default_samplerate"])

    def channels(self): return max(1, min(2, int(self.sd.query_devices(self.input)["max_input_channels"])))

    def start(self, seconds, play=None):
        frames = int(seconds * self.rate)
        if play is None:
            self.pending = self.sd.rec(frames, samplerate=self.rate, channels=self.channels(), dtype="float32", device=self.input)
        else:
            if self.output is None: raise Skipped("No audio output into CHOMPI's line in was found")
            out = max(1, min(2, int(self.sd.query_devices(self.output)["max_output_channels"])))
            padded = np.zeros((frames, out), dtype=np.float32)
            padded[:min(frames, len(play))] = play[:frames, :out]
            self.pending = self.sd.playrec(padded, samplerate=self.rate, channels=self.channels(), dtype="float32",
                                           device=(self.input, self.output))

    def wait(self):
        self.sd.wait()
        return np.asarray(self.pending, dtype=np.float32).reshape(-1, self.channels())


class FakeAudio:
    """Stand-in for tests: `source(frames, play, rate)` returns what the 'interface' hears."""
    def __init__(self, source=None, rate=RATE, output=True):
        rng = np.random.default_rng(1)
        self.source = source or (lambda frames, play, rate: (rng.standard_normal((frames, 2)) * 1e-4).astype(np.float32))
        self.rate, self.input, self.output = rate, "fake", "fake" if output else None

    def devices(self): return [{"index": "fake", "name": "Fake interface", "api": "test", "inputs": 2, "outputs": 2, "default_rate": RATE}]
    def configure(self, input, output=None): self.input, self.output = input, output
    def start(self, seconds, play=None):
        if play is not None and self.output is None: raise Skipped("No audio output into CHOMPI's line in was found")
        self.result = self.source(int(seconds * self.rate), play, self.rate)
    def wait(self): return self.result


def tone(hz, seconds, db=TONE_DB, rate=RATE):
    if not db <= MAX_TONE_DB: raise ValueError(f"Tone level must be at most {MAX_TONE_DB} dBFS")
    t = np.arange(int(seconds * rate)) / rate
    x = (10 ** (db / 20)) * np.sin(2 * np.pi * hz * t)
    fade = min(len(x) // 2, int(0.01 * rate))
    if fade: x[:fade] *= np.linspace(0, 1, fade); x[-fade:] *= np.linspace(1, 0, fade)
    return np.stack([x, x], axis=1).astype(np.float32)


# ---- files ---------------------------------------------------------------------------------
def write_wav(path, x, rate=RATE):
    data = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(str(path), "wb") as f:
        f.setnchannels(data.shape[1]); f.setsampwidth(2); f.setframerate(rate); f.writeframes(data.tobytes())


def read_wav(path):
    with wave.open(str(path), "rb") as f:
        width, channels, rate, raw = f.getsampwidth(), f.getnchannels(), f.getframerate(), f.readframes(f.getnframes())
    if width == 2: x = np.frombuffer(raw, "<i2").astype(np.float32) / 32768
    elif width == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)) << 8 >> 8) / 8388608.0
    else: raise ValueError("Only 16/24-bit PCM WAV files")
    return x.reshape(-1, channels).astype(np.float32), rate


def png(image):
    """Minimal greyscale/RGB PNG encoder (no image library needed)."""
    image = np.asarray(image, dtype=np.uint8)
    height, width = image.shape[:2]
    rows = b"".join(b"\0" + image[y].tobytes() for y in range(height))
    def chunk(kind, data): return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2 if image.ndim == 3 else 0, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def data_url(png_bytes): return "data:image/png;base64," + base64.b64encode(png_bytes).decode("ascii")


# ---- analysis ------------------------------------------------------------------------------
def db(value): return round(20 * math.log10(max(float(value), 1e-9)), 1)


def pitch(x, rate=RATE):
    """Dominant frequency (Hz) of the loudest 0.5 s, parabolic-interpolated FFT peak; None below -60 dBFS."""
    n = min(len(x), rate // 2)
    if n < 2048: return None
    hop = max(1, n // 4)
    start = max(range(0, len(x) - n + 1, hop), key=lambda s: float(np.sum(x[s:s + n] ** 2)))
    seg = x[start:start + n] - np.mean(x[start:start + n])
    if np.sqrt(np.mean(seg ** 2)) < 1e-3: return None
    spectrum = np.abs(np.fft.rfft(seg * np.hanning(n)))
    spectrum[:int(20 * n / rate)] = 0
    k = int(np.argmax(spectrum))
    if 0 < k < len(spectrum) - 1:
        a, b, c = np.log(spectrum[k - 1:k + 2] + 1e-12)
        k = k + 0.5 * (a - c) / (a - 2 * b + c)
    return round(float(k * rate / n), 2)


def clicks(x, rate=RATE):
    """Isolated discontinuities: second differences far above their local level (10 ms windows)."""
    if len(x) < 3: return []
    d2 = np.abs(x[2:] - 2 * x[1:-1] + x[:-2])
    window = int(0.01 * rate)
    found = []
    for start in range(0, len(d2) - window + 1, window):
        w = d2[start:start + window]
        peak = int(np.argmax(w))
        rest = np.delete(w, slice(max(0, peak - 2), peak + 3))
        level = np.sqrt(np.mean(rest ** 2)) if len(rest) else 0.0
        if w[peak] > 0.02 and w[peak] > 10 * max(level, 1e-6):
            found.append(round((start + peak + 1) / rate, 4))
    return found


def analyze(x, rate=RATE):
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 1: x = x[:, None]
    window = max(1, rate // 100)
    result = {"seconds": round(len(x) / rate, 3), "rate": rate, "channels": []}
    for c in range(x.shape[1]):
        s = x[:, c]
        envelope = np.sqrt(np.convolve(s ** 2, np.ones(window) / window, mode="valid")) if len(s) >= window else np.abs(s)
        loud = np.nonzero(envelope > 10 ** (-50 / 20))[0]
        quiet = np.sort(envelope)[: max(1, len(envelope) // 10)]
        result["channels"].append({
            "peak_db": db(np.max(np.abs(s))) if len(s) else -180, "rms_db": db(np.sqrt(np.mean(s ** 2))) if len(s) else -180,
            "dc": round(float(np.mean(s)), 5), "clipped": int(np.sum(np.abs(s) >= 0.999)),
            "pitch_hz": pitch(s, rate), "noise_floor_db": db(np.mean(quiet)),
            "onset_s": round(loud[0] / rate, 4) if len(loud) else None,
            "end_s": round((loud[-1] + window) / rate, 4) if len(loud) else None,
            "clicks": clicks(s, rate)[:20]})
    if x.shape[1] == 2 and np.std(x[:, 0]) > 1e-6 and np.std(x[:, 1]) > 1e-6:
        result["stereo_correlation"] = round(float(np.corrcoef(x[:, 0], x[:, 1])[0, 1]), 3)
    left = result["channels"][0]
    result.update({key: left[key] for key in ("peak_db", "rms_db", "pitch_hz", "onset_s", "end_s", "noise_floor_db")})
    result["clipped"] = sum(ch["clipped"] for ch in result["channels"])
    result["clicks"] = sorted({t for ch in result["channels"] for t in ch["clicks"]})
    result["silent"] = all(ch["rms_db"] < -60 for ch in result["channels"])
    return result


def spectrogram(x, rate=RATE, top_hz=12000):
    """Log-magnitude spectrogram of the mono sum as PNG bytes (time left to right, low notes at the bottom)."""
    mono = np.asarray(x, dtype=np.float64).reshape(len(x), -1).mean(axis=1)
    size, hop = 2048, 512
    if len(mono) < size: mono = np.pad(mono, (0, size - len(mono)))
    frames = [np.abs(np.fft.rfft(mono[i:i + size] * np.hanning(size))) for i in range(0, len(mono) - size + 1, hop)]
    s = 20 * np.log10(np.array(frames).T[: int(top_hz * size / rate)] + 1e-9)
    s = np.clip((s - (s.max() - 90)) / 90, 0, 1)
    rows = np.linspace(0, s.shape[0] - 1, 192).astype(int)
    cols = np.linspace(0, s.shape[1] - 1, min(900, s.shape[1])).astype(int)
    return png((255 * s[rows][:, cols][::-1]).astype(np.uint8))


# ---- finding the audio interface -----------------------------------------------------------
def related(a, b):
    words = lambda s: {w for w in s.lower().replace("(", " ").replace(")", " ").split() if len(w) > 2}
    return len(words(a) & words(b))


def detect(device, audio, log=print, cancel=None):
    """Find the input that hears CHOMPI (a Glass Keys C4) and the output wired to its line in
    (a -18 dBFS 1 kHz tone through the dry aux patch). Plays short quiet sounds only."""
    devices = audio.devices()
    result = {"input": None, "output": None, "attempts": []}
    device.send_patch(host.load_patch(ROOT / "presets/04-glass-keys.json"))
    best = None
    for d in [d for d in devices if d["inputs"] > 0]:
        if cancel and cancel.is_set(): break
        try:
            audio.configure(d["index"])
            audio.start(0.9)
            time.sleep(0.12); device.note(60, 100); time.sleep(0.45); device.note(60, 0)
            a = analyze(audio.wait(), audio.rate)
            heard = a["pitch_hz"] is not None and abs(a["pitch_hz"] - 261.63) < 8 and a["peak_db"] > -55
            result["attempts"].append({"input": d["name"], "pitch_hz": a["pitch_hz"], "peak_db": a["peak_db"], "hears_chompi": heard})
            if heard and (best is None or a["peak_db"] > best[1]): best = (d, a["peak_db"])
        except Exception as error:
            result["attempts"].append({"input": d["name"], "error": f"{type(error).__name__}: {error}"})
        log(f"input {d['name']}: {result['attempts'][-1]}")
    device.send_patch(host.load_patch(ROOT / "presets/01-dry.json"))
    if best is None: return result
    source = best[0]
    result["input"] = {"index": source["index"], "name": source["name"], "api": source["api"]}
    outputs = sorted([d for d in devices if d["outputs"] > 0 and d.get("api") == source.get("api")],
                     key=lambda d: -related(d["name"], source["name"]))[:8]
    for d in outputs:
        if cancel and cancel.is_set(): break
        try:
            audio.configure(source["index"], d["index"])
            audio.start(0.8, tone(1000, 0.4, TONE_DB, audio.rate))
            a = analyze(audio.wait(), audio.rate)
            reaches = a["pitch_hz"] is not None and abs(a["pitch_hz"] - 1000) < 15 and a["peak_db"] > -55
            result["attempts"].append({"output": d["name"], "pitch_hz": a["pitch_hz"], "peak_db": a["peak_db"], "reaches_line_in": reaches})
        except Exception as error:
            reaches = False
            result["attempts"].append({"output": d["name"], "error": f"{type(error).__name__}: {error}"})
        log(f"output {d['name']}: {result['attempts'][-1]}")
        if reaches:
            result["output"] = {"index": d["index"], "name": d["name"], "api": d["api"]}
            break
    audio.configure(source["index"], result["output"]["index"] if result["output"] else None)
    result["rate"] = audio.rate
    return result


# ---- plans ----------------------------------------------------------------------------------
def lookup(data, dotted):
    for part in dotted.split("."):
        data = data[int(part) if isinstance(data, list) else part]
    return data


def compare(expected, actual):
    if isinstance(expected, list) and len(expected) == 2 and all(isinstance(v, (int, float)) for v in expected) \
            and not isinstance(actual, list):
        return isinstance(actual, (int, float)) and not isinstance(actual, bool) and expected[0] <= actual <= expected[1]
    return expected == actual


class Runner:
    """Runs a plan against a device (send_patch, status, panic, panel(event), snapshot, samples,
    note, cc) and an audio backend (or None: audio steps are skipped)."""
    def __init__(self, device, audio, report_dir=None, root=ROOT, progress=None, cancel=None):
        self.device, self.audio, self.root = device, audio, Path(root)
        self.report = Path(report_dir) if report_dir else None
        if self.report: self.report.mkdir(parents=True, exist_ok=True)
        self.progress, self.cancel = progress or (lambda entry: None), cancel or threading.Event()
        self.captures, self.images, self.last = {}, {}, {}

    def run(self, plan):
        steps = []
        try:
            for step in plan["steps"]:
                if self.cancel.is_set(): break
                entry = {"id": step["id"], "title": step.get("title", ""), "result": "pass", "checks": []}
                try:
                    if self.audio is None and any(next(iter(a)) in AUDIO_ACTIONS for a in step.get("do", [])):
                        raise Skipped("No audio interface (simulation, or none detected)")
                    for action in step.get("do", []): self.action(action, entry)
                    for check in step.get("check", []): self.check(check, entry)
                except Skipped as reason:
                    entry["result"], entry["reason"] = "skipped", str(reason)
                except Exception as error:                     # a broken step must not stop the run
                    entry["result"], entry["error"] = "error", f"{type(error).__name__}: {error}"
                if entry["result"] == "pass" and any(not c["ok"] for c in entry["checks"]): entry["result"] = "fail"
                steps.append(entry); self.progress(entry)
        finally:
            if any("panel" in a for s in plan["steps"] for a in s.get("do", [])):
                try: self.device.panel({"kind": 5, "id": 0, "value": 0})   # never leave virtual keys held
                except Exception as error: steps.append({"id": "cleanup", "title": "Release virtual panel",
                                                         "result": "error", "checks": [], "error": str(error)})
        counts = {k: sum(s["result"] == k for s in steps) for k in ("pass", "fail", "error", "skipped")}
        result = {"plan": plan.get("name", ""), "counts": counts, "steps": steps, "cancelled": self.cancel.is_set()}
        if self.report:
            (self.report / "report.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            (self.report / "summary.md").write_text(summary(result), encoding="utf-8")
        return result

    def action(self, action, entry):
        (kind, arg), = action.items()
        if kind == "wait": time.sleep(float(arg))
        elif kind == "send":
            patch = host.load_patch(self.root / (arg["file"] if isinstance(arg, dict) else arg))
            for dotted, value in (arg.get("set", {}) if isinstance(arg, dict) else {}).items():
                *path, last = dotted.split("."); node = patch
                for part in path: node = node[part]
                node[last] = value
            self.last["status"] = self.device.send_patch(host.validate_patch(patch))
        elif kind == "status": self.last["status"] = self.device.status()
        elif kind == "panic": self.last["status"] = self.device.panic()
        elif kind == "panel":
            for gesture in (arg if isinstance(arg, list) else [arg]):
                for event in panel_gesture(gesture): self.device.panel(event); time.sleep(0.03)
        elif kind == "probe": time.sleep(0.08); self.last["probe"] = self.device.snapshot()   # LEDs redraw at ~30 Hz
        elif kind == "samples": self.last["samples"] = self.device.samples()
        elif kind == "cc": self.device.cc(*arg)
        elif kind == "play":                                   # notes held while recording CHOMPI's output
            seconds, tail = float(arg.get("seconds", 1)), float(arg.get("tail", 0.5))
            self.audio.start(seconds + tail)
            try:
                for n in arg["notes"]: self.device.note(n, arg.get("velocity", 100))
                time.sleep(seconds)
            finally:
                for n in arg["notes"]: self.device.note(n, 0)
            self.store(arg["capture"], self.audio.wait(), entry)
        elif kind == "capture":                                # recording while other actions run
            self.audio.start(float(arg["seconds"])); self.during(arg, entry); self.store(arg["name"], self.audio.wait(), entry)
        elif kind == "tone":                                   # tone into CHOMPI's line in, recorded at the same time
            self.audio.start(float(arg["seconds"]) + float(arg.get("tail", 0.5)),
                             tone(arg["hz"], arg["seconds"], arg.get("db", TONE_DB), self.audio.rate))
            self.during(arg, entry); self.store(arg["capture"], self.audio.wait(), entry)
        else: raise ValueError(f"Unknown action {kind}")

    def during(self, arg, entry):
        if "during" in arg:
            time.sleep(float(arg.get("after", 0.2)))
            for step in arg["during"]: self.action(step, entry)

    def store(self, name, x, entry):
        key = f"{entry['id']}-{name}"
        self.captures[name] = analyze(x, self.audio.rate)
        self.images[key] = spectrogram(x, self.audio.rate)
        if self.report:
            write_wav(self.report / f"{key}.wav", x, self.audio.rate)
            (self.report / f"{key}.png").write_bytes(self.images[key])
        entry.setdefault("captures", {})[name] = {**self.captures[name], "image": key}

    def check(self, check, entry):
        (kind, expected), = check.items()
        if kind == "led":
            leds = self.last["probe"]["panel"]["leds"]
            for key, want in expected.items():
                rgb = leds[key_led(key)]
                ok = (max(rgb) > 8) if want == "lit" else (max(rgb) <= 8) if want == "off" else all(abs(a - b) <= 12 for a, b in zip(rgb, want))
                entry["checks"].append({"what": f"led {key}", "expected": want, "actual": rgb, "ok": bool(ok)})
            return
        if kind == "capture": data, items = self.captures[expected["name"]], {k: v for k, v in expected.items() if k != "name"}
        else: data, items = self.last[kind], expected
        for dotted, want in items.items():
            if dotted == "clicks_max": actual = len(data["clicks"]); ok = actual <= want
            else:
                try: actual = lookup(data, dotted)
                except (KeyError, IndexError, TypeError): actual = None
                ok = compare(want, actual)
            entry["checks"].append({"what": f"{kind}.{dotted}", "expected": want, "actual": actual, "ok": bool(ok)})


def summary(result):
    c = result["counts"]
    lines = [f"# {result['plan']}", "", f"pass {c['pass']} · fail {c['fail']} · error {c['error']} · skipped {c['skipped']}"
             + (" · CANCELLED" if result.get("cancelled") else ""), ""]
    for s in result["steps"]:
        lines.append(f"- **{s['id']}** {s['result']}: {s['title']}" + (f" — {s.get('error') or s.get('reason')}" if s.get("error") or s.get("reason") else ""))
        for c in s["checks"]:
            if not c["ok"]: lines.append(f"  - expected {c['what']} = {c['expected']}, got {c['actual']}")
    return "\n".join(lines) + "\n"


# ---- command line: the same session the browser runs ----------------------------------------
def cli(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("devices", help="List the audio devices the bridge would consider")
    analyze_cmd = commands.add_parser("analyze", help="Measure a WAV file and write its spectrogram next to it")
    analyze_cmd.add_argument("wav")
    run = commands.add_parser("run", help="Find CHOMPI and the interface, run the automatic checks, write a report")
    run.add_argument("--plan", default=str(PLAN)); run.add_argument("--report", help="report folder (default reports/<time>)")
    run.add_argument("--input", help="MIDI input (default: found automatically)"); run.add_argument("--output", help="MIDI output")
    run.add_argument("--no-audio", action="store_true", help="skip audio detection and audio steps")
    run.add_argument("--sim", type=Path, metavar="FORGE_PROBE", help="simulated CHOMPI (no audio, no hardware evidence)")
    args = parser.parse_args(argv)
    if args.command == "devices":
        print(json.dumps(SoundDeviceAudio().devices(), indent=2)); return 0
    if args.command == "analyze":
        x, rate = read_wav(args.wav)
        Path(args.wav).with_suffix(".png").write_bytes(spectrogram(x, rate))
        print(json.dumps(analyze(x, rate), indent=2)); return 0
    import forge_bridge
    report = Path(args.report) if args.report else ROOT / "reports" / time.strftime("%Y%m%d-%H%M%S")
    bridge = forge_bridge.Bridge(threading.Lock(), args.sim, reports=report.parent, plan=args.plan,
                                 audio_factory=None if args.no_audio or args.sim else SoundDeviceAudio)
    try:
        ports = ({} if args.sim else {"input": args.input, "output": args.output} if args.input
                 else bridge.request("discover", {}))
        owner = bridge.request("connect", {"mode": "simulation" if args.sim else "hardware", **ports,
                                           "metadata": "forge_audio.py run"})["owner"]
        def wait(kind):
            while True:
                job = bridge.request("job", {"owner": owner})
                for line in job["progress"][wait.seen.get(kind, 0):]: print(line, flush=True)
                wait.seen[kind] = len(job["progress"])
                if job["finished"]: return job
                time.sleep(0.5)
        wait.seen = {}
        if not args.sim and not args.no_audio:
            bridge.request("audio_detect", {"owner": owner}); print(json.dumps(wait("audio_detect")["result"], indent=2))
        bridge.request("autorun", {"owner": owner, "folder": report.name, "confirm": True})
        job = wait("autorun")
        if job["error"]: raise RuntimeError(job["error"])
        exported = bridge.request("export", {"owner": owner})
        report.mkdir(parents=True, exist_ok=True)
        (report / "session.json").write_text(json.dumps(exported, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(job["result"]["counts"]), f"\nreport: {report.resolve()}")
        return 0 if not job["result"]["counts"]["fail"] and not job["result"]["counts"]["error"] else 2
    finally:
        bridge.close()


if __name__ == "__main__":
    try:
        sys.exit(cli())
    except (ValueError, RuntimeError, OSError, TimeoutError) as error:
        print(f"Forge automatic checks: {error}", file=sys.stderr)
        sys.exit(1)
