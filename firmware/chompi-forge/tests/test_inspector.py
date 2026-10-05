from pathlib import Path
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'host'))
import forge_host as host
import forge_inspector as inspector
from test_host import probe


def fixed(data):
    data[-1]=host.checksum(data[:-1]); return data


class InspectorTests(unittest.TestCase):
    def test_schema_lengths_checksums_and_versions(self):
        for version in range(1,6):
            patch=host.load_patch(ROOT/'presets'/'03-long-echo.json')
            if version>1: patch=host.upgrade_patch(patch,version if version>2 else 3)
            if version==2: patch=host.load_patch(ROOT/'presets'/'04-glass-keys.json')
            replies=probe(host.encode_patch(patch,1),*(inspector.request(p,p) for p in range(2,8)))
            decoded=[inspector.decode(data,p,p) for p,data in zip(range(2,8),replies[1:])]
            self.assertEqual({d['generation'] for d in decoded},{1})
            self.assertEqual(decoded[-1]['patch']['version'],version)
            self.assertTrue(decoded[0]['simulated']); self.assertIsNone(decoded[0]['cpu_average_percent'])
            for page,data in zip(range(2,8),replies[1:]):
                bad=data.copy(); bad[-1]^=1
                with self.assertRaisesRegex(ValueError,'Invalid Inspector reply'): inspector.decode(bad,page,page)
                bad=data.copy(); bad[9]=2; fixed(bad)
                with self.assertRaisesRegex(ValueError,'schema'): inspector.decode(bad,page,page)
                with self.assertRaisesRegex(ValueError,'page mismatch'): inspector.decode(data,page,1)

    def test_frozen_snapshot_and_physical_identity(self):
        turn=host.message(0x0a,12,[1,3,67])
        key=host.message(0x0a,13,[0,15,65])
        replies=probe(inspector.request(2,1),turn,key,inspector.request(3,2),inspector.request(2,3),inspector.request(3,4))
        old=inspector.decode(replies[3],2,3); new=inspector.decode(replies[5],4,3)
        self.assertEqual(old['logical_keys'],[]); self.assertEqual(old['generation'],1)
        self.assertEqual(new['physical_keys'],[]); self.assertEqual(new['logical_keys'],[15])
        self.assertEqual(new['held_notes'],[48]); self.assertTrue(new['logical']['overridden'])
        self.assertEqual(new['raw_encoder_turns'],[0]*6); self.assertEqual(new['logical_encoder_turns'][3],3)
        self.assertEqual(new['generation'],2)

    def test_knob_pages_and_assignments(self):
        patch=host.upgrade_patch(host.load_patch(ROOT/'presets'/'07-warm-pad.json'),5)
        patch['knobs']=['filter.resonance','default','default','reverb.size']
        press=lambda seq,button:[host.message(0x0a,seq,[0,button,65]),host.message(0x0a,seq+1,[0,button,64])]
        replies=probe(host.encode_patch(patch,1),
                      *press(2,3),*press(4,3),*press(6,3),*press(8,3),  # knob 1 (switch 3) to page 5: the patch's resonance
                      host.message(0x0a,10,[1,3,64+20]),            # knob 1 is encoder 3; SW4 counts 1 per click (.01)
                      *press(11,0),                                 # knob 2 (switch 0) to page 2: decay
                      host.message(0x0a,13,[1,0,64+10]),            # .03 per click
                      *press(14,2),                                 # knob 4 (switch 2) to page 2: saturation
                      host.message(0x0a,16,[1,0,64]),               # one more block: the release acts
                      inspector.request(3,17),inspector.request(7,18))
        panel=inspector.decode(replies[-2],17,3); after=inspector.decode(replies[-1],18,7)['patch']
        self.assertEqual(panel['knob_pages'],[5,2,1,2])
        self.assertAlmostEqual(after['modules']['filter']['resonance'],patch['modules']['filter']['resonance']+.2,delta=1e-3)
        self.assertGreater(after['modules']['synth']['decay_ms'],patch['modules']['synth']['decay_ms'])
        self.assertEqual(after['knobs'],patch['knobs'])
        self.assertEqual([host.knob_control(after,k,p) for k,p in zip(range(1,5),panel['knob_pages'])],
                         ['filter.resonance','synth.decay_ms','synth.release_ms','tape.saturation'])

    def test_events_are_retained_and_cursor_is_exclusive(self):
        on=host.message(0x0a,1,[0,15,65]); off=host.message(0x0a,2,[0,15,64])
        replies=probe(on,off,inspector.request(6,3),inspector.request(6,4),inspector.request(6,5,1),inspector.request(6,6,2))
        first=inspector.decode(replies[2],3,6)
        self.assertEqual([e['kind'] for e in first['events']],['key_down','key_up'])
        self.assertEqual([e['value'] for e in first['events']],[2,2])
        self.assertEqual(first['events'],inspector.decode(replies[3],4,6)['events'])
        self.assertEqual(len(inspector.decode(replies[4],5,6)['events']),1)
        self.assertEqual(inspector.decode(replies[5],6,6)['events'],[])

    def test_voice_and_sampler_state(self):
        patch=host.load_patch(ROOT/'presets'/'11-recorded-keys.json')
        replies=probe(host.encode_patch(patch,1),host.message(0x0a,2,[0,18,65]),
                      inspector.request(2,3),inspector.request(4,4),inspector.request(5,5),inspector.request(7,6))
        engine=inspector.decode(replies[3],4,4); storage=inspector.decode(replies[4],5,5)
        voice=next(v for v in engine['voices'] if v['stage']!='off')
        self.assertEqual((voice['note'],voice['source'],voice['sample_slot']),(60,'panel',15))
        self.assertTrue(voice['sampled']); self.assertEqual(storage['record_frames'],48000)
        self.assertEqual(storage['record_capacity_frames'],192000)
        self.assertEqual(inspector.decode(replies[5],6,7)['patch']['modules']['sampler']['slot'],15)

    def test_malformed_word_and_event_lengths(self):
        data=probe(inspector.request(2,1))[0]; data[14]=16; fixed(data)
        with self.assertRaisesRegex(ValueError,'32-bit'): inspector.decode(data,1,2)
        data=probe(inspector.request(6,1))[0]; data[25]=4; fixed(data)
        with self.assertRaisesRegex(ValueError,'event page'): inspector.decode(data,1,6)
        for cursor in (-1,0x100000000,True):
            with self.assertRaises(ValueError): inspector.request(6,1,cursor)

    def test_read_only_cli_and_shared_collector(self):
        offline=inspector.Offline(ROOT/'build'/'forge_probe')
        sequence=0
        def fetch(page,cursor):
            nonlocal sequence
            sequence+=1
            payload=inspector.request(page,sequence,cursor)
            self.assertEqual(payload[4],0x0b)
            return offline.fetch(payload,sequence,page)
        try:
            s=inspector.collect(fetch)
            self.assertTrue(s['system']['simulated'])
            self.assertIn('SIMULATION',inspector.display(s))
            self.assertIn('STORAGE',inspector.display(s))
            self.assertEqual(len(s['panel']['leds']),26)
            # A cursor from a previous boot must recover without losing the viewer.
            reset=inspector.collect(fetch,100)
            self.assertEqual(reset['event_cursor'],0)
        finally: offline.close()

    def test_protocol_release_rejection_propagates(self):
        reply=host.PREFIX+[0x41,*host.word14(1),5,0]; fixed(reply)
        with self.assertRaisesRegex(RuntimeError,'unknown operation'): inspector.decode(reply,1,2)


if __name__=='__main__': unittest.main()
