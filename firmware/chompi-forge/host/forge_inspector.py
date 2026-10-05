"""Read-only Forge Inspector, using development probe pages on the existing SysEx transport."""
import argparse
from collections import deque
import json
from pathlib import Path
import subprocess
import math
import sys
import time

import forge_host as host

KEY_NOTES = (0,0,0,0,0,0,0,49,50,52,53,55,51,54,56,48,57,59,60,62,64,58,61,63,65,67,69,71,72,66,68,70,0,0,0,0,0,0,0,0)
EVENTS = ("unknown", "key_down", "key_up", "knob", "voice_start", "voice_stop", "patch_apply",
          "sample_selection", "recording_start", "recording_stop", "card", "queue_error", "storage_error",
          "sample_loaded", "sample_job_done")


def word(value):
    if type(value) is not int or not 0 <= value <= 0xffffffff:
        raise ValueError("Inspector cursor must be an unsigned 32-bit integer")
    return [(value >> (7*i)) & 127 for i in range(5)]


def request(page, sequence, cursor=0):
    if type(page) is not int or not 1 <= page <= 10:
        raise ValueError("Unknown Inspector page")
    return host.message(0x0b, sequence, [page, *word(cursor)] if page == 6 else [page])


def signed(v):
    return v if v < 0x80000000 else v - 0x100000000


