"""Stateful software stand-in for a CHOMPI running Forge, for host/browser tests.

It exposes the small part of the mido API that forge_host.exchange uses and routes
SysEx through ONE long-running build/forge_probe process, i.e. the same C++
decoder/engine/encoder as firmware. State therefore persists between requests
(send, then status, then capture). It is NOT a hardware test: no USB/UART timing,
interrupts, real audio callback or CPU measurement are involved.
"""
from collections import deque
from pathlib import Path
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]
INPUT, OUTPUT = "Forge Simulator In", "Forge Simulator Out"


class Message:
    def __init__(self, type, data=()):
        self.type, self.data = type, tuple(data)


class _Port:
    def __init__(self, device): self.device = device
    def __enter__(self): return self
    def __exit__(self, *exc): return False


class _Input(_Port):
    def receive(self, block=True):
        with self.device.lock:
            return self.device.replies.popleft() if self.device.replies else None


class _Output(_Port):
    def send(self, message):
        self.device.handle(message)


class SimulatedMido:
    """Drop-in for forge_host.midi_module() during tests."""
    Message = Message

    def __init__(self, drop_replies=False):
        self.replies, self.lock = deque(), threading.Lock()
        self.drop_replies = drop_replies
        self.sent = []
        self.process = subprocess.Popen([str(ROOT / "build" / "forge_probe")], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, text=True, bufsize=1)

    def get_input_names(self): return [INPUT]
    def get_output_names(self): return [OUTPUT]
    def open_input(self, name): assert name == INPUT; return _Input(self)
    def open_output(self, name): assert name == OUTPUT; return _Output(self)

    def handle(self, message):
        assert message.type == "sysex"
        self.sent.append(list(message.data))
        self.process.stdin.write(bytes([0xF0, *message.data, 0xF7]).hex(" ") + "\n")
        self.process.stdin.flush()
        reply = list(bytes.fromhex(self.process.stdout.readline()))
        if not self.drop_replies:
            with self.lock:
                self.replies.append(Message("sysex", reply))

    def close(self):
        if self.process.poll() is None:
            self.process.stdin.close()
            self.process.wait(timeout=5)
        self.process.stdout.close()
