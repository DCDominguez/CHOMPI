"use strict";
const $ = id => document.getElementById(id);
let token, owner, connected = false, busy = false, paused = false, armed = false, jobActive = false;
let presets = [], checks = [], results = {}, events = [], lastReceived = 0, nextPoll = 0, lastHeartbeat = 0;
const value = id => $(id).value;
const number = id => Number(value(id));
const text = (id, message) => { $(id).textContent = message; };
function notice(message, error = false) { text("notice", message); $("notice").classList.toggle("error", error); }
function controls() {
  for (const id of ["mode", "input", "output", "metadata", "ports", "connect"]) $(id).disabled = busy || connected;
  for (const id of ["pause", "disconnect", "arm", "release", "panic", "save-check"]) $(id).disabled = busy || !connected || (jobActive && id !== "save-check");
  for (const id of ["detect", "autorun"]) $(id).disabled = busy || !connected || jobActive;
  $("cancel-job").disabled = !jobActive;
  for (const id of ["export", "jsonl"]) $(id).disabled = busy || !owner;
  $("controls").disabled = busy || !connected || !armed || jobActive;
  text("pause", paused ? "Resume polling" : "Pause polling");
}
async function api(path, body = {}) {
  const response = await fetch(path, {method: "POST", headers: {"Content-Type":"application/json", "X-Forge-Token":token}, body:JSON.stringify(body)});
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || "Request failed");
  return data;
}
const bridge = (op, body = {}) => api(`/api/bridge/${op}`, {owner, ...body});
async function run(fn) {
  if (busy) return;
  busy = true; controls();
  try { await fn(); } catch (error) { notice(error.message, true); }
  finally { busy = false; controls(); }
}
function options(id, choices) {
  $(id).replaceChildren(...choices.map(([v,t]) => { const option=document.createElement("option"); option.value=v; option.textContent=t; return option; }));
}
function scalar(v) { return v === null || v === undefined ? "Unavailable" : typeof v === "boolean" ? (v ? "Yes" : "No") : typeof v === "number" ? (Number.isInteger(v) ? String(v) : v.toFixed(3)) : String(v); }
function fields(id, object) {
  const dl=document.createElement("dl");
  function add(o, prefix="") {
    for (const [k,v] of Object.entries(o)) {
      if (v && typeof v === "object" && !Array.isArray(v)) { add(v,`${prefix}${k}.`); continue; }
      const dt=document.createElement("dt"), dd=document.createElement("dd");
      dt.textContent=(prefix+k).replaceAll("_"," "); dd.textContent=Array.isArray(v) ? JSON.stringify(v) : scalar(v);
      dl.append(dt,dd);
    }
  }
  add(object); $(id).replaceChildren(dl);
}
function drawEvents() {
  const filter=value("event-filter");
  text("events", events.filter(e=>e.kind.includes(filter)).map(e=>{
    const origin=e.kind.startsWith("key_") ? ` [${e.value & 1 ? "physical" : ""}${e.value === 3 ? "+" : ""}${e.value & 2 ? "injected" : ""}]` : "";
    return `${e.time_ms} ms  #${e.serial}  ${e.kind}${origin}  id=${e.id} value=${e.value}`;
  }).join("\n") || "No matching events.");
}
function render(s) {
  lastReceived=Date.now();
  const sys=s.system, p=s.panel, e=s.engine, st=s.storage;
  $("telemetry").classList.remove("stale");
  $("notice").classList.toggle("simulation",sys.simulated);
  fields("system-summary",{firmware:sys.firmware,protocol:sys.protocol,uptime_seconds:sys.uptime_ms/1000,
    CPU_average_percent:sys.cpu_average_percent,CPU_peak_percent:sys.cpu_peak_percent,dropped:sys.dropped,rejected:sys.rejected,
    audio_state_age_ms:(sys.uptime_ms-sys.audio_time_ms)>>>0});
  fields("panel-summary",{physical:p.physical,logical:p.logical,menu:p.menu,raw_encoder_turns:p.raw_encoder_turns,
    logical_encoder_turns:p.logical_encoder_turns,parameters:p.logical_parameters});
  fields("engine-summary",{source:e.patch.routing?.split(">")[0] || "aux",active_voices:e.active_voices,cutoff_Hz:e.smoothed_cutoff_hz,resolved:e.resolved});
  fields("storage-summary",{SD_ready:st.sd_ready,loaded_files:st.loaded_files??"No completed file selection",loaded_frames:st.file_loaded_frames,
    allocated_frames:st.file_allocated_frames,loading:st.loading,recording:st.recording,record_source:st.record_source,
    record_frames:st.record_frames,record_capacity:st.record_capacity_frames,pending_jobs:st.pending_jobs,active_job:st.active_job??"None",storage_errors:st.storage_errors});
  for (const name of ["system","panel","engine","storage"]) fields(`${name}-fields`,s[name]);
  $("keys").replaceChildren(...Array.from({length:40},(_,i)=>{
    const key=document.createElement("div"); key.className=`key${p.physical_keys.includes(i)?" physical":""}${p.logical_keys.includes(i)?" logical":""}`;
    key.textContent=`${i} / ${p.key_mapping[i] ?? "—"}`;
    key.title=`Switch ${i}: physical ${p.physical_keys.includes(i)}, logical ${p.logical_keys.includes(i)}`; return key;
  }));
  $("leds").replaceChildren(...p.leds.map((rgb,i)=>{
    const led=document.createElement("div"); led.className="led"; led.textContent=i;
    led.style.backgroundColor=`rgb(${rgb.map(c=>Math.round(c*255/127)).join(",")})`; led.title=`LED ${i}: RGB ${rgb.join(", ")}`; return led;
  }));
  $("voices").replaceChildren(...e.voices.map(v=>{
    const row=document.createElement("tr");
    for (const cell of [v.voice,v.stage==="off"?"—":v.note,v.source,v.stage,v.sample_slot??"—"]) { const td=document.createElement("td"); td.textContent=cell; row.append(td); }
    return row;
  }));
  events.push(...s.events); events=events.slice(-300); drawEvents();
  text("losses",`This read: ${s.event_gap} missed events · device retention overwrites: ${s.event_overwritten} · audio event drops: ${sys.event_drops}. Screen retains 300 events; export contains the bounded session trace.`);
  notice(`${sys.simulated?"SIMULATION · no physical evidence":"DEVICE · physical verification requires your observations"} · snapshot ${s.generation}${p.logical.overridden?" · PANEL OVERRIDES ACTIVE":""}`);
}
function disconnected() { connected=false; armed=false; $("arm").checked=false; $("telemetry").classList.add("stale"); }
async function action(body) {
  try {
    const response=await bridge("action",body); render(response.snapshot); text("action-result",JSON.stringify(response.result,null,2));
  } catch(error) {
    // Validation errors leave the server session alive. A heartbeat distinguishes them from disconnects.
    try { await bridge("heartbeat"); } catch { disconnected(); }
    throw error;
  }
}
const bind = (id,fn) => $(id).addEventListener("click",()=>run(fn));
bind("ports",async()=>{
  const p=await api("/api/ports");
  for (const kind of ["input","output"]) options(kind,[["","Choose an explicit port"],...p[`${kind}s`].map(n=>[n,n])]);
  notice(p.inputs.length && p.outputs.length ? "Select CHOMPI's input and output." : "No MIDI ports found. Check the data cable, development firmware and device connection.");
});
bind("connect",async()=>{
  if(value("mode")==="hardware" && !(value("input") && value("output"))) {
    notice("Looking for CHOMPI…");
    const found=await api("/api/bridge/discover",{});
    for (const kind of ["input","output"]) options(kind,[[found[kind],found[kind]]]);
    notice(`Found CHOMPI (Forge ${found.firmware}) on ${found.input} / ${found.output}. Connecting…`);
  }
  const r=await bridge("connect",{mode:value("mode"),input:value("input"),output:value("output"),metadata:value("metadata")});
  owner=r.owner; connected=true; paused=false; armed=false; $("arm").checked=false; events=[]; results={};
  sessionStorage.setItem("forge-bridge-owner",owner);
  lastHeartbeat=Date.now(); nextPoll=Date.now()+number("interval"); render(r.snapshot); showCheck();
});
bind("disconnect",async()=>{ const r=await bridge("disconnect"); disconnected(); notice(r.cleanup_errors.length?r.cleanup_errors.join(" "):"Disconnected. Export your session before starting a new one.",!!r.cleanup_errors.length); });
bind("pause",async()=>{await bridge("marker",{message:paused?"Operator resumed polling":"Operator paused polling for comparison"}); paused=!paused; nextPoll=Date.now(); notice(paused?"Polling paused. You can compare audio behaviour; heartbeat keeps the connection open.":"Polling resumed.");});
$("arm").addEventListener("change",()=>run(async()=>{const r=await bridge("arm",{enabled:$("arm").checked}); armed=r.enabled;}));
bind("release",()=>action({action:"release"})); bind("panic",()=>action({action:"panic"}));
bind("send-patch",()=>action({action:"patch",patch:presets[number("preset")]}));
$("patch-file").addEventListener("change",()=>run(async()=>{
  const file=$("patch-file").files[0]; if(!file) return;
  if(file.size>65536) throw new Error("Patch exceeds 64 KiB");
  const r=await api("/api/validate",{patch:JSON.parse(await file.text())}); presets.push(r.patch);
  options("preset",presets.map((p,i)=>[i,p.name])); $("preset").value=presets.length-1;
  notice("Imported and validated. Use Send selected patch to apply it.");
}));
function panel(kind,id,v) { return action({action:"panel",kind,id,value:v}); }
bind("key-down",()=>panel(0,number("key-id"),1)); bind("key-up",()=>panel(0,number("key-id"),0));
bind("turn-left",()=>panel(1,number("encoder"),-1)); bind("turn-right",()=>panel(1,number("encoder"),1));
bind("tone",()=>panel(2,4,0)); bind("set-toggle",()=>panel(3,0,number("toggle"))); bind("set-jack",()=>panel(4,0,number("jack")));
bind("play-note",()=>action({action:"note",note:number("note"),velocity:number("velocity"),duration_ms:number("duration"),
  notes:value("chord").trim()?value("chord").split(",").map(n=>Number(n.trim())):[number("note")],bend:number("bend"),sustain:$("sustain").checked}));
