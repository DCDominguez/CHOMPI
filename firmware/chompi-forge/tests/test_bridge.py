"""Bridge session integration against the actual C++ protocol/probe. No hardware."""
import http.client
import json
from pathlib import Path
import sys
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_bridge as bridge
import forge_host as host
import forge_web


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.lock = threading.Lock()
        self.bridge = bridge.Bridge(self.lock, ROOT / "build" / ("forge_probe.exe" if sys.platform == "win32" else "forge_probe"))
        self.owner = None

    def tearDown(self):
        self.bridge.close()
        self.assertFalse(self.lock.locked())

    def request(self, op, **body):
        return self.bridge.request(op, {"owner": self.owner, **body})

    def connect(self):
        result = self.request("connect", mode="simulation", metadata="software-test only")
        self.owner = result["owner"]
        return result["snapshot"]

    def arm(self): self.request("arm", enabled=True)

    def test_connect_is_read_only_and_shared_model_is_complete(self):
        s = self.connect()
        self.assertTrue(s["system"]["simulated"])
        self.assertIsNone(s["system"]["cpu_average_percent"])
        self.assertFalse(s["panel"]["logical"]["overridden"])
        self.assertEqual(len(s["panel"]["leds"]), 26)
        self.assertEqual(set(s) & {"system","panel","engine","storage","events"}, {"system","panel","engine","storage","events"})
        with self.assertRaisesRegex(ValueError, "Enable"):
            self.request("action", action="panel", kind=0, id=15, value=1)
        self.assertTrue(self.lock.locked())

    def test_exclusive_owner_and_invalid_requests_preserve_session(self):
        self.connect()
        with self.assertRaisesRegex(RuntimeError,"already owns"): self.request("connect",mode="simulation")
        with self.assertRaisesRegex(ValueError,"own"): self.bridge.request("poll",{"owner":"other"})
        self.arm()
        for body in ({"action":"panel","kind":1,"id":6,"value":1},
                     {"action":"panel","kind":True,"id":0,"value":0},
                     {"action":"note","note":128,"velocity":100,"duration_ms":50},
                     {"action":"note","note":48,"velocity":100,"duration_ms":6000},
                     {"action":"preset","verb":"erase","bank":1,"slot":1}):
            with self.assertRaises(ValueError): self.request("action",**body)
        self.assertTrue(self.request("heartbeat")["connected"])

    def test_panel_patch_storage_and_retained_evidence(self):
        self.connect(); self.arm()
        self.request("action",action="patch",patch=host.load_patch(ROOT/"presets/04-glass-keys.json"))
        r=self.request("action",action="panel",kind=0,id=15,value=1)
        self.assertIn(15,r["snapshot"]["panel"]["logical_keys"])
        self.assertNotIn(15,r["snapshot"]["panel"]["physical_keys"])
        self.assertTrue(any(e["kind"]=="key_down" and e["value"]==2 for e in r["snapshot"]["events"]))
        self.assertTrue(any(v["note"]==48 and v["source"]=="panel" and v["stage"]!="off" for v in r["snapshot"]["engine"]["voices"]))
        self.request("action",action="release")
        self.request("action",action="preset",verb="store",bank=1,slot=1,confirm_storage=True)
        r=self.request("action",action="preset",verb="list")
        self.assertIn(1,r["result"]["occupied"][1])
        self.request("action",action="sample",verb="copy",sample_mode="kit",bank="a",slot=1,to_mode="kit",to_bank="b",to_slot=2,confirm_storage=True)
        r=self.request("action",action="sample",verb="list")
        self.assertIn(2,r["result"]["samples"]["kit"]["b"])
        r=self.request("check",id="1A.1",result="pass",notes="<script>literal observation</script>")
        self.assertEqual(r["mode"],"simulation")
        self.assertIn("evidence",r)
        self.request("marker",message="polling paused")
        report=self.request("export")
        self.assertFalse(report["hardware_verified"])
        self.assertEqual(report["checks"]["1A.1"]["notes"],"<script>literal observation</script>")
        self.assertTrue(any(r["kind"]=="marker" for r in report["records"]))

    def test_disconnect_cleanup_and_archive_remain_exportable(self):
        self.connect(); self.arm()
        self.request("action",action="panel",kind=0,id=15,value=1)
        self.request("action",action="cc",control=64,value=127)
        self.request("disconnect")
        self.assertFalse(self.lock.locked())
        report=self.request("export")
        self.assertEqual(sum(r["kind"]=="cleanup" for r in report["records"]),2)
        with self.assertRaisesRegex(RuntimeError,"disconnected"): self.request("poll")
        self.connect()
        self.assertEqual(self.request("export")["checks"],{})

    def test_sampler_parameters_follow_string_routing(self):
        self.connect(); self.arm()
        patch_data=host.load_patch(ROOT/"presets/12-tape-kit-a.json")
        r=self.request("action",action="patch",patch=patch_data)
        s=r["snapshot"]
        self.assertIn("knob1_pitch_semitones",s["panel"]["logical_parameters"])
        self.assertNotIn("knob1_mix",s["panel"]["logical_parameters"])
        self.assertIn("ENGINE  sampler",bridge.inspector.display(s))

    def test_timeout_disconnects_without_retry_and_reports_cleanup_failure(self):
        self.connect(); self.arm()
        self.request("action",action="panel",kind=3,id=0,value=1)
        with patch.object(self.bridge.transport,"exchange",side_effect=TimeoutError("timeout")) as send:
            with self.assertRaises(TimeoutError): self.request("poll")
            self.assertEqual(send.call_count,2) # one read, one best-effort override release
        self.assertFalse(self.lock.locked())
        self.assertTrue(any(r["kind"]=="cleanup_error" for r in self.request("export")["records"]))

    def test_timed_note_off_even_when_interrupted(self):
        self.connect(); self.arm()
        with patch.object(self.bridge.transport,"raw",wraps=self.bridge.transport.raw) as send:
            with patch.object(bridge.time,"sleep",side_effect=RuntimeError("interrupted")):
                with self.assertRaises(RuntimeError): self.request("action",action="note",note=48,velocity=80,duration_ms=50)
            self.assertEqual(send.call_args_list[0].args[0],[0x90,48,80])
            self.assertEqual(send.call_args_list[1].args[0],[0x80,48,0])

    def test_watchdog_and_bounded_retention(self):
        self.connect()
        for i in range(1300): self.bridge.log("marker",str(i))
        report=self.request("export")
        self.assertEqual(len(report["records"]),1200)
        self.assertEqual(report["records_omitted"],101)
        self.bridge.touched=time.monotonic()-40
        deadline=time.monotonic()+3
        while self.bridge.transport and time.monotonic()<deadline: time.sleep(.05)
        self.assertIsNone(self.bridge.transport)
        self.assertFalse(self.lock.locked())

    def test_chord_sustain_bend_cleanup_and_resume(self):
        self.connect(); self.arm()
        with patch.object(self.bridge.transport,"raw",wraps=self.bridge.transport.raw) as send:
            self.request("action",action="note",note=48,notes=[48,52,55],velocity=80,duration_ms=50,bend=4096,sustain=True)
            messages=[c.args[0] for c in send.call_args_list if c.args[0][0] != 0xf0]
            self.assertIn([0xb0,64,127],messages)
            self.assertIn([0xe0,0,96],messages)
            self.assertEqual(messages[-2:],[[0xb0,64,0],[0xe0,0,64]])
            for n in (48,52,55): self.assertIn([0x80,n,0],messages)
        self.assertTrue(self.request("resume")["connected"])
        self.request("disconnect")
        self.assertFalse(self.request("resume")["connected"])

    def test_failing_initial_read_releases_transport(self):
        with patch.object(bridge.Transport,"exchange",side_effect=RuntimeError("release firmware")):
            with self.assertRaisesRegex(RuntimeError,"release firmware"): self.connect()
        self.assertFalse(self.lock.locked())