def decode(data, sequence, expected_page=None):
    data = list(data)
    if len(data) == 9 and data[4] == 0x41:
        return host.decode_response(data, sequence)
    if (len(data) < 10 or data[:4] != host.PREFIX or data[4] != 0x46 or data[7] != 0
            or any(type(v) is not int or not 0 <= v < 128 for v in data)
            or host.read14(data, 5) != sequence or host.checksum(data) != 0):
        raise ValueError("Invalid Inspector reply")
    page = data[8]
    if expected_page is not None and page != expected_page:
        raise ValueError("Inspector reply page mismatch")
    if page == 1:
        if len(data) != 88:
            raise ValueError("Invalid LED page")
        return {"page": 1, "rgb": [data[9+3*i:12+3*i] for i in range(26)]}
    if page not in range(2,11) or len(data) < 16 or data[9] != 1:
        raise ValueError("Unsupported Inspector schema/page; use the matching host")
    lengths = {2:(88, 91, 98), 3:(97,), 4:(88,), 5:(94,), 8:(38,), 9:(50,), 10:(29,)}  # page 2: 91 from firmware 0.8 (power), 98 from 0.10 (restart)
    if page in lengths and len(data) not in lengths[page]:
        raise ValueError("Invalid Inspector page length")
    pos = 10
    def byte():
        nonlocal pos
        if pos >= len(data)-1:
            raise ValueError("Truncated Inspector page")
        value=data[pos]; pos+=1; return value
    def u32():
        chunks=[byte() for _ in range(5)]
        if chunks[-1] > 15:
            raise ValueError("Invalid 32-bit Inspector word")
        return sum(v << (7*i) for i,v in enumerate(chunks))
    def u14():
        return byte() | (byte()<<7)
    def unit():
        return u14()/16383
    result={"page":page,"generation":u32()}
    if page == 2:
        minor,protocol,simulation=byte(),byte(),byte()
        if protocol!=1 or simulation>1: raise ValueError("Invalid Inspector system flags")
        result.update(firmware=f"0.{minor}", protocol=protocol, simulated=bool(simulation))
        result.update(uptime_ms=u32(), audio_time_ms=u32(), audio_blocks=u32())
        result.update(cpu_average_percent=u14()/10, cpu_peak_percent=u14()/10)
        result["midi"]={name:{key:u32() for key in ("rx_frames","tx_accepted","tx_errors","ingress_drops")}
                        for name in ("uart","usb")}
        result.update(dropped=u32(), rejected=u32(), audio_underruns=None, midi_framing_errors=None,
                      heap_used_bytes=None, stack_headroom_bytes=None)
        if result["simulated"] or not result["audio_blocks"]:
            result["cpu_average_percent"]=result["cpu_peak_percent"]=None
        result["power"]=None; result["restart"]=None
        if len(data) >= 91:
            level,flags,charge=byte(),byte(),byte()
            if level>4 or flags>63 or charge>7: raise ValueError("Invalid Inspector power fields")
            result["power"]={"battery":["full","high","medium","low","unknown"][level],"usb_power":bool(flags&1),
                             "charger_fault":bool(flags&2),"usb_lines_to_charger":bool(flags&4),"charge_state":charge,
                             "charge_done":charge==5,
                             # 0.12: weak USB supply, a reading below 3.0 V, an install would be refused
                             "weak_supply":bool(flags&8),"battery_low_reading":bool(flags&16),"install_blocked":bool(flags&32)}
        if len(data) == 98:
            flags,crashed=byte(),byte()
            if crashed>1: raise ValueError("Invalid Inspector restart fields")
            pc=u32()
            names=("power-on","brown-out","reset pin","software","watchdog","window watchdog","low-power")
            result["restart"]={"causes":[n for i,n in enumerate(names) if flags>>i&1],"crashed":bool(crashed),
                               "crash_pc":pc if crashed else None}
    elif page == 3:
        for name in ("physical","logical"):
            chunks=[byte() for _ in range(6)]
            if chunks[-1] > 31: raise ValueError("Invalid panel key mask")
            mask=sum(v << (7*i) for i,v in enumerate(chunks))
            result[name+"_keys"]=[i for i in range(40) if mask >> i & 1]
        physical,logical=byte(),byte()
        if physical>7 or logical>15: raise ValueError("Invalid panel flags")
        result["physical"]={"toggle_up":bool(physical&1),"line_jack":bool(physical&2),"tone_press":bool(physical&4)}
        result["logical"]={"toggle_up":bool(logical&1),"line_jack":bool(logical&2),"tone_press":bool(logical&4),"overridden":bool(logical&8)}
        menu=u32()
        result["menu"]={"packed":menu,"open":bool(menu&1),
                        "page":"samples" if menu>>21&1 else "harmony" if (menu>>1)&7==7 else "parts" if (menu>>1)&7==6 else "presets","bank_index":menu>>4&7}
        result["raw_encoder_turns"]=[signed(u32()) for _ in range(6)]
        result["logical_encoder_turns"]=[signed(u32()) for _ in range(6)]
        pages=byte()|(byte()<<7)
        if pages>4095: raise ValueError("Invalid knob pages")
        result["knob_pages"]=[(pages>>(3*k)&7)+1 for k in range(4)]
        result["key_mapping"]={str(i):KEY_NOTES[i] or None for i in range(40)}
        result["held_notes"]=[KEY_NOTES[i] for i in result["logical_keys"] if KEY_NOTES[i]]
    elif page == 4:
        voices=[]
        for i in range(7):
            note,source,stage,slot,flags=byte(),byte(),byte(),byte(),byte()
            if source>3 or stage>4 or slot not in (*range(15),127) or flags>7:
                raise ValueError("Invalid Inspector voice")
            voices.append({"voice":i,"note":note,"source":("uart","usb","panel","sequence")[source],
                           "stage":("off","attack","decay","sustain","release")[stage],
                           "sample_slot":slot+1 if slot<15 else None,"sampled":bool(flags&1),
                           "sustained":bool(flags&2),"reverse":bool(flags&4),"envelope":unit()})
        result["voices"]=voices; result["active_voices"]=sum(v["stage"]!="off" for v in voices)
        result.update(smoothed_cutoff_hz=40*400**unit(), lfo_value=unit()*2-1, mod_wheel=unit(), pedal_mask=byte())
        if result["pedal_mask"]>7: raise ValueError("Invalid Inspector pedal mask")
        result["bend_ratios"]=[u14()/4096 for _ in range(3)]
        result["resolved"]={"mix":unit(),"feedback":unit()*.85,"level":unit(),"delay_ms":unit()*1000,"reverb_mix":unit()}
    elif page == 5:
        flags=byte()
        if flags>63: raise ValueError("Invalid storage flags")
        result.update({name:bool(flags&(1<<i)) for i,name in enumerate(("sd_present","sd_mount_configured","busy","loading","recording","recording_locked"))})
        source,pending,job,last_error,partial=byte(),byte(),byte(),byte(),byte()
        if source>2 or job not in (0,1,2,127) or last_error>9 or partial>14:
            raise ValueError("Invalid storage state")
        result.update(record_source=("mic","line","resample")[source],pending_jobs=pending,
                      active_job=("save","copy","erase")[job] if job<3 else None,
                      last_error=last_error,partial_slots=partial)
        for key in ("loaded_selection","file_loaded_frames","file_allocated_frames","pool_used_bytes","pool_capacity_bytes",
                    "record_frames","record_capacity_frames","storage_errors","event_drops","emergencies","panel_queue_drops","sample_queue_drops"):
            result[key]=u32()
        loop=byte()
        if loop&7>4 or loop>63: raise ValueError("Invalid looper state")
        result["looper"]={"state":("empty","armed","first_take","playing","paused")[loop&7],"overdub":bool(loop&8),
                          "effects_before_loop":bool(loop&16),"locked":bool(loop&32),"length_frames":u32(),
                          "position":unit(),"speed":round(unit()*4-2,3),"feedback":unit()}
        result["sd_ready"]=result["sd_present"] and result["sd_mount_configured"]
        selection=result["loaded_selection"]
        result["loaded_files"]=(None if selection==0xffffffff or not selection&1 else
                                {"mode":"kit" if selection&2 else "chromatic","bank":chr(97+(selection>>2&7)),
                                 "slot":None if selection&2 else (selection>>5&15)+1})
    elif page == 6:
        result.update(latest=u32(),overwritten=u32())
        count=byte()
        if count>3 or len(data)!=27+17*count: raise ValueError("Invalid event page")
        result["events"]=[]
        for _ in range(count):
            serial,stamp,kind,identity,value=u32(),u32(),byte(),byte(),u32()
            if not 0<kind<len(EVENTS): raise ValueError("Unknown Inspector event kind")
            e={"serial":serial,"time_ms":stamp,"kind":EVENTS[kind],"id":identity,"value":value}
            if kind==3: e["turns"]=signed(value)
            if kind in (4,5): e.update(note=value&127,source=(value>>8)&127,sample_slot=(value>>16)&127)
            result["events"].append(e)
    elif page == 7:
        patch_data=data[15:-1]
        if not patch_data or len(patch_data)!={1:10,2:22,3:61,4:76,5:80,6:83,7:89}.get(patch_data[0]):
            raise ValueError("Invalid Inspector patch page")
        # Reuse the established patch decoder via an internal status envelope.
        # Only the decoded patch is returned; these shim diagnostics are never displayed.
        status=[*host.PREFIX,0x40,*host.word14(sequence),0,*patch_data,*([0]*10),5]
        status.append(host.checksum(status))
        result["patch"]=host.decode_response(status,sequence)["patch"]
        pos=len(data)-1
    elif page == 8:
        result["harmony"]=harmony_state(u32())
        root,degree,kind,quality,shifted,count=(byte() for _ in range(6))
        notes=[byte() for _ in range(5)]
        if root>11 or degree>6 or kind>5 or quality>4 or shifted>1 or count>5: raise ValueError("Invalid harmony page")
        result["chord"]=chord_label(root,degree,kind,quality,bool(shifted),notes[:count],result["harmony"])
        result["sounding_notes"]=byte(); result["changes"]=u32()
    elif page == 9:
        arp,clock=u32(),u32(); bpm=(byte()|byte()<<7)/10; flags=byte(); ticks=u32(); count=byte()
        notes=[byte() for _ in range(8)]; arp_note,bass_note=byte(),byte(); drops=u32()
        if count>16 or flags>15: raise ValueError("Invalid parts page")
        try: settings=host.parts_from_words(arp,clock)
        except ValueError: raise ValueError("Invalid parts page") from None
        result.update({"parts":settings,"tempo_bpm":bpm,"running":bool(flags&1),"midi_clock":bool(flags&2),
                       "latched":bool(flags&4),"active":bool(flags&8),"ticks":ticks,"set":notes[:min(count,8)],"set_count":count,
                       "arp_note":arp_note or None,"bass_note":bass_note or None,"drops":drops})
    elif page == 10:
        state,overdub=byte(),byte(); length,position,count=(byte()|byte()<<7 for _ in range(3)); drops=u32()
        if state>4 or overdub>1 or length>768 or length%96 or count>1024 or (length and position>=length):
            raise ValueError("Invalid sequence page")
        result.update({"state":("empty","armed","recording","playing","stopped")[state],"overdub":bool(overdub),
                       "bars":length//96,"length_ticks":length,"position_ticks":position,"events":count,"drops":drops})
    if pos!=len(data)-1: raise ValueError("Unexpected Inspector trailing data")
    return result


# Harmony mode (firmware 0.13, core/harmony.h): names for the packed numbers.
NOTE_NAMES=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
MODES=["major","natural minor","harmonic minor","melodic minor","dorian","phrygian","lydian","mixolydian","locrian"]
EXTENSIONS=["triad","7th","9th","11th","13th","fifth"]
ROMAN=["I","II","III","IV","V","VI","VII"]
def harmony_state(w):
    if w>>18 or (w&15)>11 or (w>>4&15)>8 or (w>>8&7)>5: raise ValueError("Invalid harmony state")
    return {"enabled":bool(w>>16&1),"tonic":NOTE_NAMES[w&15],"mode":MODES[w>>4&15],"extension":EXTENSIONS[w>>8&7],
            "inversion":w>>11&3,"open":bool(w>>13&1),"voice_leading":not w>>14&1,"layout":"real" if w>>15&1 else "static",
            "shift":bool(w>>17&1),"packed":w}
def chord_label(root,degree,kind,quality,shifted,notes,state):
    """A name for the host to show (e.g. D#m9); computed from the voiced notes, so it is what sounds."""
    kinds=["none","diatonic","secondary","borrowed","shift","interchange"]
    pcs={(n-root)%12 for n in notes}
    third="m" if 3 in pcs and 4 not in pcs else "sus4" if 5 in pcs and 4 not in pcs and 3 not in pcs else ""
    if 3 in pcs and 6 in pcs and 7 not in pcs: third="dim"
    ext=""
    if 10 in pcs: ext="7"
    if 11 in pcs: ext="maj7"
    if 2 in pcs and (10 in pcs or 11 in pcs): ext="9" if ext=="7" else "maj9" if ext=="maj7" else ext
    if 9 in pcs and (10 in pcs or 11 in pcs): ext="13"
    if 5 in pcs and third!="sus4" and (10 in pcs or 11 in pcs) and ext!="13": ext="11"
    name=NOTE_NAMES[root]+(third if third!="m" or not ext.startswith("maj") else "m")+ext
    if not notes: name=None
    function={0:"tonic",2:"tonic",5:"tonic",1:"subdominant",3:"subdominant",4:"dominant",6:"dominant"}.get(degree) \
        if kinds[kind]=="diatonic" else {"secondary":"secondary dominant","borrowed":"borrowed","interchange":"modal interchange"}.get(kinds[kind])
    numeral=ROMAN[degree] if kinds[kind] in ("diatonic","interchange") else f"V/{ROMAN[degree]}" if kinds[kind]=="secondary" else None
    return {"name":name,"root":NOTE_NAMES[root],"numeral":numeral,"function":function,"kind":kinds[kind],"shifted":shifted,
            "notes":[f"{NOTE_NAMES[n%12]}{n//12-1}" for n in notes],"midi":notes}


def collect(fetch, cursor=0):
    """Latch page 2 then read the same main-loop snapshot across all state groups."""
    pages={p:fetch(p,0) for p in (2,3,4,5,7)}
    minor=int(pages[2]["firmware"].split(".")[1])
    if minor>=13: pages[8]=fetch(8,0)   # harmony page from 0.13
    if minor>=14: pages[9]=fetch(9,0)   # parts page from 0.14
    if minor>=15: pages[10]=fetch(10,0) # event recorder page from 0.15
    if len({v["generation"] for v in pages.values()})!=1:
        raise RuntimeError("Inspector snapshot changed during read; another host may be polling")
    leds=fetch(1,0)
    events=[]; lost=0
    for _ in range(24): # bounded catch-up, at most the 64 retained events
        log=fetch(6,cursor)
        if cursor and (log["latest"]-cursor)&0xffffffff>=0x80000000:
            cursor=0 # device reboot or a cursor from an earlier firmware session
            log=fetch(6,cursor)
        batch=log["events"]
        if not batch: break
        for e in batch:
            if (e["serial"]-cursor)&0xffffffff!=1:
                lost+=((e["serial"]-cursor)&0xffffffff)-1
            cursor=e["serial"]; events.append(e)
        if cursor==log["latest"]: break
    patch=pages[7]["patch"]
    pages[4]["patch"]=patch
    pages[3]["leds"]=leds["rgb"]
    for key in ("event_drops","emergencies","panel_queue_drops","sample_queue_drops"):
        pages[2][key]=pages[5].pop(key) # SYSTEM counters packed on page 5 to keep replies bounded
    pages[2]["memory"]={"sample_pool_used_bytes":pages[5]["pool_used_bytes"],
                        "sample_pool_capacity_bytes":pages[5]["pool_capacity_bytes"],
                        "record_buffer_capacity_bytes":pages[5]["record_capacity_frames"]*4}
    modules=patch.get("modules",{})
    # Page-1 knobs (the patch's assignment or the source's default), then each knob's current page.
    pages[3]["logical_parameters"]={f"knob{k}_{host.knob_control(patch,k).split('.')[1]}":
                                    host.control_value(patch,host.knob_control(patch,k)) for k in range(1,5)}
    pages[3]["knobs"]=[{"knob":k,"switch":host.KNOB_SWITCHES[k-1],"page":page,
                        "control":host.knob_control(patch,k,page),
                        "value":host.control_value(patch,host.knob_control(patch,k,page))}
                       for k,page in zip(range(1,5),pages[3]["knob_pages"])]
    pages[3]["logical_parameters"].update(
        sw5_cutoff_hz=modules.get("filter",modules.get("synth",{})).get("cutoff_hz"),
        sw6_level=modules.get("output",patch.get("parameters",{})).get("level"))
    return {"schema":1,"generation":pages[2]["generation"],"system":pages[2],"panel":pages[3],
            "engine":pages[4],"storage":pages[5],"harmony":pages.get(8),"parts":pages.get(9),"sequence":pages.get(10),"events":events,"event_cursor":cursor,"event_gap":lost,
            "event_overwritten":log["overwritten"]}


def sequence_line(page):
    """One line for the event recorder, e.g. SEQ     playing, 2 bars, beat 3 of 8, 14 events."""
    if page["state"]=="empty": return "SEQ     empty"
    where=f', beat {page["position_ticks"]//24+1} of {page["length_ticks"]//24}' if page['length_ticks'] else ''
    bars=f', {page["bars"]} bar'+('s' if page['bars']!=1 else '') if page['bars'] else ''
    return f'SEQ     {page["state"]}'+(' + overdub' if page['overdub'] else '')+bars+where+f', {page["events"]} events'


def parts_line(page):
    """One line for the parts page, e.g. PARTS 120.0 BPM internal, running; arp up 1/16 x1 50% latch; bass root 1/4 C2; set C4 E4 G4."""
    p=page["parts"]; a,b=p["arp"],p["bass"]
    name=lambda n:f'{NOTE_NAMES[n%12]}{n//12-1}'
    clock=f'{page["tempo_bpm"]:.1f} BPM {"MIDI" if page["midi_clock"] else "internal"}, {"running" if page["running"] else "stopped"}'
    arp=f'arp {a["pattern"]}'+(f' {a["rate"]} x{a["octaves"]} {a["gate"]}%'+(' latch' if a['latch'] else '') if a['pattern']!='off' else '')
    bass=f'bass {b["mode"]}'+(f' {b["rate"]} C{b["octave"]}' if b['mode']!='off' else '')
    playing=' '.join(name(n) for n in page['set']) or 'none'
    return f'PARTS   {clock}; {arp}; {bass}; set {playing}'+(' (latched)' if page['latched'] else '')+f'; ticks {page["ticks"]}'


def harmony_line(page):
    """One line for the harmony page, e.g. HARMONY on, A natural minor, 7th, static; last Am7 · I · tonic (A4 C5 E5 G5)."""
    h,c=page["harmony"],page["chord"]
    state=f'{"on" if h["enabled"] else "off"}, {h["tonic"]} {h["mode"]}, {h["extension"]}, {h["layout"]}' \
        +(", voice leading" if h["voice_leading"] else f', inversion {h["inversion"]}')+(", open" if h["open"] else "")+(", SHIFT" if h["shift"] else "")
    last=" · ".join(x for x in (c["name"],c["numeral"],c["function"]) if x)+f' ({" ".join(c["notes"])})' if c["name"] else "none"
    return f'HARMONY {state}; last {last}; sounding {page["sounding_notes"]}; chords {page["changes"]}'


def display(s, recent=()):
    sys,panel,engine,storage=(s[k] for k in ("system","panel","engine","storage"))
    cpu="unavailable" if sys["cpu_average_percent"] is None else f'{sys["cpu_average_percent"]:.1f}% avg / {sys["cpu_peak_percent"]:.1f}% peak'
    mode=engine["patch"].get("routing","aux").split(">")[0]
    lines=[f'Forge Inspector v1 | snapshot {s["generation"]} | {"SIMULATION" if sys["simulated"] else "DEVICE"}',
           f'SYSTEM  firmware {sys["firmware"]}, protocol {sys["protocol"]}, uptime {sys["uptime_ms"]/1000:.1f}s',
           f'        audio {cpu}; state age {(sys["uptime_ms"]-sys["audio_time_ms"])&0xffffffff}ms; underruns unavailable',
           f'        drops {sys["dropped"]}, rejected {sys["rejected"]}, panel/sample queue {sys["panel_queue_drops"]}/{sys["sample_queue_drops"]}, emergencies {sys["emergencies"]}; MIDI {json.dumps(sys["midi"])}',
           f'PANEL   physical keys {panel["physical_keys"]}; logical keys {panel["logical_keys"]}; notes {panel["held_notes"]}',
           f'        physical {panel["physical"]}; logical {panel["logical"]}; menu {panel["menu"]}',
           f'        encoders raw {panel["raw_encoder_turns"]}; logical {panel["logical_encoder_turns"]}',
           f'        parameters {panel["logical_parameters"]}; lit LEDs {[i for i,c in enumerate(panel["leds"]) if any(c)]}',
           '        knob pages '+', '.join(f'{k["switch"]} p{k["page"]} {k["control"]}' for k in panel["knobs"]),
           f'ENGINE  {mode}; {engine["active_voices"]} voices; targets {engine["patch"].get("modules",engine["patch"].get("parameters"))}',
           f'        voices {[v for v in engine["voices"] if v["stage"]!="off"]}',
           f'        resolved {engine["resolved"]}; cutoff {engine["smoothed_cutoff_hz"]:.1f}Hz; LFO {engine["lfo_value"]:.3f}; wheel {engine["mod_wheel"]:.3f}',
           f'STORAGE ready {storage["sd_ready"]}; loaded {storage["loaded_files"]}; loading {storage["loading"]}; partial slots {storage["partial_slots"]}',
           f'        files {storage["file_loaded_frames"]}/{storage["file_allocated_frames"]} frames; pool {storage["pool_used_bytes"]}/{storage["pool_capacity_bytes"]} bytes',
           f'        recording {storage["recording"]}, source {storage["record_source"]}, frames {storage["record_frames"]}/{storage["record_capacity_frames"]}',
           f'        jobs {storage["pending_jobs"]}, active {storage["active_job"]}; errors {storage["storage_errors"]}, last {storage["last_error"]}',
           f'EVENTS  gaps {s["event_gap"]}; overwritten {s["event_overwritten"]}; audio event drops {sys["event_drops"]}']
    if s.get("harmony"): lines.insert(-1,harmony_line(s["harmony"]))
    if s.get("parts"): lines.insert(-1,parts_line(s["parts"]))
    if s.get("sequence"): lines.insert(-1,sequence_line(s["sequence"]))
    lines.extend(f'        {e["serial"]} @ {e["time_ms"]}ms {e["kind"]} id={e["id"]} value={e["value"]}' for e in recent)
    return "\n".join(lines)


class Offline:
    def __init__(self, probe):
        self.process=subprocess.Popen([str(probe)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
    def fetch(self,payload,sequence,page):
        self.process.stdin.write(bytes([0xf0,*payload,0xf7]).hex(" ")+"\n"); self.process.stdin.flush()
        line=self.process.stdout.readline()
        if not line: raise RuntimeError("Offline probe exited")
        return decode(bytes.fromhex(line),sequence,page)
    def close(self):
        self.process.stdin.close()
        try: self.process.wait(timeout=2)
        except subprocess.TimeoutExpired: self.process.kill(); self.process.wait()
        self.process.stdout.close()


def cli(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input"); parser.add_argument("--output")
    parser.add_argument("--offline",type=Path,help="Use the offline probe (simulation, never hardware evidence)")
    parser.add_argument("--watch",action="store_true"); parser.add_argument("--interval",type=float,default=1.0)
    parser.add_argument("--json",action="store_true"); parser.add_argument("--record",type=Path,help="Append snapshots as JSONL")
    args=parser.parse_args(argv)
    if not math.isfinite(args.interval) or not .5<=args.interval<=30: parser.error("Interval must be 0.5–30 seconds")
    if not args.offline and not (args.input and args.output): parser.error("Provide both exact MIDI port names or --offline")
    offline=Offline(args.offline) if args.offline else None
    sequence=0; cursor=0; recent=deque(maxlen=12)
    def fetch(page,after):
        nonlocal sequence
        sequence=(sequence+1)&16383; payload=request(page,sequence,after)
        if offline: return offline.fetch(payload,sequence,page)
        return host.exchange(payload,args.input,args.output,decoder=lambda data,seq:decode(data,seq,page))
    try:
        while True:
            started=time.monotonic(); snapshot=collect(fetch,cursor); cursor=snapshot["event_cursor"]
            recent.extend(snapshot["events"])
            if args.record:
                with args.record.open("a",encoding="utf-8") as out: out.write(json.dumps(snapshot)+"\n")
            if args.json: print(json.dumps(snapshot),flush=True)
            else:
                if args.watch and sys.stdout.isatty(): print("\x1b[2J\x1b[H",end="")
                print(display(snapshot,recent),flush=True)
            if not args.watch: break
            time.sleep(max(0,args.interval-(time.monotonic()-started)))
    finally:
        if offline: offline.close()
    return 0


if __name__=="__main__":
    try: sys.exit(cli())
    except KeyboardInterrupt: sys.exit(0)
    except (ValueError,RuntimeError,OSError,TimeoutError) as e:
        print(f"Forge Inspector: {e}",file=sys.stderr); sys.exit(1)
