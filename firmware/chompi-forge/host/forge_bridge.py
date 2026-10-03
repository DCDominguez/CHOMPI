"""Local hardware-test session; shares Inspector schema and the web server MIDI lock."""
from collections import deque
from datetime import datetime, timezone
import json
from pathlib import Path
import queue
import secrets
import subprocess
import threading
import time

import forge_host as host
import forge_inspector as inspector

ROOT = Path(__file__).resolve().parent
CHECKS = json.loads((ROOT / "bridge_checks.json").read_text(encoding="utf-8"))


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

    def exchange(self, payload, decoder=host.decode_response):
        sequence = host.read14(payload, 5)
        self.raw([0xf0, *payload, 0xf7])
        deadline = time.monotonic() + 2
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


class Bridge:
    def __init__(self, midi_lock, probe=None, factory=Transport):
        self.midi_lock, self.probe, self.factory = midi_lock, probe, factory
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
                "hardware_verified": False,
                "claim": "Check results are operator observations; simulation and injected events are not physical verification."}

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

    def watchdog(self):
        while not self.stop.wait(1):
            with self.lock:
                if self.transport and time.monotonic()-self.touched > 35:
                    self.disconnect("Browser heartbeat expired")

    def close(self):
        self.stop.set()
        self.worker.join(timeout=2)
        with self.lock: self.disconnect("Server closed")

    def request(self, operation, body):
        if not self.lock.acquire(blocking=False):
            raise RuntimeError("Bridge operation in progress; wait for it to finish")
        try:
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
            if not self.transport: raise RuntimeError("Session disconnected; export the report or reconnect")
            self.touched = time.monotonic()
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
