"use strict";
// Take it to CHOMPI (0.19.2): Web MIDI carries the bytes; the local Forge server encodes the patch and
// decodes CHOMPI's reply (forge_host, the same wire format as the bridge and the tests).
const $ = id => document.getElementById(id);
let token = "", access = null, sound = null, busy = false;
const PREFIX = [0xF0, 0x7D, 0x46, 0x47, 0x01];

function notice(message, error = false) { $("notice").textContent = message; $("notice").classList.toggle("error", error); }
function connected() { return !!(access && $("output").value && $("input").value); }
function refresh() {
  for (const id of ["connect", "from-workshop", "file"]) $(id).disabled = busy;
  for (const id of ["status", "stop"]) $(id).disabled = busy || !connected();
  $("send").disabled = busy || !connected() || !sound;
  $("output").disabled = $("input").disabled = busy || !access;
}
async function api(path, body) {
  const response = await fetch(`/api/${path}`, {method: "POST", headers: {"Content-Type": "application/json", "X-Forge-Token": token}, body: JSON.stringify(body)});
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || "Request failed");
  return result;
}
async function run(message, action) {
  if (busy) return;
  busy = true; refresh(); notice(message);
  try { await action(); } catch (error) { notice(error.message || "That didn't work", true); }
  finally { busy = false; refresh(); }
}
// Send one request and wait for CHOMPI's reply with the same sequence number.
function exchange(request, timeout = 2500) {
  const input = access.inputs.get($("input").value), output = access.outputs.get($("output").value);
  if (!input || !output) return Promise.reject(new Error("CHOMPI's MIDI ports went away: plug it in and press Connect."));
  return new Promise((resolve, reject) => {
    const sent = request.sysex, timer = setTimeout(() => { input.onmidimessage = null; reject(new Error("CHOMPI didn't answer. Is it on, and running Forge (not stock TAPE)?")); }, timeout);
    input.onmidimessage = event => {
      const data = Array.from(event.data);
      if (data.length < 9 || PREFIX.some((b, i) => data[i] !== b) || data[6] !== sent[6] || data[7] !== sent[7]) return;
      clearTimeout(timer); input.onmidimessage = null;
      api("midi/reply", {data, sent}).then(resolve, reject);
    };
    output.send(sent);
  });
}
function describe(status) {
  const power = status.power ? ` · battery ${status.power.battery}${status.power.charging ? ", charging" : ""}` : "";
  return `CHOMPI · Forge ${status.firmware}${status.build ? ` (build ${status.build})` : ""}${status.safe_mode ? " · SAFE MODE" : ""}${power}`;
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
  if (!navigator.requestMIDIAccess) throw new Error("This browser has no Web MIDI. Use Chrome or Edge.");
  try { access = await navigator.requestMIDIAccess({sysex: true}); }
  catch { throw new Error("MIDI permission was refused. Allow MIDI devices for this page (the icon in the address bar), then Connect again."); }
  access.onstatechange = () => { fillPorts(); refresh(); };
  fillPorts(); refresh();
  if (!connected()) { notice("Connected to MIDI, but no CHOMPI found. Plug it in, switch it on, or pick its ports."); return; }
  const status = await exchange(await api("midi/status", {}));
  $("device").textContent = describe(status); notice("CHOMPI found. Pick a sound and send it.");
}));
for (const id of ["output", "input"]) $(id).addEventListener("change", refresh);
$("status").addEventListener("click", () => run("Checking CHOMPI…", async () => {
  const status = await exchange(await api("midi/status", {}));
  $("device").textContent = describe(status); notice("CHOMPI answered.");
}));
$("stop").addEventListener("click", () => run("Stopping the sound…", async () => {
  await exchange(await api("midi/stop", {})); notice("Stopped. Play a key to start again.");
}));
function pick(patch, from) { sound = patch; $("sound").textContent = `${patch.name} (v${patch.version}, ${from})`; refresh(); }
$("from-workshop").addEventListener("click", () => run("Getting the workshop's sound…", async () => {
  let raw = null;
  try { raw = localStorage.getItem("forge.lastSound"); } catch { /* storage blocked */ }
  if (!raw) throw new Error("No sound from the workshop yet. Make or open one there first, or open a JSON file.");
  pick((await api("validate", {patch: JSON.parse(raw)})).patch, "from the workshop"); notice("Sound picked. Send it when CHOMPI is connected.");
}));
$("file").addEventListener("change", () => run("Checking the file…", async () => {
  try {
    const file = $("file").files[0]; if (!file) return;
    if (file.size > 65536) throw new Error("That file is over 64 KiB: not a Forge sound.");
    const response = await fetch("/api/validate", {method: "POST", headers: {"Content-Type": "application/json", "X-Forge-Token": token}, body: `{"patch":${await file.text()}}`});
    const result = await response.json(); if (!response.ok) throw new Error(result.error);
    pick(result.patch, file.name); notice("Sound picked. Send it when CHOMPI is connected.");
  } finally { $("file").value = ""; }
}));
$("send").addEventListener("click", () => run(`Sending ${sound.name}…`, async () => {
  const status = await exchange(await api("midi/send", {patch: sound}));
  $("device").textContent = describe(status);
  notice(`CHOMPI is playing ${sound.name}. Save it to a slot on the panel to keep it.`);
}));
(async () => {
  busy = true; refresh();
  try {
    const response = await fetch("/api/session"); if (!response.ok) throw new Error("Could not start a local session. Reload the URL printed by Forge.");
    token = (await response.json()).token;
  } catch (error) { notice(error.message, true); }
  finally { busy = false; refresh(); }
})();
