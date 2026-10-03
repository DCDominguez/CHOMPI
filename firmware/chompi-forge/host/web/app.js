"use strict";
const $ = id => document.getElementById(id);
const WAVES = ["sine", "triangle", "saw", "square"], LFO_WAVES = ["sine", "triangle", "square", "sample_hold"];
const ROUTES = {2: [["synth>delay>output", "Synth → Delay → Output"], ["aux>delay>output", "Aux input → Delay → Output"]],
  3: [["synth>delay>reverb>output", "Synth → Delay → Reverb → Output"], ["aux>delay>reverb>output", "Aux input → Delay → Reverb → Output"]]};
// One row per editor control. m/k = v3 module/key; id = `${m}-${k}`. type: number (default), select, check.
// v = patch versions that have the field. log = slider runs 0–1000 on the firmware's log curve.
const GROUPS = [
  ["source", "Source · oscillators"], ["amp", "Amplitude · voices"], ["filter", "Filter"], ["lfo", "LFO · mod wheel"],
  ["delay", "Stereo delay"], ["reverb", "Reverb"], ["output", "Output"]];
const CONTROLS = [
  {g: "source", m: "synth", k: "waveform", label: "Oscillator", type: "select", options: WAVES, v: [2, 3]},
  {g: "source", m: "synth", k: "osc2_waveform", label: "Oscillator 2", type: "select", options: WAVES, v: [3]},
  {g: "source", m: "synth", k: "osc2_level", label: "Oscillator 2 level", unit: "0–1", min: 0, max: 1, step: .01, v: [3]},
  {g: "source", m: "synth", k: "osc2_semitones", label: "Oscillator 2 interval", unit: "semitones", min: -24, max: 24, step: 1, v: [3]},
  {g: "source", m: "synth", k: "osc2_detune_cents", label: "Oscillator 2 detune", unit: "cents", min: -50, max: 50, step: 1, v: [3]},
  {g: "source", m: "synth", k: "noise", label: "Noise", unit: "0–1", min: 0, max: 1, step: .01, v: [3]},
  {g: "amp", m: "synth", k: "attack_ms", label: "Attack", unit: "ms", min: 1, max: 2000, step: 1, v: [2, 3]},
  {g: "amp", m: "synth", k: "decay_ms", label: "Decay", unit: "ms", min: 1, max: 2000, step: 1, v: [2, 3]},
  {g: "amp", m: "synth", k: "sustain", label: "Sustain", unit: "0–1", min: 0, max: 1, step: .01, v: [2, 3]},
  {g: "amp", m: "synth", k: "release_ms", label: "Release", unit: "ms", min: 5, max: 5000, step: 1, v: [2, 3]},
  {g: "amp", m: "synth", k: "voices", label: "Voices", unit: "1 = mono", min: 1, max: 4, step: 1, v: [3]},
  {g: "amp", m: "synth", k: "glide_ms", label: "Glide", unit: "ms", min: 0, max: 2000, step: 1, v: [3]},
  {g: "filter", m: "filter", k: "cutoff_hz", label: "Cutoff", unit: "Hz · log", min: 40, max: 16000, step: 1, log: true, v: [2, 3]},
  {g: "filter", m: "filter", k: "resonance", label: "Resonance", unit: "0–1", min: 0, max: 1, step: .01, v: [3]},
  {g: "filter", m: "filter", k: "env_octaves", label: "Envelope amount", unit: "octaves ±6", min: -6, max: 6, step: .1, v: [3]},
  {g: "filter", m: "filter", k: "attack_ms", label: "Filter attack", unit: "ms", min: 1, max: 2000, step: 1, v: [3]},
  {g: "filter", m: "filter", k: "decay_ms", label: "Filter decay", unit: "ms", min: 1, max: 2000, step: 1, v: [3]},
  {g: "filter", m: "filter", k: "sustain", label: "Filter sustain", unit: "0–1", min: 0, max: 1, step: .01, v: [3]},
  {g: "filter", m: "filter", k: "release_ms", label: "Filter release", unit: "ms", min: 5, max: 5000, step: 1, v: [3]},
  {g: "lfo", m: "lfo", k: "waveform", label: "LFO shape", type: "select", options: LFO_WAVES, v: [3]},
  {g: "lfo", m: "lfo", k: "rate_hz", label: "LFO rate", unit: "Hz · log", min: .05, max: 20, step: .01, log: true, v: [3]},
  {g: "lfo", m: "lfo", k: "pitch_cents", label: "Vibrato depth", unit: "cents", min: 0, max: 200, step: 1, v: [3]},
  {g: "lfo", m: "lfo", k: "filter_octaves", label: "Filter sweep", unit: "octaves", min: 0, max: 4, step: .1, v: [3]},
  {g: "lfo", m: "lfo", k: "amp_depth", label: "Tremolo depth", unit: "0–1", min: 0, max: 1, step: .01, v: [3]},
  {g: "lfo", m: "lfo", k: "mod_wheel", label: "Mod wheel (CC1) controls LFO depth", type: "check", v: [3]},
  {g: "delay", m: "delay", k: "mix", label: "Wet / dry", unit: "0–1", min: 0, max: 1, step: .01, v: [1, 2, 3]},
  {g: "delay", m: "delay", k: "time_ms", label: "Delay time", unit: "ms", min: 10, max: 1000, step: 1, v: [1, 2, 3]},
  {g: "delay", m: "delay", k: "feedback", label: "Feedback", unit: "0–0.85", min: 0, max: .85, step: .01, v: [1, 2, 3]},
  {g: "delay", m: "delay", k: "bypass", label: "Bypass wet signal", type: "check", v: [1, 2, 3]},
  {g: "reverb", m: "reverb", k: "mix", label: "Reverb mix", unit: "0–1", min: 0, max: 1, step: .01, v: [3]},
  {g: "reverb", m: "reverb", k: "size", label: "Size / decay", unit: "0–1", min: 0, max: 1, step: .01, v: [3]},
  {g: "reverb", m: "reverb", k: "damping", label: "Damping", unit: "0–1", min: 0, max: 1, step: .01, v: [3]},
  {g: "output", m: "output", k: "level", label: "Output level", unit: "0–1", min: 0, max: 1, step: .01, v: [1, 2, 3]},
];
for (const c of CONTROLS) c.id = `${c.m}-${c.k}`;
// Where a control lives in a patch of the given version (null: not in that format).
function path(c, version) {
  if (!c.v.includes(version)) return null;
  if (version === 1) return ["parameters", c.k];
  if (version === 2 && c.m === "filter") return ["modules", "synth", c.k];
  return ["modules", c.m, c.k];
}
function read(c) { const p = path(c, patch.version); return p ? p.reduce((node, key) => node[key], patch) : undefined; }
function write(c, value) { const p = path(c, patch.version); if (p) p.slice(0, -1).reduce((node, key) => node[key], patch)[p.at(-1)] = value; }
function toSlider(c, value) {
  if (value === null || value === undefined || value === "") return c.log ? 500 : c.min;
  return c.log ? Math.round(1000 * Math.log(value / c.min) / Math.log(c.max / c.min)) : value;
}
function fromSlider(c, raw) {
  if (!c.log) return Number(raw);
  const value = c.min * (c.max / c.min) ** (Number(raw) / 1000);
  return c.step >= 1 ? Math.round(value) : Math.round(value * 100) / 100;
}
let token = "", patch = null, presets = [], busy = false;
function notice(message, error = false) { $("notice").textContent = message; $("notice").classList.toggle("error", error); }
async function api(path, body) {
  const response = await fetch(`/api/${path}`, {method: "POST", headers: {"Content-Type": "application/json", "X-Forge-Token": token}, body: JSON.stringify(body)});
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || "Request failed");
  return result;
}
function updateButtons() {
  document.querySelectorAll("button").forEach(button => { button.disabled = busy; });
  for (const id of ["download", "send"]) $(id).disabled = busy || !patch;
  $("upgrade").disabled = busy || !patch || patch.version === 3;
  $("editor").disabled = busy || !patch;
  $("routing").disabled = !patch || patch.version === 1;
  for (const c of CONTROLS) {
    const off = !patch || !path(c, patch.version);
    $(c.id).disabled = off; if ($(`${c.id}-range`)) $(`${c.id}-range`).disabled = off;
  }
  document.querySelectorAll("#slots button").forEach(button => { button.disabled = busy; });
  for (const [group] of GROUPS) $(`group-${group}`).classList.toggle("unavailable", !!patch && !CONTROLS.some(c => c.g === group && path(c, patch.version)));
  for (const id of ["kind", "provider", "model", "api-key", "prompt", "preset", "import", "input-port", "output-port"]) $(id).disabled = busy;
}
async function run(message, action) {
  if (busy) return;
  busy = true; updateButtons(); notice(message);
  try { await action(); } catch (error) { notice(error.message || "Operation failed", true); }
  finally { busy = false; updateButtons(); }
}
function showJSON() { $("json").textContent = JSON.stringify(patch, null, 2); }
function loadPatch(value) {
  patch = structuredClone(value); $("patch-name").value = patch.name;
  const routes = ROUTES[patch.version] || [];
  $("routing").replaceChildren(...routes.map(([value, label]) => new Option(label, value)));
  if (routes.length) $("routing").value = patch.routing;
  for (const c of CONTROLS) {
    const value = read(c);
    if (c.type === "check") $(c.id).checked = !!value;
    else if (c.type === "select") $(c.id).value = value ?? c.options[0];
    else { $(c.id).value = value ?? ""; $(`${c.id}-range`).value = toSlider(c, value); }
  }
  $("version-note").textContent = {1: "v1 delay preset: external audio through the delay. Convert to v3 to add synth, filter, LFO and reverb.",
    2: "v2 instrument preset (firmware 0.3 format). Convert to v3 for the second oscillator, resonant filter, LFO and reverb.",
    3: "v3 instrument: all installed modules available (firmware 0.4)."}[patch.version];
  showJSON(); updateButtons();
}
function build() {
  for (const [group, legend] of GROUPS) {
    const set = document.createElement("fieldset"); set.id = `group-${group}`; set.className = "group-set";
    const title = document.createElement("legend"); title.className = "group"; title.textContent = legend;
    set.append(title); $("groups").append(set);
  }
  for (const c of CONTROLS) {
    const holder = $(`group-${c.g}`);
    if (c.type === "check") {
      const label = document.createElement("label"); label.className = "check";
      const box = document.createElement("input"); box.type = "checkbox"; box.id = c.id;
      label.append(box, " " + c.label); holder.append(label);
      box.addEventListener("change", () => { if (patch) { write(c, box.checked); showJSON(); } });
      continue;
    }
    const label = document.createElement("label"); label.htmlFor = c.id; label.textContent = c.label;
    if (c.type === "select") {
      const select = document.createElement("select"); select.id = c.id;
      for (const option of c.options) select.add(new Option(option.replace("_", " & "), option));
      holder.append(label, select);
      select.addEventListener("change", () => { if (patch) { write(c, select.value); showJSON(); } });
      continue;
    }
    const row = document.createElement("div"); row.className = "control";
    const unit = document.createElement("span"); unit.className = "unit"; unit.textContent = c.unit; label.append(unit);
    const range = document.createElement("input"); range.type = "range"; range.id = `${c.id}-range`;
    [range.min, range.max, range.step] = c.log ? [0, 1000, 1] : [c.min, c.max, c.step];
    range.setAttribute("aria-label", `${c.label} slider`);
    const number = document.createElement("input"); number.type = "number"; number.id = c.id;
    number.min = c.min; number.max = c.max; number.step = "any"; number.required = true;
    row.append(label, range, number); holder.append(row);
    number.addEventListener("input", () => {
      if (!patch) return;
      write(c, number.value === "" ? null : Number(number.value)); range.value = toSlider(c, read(c)); showJSON();
    });
    range.addEventListener("input", () => { number.value = fromSlider(c, range.value); number.dispatchEvent(new Event("input")); });
  }
}
build();
$("patch-name").addEventListener("input", () => { if (patch) { patch.name = $("patch-name").value; showJSON(); } });
$("routing").addEventListener("change", () => { if (patch && patch.version !== 1) { patch.routing = $("routing").value; showJSON(); } });
$("upgrade").addEventListener("click", () => run("Converting to a v3 instrument…", async () => {
  const result = await api("upgrade", {patch}); loadPatch(result.patch); $("preset").value = "";
  notice("Converted to v3. New modules start neutral; the v3 filter is steeper, so tone may differ slightly. Nothing sent to CHOMPI.");
}));
$("clear-key").addEventListener("click", () => { $("api-key").value = ""; notice("API key cleared from the form."); });
$("provider").addEventListener("change", () => { $("api-key").value = ""; $("model").value = ""; notice("Provider changed. Enter its model ID and API key."); });
window.addEventListener("pagehide", () => { $("api-key").value = ""; });
$("generate-form").addEventListener("submit", event => {
  event.preventDefault();
  run("Generating with your selected provider…", async () => {
    const result = await api("generate", {kind: $("kind").value, provider: $("provider").value, api_key: $("api-key").value.trim(), model: $("model").value.trim(), prompt: $("prompt").value});
    loadPatch(result.patch); $("preset").value = ""; notice("Patch generated and validated. Review it before sending to CHOMPI.");
  });
});
$("preset").addEventListener("change", () => { if ($("preset").value !== "") { loadPatch(presets[Number($("preset").value)]); notice("Preset loaded for review. Nothing sent to CHOMPI."); } });
$("download").addEventListener("click", () => run("Validating your preset…", async () => {
  const result = await api("validate", {patch});
  const url = URL.createObjectURL(new Blob([JSON.stringify(result.patch, null, 2) + "\n"], {type: "application/json"}));
  const link = document.createElement("a"); link.href = url; link.download = (patch.name.replace(/[^a-z0-9_-]/gi, "_").slice(0, 60) || "forge-patch") + ".json";
  link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000); notice("Validated preset downloaded. No API key is included.");
}));
$("import").addEventListener("change", () => run("Validating imported preset…", async () => {
  try {
    const file = $("import").files[0]; if (!file) return;
    if (file.size > 65536) throw new Error("Preset exceeds 64 KiB.");
    // Preserve the raw JSON for server parsing so duplicate keys cannot disappear in JSON.parse.
    const raw = await file.text();
    const response = await fetch("/api/validate", {method: "POST", headers: {"Content-Type": "application/json", "X-Forge-Token": token}, body: `{"patch":${raw}}`});
    const result = await response.json(); if (!response.ok) throw new Error(result.error);
    loadPatch(result.patch); $("preset").value = ""; notice("Preset imported and validated. Nothing sent to CHOMPI.");
  } finally { $("import").value = ""; }
}));
$("ports").addEventListener("click", () => run("Looking for MIDI ports…", async () => {
  const result = await api("ports", {});
  for (const [id, names] of [["input-port", result.inputs], ["output-port", result.outputs]]) {
    const select = $(id), previous = select.value; select.replaceChildren(new Option(names.length ? "Select a port" : "No ports found", ""));
    for (const name of names) select.add(new Option(name, name));
    if (names.includes(previous)) select.value = previous;
  }
  notice("Port list refreshed. Select both CHOMPI ports explicitly.");
}));
function ports() { return {input: $("input-port").value, output: $("output-port").value}; }
function deviceStatus(result) { $("device-state").textContent = `Firmware ${result.firmware} · CPU average ${result.cpu_average_percent}% / peak ${result.cpu_max_percent}% · Dropped ${result.dropped} · Rejected ${result.rejected}`; }
for (const id of ["status", "capture"]) $(id).addEventListener("click", () => run("Reading CHOMPI…", async () => {
  const result = await api("status", ports()); deviceStatus(result);
  if (id === "capture") { loadPatch(result.patch); $("preset").value = ""; }
  notice(id === "capture" ? "Device targets captured to the editor. Save JSON to keep them." : "Device status received. Your editor patch is unchanged.");
}));
$("send").addEventListener("click", () => run("Sending patch and waiting for acknowledgement…", async () => {
  const result = await api("send", {...ports(), patch}); deviceStatus(result);
  notice("CHOMPI acknowledged the requested patch. Device values use 14-bit precision.");
}));
$("panic").addEventListener("click", () => run("Silencing CHOMPI…", async () => {
  const result = await api("panic", ports()); deviceStatus(result); notice("Panic acknowledged. Voices, delay and reverb tails stopped; retrigger notes to play.");
}));
// Device presets (SD card): bank 1-8, slot 1-15, as on the panel.
let occupied = {}, selectedSlot = 0, eraseArmed = null;   // eraseArmed: {key, at} after the first Erase click
for (let bank = 1; bank <= 8; ++bank) $("bank").add(new Option(`Bank ${bank}`, String(bank)));
function drawSlots() {
  const bank = Number($("bank").value), filled = occupied[bank] || [];
  $("slots").replaceChildren(...Array.from({length: 15}, (_, i) => {
    const slot = i + 1, button = document.createElement("button");
    button.type = "button"; button.id = `slot-${slot}`; button.textContent = String(slot);
    button.classList.toggle("filled", filled.includes(slot));
    button.setAttribute("aria-pressed", String(slot === selectedSlot));
    button.setAttribute("aria-label", `Slot ${slot}${filled.includes(slot) ? ", stored" : ", empty"}`);
    button.disabled = busy;
    button.addEventListener("click", () => { selectedSlot = slot; eraseArmed = null; drawSlots(); });
    return button;
  }));
}
$("bank").addEventListener("change", () => { selectedSlot = 0; eraseArmed = null; drawSlots(); });
function slotTarget() {
  if (!selectedSlot) throw new Error("Choose a slot first.");
  return {...ports(), bank: Number($("bank").value), slot: selectedSlot};
}
async function readSlots() { occupied = (await api("preset", {...ports(), action: "list"})).occupied; drawSlots(); }
$("slots-read").addEventListener("click", () => run("Reading device presets…", async () => {
  await readSlots(); notice("Device preset slots read from the SD card.");
}));
$("slot-recall").addEventListener("click", () => run("Recalling device preset…", async () => {
  const target = slotTarget(), result = await api("preset", {...target, action: "recall"});
  deviceStatus(result); loadPatch({...result.patch, name: `Bank ${target.bank} slot ${target.slot}`}); $("preset").value = "";
  notice(`Bank ${target.bank} slot ${target.slot} is playing on CHOMPI and loaded into the editor.`);
}));
$("slot-store").addEventListener("click", () => run("Storing the device's current sound…", async () => {
  const target = slotTarget(); await api("preset", {...target, action: "store"}); await readSlots();
  notice(`CHOMPI's current sound saved to bank ${target.bank} slot ${target.slot} on the SD card.`);
}));
$("slot-erase").addEventListener("click", () => run("Erasing device preset…", async () => {
  const target = slotTarget(), key = `${target.bank}:${target.slot}`;
  if (!eraseArmed || eraseArmed.key !== key || Date.now() - eraseArmed.at > 4000) {
    eraseArmed = {key, at: Date.now()};
    notice(`Press Erase slot again within 4 seconds to delete bank ${target.bank} slot ${target.slot}.`);
    return;
  }
  eraseArmed = null;
  await api("preset", {...target, action: "erase"}); await readSlots();
  notice(`Bank ${target.bank} slot ${target.slot} erased.`);
}));
async function start() {
  busy = true; updateButtons();
  try {
    const response = await fetch("/api/session"); if (!response.ok) throw new Error("Could not start a local session. Reload the URL printed by Forge.");
    const session = await response.json(); token = session.token; presets = session.presets;
    presets.forEach((item, index) => $("preset").add(new Option(item.name, String(index))));
    if (presets.length) { const initial = Math.max(0, presets.findIndex(item => item.version === 3)); loadPatch(presets[initial]); $("preset").value = String(initial); }
    drawSlots();
    notice("Ready. Start with AI, a preset, or an imported patch.");
  } catch (error) { notice(error.message, true); }
  finally { busy = false; updateButtons(); }
}
start();
