"""Local hardware-test session; shares Inspector schema and the web server MIDI lock."""
from collections import deque
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import queue
import secrets
import subprocess
import threading
import time

import forge_host as host
import forge_inspector as inspector
try:                       # automatic checks need numpy (+ sounddevice for a real interface)
    import forge_audio
    import forge_walk
except ImportError as missing:
    forge_audio = forge_walk = None
    AUDIO_MISSING = f"Automatic checks need numpy and sounddevice ({missing.name} is not installed)"
else:
    AUDIO_MISSING = None
PHOTO_LIMIT = 600_000      # characters of one camera snapshot (JPEG data URL) kept as light-check evidence

ROOT = Path(os.environ.get("FORGE_HOST_DIR") or Path(__file__).resolve().parent)   # FORGE_HOST_DIR: Forge Bridge.exe
# Where reports go and where card files are picked up: the kit folder, or next to Forge Bridge.exe.
import sys
DATA = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else ROOT.parent
import forge_card
CHECKS = json.loads((ROOT / "bridge_checks.json").read_text(encoding="utf-8"))
# Only ports whose names say CHOMPI (USB product string) or Daisy are ever probed by discover.
CHOMPI_NAMES = ("chompi", "daisy")


def now():
    return datetime.now(timezone.utc).isoformat()


def integer(body, key, low, high):
    value = body.get(key)
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{key} must be an integer {low}–{high}")
    return value


def panel_message(body, sequence):
    kind = integer(body, "kind", 0, 5)
    identity = integer(body, "id", 0, 39)
    value = integer(body, "value", -63, 63)
    if not ((kind == 0 and value in (0, 1)) or
            (kind == 1 and identity <= 5) or
            (kind == 2 and identity == 4 and value == 0) or
            (kind in (3, 4) and identity == 0 and value in (-1, 0, 1)) or
            (kind == 5 and identity == 0 and value == 0)):
        raise ValueError("Invalid panel test event")
    return host.message(10, sequence, [kind, identity, value + 64])


def panel_ack(data, sequence):
    if any(type(v) is not int or not 0 <= v < 128 for v in data):
        raise ValueError("Invalid panel acknowledgement byte")
    if len(data) == 9 and data[4] == 0x41:
        return host.decode_response(data, sequence)
    if (list(data[:4]) != host.PREFIX or len(data) != 9 or data[4] != 0x47
            or host.read14(data, 5) != sequence or data[7] != 0 or host.checksum(data)):
        raise ValueError("Invalid panel acknowledgement")
    return {"queued": True}


class Transport:
    """One open input/output pair for the entire hardware session."""
    def __init__(self, input_name, output_name, probe=None):
        self.source = self.destination = self.process = None
        if probe:
            self.process = subprocess.Popen([str(probe)], stdin=subprocess.PIPE,
                                            stdout=subprocess.PIPE, text=True)
            self.lines = queue.Queue(maxsize=64)
            def reader():
                for line in self.process.stdout:
                    try: self.lines.put_nowait(line)
                    except queue.Full: break
                try: self.lines.put_nowait(None)
                except queue.Full: pass
            self.reader = threading.Thread(target=reader, daemon=True)
            self.reader.start()
        else:
            self.midi = host.midi_module()
            if input_name not in self.midi.get_input_names() or output_name not in self.midi.get_output_names():
                raise ValueError("Select exact MIDI input and output ports from Refresh ports")
            self.source = self.midi.open_input(input_name)
            try: self.destination = self.midi.open_output(output_name)
            except Exception:
                self.source.close()
                raise

    def raw(self, data):
        if self.process:
            self.process.stdin.write(bytes(data).hex(" ") + "\n")
            self.process.stdin.flush()
        else:
            self.destination.send(self.midi.Message.from_bytes(data))

    def exchange(self, payload, decoder=host.decode_response, timeout=2):
        self.raw([0xf0, *payload, 0xf7])
        return self.receive(payload, decoder, timeout)

    def receive(self, payload, decoder=host.decode_response, timeout=2):
        """The reply to an already sent request (pipelined file transfer sends several first)."""
        sequence = host.read14(payload, 5)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.process:
                try: line = self.lines.get(timeout=max(.001, deadline-time.monotonic()))
                except queue.Empty: raise TimeoutError("Simulation stopped responding") from None
                if line is None: raise RuntimeError("Simulation exited")
                data = list(bytes.fromhex(line))
            else:
                reply = self.source.receive(block=False)
                if reply is None:
                    time.sleep(.002)
                    continue
                if reply.type != "sysex": continue
                data = list(reply.data)
            if len(data) < 7 or data[:3] != host.PREFIX[:3] or host.read14(data, 5) != sequence:
                continue
            result = decoder(data, sequence)
            if payload[4] == 1 and data[8:len(payload)] != list(payload[7:-1]):
                raise RuntimeError("Patch acknowledgement mismatch; inspect state before retrying")
            return result
        raise TimeoutError("Device did not acknowledge; action may have applied. Reconnect and inspect before retrying.")

    def close(self):
        if self.process:
            self.process.stdin.close()
            try: self.process.wait(timeout=1)
            except subprocess.TimeoutExpired:
                self.process.kill(); self.process.wait()
            self.reader.join(timeout=1)
            self.process.stdout.close()
        else:
            try:
                if self.destination: self.destination.close()
            finally:
                if self.source: self.source.close()