bind("send-cc",()=>action({action:"cc",control:number("cc"),value:number("cc-value")}));
for (const button of document.querySelectorAll("[data-preset],[data-sample]")) button.addEventListener("click",()=>run(async()=>{
  const isPreset=!!button.dataset.preset;
  const body={action:isPreset?"preset":"sample",verb:button.dataset.preset||button.dataset.sample,
    bank:isPreset?number("preset-bank"):value("sample-bank"),slot:number(isPreset?"preset-slot":"sample-slot"),
    sample_mode:value("sample-mode"),to_mode:value("to-mode"),to_bank:value("to-bank"),to_slot:number("to-slot"),
    confirm_storage:$("confirm-storage").checked};
  $("confirm-storage").checked=false; await action(body);
}));
function showCheck() {
  const c=checks[number("check")]; if(!c) return;
  text("check-title",`${c.id} · ${c.group}`); text("check-do",c.do); text("check-expect",`Expected: ${c.expect}`);
  const r=results[c.id]; $("observation").value=r?.notes||""; $("result").value=r?.result||"not_run";
  text("check-progress",`${Object.values(results).filter(r=>r.result!=="not_run").length} / ${checks.length} checks recorded. Results are operator assessments.`);
}
$("check").addEventListener("change",showCheck);
bind("next-check",async()=>{$("check").selectedIndex=Math.min(checks.length-1,number("check")+1); showCheck();});
bind("save-check",async()=>{
  const c=checks[number("check")]; results[c.id]=await bridge("check",{id:c.id,result:value("result"),notes:value("observation")}); showCheck(); notice(`Recorded ${c.id}: ${results[c.id].result} (${results[c.id].mode}).`);
});
function download(name,data,type) {
  const url=URL.createObjectURL(new Blob([data],{type})); const a=document.createElement("a"); a.href=url; a.download=name; a.click(); setTimeout(()=>URL.revokeObjectURL(url),1000);
}
bind("export",async()=>{ const report=await bridge("export"); download("forge-session.json",JSON.stringify(report,null,2),"application/json"); });
bind("jsonl",async()=>{ const report=await bridge("export"); const {records,...metadata}=report; download("forge-trace.jsonl",[JSON.stringify({kind:"session",value:metadata}),...records.map(r=>JSON.stringify(r))].join("\n")+"\n","application/x-ndjson"); });
$("event-filter").addEventListener("change",drawEvents);
// ---- automatic checks: background jobs on the server; the page polls their progress ----
function el(tag, className, content) { const e=document.createElement(tag); if(className) e.className=className; if(content!==undefined) e.textContent=content; return e; }
function showAudio(info) {
  if(!info) return;
  text("audio-status",`Audio interface: ${info.input?`hears CHOMPI on “${info.input.name}”`:"none hears CHOMPI (audio steps will be skipped)"} · ${info.output?`plays into line in from “${info.output.name}”`:"no output reaches line in (tone steps will be skipped)"}`);
}
function showRun(result) {
  const c=result.counts, box=$("auto-results");
  const head=el("p","auto-counts",`pass ${c.pass} · fail ${c.fail} · error ${c.error} · skipped ${c.skipped}${result.cancelled?" · cancelled":""}. Files: ${result.folder}`);
  const rows=result.steps.map(s=>{
    const d=el("details",`auto-step ${s.result}`); const sum=el("summary");
    sum.append(el("span",`badge-${s.result}`,s.result.toUpperCase()),` ${s.id} · ${s.title}`); d.append(sum);
    if(s.reason||s.error) d.append(el("p","hint",s.reason||s.error));
    const bad=s.checks.filter(k=>!k.ok);
    if(bad.length) d.append(el("pre","",bad.map(k=>`expected ${k.what} = ${JSON.stringify(k.expected)}, got ${JSON.stringify(k.actual)}`).join("\n")));
    for (const [name,cap] of Object.entries(s.captures||{})) {
      d.append(el("p","hint",`${name}: peak ${cap.peak_db} dBFS · pitch ${cap.pitch_hz??"none"} Hz · clicks ${cap.clicks.length} · ${cap.silent?"silent":"sound"}`));
      const show=el("button","","Show spectrogram");
      show.addEventListener("click",()=>run(async()=>{ const r=await bridge("capture",{image:cap.image}); const img=el("img","spectrogram"); img.src=r.png; img.alt=`Spectrogram of ${s.id} ${name}`; show.replaceWith(img); }));
      d.append(show);
    }
    return d;
  });
  box.replaceChildren(head,...rows);
}
async function followJob() {
  const j=await bridge("job");
  text("job-log",j.progress.join("\n")||"Starting…");
  if(!j.finished) return false;
  jobActive=false; controls();
  if(j.error) notice(`${j.kind==="autorun"?"Automatic checks":"Audio search"} stopped: ${j.error}`,true);
  else if(j.kind==="audio_detect") { showAudio(j.result); notice("Audio search finished."); }
  else { showRun(j.result); notice(`Automatic checks finished: ${j.result.counts.pass} pass, ${j.result.counts.fail} fail, ${j.result.counts.error} error, ${j.result.counts.skipped} skipped.`, j.result.counts.fail+j.result.counts.error>0); }
  nextPoll=Date.now();
  return true;
}
async function startJob(op, body={}) {
  await bridge(op, body); jobActive=true; controls(); text("job-log","Starting…"); $("auto-results").replaceChildren();
}
bind("detect",()=>startJob("audio_detect"));
bind("autorun",async()=>{
  if(!window.confirm("Automatic checks send patches, play notes and quiet test tones, press virtual panel keys and record a short take into CHOMPI's RAM (nothing is written to the SD card). About 2 minutes. Keep monitoring volume low. Start?")) return;
  await startJob("autorun",{confirm:true});
});
bind("cancel-job",async()=>{ await bridge("cancel"); notice("Cancelling after the current step…"); });
setInterval(()=>{
  if(lastReceived) text("freshness",`${connected?(paused?"PAUSED":"CONNECTED"):"DISCONNECTED"} · last complete state ${Math.floor((Date.now()-lastReceived)/1000)}s ago`);
  $("telemetry").classList.toggle("stale",!connected || paused || Date.now()-lastReceived>number("interval")*2+2000);
  if(!connected || busy) return;
  if(jobActive) { run(async()=>{ try { await followJob(); lastHeartbeat=Date.now(); } catch(error) { jobActive=false; disconnected(); throw error; } }); return; }
  if(!paused && Date.now()>=nextPoll) run(async()=>{
    try { render(await bridge("poll")); lastHeartbeat=Date.now(); }
    catch(error) { disconnected(); throw error; }
    finally { nextPoll=Date.now()+number("interval"); }
  });
  else if(Date.now()-lastHeartbeat>8000) run(async()=>{
    try { await bridge("heartbeat"); lastHeartbeat=Date.now(); }
    catch(error) { disconnected(); throw error; }
  });
},500);
window.addEventListener("pagehide",()=>{
  if(connected) fetch("/api/bridge/disconnect",{method:"POST",keepalive:true,headers:{"Content-Type":"application/json","X-Forge-Token":token},body:JSON.stringify({owner})}).catch(()=>{});
});
async function init() {
  const response=await fetch("/api/session"); if(!response.ok) throw new Error("Could not open local session");
  const session=await response.json(); token=session.token; presets=session.presets; checks=session.checks;
  if(session.simulation_available) $("mode").append(new Option("SIMULATION · C++ probe, no hardware evidence","simulation"));
  options("preset",presets.map((p,i)=>[i,p.name])); options("check",checks.map((c,i)=>[i,`${c.id} · ${c.group}`]));
  owner=sessionStorage.getItem("forge-bridge-owner");
  if(owner) {
    try {
      const r=await bridge("resume"); connected=r.connected; paused=true; armed=r.armed; results=r.checks;
      $("arm").checked=armed; $("metadata").value=r.metadata; $("mode").value=r.mode;
      if(r.snapshot) render(r.snapshot);
      const j=await bridge("job");
      if(j.kind && !j.finished) { jobActive=true; text("job-log",j.progress.join("\n")); }
      else if(j.kind==="autorun" && j.result) showRun(j.result);
      showCheck(); controls(); notice(connected?"Session restored with polling paused. Resume when ready.":"Previous session retained. Export before starting a new one.");
      return;
    } catch { owner=null; sessionStorage.removeItem("forge-bridge-owner"); }
  }
  showCheck(); controls(); notice("Ready. Select a connection to begin; no device actions have been sent.");
}
init().catch(e=>notice(e.message,true));
