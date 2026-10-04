"""USB file transfer and firmware install through the C++ simulation (the firmware's own code)."""
import io
from pathlib import Path
import struct
import sys
import threading
import unittest
import wave
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_bridge as bridge
import forge_card as card
import forge_host as host

PROBE = ROOT / "build" / "forge_probe"
FIRMWARE = ROOT / "src" / "build-dev" / "FORGE.bin"


def wav(seconds=0.5, hz=440):
    out = io.BytesIO()
    with wave.open(out, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(48000)
        frames = int(seconds * 48000)
        import math
        w.writeframes(b"".join(struct.pack("<hh", int(8000 * math.sin(6.283 * hz * i / 48000)), 0) for i in range(frames)))
    return out.getvalue()


class Sequence:
    def __init__(self): self.value = 0
    def __call__(self): self.value = (self.value + 1) & 16383; return self.value


class LossyTransport:
    """Drops the reply to one data request, as a USB hiccup would."""
    def __init__(self, inner, drop_at): self.inner, self.drop_at, self.count = inner, drop_at, 0
    def exchange(self, payload, decoder, timeout=2): return self.inner.exchange(payload, decoder, timeout)
    def raw(self, data): self.inner.raw(data)
    def receive(self, payload, decoder, timeout=2):
        self.count += 1
        if self.count == self.drop_at:
            self.inner.receive(payload, decoder, timeout); raise TimeoutError("reply lost")
        return self.inner.receive(payload, decoder, timeout)


class LostEndReply(LossyTransport):
    """CHOMPI gets the End request and writes the file, but its reply never arrives."""
    def __init__(self, inner): super().__init__(inner, 0); self.lost = False
    def exchange(self, payload, decoder, timeout=2):
        if payload[4] == 0x0C and payload[7] == card.OPS["end"] and not self.lost:
            self.lost = True
            self.inner.exchange(payload, decoder, timeout); raise TimeoutError("reply lost")
        return self.inner.exchange(payload, decoder, timeout)


class DeadLink(LossyTransport):
    """Every data reply is lost (a cable pulled mid-upload)."""
    def receive(self, payload, decoder, timeout=2):
        self.inner.receive(payload, decoder, timeout); raise TimeoutError("reply lost")


class CardTests(unittest.TestCase):
    def setUp(self):
        self.transport = bridge.Transport(None, None, PROBE)
        self.seq = Sequence()
        self.card = card.Card(self.transport, self.seq)

    def tearDown(self): self.transport.close()

    def samples(self):
        return self.transport.exchange(host.sample_message(8, self.seq()))

    def test_codec_helpers(self):
        raw = bytes(range(256))
        packed = card.pack7(raw)
        self.assertTrue(all(b < 128 for b in packed)); self.assertEqual(len(packed), 256 // 7 * 8 + 1 + 256 % 7)
        self.assertTrue(card.allowed("JAMMI_A14.WAV")); self.assertFalse(card.allowed("jammi_a1_double.wav"))
        self.assertFalse(card.allowed("../FORGE.bin")); self.assertFalse(card.allowed("tape.bin"))

    def test_sample_upload_appears_in_the_sample_list(self):
        self.assertEqual(self.samples()["samples"]["chromatic"]["b"], [])
        data = wav()
        seen = []
        self.card.upload("jammi_b1.wav", data, lambda done, total: seen.append(done))
        self.assertEqual(seen[-1], len(data))
        self.assertEqual(self.samples()["samples"]["chromatic"]["b"], [1])
        state = self.card.status()
        self.assertFalse(state["active"]); self.assertFalse(state["firmware_staged"])

    def test_lost_reply_resumes_and_wrong_crc_never_replaces(self):
        data = wav(1.0)
        lossy = card.Card(LossyTransport(self.transport, 3), self.seq, log=lambda *_: None)
        lossy.upload("cubbi_c2.wav", data)
        self.assertEqual(self.samples()["samples"]["kit"]["c"], [2])
        # A wrong CRC at the end: rejected, the previous file stays.
        self.card.request("begin", [*card.word35(10), 12, *b"cubbi_c2.wav"])
        self.transport.exchange(card.message("data", self.seq(), [*card.word35(0), *card.pack7(b"0123456789")]), card.decode_reply)
        with self.assertRaisesRegex(RuntimeError, "checksum"):
            self.card.request("end", card.word35(zlib.crc32(b"0123456789") ^ 1))
        self.assertEqual(self.samples()["samples"]["kit"]["c"], [2])
        with self.assertRaisesRegex(ValueError, "TAPE sample names"): self.card.upload("evil.bin", b"x")

    def test_lost_end_reply_and_failed_upload_leave_nothing_open(self):
        data = wav(0.4)
        card.Card(LostEndReply(self.transport), self.seq, log=lambda *_: None).upload("cubbi_e3.wav", data)
        self.assertEqual(self.samples()["samples"]["kit"]["e"], [3])      # the repeated End was accepted
        dead = card.Card(DeadLink(self.transport, 0), self.seq, log=lambda *_: None)
        import time
        sleep, card.time.sleep = card.time.sleep, lambda seconds: None
        try:
            with self.assertRaisesRegex(RuntimeError, "upload failed"): dead.upload("cubbi_e4.wav", data)
        finally: card.time.sleep = sleep
        self.assertFalse(self.card.status()["active"])                      # the host aborted it
        self.assertEqual(self.samples()["samples"]["kit"]["e"], [3])

    @unittest.skipUnless(FIRMWARE.exists(), "development firmware not built (make firmware-dev)")
    def test_firmware_install_waits_for_the_chompi_key(self):
        with self.assertRaisesRegex(ValueError, "Not a valid Forge firmware image"): self.card.upload("FORGE.bin", b"\0" * 4096)
        with self.assertRaisesRegex(RuntimeError, "empty"): self.card.request("install")      # nothing staged yet
        self.card.upload("forge.bin", FIRMWARE.read_bytes())
        self.assertTrue(self.card.status()["firmware_staged"])
        state = self.card.request("install")
        self.assertTrue(state["install_pending"]); self.assertFalse(state["restarting"])
        # A note key does nothing; the CHOMPI key (switch 5) confirms and never reaches the menu.
        for key, value in ((15, 1), (15, 0)):
            self.transport.exchange(host.message(0x0A, self.seq(), [0, key, 64 + value]), bridge.panel_ack)
        self.assertFalse(self.card.status()["restarting"])
        self.transport.exchange(host.message(0x0A, self.seq(), [0, 5, 65]), bridge.panel_ack)
        state = self.card.status()
        self.assertTrue(state["restarting"]); self.assertFalse(state["install_pending"])


class BridgeCardTests(unittest.TestCase):
    def setUp(self):
        import tempfile, time
        self.time = time
        self.tmp = tempfile.TemporaryDirectory(); self.lock = threading.Lock()
        self.bridge = bridge.Bridge(self.lock, PROBE, reports=self.tmp.name)
        self.bridge.card_folder = Path(self.tmp.name) / "card"; self.bridge.card_folder.mkdir()
        (self.bridge.card_folder / "cubbi_d4.wav").write_bytes(wav(0.3))
        (self.bridge.card_folder / "notes.txt").write_text("ignored")
        self.bridge.firmware = FIRMWARE
        self.owner = self.bridge.request("connect", {"mode": "simulation"})["owner"]

    def tearDown(self):
        self.bridge.close(); self.tmp.cleanup()

    def call(self, op, **body): return self.bridge.request(op, {"owner": self.owner, **body})

    def wait(self, until=lambda job: job["finished"]):
        for _ in range(400):
            job = self.call("job")
            if until(job): return job
            self.time.sleep(0.05)
        self.fail("job did not get there")

    def test_card_folder_upload_and_install_prompt(self):
        listing = self.call("card_list")
        self.assertEqual([f["name"] for f in listing["files"]], ["cubbi_d4.wav"])
        with self.assertRaisesRegex(ValueError, "Choose files"): self.call("card_upload", files=["notes.txt"])
        self.call("card_upload", files=["cubbi_d4.wav"])
        job = self.wait()
        self.assertIsNone(job["error"]); self.assertEqual(job["result"]["written"], ["cubbi_d4.wav"])
        self.assertEqual(self.call("poll")["storage"]["sd_ready"], True)
        if not FIRMWARE.exists(): return
        self.assertEqual(listing["firmware"]["kb"], round(FIRMWARE.stat().st_size / 1024))
        with self.assertRaisesRegex(ValueError, "Confirm"): self.call("install")
        self.call("install", confirm=True)
        job = self.wait(lambda job: job["prompt"] or job["finished"])
        self.assertEqual(job["prompt"]["id"], "install")
        self.call("cancel")                                   # nobody presses CHOMPI in the simulation
        job = self.wait()
        self.assertIsNone(job["error"]); self.assertFalse(job["result"]["restarting"])
        self.assertIn("nothing installed", job["progress"][-1])


if __name__ == "__main__":
    unittest.main()
