"use strict";
const $ = id => document.getElementById(id);
const WAVES = ["sine", "triangle", "saw", "square"], LFO_WAVES = ["sine", "triangle", "square", "sample_hold"];
const SAMPLE_MODES = ["chromatic", "kit"], SAMPLE_BANKS = ["a", "b", "c", "d", "e"];
const ROUTES = {2: [["synth>delay>output", "Synth → Delay → Output"], ["aux>delay>output", "Aux input → Delay → Output"]],
  3: [["synth>delay>reverb>output", "Synth → Delay → Reverb → Output"], ["aux>delay>reverb>output", "Aux input → Delay → Reverb → Output"]],
  4: [["synth>delay>reverb>output", "Synth → Delay → Reverb → Output"], ["sampler>delay>reverb>output", "Sampler → Delay → Reverb → Output"],
      ["aux>delay>reverb>output", "Aux input → Delay → Reverb → Output"]]};
ROUTES[5] = ROUTES[6] = ROUTES[7] = ROUTES[8] = ROUTES[4];
// v5: what panel knobs 1-4 control on their first page (forge_host.KNOB_TARGETS).
const KNOB_TARGETS = ["delay.mix", "delay.time_ms", "delay.feedback", "output.level", "filter.cutoff_hz", "filter.resonance",
  "reverb.mix", "sampler.pitch_semitones", "sampler.start", "sampler.end", "filter.env_octaves", "synth.attack_ms",
  "synth.decay_ms", "synth.sustain", "synth.release_ms", "lfo.rate_hz", "lfo.pitch_cents", "lfo.filter_octaves",
  "lfo.amp_depth", "synth.osc2_level", "synth.osc2_detune_cents", "synth.noise", "synth.glide_ms", "reverb.size",
  "reverb.damping", "sampler.crossfade_ms"];
const KNOB_SWITCHES = ["SW4", "SW1", "SW2", "SW3"];
// One row per editor control. m/k = v3 module/key; id = `${m}-${k}`. type: number (default), select, check.
// v = patch versions that have the field. log = slider runs 0–1000 on the firmware's log curve.
const GROUPS = [
  ["source", "Source · oscillators"], ["sampler", "Sampler · TAPE sample slots"], ["amp", "Amplitude · voices"], ["filter", "Filter"], ["lfo", "LFO · mod wheel"],
  ["delay", "Stereo delay"], ["reverb", "Reverb"], ["output", "Output"], ["knobs", "Panel knobs · page 1 (CC 20–23)"], ["harmony", "Harmony · one key plays a chord (v6)"],
  ["parts", "Arp · bass · tempo (v7)"], ["fx", "FX section · four modules in your order (v8)"]];