def midi_ports():
    midi = host.midi_module()
    return midi.get_input_names(), midi.get_output_names()


class SessionDevice:
    """The bridge session's transport as the device interface of forge_audio.Runner (job thread only)."""
    def __init__(self, bridge): self.bridge = bridge
    def exchange(self, payload, decoder=host.decode_response): return self.bridge.transport.exchange(payload, decoder)
    def send_patch(self, patch): return self.exchange(host.encode_patch(patch, self.bridge.seq()))
    def status(self, reset_cpu=False): return self.exchange(host.message(2, self.bridge.seq(), [1] if reset_cpu else []))
    def panic(self): return self.exchange(host.message(3, self.bridge.seq()))
    def samples(self): return self.exchange(host.sample_message(8, self.bridge.seq()))
    def snapshot(self): return self.bridge.snapshot()
    def panel(self, event):
        payload = panel_message(event, self.bridge.seq())
        self.bridge.injected = True
        result = self.exchange(payload, panel_ack)
        self.bridge.log("injection", {**{k: event[k] for k in ("kind", "id", "value")}, "by": "automatic checks"})
        if event["kind"] == 5: self.bridge.injected = False
        return result
    def note(self, note, velocity): self.bridge.transport.raw([0x90 if velocity else 0x80, note, velocity])
    def cc(self, control, value):
        if control == 64: self.bridge.pedal = value >= 64
        self.bridge.transport.raw([0xb0, control, value])


def Card(bridge, device):
    """forge_card.Card over the bridge session's transport (job thread only)."""
    return forge_card.Card(bridge.transport, bridge.seq, log=lambda line: None)


