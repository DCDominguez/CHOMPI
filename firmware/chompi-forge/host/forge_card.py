#!/usr/bin/env python3
"""Put files on CHOMPI's SD card over USB and install firmware, without taking the card out.

Firmware 0.7 or newer. Accepted names: FORGE.bin and TAPE samples (jammi_/cubbi_<a-e><1-14>.wav;
TAPE makes its own _double files). Each file is written to FORGE/UPLOAD.TMP and replaces the
target only after its CRC-32 matches. Installing firmware asks for a CHOMPI key press on the
panel; CHOMPI then restarts and its bootloader flashes the new FORGE.bin from the card.

  python host/forge_card.py upload jammi_b1.wav cubbi_b1.wav ...
  python host/forge_card.py sync FOLDER            (every allowed file in FOLDER)
  python host/forge_card.py install firmware/FORGE.bin
"""
import argparse
from pathlib import Path
import re
import sys
import time
import zlib

import forge_host as host

OPS = {"begin": 0, "data": 1, "end": 2, "abort": 3, "install": 4, "status": 5}
CHUNK = 224                       # raw bytes per data request (32 groups of 7 -> 256 SysEx bytes)
WINDOW = 4                        # requests in flight (CHOMPI queues 16 frames per port)
FIRMWARE_MAX, SAMPLE_MAX = 480 * 1024, 64 * 1024 * 1024
SAMPLE_NAME = re.compile(r"^(jammi|cubbi)_[a-e](1[0-4]|[1-9])\.wav$", re.IGNORECASE)


def allowed(name):
    return name.lower() == "forge.bin" or bool(SAMPLE_NAME.match(name))


def word35(value):
    return [(value >> (7 * i)) & 127 for i in range(5)]


def pack7(raw):
    out = []
    for at in range(0, len(raw), 7):
        group = raw[at:at + 7]
        out.append(sum(((b >> 7) & 1) << i for i, b in enumerate(group)))
        out.extend(b & 127 for b in group)
    return out


def message(op, sequence, body=()):
    return host.message(0x0C, sequence, [OPS[op], *body])


def decode_reply(data, sequence):
    data = list(data)
    if len(data) == 9 and data[4] == 0x41:
        return host.decode_response(data, sequence)         # raises with the device's reason
    if (len(data) != 16 or data[:4] != host.PREFIX or data[4] != 0x48 or host.read14(data, 5) != sequence
            or data[7] != 0 or host.checksum(data) or data[8] > 5):
        raise ValueError("Invalid file-transfer reply (firmware 0.7 or newer is needed)")
    flags = data[9]
    return {"op": [k for k, v in OPS.items() if v == data[8]][0], "offset": sum(data[10 + i] << (7 * i) for i in range(5)),
            "active": bool(flags & 1), "install_pending": bool(flags & 2), "firmware_staged": bool(flags & 4),
            "restarting": bool(flags & 8), "power_low": bool(flags & 16)}


