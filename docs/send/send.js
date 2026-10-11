"use strict";
// Take it to CHOMPI, the static version for GitHub Pages: everything runs in the browser. forge-wire.js
// encodes the sound and checks CHOMPI's reply (the same wire format as forge_host, tested byte for byte
// in tests/test_wire_js.py); Web MIDI carries the bytes.
const $ = id => document.getElementById(id);
const W = window.ForgeWire;
let access = null, sound = null, busy = false;

function notice(message, error = false) { $("notice").textContent = message; $("notice").classList.toggle("error", error); }
function connected() { return !!(access && $("output").value && $("input").value); }
function refresh() {
  for (const id of ["connect", "file", "paste", "use-paste"]) $(id).disabled = busy;
  for (const id of ["status", "stop"]) $(id).disabled = busy || !connected();
  $("send").disabled = busy || !connected() || !sound;
  $("output").disabled = $("input").disabled = busy || !access;
}
async function run(message, action) {
  if (busy) return;
  busy = true; refresh(); notice(message);
  try { await action(); } catch (error) { notice(error.message || "That didn't work", true); }
  finally { busy = false; refresh(); }
}
const sequence = () => Math.floor(Math.random() * 16384);
// Send one request (payload without F0 / F7) and wait for CHOMPI's reply with the same sequence.
function exchange(payload, timeout = 2500) {
  const input = access.inputs.get($("input").value), output = access.outputs.get($("output").value);
  if (!input || !output) return Promise.reject(new Error("CHOMPI's MIDI ports went away: plug it in and press Connect."));
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => { input.onmidimessage = null; reject(new Error("CHOMPI didn't answer. Is it on, and running Choowi (not stock TAPE)?")); }, timeout);
    input.onmidimessage = event => {
      const data = Array.from(event.data);
      if (data.length < 10 || data[0] !== 0xF0 || data[1] !== 0x7D || data[6] !== payload[5] || data[7] !== payload[6]) return;
      clearTimeout(timer); input.onmidimessage = null;
      try { resolve(W.decodeReply(data.slice(1, -1), payload)); } catch (error) { reject(error); }
    };
    output.send(W.frame(payload));
  });
}
function describe(s) {
  const power = s.power ? ` · battery ${s.power.battery}${s.power.charging ? ", charging" : ""}` : "";
  return `CHOMPI · Forge ${s.firmware}${s.build ? ` (build ${s.build})` : ""}${s.safeMode ? " · SAFE MODE" : ""}${power}`;
}
function fillPorts() {
  for (const [id, ports] of [["output", access.outputs], ["input", access.inputs]]) {
    const select = $(id), list = [...ports.values()];
    select.replaceChildren(new Option(list.length ? "pick a port" : "no MIDI ports found", ""));
    for (const port of list) select.add(new Option(port.name, port.id));
    const chompi = list.find(port => /chompi/i.test(port.name));
    if (chompi) select.value = chompi.id;
  }
}
$("connect").addEventListener("click", () => run("Asking the browser for MIDI…", async () => {
  if (!navigator.requestMIDIAccess) throw new Error("This browser has no Web MIDI. Use Chrome or Edge on a computer.");
  try { access = await navigator.requestMIDIAccess({sysex: true}); }
  catch { throw new Error("MIDI permission was refused. Allow MIDI devices for this page (the icon in the address bar), then Connect again."); }
  access.onstatechange = () => { fillPorts(); refresh(); };
  fillPorts(); refresh();
  if (!connected()) { notice("Connected to MIDI, but no CHOMPI found. Plug it in, switch it on, or pick its ports."); return; }
  $("device").textContent = describe(await exchange(W.statusMessage(sequence()))); notice("CHOMPI found. Pick a sound and send it.");
}));
for (const id of ["output", "input"]) $(id).addEventListener("change", refresh);
$("status").addEventListener("click", () => run("Checking CHOMPI…", async () => {
  $("device").textContent = describe(await exchange(W.statusMessage(sequence()))); notice("CHOMPI answered.");
}));
$("stop").addEventListener("click", () => run("Stopping the sound…", async () => {
  await exchange(W.stopMessage(sequence())); notice("Stopped. Play a key to start again.");
}));
function pick(patch, from) { sound = patch; $("sound").textContent = `${patch.name} (${from})`; refresh(); notice("Sound picked. Send it when CHOMPI is connected."); }
$("file").addEventListener("change", () => run("Checking the file…", async () => {
  try {
    const file = $("file").files[0]; if (!file) return;
    if (file.size > 65536) throw new Error("That file is over 64 KiB: not a Choowi sound.");
    pick(W.readSound(await file.text()), file.name);
  } finally { $("file").value = ""; }
}));
$("use-paste").addEventListener("click", () => run("Checking what you pasted…", async () => { pick(W.readSound($("paste").value), "pasted"); }));
$("send").addEventListener("click", () => run(`Sending ${sound.name}…`, async () => {
  $("device").textContent = describe(await exchange(W.encodePatch(sound, sequence())));
  notice(`CHOMPI is playing ${sound.name}. Save it to a slot on the panel to keep it.`);
}));
refresh();