const CONTROLS = [
  {g: "source", m: "synth", k: "waveform", label: "Oscillator", type: "select", options: WAVES, v: [2, 3, 4, 5, 6, 7, 8]},
  {g: "source", m: "synth", k: "osc2_waveform", label: "Oscillator 2", type: "select", options: WAVES, v: [3, 4, 5, 6, 7, 8]},
  {g: "source", m: "synth", k: "osc2_level", label: "Oscillator 2 level", unit: "0–1", min: 0, max: 1, step: .01, v: [3, 4, 5, 6, 7, 8]},
  {g: "source", m: "synth", k: "osc2_semitones", label: "Oscillator 2 interval", unit: "semitones", min: -24, max: 24, step: 1, v: [3, 4, 5, 6, 7, 8]},
  {g: "source", m: "synth", k: "osc2_detune_cents", label: "Oscillator 2 detune", unit: "cents", min: -50, max: 50, step: 1, v: [3, 4, 5, 6, 7, 8]},
  {g: "source", m: "synth", k: "noise", label: "Noise", unit: "0–1", min: 0, max: 1, step: .01, v: [3, 4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "mode", label: "Mode (chromatic: one sample on all keys · kit: one per white key)", type: "select", options: SAMPLE_MODES, v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "bank", label: "Bank", type: "select", options: SAMPLE_BANKS, v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "slot", label: "Slot (chromatic)", unit: "1–14 · 15 = recording", min: 1, max: 15, step: 1, v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "pitch_semitones", label: "Pitch", unit: "semitones", min: -24, max: 24, step: .1, v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "start", label: "Start", unit: "0–1 of length", min: 0, max: 1, step: .001, v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "end", label: "End", unit: "0–1 of length", min: 0, max: 1, step: .001, v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "crossfade_ms", label: "Loop crossfade", unit: "ms", min: 0, max: 250, step: 1, v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "loop", label: "Loop start → end", type: "check", v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "hold", label: "Sound while held (off: one-shot trigger)", type: "check", v: [4, 5, 6, 7, 8]},
  {g: "sampler", m: "sampler", k: "reverse", label: "Reverse", type: "check", v: [4, 5, 6, 7, 8]},
  {g: "amp", m: "synth", k: "attack_ms", label: "Attack", unit: "ms", min: 1, max: 2000, step: 1, v: [2, 3, 4, 5, 6, 7, 8]},
  {g: "amp", m: "synth", k: "decay_ms", label: "Decay", unit: "ms", min: 1, max: 2000, step: 1, v: [2, 3, 4, 5, 6, 7, 8]},
  {g: "amp", m: "synth", k: "sustain", label: "Sustain", unit: "0–1", min: 0, max: 1, step: .01, v: [2, 3, 4, 5, 6, 7, 8]},
  {g: "amp", m: "synth", k: "release_ms", label: "Release", unit: "ms", min: 5, max: 5000, step: 1, v: [2, 3, 4, 5, 6, 7, 8]},
  {g: "amp", m: "synth", k: "voices", label: "Voices", unit: "1 = mono · v3 max 4, v4 max 7", min: 1, max: 7, step: 1, v: [3, 4, 5, 6, 7, 8]},
  {g: "amp", m: "synth", k: "glide_ms", label: "Glide", unit: "ms", min: 0, max: 2000, step: 1, v: [3, 4, 5, 6, 7, 8]},
  {g: "filter", m: "filter", k: "cutoff_hz", label: "Cutoff", unit: "Hz · log", min: 40, max: 16000, step: 1, log: true, v: [2, 3, 4, 5, 6, 7, 8]},
  {g: "filter", m: "filter", k: "resonance", label: "Resonance", unit: "0–1", min: 0, max: 1, step: .01, v: [3, 4, 5, 6, 7, 8]},
  {g: "filter", m: "filter", k: "env_octaves", label: "Envelope amount", unit: "octaves ±6", min: -6, max: 6, step: .1, v: [3, 4, 5, 6, 7, 8]},
  {g: "filter", m: "filter", k: "attack_ms", label: "Filter attack", unit: "ms", min: 1, max: 2000, step: 1, v: [3, 4, 5, 6, 7, 8]},
  {g: "filter", m: "filter", k: "decay_ms", label: "Filter decay", unit: "ms", min: 1, max: 2000, step: 1, v: [3, 4, 5, 6, 7, 8]},
  {g: "filter", m: "filter", k: "sustain", label: "Filter sustain", unit: "0–1", min: 0, max: 1, step: .01, v: [3, 4, 5, 6, 7, 8]},
  {g: "filter", m: "filter", k: "release_ms", label: "Filter release", unit: "ms", min: 5, max: 5000, step: 1, v: [3, 4, 5, 6, 7, 8]},
  {g: "lfo", m: "lfo", k: "waveform", label: "LFO shape", type: "select", options: LFO_WAVES, v: [3, 4, 5, 6, 7, 8]},
  {g: "lfo", m: "lfo", k: "rate_hz", label: "LFO rate", unit: "Hz · log", min: .05, max: 20, step: .01, log: true, v: [3, 4, 5, 6, 7, 8]},
  {g: "lfo", m: "lfo", k: "pitch_cents", label: "Vibrato depth", unit: "cents", min: 0, max: 200, step: 1, v: [3, 4, 5, 6, 7, 8]},
  {g: "lfo", m: "lfo", k: "filter_octaves", label: "Filter sweep", unit: "octaves", min: 0, max: 4, step: .1, v: [3, 4, 5, 6, 7, 8]},
  {g: "lfo", m: "lfo", k: "amp_depth", label: "Tremolo depth", unit: "0–1", min: 0, max: 1, step: .01, v: [3, 4, 5, 6, 7, 8]},
  {g: "lfo", m: "lfo", k: "mod_wheel", label: "Mod wheel (CC1) controls LFO depth", type: "check", v: [3, 4, 5, 6, 7, 8]},
  {g: "delay", m: "delay", k: "mix", label: "Wet / dry", unit: "0–1", min: 0, max: 1, step: .01, v: [1, 2, 3, 4, 5, 6, 7, 8]},
  {g: "delay", m: "delay", k: "time_ms", label: "Delay time", unit: "ms", min: 10, max: 1000, step: 1, v: [1, 2, 3, 4, 5, 6, 7, 8]},
  {g: "delay", m: "delay", k: "feedback", label: "Feedback", unit: "0–0.85", min: 0, max: .85, step: .01, v: [1, 2, 3, 4, 5, 6, 7, 8]},
  {g: "delay", m: "delay", k: "bypass", label: "Bypass wet signal", type: "check", v: [1, 2, 3, 4, 5, 6, 7, 8]},
  {g: "reverb", m: "reverb", k: "mix", label: "Reverb mix", unit: "0–1", min: 0, max: 1, step: .01, v: [3, 4, 5, 6, 7, 8]},
  {g: "reverb", m: "reverb", k: "size", label: "Size / decay", unit: "0–1", min: 0, max: 1, step: .01, v: [3, 4, 5, 6, 7, 8]},
  {g: "reverb", m: "reverb", k: "damping", label: "Damping", unit: "0–1", min: 0, max: 1, step: .01, v: [3, 4, 5, 6, 7, 8]},
  {g: "output", m: "output", k: "level", label: "Output level", unit: "0–1", min: 0, max: 1, step: .01, v: [1, 2, 3, 4, 5, 6, 7, 8]},
];
const KNOB_LABELS = {default: "Default (mix · time · feedback · level; sampler: pitch · start · end · mix)"};
for (const t of KNOB_TARGETS) {
  const [m, k] = t.split("."), c = CONTROLS.find(c => c.m === m && c.k === k);
  KNOB_LABELS[t] = `${GROUPS.find(([g]) => g === c.g)[1].split(" · ")[0]}: ${c.label}`;
}
for (let n = 0; n < 4; n++)
  CONTROLS.push({g: "knobs", m: "knobs", k: n, label: `Knob ${n + 1} (${KNOB_SWITCHES[n]})`, type: "select",
                 options: ["default", ...KNOB_TARGETS], labels: KNOB_LABELS, v: [5, 6, 7, 8]});
// v6 (firmware 0.13): harmony mode (core/harmony.h; MANUAL section 8a).
CONTROLS.push(
  {g: "harmony", m: "harmony", k: "enabled", label: "Harmony on (keys play chords)", type: "check", v: [6, 7, 8]},
  {g: "harmony", m: "harmony", k: "tonic", label: "Key (tonic)", type: "select", options: ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"], v: [6, 7, 8]},
  {g: "harmony", m: "harmony", k: "mode", label: "Mode", type: "select", options: ["major", "natural_minor", "harmonic_minor", "melodic_minor", "dorian", "phrygian", "lydian", "mixolydian", "locrian"], v: [6, 7, 8]},
  {g: "harmony", m: "harmony", k: "extension", label: "Chord size", type: "select", options: ["fifth", "triad", "7th", "9th", "11th", "13th"], v: [6, 7, 8]},
  {g: "harmony", m: "harmony", k: "layout", label: "Layout (static: white keys are degrees · real: the key is the root)", type: "select", options: ["static", "real"], v: [6, 7, 8]},
  {g: "harmony", m: "harmony", k: "inversion", label: "Inversion (without voice leading)", unit: "0–3", min: 0, max: 3, step: 1, v: [6, 7, 8]},
  {g: "harmony", m: "harmony", k: "voice_leading", label: "Voice leading (each chord moves least from the last)", type: "check", v: [6, 7, 8]},
  {g: "harmony", m: "harmony", k: "open", label: "Open spread", type: "check", v: [6, 7, 8]});
// v7 (firmware 0.14): the clock, arpeggiator and bass (core/parts.h; MANUAL section 8b).
CONTROLS.push(
  {g: "parts", m: "parts", s: "arp", k: "pattern", label: "Arp (off: keys play as usual)", type: "select", options: ["off", "up", "down", "updown", "order", "random"], v: [7, 8]},
  {g: "parts", m: "parts", s: "arp", k: "rate", label: "Arp rate (t = triplet)", type: "select", options: ["1/4", "1/8", "1/8t", "1/16", "1/16t", "1/32"], v: [7, 8]},
  {g: "parts", m: "parts", s: "arp", k: "octaves", label: "Arp octaves", unit: "1–4", min: 1, max: 4, step: 1, v: [7, 8]},
  {g: "parts", m: "parts", s: "arp", k: "gate", label: "Arp gate (each note's length)", unit: "%", min: 5, max: 100, step: 5, v: [7, 8]},
  {g: "parts", m: "parts", s: "arp", k: "latch", label: "Latch (keeps playing after the keys are released)", type: "check", v: [7, 8]},
  {g: "parts", m: "parts", s: "bass", k: "mode", label: "Bass (MIDI channel + 1)", type: "select", options: ["off", "root", "fifth", "alternate", "octave"], v: [7, 8]},
  {g: "parts", m: "parts", s: "bass", k: "rate", label: "Bass rate (chord: once per chord change)", type: "select", options: ["chord", "1/2", "1/4", "1/8"], v: [7, 8]},
  {g: "parts", m: "parts", s: "bass", k: "octave", label: "Bass octave (C1–C3)", unit: "1–3", min: 1, max: 3, step: 1, v: [7, 8]},
  {g: "parts", m: "parts", s: "clock", k: "bpm", label: "Tempo (MIDI clock takes over while it arrives)", unit: "BPM", min: 40, max: 300, step: 1, v: [7, 8]},
  {g: "parts", m: "parts", s: "clock", k: "seed", label: "Random pattern number", unit: "0–2047", min: 0, max: 2047, step: 1, v: [7, 8]},
  {g: "parts", m: "parts", s: "clock", k: "send_clock", label: "Send MIDI clock", type: "check", v: [7, 8]});
// v8 (firmware 0.20): the FX section (core/fx.h; MANUAL section 8e). Each module processes the one before's sound.
const FX_CLASSIC = ["colour", "motion", "texture", "space"];
const FX_ORDERS = (function permute(left) {
  return left.length ? left.flatMap((m, i) => permute([...left.slice(0, i), ...left.slice(i + 1)]).map(rest => [m, ...rest])) : [[]];
})(FX_CLASSIC).map(o => o.join(","));
const FX_ORDER_NAMES = {"colour,motion,texture,space": "Classic", "colour,motion,space,texture": "Wear last", "colour,texture,space,motion": "Wash first"};
const FX_ORDER_LABELS = Object.fromEntries(FX_ORDERS.map(o => [o, o.split(",").join(" → ") + (FX_ORDER_NAMES[o] ? ` (${FX_ORDER_NAMES[o]})` : "")]));
CONTROLS.push(
  {g: "fx", m: "fx", k: "order", label: "Order (first to last)", type: "select", options: FX_ORDERS, labels: FX_ORDER_LABELS, v: [8]},
  {g: "fx", m: "fx", k: "side_by_side", label: "Side by side (the reverb hears the sound before the delay)", type: "check", v: [8]},
  {g: "fx", m: "fx", k: "space", label: "Space (its amount: Delay wet / dry)", type: "select", options: ["echo_room", "cloud", "tape_echo"], labels: {echo_room: "echo + room", cloud: "cloud", tape_echo: "tape echo"}, v: [8]},
  {g: "fx", m: "fx", k: "colour", label: "Colour", type: "select", options: ["saturation", "fold", "crush"], v: [8]},
  {g: "fx", m: "fx", k: "colour_amount", label: "Colour amount (0 = off)", unit: "0–1", min: 0, max: 1, step: .01, v: [8]},
  {g: "fx", m: "fx", k: "colour_character", label: "Colour character (fold: bias · crush: lower rate)", unit: "0–1", min: 0, max: 1, step: .01, v: [8]},
  {g: "fx", m: "fx", k: "texture", label: "Texture", type: "select", options: ["cassette", "broken", "squash"], v: [8]},
  {g: "fx", m: "fx", k: "texture_amount", label: "Texture amount (0 = off)", unit: "0–1", min: 0, max: 1, step: .01, v: [8]},
  {g: "fx", m: "fx", k: "texture_character", label: "Texture character (cassette: hiss · broken: longer dropouts · squash: slower release)", unit: "0–1", min: 0, max: 1, step: .01, v: [8]},
  {g: "fx", m: "fx", k: "motion", label: "Motion", type: "select", options: ["chorus", "phaser", "tremolo"], v: [8]},
  {g: "fx", m: "fx", k: "motion_amount", label: "Motion amount (0 = off)", unit: "0–1", min: 0, max: 1, step: .01, v: [8]},
  {g: "fx", m: "fx", k: "motion_rate", label: "Motion speed", unit: "0–1", min: 0, max: 1, step: .01, v: [8]});
for (const c of CONTROLS) c.id = c.s ? `${c.m}-${c.s}-${c.k}` : `${c.m}-${c.k}`;
// Where a control lives in a patch of the given version (null: not in that format).
function path(c, version) {
  if (!c.v.includes(version)) return null;
  if (version === 1) return ["parameters", c.k];
  if (c.m === "knobs") return ["knobs", c.k];
  if (c.m === "harmony") return ["harmony", c.k];
  if (c.m === "parts") return ["parts", c.s, c.k];
  if (c.m === "fx") return ["fx", c.k];
  if (version === 2 && c.m === "filter") return ["modules", "synth", c.k];
  return ["modules", c.m, c.k];
}
// The FX order is a list in the patch and one choice in the editor ("colour,motion,texture,space").
function read(c) { const p = path(c, patch.version), v = p ? p.reduce((node, key) => node[key], patch) : undefined; return c.k === "order" && v ? v.join(",") : v; }
function write(c, value) { const p = path(c, patch.version); if (p) p.slice(0, -1).reduce((node, key) => node[key], patch)[p.at(-1)] = c.k === "order" ? value.split(",") : value; }
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
  $("download").disabled = busy || !patch;
  $("editor").disabled = busy || !patch;
  $("routing").disabled = !patch || patch.version === 1;
  for (const c of CONTROLS) {
    const off = !patch || !path(c, patch.version);
    $(c.id).disabled = off; if ($(`${c.id}-range`)) $(`${c.id}-range`).disabled = off;
  }
  for (const [group] of GROUPS) $(`group-${group}`).classList.toggle("unavailable", !!patch && !CONTROLS.some(c => c.g === group && path(c, patch.version)));
  for (const id of ["kind", "prompt", "reply", "use-reply", "preset", "import"]) $(id).disabled = busy;
}
async function run(message, action) {
  if (busy) return;
  busy = true; updateButtons(); notice(message);
  try { await action(); } catch (error) { notice(error.message || "Operation failed", true); }
  finally { busy = false; updateButtons(); }
}
function showJSON() {
  $("json").textContent = JSON.stringify(patch, null, 2);
  try { localStorage.setItem("forge.lastSound", JSON.stringify(patch)); } catch { /* storage blocked: the send page falls back to Open JSON */ }
}
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
  showJSON(); updateButtons();
}
function build() {
  for (const [group, legend] of GROUPS) {
    const set = document.createElement("details"); set.id = `group-${group}`; set.className = "group-set";
    const title = document.createElement("summary"); title.className = "group"; title.textContent = legend;
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
      for (const option of c.options) select.add(new Option(c.labels ? c.labels[option] : option.replace("_", " & "), option));
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
// Every sound is edited as the newest version, v8 since 0.20 (the owner, 2026-10-12: one version in the
// editor): older presets, files and replies convert as they open (the FX section at today's sound).
async function adopt(value) {
  if (value.version === 8) { loadPatch(value); return false; }
  loadPatch((await api("upgrade", {patch: value, to: 8})).patch); return true;
}
$("fold-all").addEventListener("click", () => {
  const sets = [...document.querySelectorAll("#groups details")], open = !sets.every(d => d.open);
  sets.forEach(d => { d.open = open; }); $("fold-all").textContent = open ? "close all" : "open all";
});
// No key, no hardware (the owner, 2026-10-12): the prompt goes to any chatbot and its reply comes back here.
function chatRequest(extra) { return {kind: $("kind").value, ...extra}; }
$("use-reply").addEventListener("click", () => run("Checking the chatbot's reply…", async () => {
  const result = await api("chat-reply", chatRequest({reply: $("reply").value}));
  await adopt(result.patch); $("preset").value = ""; notice("Reply checked: a valid patch. Poke at it, then Save JSON.");
}));
$("generate-form").addEventListener("submit", event => {
  event.preventDefault();
  run("Building the prompt…", async () => {
    const text = (await api("chat-prompt", chatRequest({prompt: $("prompt").value}))).prompt;
    const box = $("chat-prompt-text"); box.value = text;
    try { await navigator.clipboard.writeText(text); box.hidden = true; notice("Prompt copied. Paste it into any chatbot, then paste its reply below."); }
    catch { box.hidden = false; box.select(); notice("Copy the prompt from the box below (Ctrl+C), paste it into any chatbot, then paste its reply."); }
  });
});
$("preset").addEventListener("change", () => { if ($("preset").value !== "") run("Opening the preset…", async () => { await adopt(presets[Number($("preset").value)]); notice("Preset opened. Poke at it, then Save JSON."); }); });
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
    await adopt(result.patch); $("preset").value = ""; notice("File opened and checked.");
  } finally { $("import").value = ""; }
}));
async function start() {
  busy = true; updateButtons();
  try {
    const response = await fetch("/api/session"); if (!response.ok) throw new Error("Could not start a local session. Reload the URL printed by Forge.");
    const session = await response.json(); token = session.token; presets = session.presets;
    presets.forEach((item, index) => $("preset").add(new Option(item.name, String(index))));
    if (presets.length) { const initial = Math.max(0, presets.findIndex(item => item.version === 8)); await adopt(presets[initial]); $("preset").value = String(initial); }
    const skipped = session.preset_errors || [];
    notice(skipped.length ? `Ready. Skipped damaged preset file(s): ${skipped.join("; ")}` : "Forge is lit. Describe a sound, pick a preset, or open a JSON file.", skipped.length > 0);
  } catch (error) { notice(error.message, true); }
  finally { busy = false; updateButtons(); }
}
start();