class Card:
    """File operations over an open bridge Transport (exchange/raw/receive) and a sequence source."""
    def __init__(self, transport, next_sequence, log=print):
        self.transport, self.seq, self.log = transport, next_sequence, log

    def request(self, op, body=(), timeout=3):
        return self.transport.exchange(message(op, self.seq(), body), decode_reply, timeout)

    def status(self): return self.request("status")

    def upload(self, name, data, progress=None, cancel=None):
        """Write `data` to the card as `name`; returns seconds taken. Resumes after a lost reply."""
        if not allowed(name): raise ValueError(f"{name}: only FORGE.bin and TAPE sample names (jammi_/cubbi_<a-e><1-14>.wav)")
        limit = FIRMWARE_MAX if name.lower() == "forge.bin" else SAMPLE_MAX
        if not 0 < len(data) <= limit: raise ValueError(f"{name}: size must be 1 byte to {limit // 1024} KB")
        if name.lower() == "forge.bin":
            import check_firmware_layout
            problems = check_firmware_layout.image_problems(bytes(data))
            if problems: raise ValueError("Not a valid Forge firmware image: " + "; ".join(problems))
            name = "FORGE.bin"
        started = time.monotonic()
        try:
            self._upload(name, data, progress, cancel)
        except BaseException:
            try: self.request("abort", timeout=1)               # best effort: leave nothing half-written open
            except Exception: pass
            raise
        return time.monotonic() - started

    def _retry(self, op, body, attempts=4):
        """Begin and End are safe to repeat (a repeated End for the file just written succeeds again)."""
        for attempt in range(attempts):
            try: return self.request(op, body)
            except TimeoutError:
                if attempt == attempts - 1: raise

    def _upload(self, name, data, progress, cancel):
        self._retry("begin", [*word35(len(data)), len(name), *name.encode("ascii")])
        offset, retries = 0, 0
        while offset < len(data):
            if cancel and cancel.is_set():
                raise RuntimeError("Upload cancelled; the card is unchanged")
            sent = []
            for at in range(offset, min(len(data), offset + CHUNK * WINDOW), CHUNK):
                payload = message("data", self.seq(), [*word35(at), *pack7(data[at:at + CHUNK])])
                self.transport.raw([0xF0, *payload, 0xF7]); sent.append((payload, at))
            try:
                for payload, at in sent: self.transport.receive(payload, decode_reply, 3)
                offset = min(len(data), sent[-1][1] + CHUNK); retries = 0
            except (RuntimeError, TimeoutError, ValueError) as error:
                retries += 1
                if retries > 4: raise RuntimeError(f"{name}: upload failed ({error}); the card is unchanged") from None
                time.sleep(0.3)
                offset = self.status()["offset"]                 # resume where CHOMPI got to
            if progress: progress(offset, len(data))
        for attempt in range(480):                             # CHOMPI may be loading samples: wait (up to 2 min)
            try:
                self._retry("end", word35(zlib.crc32(bytes(data)))); break
            except RuntimeError as error:
                if "storage busy" not in str(error) or attempt == 479: raise
                time.sleep(0.25)

    def check_power(self):
        """Firmware 0.11+: raise before a long upload if CHOMPI would refuse the install (low battery, weak supply)."""
        if self.status().get("power_low"): raise RuntimeError(host.ERRORS[10])

    def install(self, wait=30, cancel=None, prompt=None):
        """Ask for the panel confirmation; True once CHOMPI restarts (the connection then drops)."""
        state = self.request("install")
        if prompt: prompt()
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            if cancel and cancel.is_set(): return False
            time.sleep(0.25)
            try: state = self.status()
            except Exception:
                return True                                     # CHOMPI went away: it is restarting
            if state["restarting"]: return True
            if not state["install_pending"]:                    # the 15 s window closed, or power dropped at the press
                if state.get("power_low"): raise RuntimeError(host.ERRORS[10])
                return False
        return False


def files_in(folder):
    return sorted((p for p in Path(folder).iterdir() if p.is_file() and allowed(p.name)), key=lambda p: p.name.lower())


def cli(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    for name, help_text in (("upload", "Write files to the card root"), ("sync", "Write every allowed file in a folder"),
                            ("install", "Write FORGE.bin and install it (needs a CHOMPI key press)")):
        command = commands.add_parser(name, help=help_text)
        command.add_argument("paths", nargs="+" if name == "upload" else 1)
        command.add_argument("--input"); command.add_argument("--output")
        command.add_argument("--sim", type=Path, metavar="FORGE_PROBE", help="simulated CHOMPI")
    args = parser.parse_args(argv)
    import threading
    import forge_bridge
    bridge = forge_bridge.Bridge(threading.Lock(), args.sim, audio_factory=None)
    try:
        ports = {} if args.sim else {"input": args.input, "output": args.output} if args.input else bridge.request("discover", {})
        transport = forge_bridge.Transport(ports.get("input"), ports.get("output"), args.sim)
        card = Card(transport, bridge.seq)
        try:
            paths = [Path(p) for p in args.paths]
            if args.command == "sync": paths = files_in(paths[0])
            if args.command == "install": card.check_power()
            for path in paths:
                data = path.read_bytes()
                last = [0]
                def progress(done, total):
                    if done - last[0] >= 65536 or done == total:
                        last[0] = done; print(f"\r{path.name}: {done * 100 // total}%", end="", flush=True)
                seconds = card.upload(path.name, data, progress)
                print(f"\r{path.name}: written ({len(data) / 1024:.0f} KB in {seconds:.1f} s)")
            if args.command == "install":
                print("Press the CHOMPI key on the panel now (it blinks white; 15 seconds).", flush=True)
                if card.install(): print("CHOMPI is restarting: the bootloader installs the new firmware (rainbow lights), then Forge starts.")
                else: print("No key press within 15 seconds: nothing installed. FORGE.bin stays on the card; run install again.")
        finally:
            transport.close()
    finally:
        bridge.close()
    return 0


if __name__ == "__main__":
    sys.exit(cli())