class BridgeWebTests(unittest.TestCase):
    def setUp(self):
        self.server=forge_web.ForgeServer(0,ROOT/"build"/("forge_probe.exe" if sys.platform=="win32" else "forge_probe"))
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True); self.thread.start()

    def tearDown(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join()

    def request(self,path,body=None,**headers):
        connection=http.client.HTTPConnection("127.0.0.1",self.server.server_port,timeout=10)
        connection.request("GET" if body is None else "POST",path,
                           None if body is None else json.dumps(body),
                           {"Content-Type":"application/json","X-Forge-Token":self.server.token,**headers})
        response=connection.getresponse(); status=response.status; data=response.read(); connection.close()
        return status,data

    def test_http_assets_ownership_security_and_workshop_lock(self):
        for path in ("/inspector","/inspector.css","/inspector.js"):
            self.assertEqual(self.request(path)[0],200)
        self.assertEqual(self.request("/bridge_checks.json")[0],404)
        self.assertEqual(self.request("/api/bridge/connect",{"mode":"simulation"},Origin="https://evil.example")[0],403)
        status,data=self.request("/api/bridge/connect",{"mode":"simulation"})
        self.assertEqual(status,200); owner=json.loads(data)["owner"]
        self.assertEqual(self.request("/api/status",{"input":"x","output":"y"})[0],409)
        self.assertEqual(self.request("/api/bridge/poll",{"owner":"wrong"})[0],400)
        self.assertEqual(self.request("/api/bridge/poll",{"owner":owner})[0],200)
        self.assertEqual(self.request("/api/bridge/disconnect",{"owner":owner})[0],200)
        self.assertEqual(self.request("/api/bridge/export",{"owner":owner})[0],200)


if __name__=="__main__": unittest.main()