class Bridge:
    # Operations allowed while a background job (audio detection, automatic checks) owns the transport.
    DURING_JOB = ("job", "cancel", "capture", "heartbeat", "resume", "export", "check", "marker", "answer")

    def __init__(self, midi_lock, probe=None, factory=Transport, ports=midi_ports, audio_factory=None, reports=None, plan=None):
        self.midi_lock, self.probe, self.factory, self.ports = midi_lock, probe, factory, ports
        self.audio_factory = audio_factory or (forge_audio.SoundDeviceAudio if forge_audio else None)
        self.reports = Path(reports) if reports else DATA / "reports"
        self.card_folder = DATA / "card"                                    # files to copy onto CHOMPI's SD card
        self.firmware = ROOT.parent / "firmware" / "FORGE.bin"              # the kit's (or the exe's) firmware
        self.plan = Path(plan) if plan else ROOT / "auto_checks.json"   # server-side choice; never from a browser
        self.job = None
        self.log_lock = threading.Lock()
        self.lock = threading.Lock()
        self.transport = None
        self.owner = None
        self.sequence = secrets.randbelow(16384)
        self.stop = threading.Event()
        self.archive = None
        self.worker = threading.Thread(target=self.watchdog, daemon=True)
        self.worker.start()

    def seq(self):
        self.sequence = (self.sequence + 1) & 16383
        return self.sequence

    def log(self, kind, value):
        with self.log_lock:                        # request threads and a job thread both log
            self.total_records += 1
            self.records.append({"utc": now(), "kind": kind, "value": value})

    def snapshot(self):
        deadline = time.monotonic() + 15
        def fetch(page, cursor):
            if time.monotonic() > deadline: raise TimeoutError("Inspector read exceeded 15 seconds")
            return self.transport.exchange(inspector.request(page, self.seq(), cursor),
                    lambda data, seq: inspector.decode(data, seq, page))
        snapshot = inspector.collect(fetch, self.cursor)
        self.cursor = snapshot["event_cursor"]
        self.latest = snapshot
        self.latest_at, self.latest_clock = now(), time.monotonic()
        self.log("snapshot", snapshot)
        return snapshot

    def report(self):
        return {"format": "forge-bridge-session-1", "started": self.started,
                "exported": now(), "mode": self.mode, "metadata": self.metadata,
                "connection": self.connection, "checks": dict(self.results),
                "checklist": CHECKS, "records": list(self.records),
                "records_omitted": self.total_records-len(self.records),
                "automatic": {"audio": self.audio_info, "runs": list(self.runs), "setup": list(self.setups),
                              "panel_walks": list(self.walks)},
                "hardware_verified": False,
                "claim": "Check results are operator observations; simulation and injected events are not physical verification. "
                         "Automatic results are the bridge's measurements of CHOMPI's audio output and telemetry: "
                         "evidence only for what each step measured."}

    def disconnect(self, reason):
        if not self.transport: return
        self.cleanup_errors = []
        try:
            if self.pedal:
                try:
                    self.transport.raw([0xb0,64,0])
                    self.log("cleanup", "Test sustain pedal released (sent)")
                except Exception:
                    self.cleanup_errors.append("Sustain release unconfirmed; use Panic after reconnect.")
            if self.injected:
                try:
                    self.transport.exchange(panel_message({"kind":5,"id":0,"value":0},self.seq()), panel_ack)
                    self.log("cleanup", "Panel overrides released (queued)")
                except Exception:
                    self.cleanup_errors.append("Override release unconfirmed. Use Release overrides after reconnect, or reboot.")
        finally:
            for message in self.cleanup_errors: self.log("cleanup_error", message)
            try: self.transport.close()
            except Exception: self.log("cleanup_error", "Transport close failed")
            self.transport = None
            self.midi_lock.release()
            self.log("disconnect", reason)
            self.archive = self.report()

    def busy(self): return bool(self.job and self.job["thread"].is_alive())

    def watchdog(self):
        while not self.stop.wait(1):
            with self.lock:
                if self.transport and not self.busy() and time.monotonic()-self.touched > 35:
                    self.disconnect("Browser heartbeat expired")

    def close(self):
        self.stop.set()
        self.worker.join(timeout=2)
        if self.job:
            self.job["cancel"].set(); self.job["thread"].join(timeout=30)
        with self.lock: self.disconnect("Server closed")

    def request(self, operation, body):
        if not self.lock.acquire(blocking=False):
            raise RuntimeError("Bridge operation in progress; wait for it to finish")
        try:
            if operation == "discover": return self.discover()
            if operation == "connect":
                if self.transport: raise RuntimeError("A bridge session already owns MIDI; disconnect it first")
                mode = body.get("mode")
                if mode not in ("hardware", "simulation") or (mode == "simulation" and not self.probe):
                    raise ValueError("Simulation needs the server --probe option; otherwise choose hardware")
                if mode == "hardware" and any(not isinstance(body.get(k), str) or not body[k] for k in ("input", "output")):
                    raise ValueError("Select both MIDI ports")
                metadata = body.get("metadata", "")
                if not isinstance(metadata, str) or len(metadata) > 2000: raise ValueError("Session notes: maximum 2000 characters")
                if not self.midi_lock.acquire(blocking=False): raise RuntimeError("Another Forge MIDI operation is active")
                try:
                    self.transport = self.factory(body.get("input"), body.get("output"), self.probe if mode == "simulation" else None)
                except Exception:
                    self.midi_lock.release()
                    raise
                self.owner = secrets.token_urlsafe(24)
                self.started, self.mode, self.metadata = now(), mode, metadata
                self.connection = {"input": body.get("input"), "output": body.get("output")}
                self.records, self.results = deque(maxlen=1200), {}
                self.total_records, self.cursor = 0, 0
                self.injected = self.armed = self.pedal = False
                self.latest = None
                self.audio, self.audio_info, self.runs, self.images = None, None, [], {}
                self.walks, self.setups = [], []
                self.touched = time.monotonic()
                try: snapshot = self.snapshot()
                except Exception:
                    self.disconnect("Initial Inspector read failed; use matching development firmware")
                    raise
                return {"owner": self.owner, "snapshot": snapshot}
            if not self.owner or body.get("owner") != self.owner:
                raise ValueError("This browser does not own the bridge session")
            if operation == "export": return self.report() if self.transport else self.archive
            if operation == "resume":
                if self.transport: self.touched = time.monotonic()
                return {"connected":bool(self.transport), "snapshot":self.latest,
                        "checks":dict(self.results), "metadata":self.metadata, "mode":self.mode,
                        "armed":self.armed if self.transport else False}
            if operation in ("job", "cancel", "capture", "answer"): return self.job_request(operation, body)
            if not self.transport: raise RuntimeError("Session disconnected; export the report or reconnect")
            self.touched = time.monotonic()
            if self.busy() and operation not in self.DURING_JOB:
                raise RuntimeError("Automatic checks are running; wait for them or Cancel")
            if operation in ("audio_detect", "autorun", "setup", "walk", "card_upload", "install"): return self.start_job(operation, body)
            if operation == "card_list": return self.card_list()
            if operation == "disconnect":
                self.disconnect("Operator disconnected")
                return {"disconnected": True, "cleanup_errors":self.cleanup_errors}
            if operation == "heartbeat": return {"connected": True}
            if operation == "marker":
                message = body.get("message")
                if not isinstance(message, str) or len(message) > 2000: raise ValueError("Marker: maximum 2000 characters")
                self.log("marker", message)
                return {"recorded": True}
            if operation == "arm":
                if type(body.get("enabled")) is not bool: raise ValueError("enabled must be boolean")
                self.armed = body["enabled"]
                self.log("test_controls", self.armed)
                return {"enabled": self.armed}
            if operation == "check":
                identity, result, notes = body.get("id"), body.get("result"), body.get("notes", "")
                if identity not in {c["id"] for c in CHECKS} or result not in ("pass", "fail", "blocked", "not_run"):
                    raise ValueError("Unknown checklist item/result")
                if not isinstance(notes, str) or len(notes) > 2000: raise ValueError("Observation: maximum 2000 characters")
                entry = {"result":result, "notes":notes, "utc":now(), "mode":self.mode,
                         "generation":self.latest["generation"], "overridden":self.latest["panel"]["logical"]["overridden"],
                         "evidence":self.latest, "evidence_received_at":self.latest_at,
                         "evidence_age_ms":int((time.monotonic()-self.latest_clock)*1000)}
                self.results[identity] = entry
                self.log("check", {"id":identity, **entry})
                return entry
            if operation not in ("poll", "action"): raise ValueError("Unknown bridge operation")
            # Only transport errors disconnect. Input mistakes keep the session available.
            if operation == "action":
                prepared = self.prepare(body)
            try:
                if operation == "poll": return self.snapshot()
                self.log("action_attempt", {k:body[k] for k in (
                    "action","kind","id","value","note","notes","velocity","duration_ms","control","bend","sustain",
                    "verb","bank","slot","sample_mode","to_mode","to_bank","to_slot") if k in body})
                result = prepared()
                self.log("action", {"action":body["action"], "result":result})
                return {"result": result, "snapshot": self.snapshot()}
            except Exception:
                self.log("error", "Transport/action failed; result uncertain, no automatic retry")
                self.disconnect("Transport/action failed")
                raise
        finally: self.lock.release()

    def discover(self):
        """Find CHOMPI's MIDI ports: only ports named CHOMPI/Daisy, confirmed by a Forge status reply."""
        if self.transport: raise RuntimeError("A bridge session already owns MIDI; disconnect it first")
        inputs, outputs = self.ports()
        named = lambda names: [n for n in names if any(word in n.lower() for word in CHOMPI_NAMES)]
        pairs = sorted(((i, o) for i in named(inputs) for o in named(outputs)),
                       key=lambda pair: -host.shared_words(*pair))
        if not pairs:
            raise RuntimeError("No MIDI port named CHOMPI found. Check the USB data cable and that the development "
                               "firmware is running; close other MIDI programs. Ports seen: " + (", ".join(inputs) or "none"))
        if not self.midi_lock.acquire(blocking=False): raise RuntimeError("Another Forge MIDI operation is active")
        try:
            tried = []
            for i, o in pairs[:6]:
                try:
                    transport = self.factory(i, o, None)
                except Exception as error:
                    tried.append(f"{i} / {o}: {error}"); continue
                try:
                    status = transport.exchange(host.message(2, self.seq()), timeout=0.8)
                    if "firmware" in status:
                        return {"input": i, "output": o, "firmware": status["firmware"]}
                    tried.append(f"{i} / {o}: unexpected reply")
                except Exception as error:
                    tried.append(f"{i} / {o}: {type(error).__name__}")
                finally:
                    transport.close()
            raise RuntimeError("CHOMPI's ports were found but Forge did not answer (" + "; ".join(tried) +
                               "). Is Forge's development firmware installed? Is another program using the ports?")
        finally:
            self.midi_lock.release()

    def start_job(self, kind, body):
        if kind == "autorun" and body.get("confirm") is not True:
            raise ValueError("Confirm that automatic checks may send patches, notes, tones and virtual panel presses")
        if forge_audio is None and kind not in ("card_upload", "install"): raise RuntimeError(AUDIO_MISSING)
        only = None
        if kind == "autorun":
            plan = json.loads(self.plan.read_text(encoding="utf-8"))
            folder = body.get("folder") or time.strftime("%Y%m%d-%H%M%S")
            if not isinstance(folder, str) or not folder.replace("-", "").isalnum(): raise ValueError("Invalid report folder")
            if body.get("only") is not None:
                only = body["only"]
                ids = {step["id"] for step in plan["steps"]}
                if not isinstance(only, list) or not only or any(i not in ids for i in only):
                    raise ValueError("Re-run needs step ids from the automatic checks")
        if kind == "walk" and body.get("parts") is not None and (not isinstance(body["parts"], list) or not body["parts"]
                                                                 or any(p not in ("controls", "lights") for p in body["parts"])):
            raise ValueError("Walk parts: controls and/or lights")
        if kind == "walk" and self.mode == "simulation" and body.get("parts") != ["lights"]:
            raise RuntimeError("The panel walk needs your hands on a physical CHOMPI; the simulation can only show the light questions")
        if kind == "card_upload":
            available = {p.name: p for p in self.card_files()}
            names = body.get("files")
            if not isinstance(names, list) or not names or any(n not in available for n in names):
                raise ValueError(f"Choose files from {self.card_folder}")
            uploads = [available[n] for n in names]
        if kind == "install":
            if not self.firmware.is_file(): raise RuntimeError("This kit has no firmware/FORGE.bin to install")
            if body.get("confirm") is not True: raise ValueError("Confirm that CHOMPI may restart to install firmware")
        if kind == "setup" and self.mode == "simulation":
            raise RuntimeError("The simulation has no audio; the setup check measures your interface and cables")
        if kind == "audio_detect" and self.mode == "simulation":
            raise RuntimeError("The simulation has no audio; automatic checks skip audio steps there")
        if kind == "audio_detect" and self.audio_factory is None:
            raise RuntimeError("Audio detection is disabled for this session")
        job = {"kind": kind, "started": now(), "finished": None, "progress": [], "result": None, "error": None,
               "cancel": threading.Event(), "prompt": None, "answers": queue.Queue()}
        device = SessionDevice(self)
        def say(line):
            job["progress"].append(line); self.touched = time.monotonic()
        def body_():
            try:
                if kind in ("audio_detect", "setup") and (kind == "audio_detect" or self.audio is None):
                    audio = self.audio_factory()
                    info = forge_audio.detect(device, audio, say, job["cancel"])
                    self.audio = audio if info["input"] else None
                    self.audio_info = info
                    say("Hears CHOMPI on: " + (info["input"]["name"] if info["input"] else "nothing (audio steps will be skipped)"))
                    say("Plays into line in from: " + (info["output"]["name"] if info["output"] else "nothing (tone steps will be skipped)"))
                    job["result"] = info
                if kind == "setup":
                    if self.audio is None: raise RuntimeError("No audio input hears CHOMPI; check the cable from CHOMPI's main outputs")
                    say("Checking the test setup…")
                    setup = forge_audio.setup_check(device, self.audio, bool(self.audio_info and self.audio_info.get("output")), say)
                    setup["utc"] = now(); self.setups.append(setup); job["result"] = setup
                elif kind in ("card_upload", "install"):
                    files = Card(self, device)
                    done = []
                    for path in (uploads if kind == "card_upload" else [self.firmware]):
                        data = path.read_bytes(); shown = [0]
                        def progress(at, total, name=path.name):
                            if at - shown[0] >= 32768 or at == total: shown[0] = at; job["progress_bar"] = [name, at, total]
                        say(f"{path.name}: writing {len(data) / 1024:.0f} KB…")
                        seconds = files.upload(path.name, data, progress, job["cancel"])
                        say(f"{path.name}: written in {seconds:.1f} s"); done.append(path.name)
                    result = {"written": done}
                    if kind == "install":
                        job["prompt"] = {"seq": 1, "id": "install", "title": "Install firmware",
                                         "text": "Press the CHOMPI key on the panel now.", "choices": [],
                                         "hint": "Its light blinks white for 15 seconds. Pressing it restarts CHOMPI: the bootloader "
                                                 "installs the new firmware (rainbow lights), then Forge starts.", "photo": False, "done": 0}
                        restarted = files.install(wait=20, cancel=job["cancel"])
                        job["prompt"] = None
                        result["restarting"] = restarted
                        say("CHOMPI is restarting to install the firmware. Wait for the rainbow lights to finish, then press Connect CHOMPI."
                            if restarted else "No CHOMPI key press within 15 seconds: nothing installed (FORGE.bin stays on the card).")
                        if restarted and self.mode == "hardware":
                            with self.lock: self.disconnect("CHOMPI restarting to install firmware")
                    job["result"] = result
                elif kind == "walk":
                    walk = forge_walk.Walk(device, lambda p: job.__setitem__("prompt", p), job["answers"], job["cancel"], say)
                    result = walk.run(tuple(body.get("parts") or ("controls", "lights")))
                    result["utc"] = now(); self.walks.append(result); job["result"] = result
                elif kind == "autorun":
                    if self.audio is not None and only is None:
                        say("Checking the test setup first…")
                        setup = forge_audio.setup_check(device, self.audio, bool(self.audio_info and self.audio_info.get("output")), say)
                        setup["utc"] = now(); self.setups.append(setup)
                        if not setup["ok"]: say("The setup has problems (above): audio results may reflect the cables, not CHOMPI.")
                    runner = forge_audio.Runner(device, self.audio, self.reports / folder, ROOT.parent,
                                                lambda e: say(f"{e['id']:>6}  {e['result']:<8} {e['title']}"), job["cancel"],
                                                simulated=self.mode == "simulation")
                    result = runner.run(plan, only)
                    if self.setups and only is None and self.audio is not None: result["setup"] = self.setups[-1]
                    self.images.update(runner.images)
                    result.update(folder=str((self.reports / folder).resolve()), audio=self.audio_info)
                    self.runs.append(result)
                    job["result"] = result
                self.log(kind, {k: v for k, v in job["result"].items() if k != "results"} if kind == "walk" else job["result"])
            except Exception as error:
                job["error"] = f"{type(error).__name__}: {error}"
                self.log("error", f"{kind}: {job['error']}")
            finally:
                job["finished"] = now(); self.touched = time.monotonic()
        job["thread"] = threading.Thread(target=body_, daemon=True)
        self.job = job
        self.log(kind + "_started", {"plan": self.plan.name} if kind == "autorun" else {})
        job["thread"].start()
        return {"started": kind}

    def card_files(self):
        self.card_folder.mkdir(parents=True, exist_ok=True)
        return forge_card.files_in(self.card_folder)

    def card_list(self):
        files = [{"name": p.name, "kb": round(p.stat().st_size / 1024)} for p in self.card_files()]
        firmware = None
        if self.firmware.is_file():
            import hashlib
            firmware = {"kb": round(self.firmware.stat().st_size / 1024),
                        "sha256": hashlib.sha256(self.firmware.read_bytes()).hexdigest()}
        return {"folder": str(self.card_folder.resolve()), "files": files, "firmware": firmware}

    def job_request(self, operation, body):
        job = self.job
        if operation == "capture":
            image = self.images.get(body.get("image"))
            if image is None: raise ValueError("Unknown capture")
            return {"image": body["image"], "png": forge_audio.data_url(image)}
        if not job: return {"kind": None, "finished": None, "progress": [], "result": None, "error": None, "prompt": None}
        if operation == "cancel": job["cancel"].set()
        if operation == "answer":
            prompt, value, image = job["prompt"], body.get("value"), body.get("image")
            if not prompt or body.get("seq") != prompt["seq"]: raise ValueError("That question is no longer open")
            if value not in (*prompt["choices"], "skip", "stop"): raise ValueError("Unknown answer")
            if image is not None and (not isinstance(image, str) or not image.startswith("data:image/jpeg;base64,")
                                      or len(image) > PHOTO_LIMIT):
                raise ValueError("Camera photo must be a JPEG under 450 KB")
            job["answers"].put({"value": value, **({"image": image} if image else {})})
            self.log("walk_answer", {"id": prompt["id"], "value": value, "photo": bool(image)})
            return {"accepted": True}
        return {"kind": job["kind"], "started": job["started"], "finished": job["finished"], "progress": list(job["progress"]),
                "result": job["result"], "error": job["error"], "cancelling": job["cancel"].is_set(), "prompt": job["prompt"],
                "progress_bar": job.get("progress_bar")}

    def prepare(self, body):
        action = body.get("action")
        if action not in ("release", "panic") and not self.armed:
            raise ValueError("Enable test controls first")
        if action in ("panel", "release"):
            event = body if action == "panel" else {"kind":5,"id":0,"value":0}
            payload = panel_message(event, self.seq())
            def send_panel():
                self.injected = True # includes uncertain acknowledgement / partially applied actions
                result = self.transport.exchange(payload, panel_ack)
                self.log("injection", {k:event[k] for k in ("kind","id","value")})
                if event["kind"] == 5: self.injected = False
                return result
            return send_panel
        if action == "patch":
            payload = host.encode_patch(body.get("patch"), self.seq())
        elif action == "panic":
            payload = host.message(3, self.seq())
        elif action in ("preset", "sample"):
            verb = body.get("verb")
            if verb in ("store", "save", "copy", "erase") and body.get("confirm_storage") is not True:
                raise ValueError("Confirm the SD write/overwrite/erase before sending")
            if action == "preset":
                codes = {"store":4,"recall":5,"erase":6,"list":7}
                if verb not in codes: raise ValueError("Unknown preset operation")
                payload = host.preset_message(codes[verb], self.seq(), body.get("bank"), body.get("slot"))
            else:
                if verb not in ("list","save","copy","erase"): raise ValueError("Unknown sample operation")
                payload = host.sample_message(8,self.seq()) if verb == "list" else host.sample_message(
                    9,self.seq(),verb,body.get("sample_mode"),body.get("bank"),body.get("slot"),
                    body.get("to_mode"),body.get("to_bank"),body.get("to_slot"))
        elif action == "note":
            note, velocity = integer(body,"note",0,127), integer(body,"velocity",1,127)
            duration = integer(body,"duration_ms",50,5000)
            notes = body.get("notes", [note])
            if (not isinstance(notes, list) or not 1 <= len(notes) <= 7 or
                    any(type(n) is not int or not 0 <= n <= 127 for n in notes) or len(set(notes)) != len(notes)):
                raise ValueError("Choose 1–7 distinct MIDI notes, each 0–127")
            bend = integer({"bend":body.get("bend",0)},"bend",-8192,8191)
            sustain = body.get("sustain",False)
            if type(sustain) is not bool: raise ValueError("sustain must be boolean")
            def play():
                try:
                    if sustain: self.transport.raw([0xb0,64,127])
                    for n in notes: self.transport.raw([0x90,n,velocity])
                    if bend: self.transport.raw([0xe0,(bend+8192)&127,(bend+8192)>>7])
                    if sustain:
                        for n in notes: self.transport.raw([0x80,n,0])
                    time.sleep(duration/1000)
                finally:
                    # Attempt every cleanup even if one send fails, retaining the failure.
                    cleanup = [[0x80,n,0] for n in notes]
                    if sustain: cleanup.append([0xb0,64,0])
                    if bend: cleanup.append([0xe0,0,64])
                    error = None
                    for message in cleanup:
                        try: self.transport.raw(message)
                        except Exception as failure: error = failure
                    if error: raise error
                return {"notes":notes,"velocity":velocity,"duration_ms":duration,
                        "sustain":sustain,"bend":bend,"note_off_sent":True}
            return play
        elif action == "cc":
            control, value = integer(body,"control",0,127), integer(body,"value",0,127)
            def cc():
                if control == 64: self.pedal = value >= 64
                self.transport.raw([0xb0,control,value])
                return {"control":control,"value":value,"sent":True,"acknowledged":False}
            return cc
        else: raise ValueError("Unknown bridge action")
        return lambda: self.transport.exchange(payload)
