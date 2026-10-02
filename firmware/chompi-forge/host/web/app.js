"use strict";
const $ = id => document.getElementById(id);
const fields = {mix: [0, 1, .01, "Wet / dry", "0–1"], time_ms: [10, 1000, 1, "Delay time", "ms"],
  feedback: [0, .85, .01, "Feedback", "0–0.85"], level: [0, 1, .01, "Output level", "0–1"]};
const synthFields = {attack_ms: [1,2000,1,"Attack","ms"], decay_ms: [1,2000,1,"Decay","ms"],
  sustain: [0,1,.01,"Sustain","0–1"], release_ms: [5,5000,1,"Release","ms"], cutoff_hz: [40,16000,1,"Tone cutoff","Hz · log"]};
// Sliders for logarithmic controls run 0–1000 and map to the same curve the firmware uses.
const logFields = new Set(["cutoff_hz"]);
function toSlider(key, value) {
  const [min, max] = synthFields[key] || fields[key];
  if (value === null || value === undefined || value === "") return logFields.has(key) ? 500 : min;
  return logFields.has(key) ? Math.round(1000 * Math.log(value / min) / Math.log(max / min)) : value;
}
function fromSlider(key, raw) {
  const [min, max] = synthFields[key] || fields[key];
  return logFields.has(key) ? Math.round(min * (max / min) ** (Number(raw) / 1000)) : Number(raw);
}
function params() { return patch.version === 1 ? patch.parameters : {...patch.modules.delay, ...patch.modules.output, ...patch.modules.synth}; }
function setParam(key, value) {
  if (patch.version === 1) patch.parameters[key] = value;
  else if (key === "level") patch.modules.output[key] = value;
  else if (key in synthFields || key === "waveform") patch.modules.synth[key] = value;
  else patch.modules.delay[key] = value;
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
  $("editor").disabled = busy || !patch;
  $("synth-editor").disabled = busy || !patch || patch.version !== 2;
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
  const values = params();
  for (const key of Object.keys({...fields, ...synthFields})) {
    $(key).value = values[key] ?? ""; $(`${key}-range`).value = toSlider(key, values[key]);
  }
  $("routing").value = patch.routing || "aux>delay>output";
  $("waveform").value = patch.modules?.synth.waveform || "sine";
  $("bypass").checked = params().bypass; showJSON(); updateButtons();
}
for (const [key, [min, max, step, label, unit]] of Object.entries({...fields, ...synthFields})) {
  const row = document.createElement("div"); row.className = "control";
  // All interpolated values in this template are fixed local constants.
  const [sliderMin, sliderMax, sliderStep] = logFields.has(key) ? [0, 1000, 1] : [min, max, step];
  row.innerHTML = `<label for="${key}">${label}<span class="unit">${unit}</span></label><input id="${key}-range" type="range" min="${sliderMin}" max="${sliderMax}" step="${sliderStep}" aria-label="${label} slider"><input id="${key}" type="number" min="${min}" max="${max}" step="any" required>`;
  $(key in synthFields ? "synth-controls" : "controls").append(row);
  $(key).addEventListener("input", () => { if (!patch) return; setParam(key, $(key).value === "" ? null : Number($(key).value)); $(`${key}-range`).value = toSlider(key, params()[key]); showJSON(); });
  $(`${key}-range`).addEventListener("input", () => { $(key).value = fromSlider(key, $(`${key}-range`).value); $(key).dispatchEvent(new Event("input")); });
}
$("patch-name").addEventListener("input", () => { if (patch) { patch.name = $("patch-name").value; showJSON(); } });
$("bypass").addEventListener("change", () => { if (patch) { setParam("bypass", $("bypass").checked); showJSON(); } });
$("routing").addEventListener("change", () => { if (patch?.version === 2) { patch.routing = $("routing").value; showJSON(); } });
$("waveform").addEventListener("change", () => { if (patch?.version === 2) { setParam("waveform", $("waveform").value); showJSON(); } });
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
  const result = await api("panic", ports()); deviceStatus(result); notice("Panic acknowledged. Voices and delay tail stopped; retrigger notes to play.");
}));
async function start() {
  busy = true; updateButtons();
  try {
    const response = await fetch("/api/session"); if (!response.ok) throw new Error("Could not start a local session. Reload the URL printed by Forge.");
    const session = await response.json(); token = session.token; presets = session.presets;
    presets.forEach((item, index) => $("preset").add(new Option(item.name, String(index))));
    if (presets.length) { const initial = Math.max(0, presets.findIndex(item => item.version === 2)); loadPatch(presets[initial]); $("preset").value = String(initial); }
    notice("Ready. Start with AI, a preset, or an imported patch.");
  } catch (error) { notice(error.message, true); }
  finally { busy = false; updateButtons(); }
}
start();
