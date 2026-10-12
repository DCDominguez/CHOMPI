"use strict";
// Choowi's forge, the static workshop for GitHub Pages (0.20: v8 sounds with the FX section). Connect CHOMPI over Web MIDI, describe a
// sound, let any chatbot write it (the prompt is baked in data.js from forge_ai), hear it live on CHOMPI,
// tweak it (controls from editor.js, generated from the local workshop), keep it. forge-wire.js encodes and
// checks everything with forge_host's wire format (tests/test_wire_js.py, tests/test_site.py).
const W = window.ForgeWire, DATA = window.CHOOWI_DATA;
let patch = null, access = null, busy = false, sending = false, pending = false, timer = 0;

function notice(message, error = false) { $("notice").textContent = message; $("notice").classList.toggle("error", error); }
const connected = () => !!(access && $("output").value && $("input").value);
const sequence = () => Math.floor(Math.random() * 16384);

// ---- CHOMPI over Web MIDI -------------------------------------------------------------------------
function exchange(payload, timeout = 2500) {
  const input = access.inputs.get($("input").value), output = access.outputs.get($("output").value);
  if (!input || !output) return Promise.reject(new Error("CHOMPI's MIDI ports went away: plug it in and press Connect."));
  return new Promise((resolve, reject) => {
    const t = setTimeout(() => { input.onmidimessage = null; reject(new Error("CHOMPI didn't answer. Is it on, and running Choowi (not stock TAPE)?")); }, timeout);
    input.onmidimessage = event => {
      const data = Array.from(event.data);
      if (data.length < 10 || data[0] !== 0xF0 || data[1] !== 0x7D || data[6] !== payload[5] || data[7] !== payload[6]) return;
      clearTimeout(t); input.onmidimessage = null;
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
    const select = $(id), list = [...ports.values()], previous = select.value;
    select.replaceChildren(new Option(list.length ? "pick a port" : "no MIDI ports found", ""));
    for (const port of list) select.add(new Option(port.name, port.id));
    const chompi = list.find(port => /chompi/i.test(port.name));
    select.value = list.some(p => p.id === previous) ? previous : chompi ? chompi.id : "";
  }
  $("output").disabled = $("input").disabled = false;
}
function openForge() {
  $("forge").hidden = false; $("connect-card").classList.add("done");
  const on = connected();
  $("audition").hidden = !on; $("device-short").textContent = on ? $("device").textContent : "";
  $("save-device").hidden = !on; if (on) readSlots(); updateButtons();
  if (!patch && DATA.presets.length) loadPatch(DATA.presets[Math.max(0, DATA.presets.findIndex(p => p.name === "Warm Pad"))], false);
}
$("connect").addEventListener("click", async () => {
  try {
    if (!navigator.requestMIDIAccess) throw new Error("This browser has no Web MIDI. Use Chrome or Edge on a computer, or press 'make sounds anyway'.");
    notice("Asking the browser for MIDI…");
    try { access = await navigator.requestMIDIAccess({sysex: true}); }
    catch { throw new Error("MIDI permission was refused. Allow MIDI devices for this page (the icon in the address bar), then Connect again."); }
    access.onstatechange = fillPorts; fillPorts();
    if (!connected()) { notice("Connected to MIDI, but no CHOMPI found. Plug it in, switch it on, or pick its ports, then Connect again.", true); return; }
    $("device").textContent = describe(await exchange(W.statusMessage(sequence())));
    notice("CHOMPI found. Describe a sound below, or pick a preset in tweak.");
    openForge();
  } catch (error) { notice(error.message, true); }
});
$("skip").addEventListener("click", () => { openForge(); notice("No CHOMPI: you can make, tweak and save sounds, and send them later."); });
for (const id of ["output", "input"]) $(id).addEventListener("change", () => { if (!$("forge").hidden) openForge(); });

// Live audition: every change goes to CHOMPI (the latest one wins; one request in flight at a time).
function audition(now = false) {
  if (!patch || !connected() || !$("live").checked) return;
  clearTimeout(timer); timer = setTimeout(sendNow, now ? 0 : 120);
}
async function sendNow() {
  if (sending) { pending = true; return; }
  let payload;
  try { payload = W.encodePatch(patch, sequence()); }
  catch (error) { notice(`Not sent: ${error.message.replace("Not a Choowi sound: ", "")}`, true); return; }
  sending = true;
  try { const s = await exchange(payload); $("device-short").textContent = describe(s); if ($("notice").classList.contains("error")) notice(`CHOMPI is playing ${patch.name}.`); }
  catch (error) { notice(error.message, true); }
  finally { sending = false; if (pending) { pending = false; sendNow(); } }
}
$("live").addEventListener("change", () => audition(true));
$("stop").addEventListener("click", async () => {
  try { await exchange(W.stopMessage(sequence())); notice("Stopped. Play a key to start again."); } catch (error) { notice(error.message, true); }
});
// The on-screen keys: two octaves on MIDI channel 1, C3-C5 to start; octave - / + moves them C1-C7.
const NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
let base = 48;                                              // the lowest key (C3)
function note(n, on) { const out = access && access.outputs.get($("output").value); if (out) out.send(on ? [0x90, n, 100] : [0x80, n, 0]); }
function labelKeys() {
  [...$("keys").children].forEach((key, i) => {
    const n = base + i, name = NAMES[n % 12] + (Math.floor(n / 12) - 1);
    key.textContent = n % 12 === 0 ? name : ""; key.setAttribute("aria-label", `play ${name}`);
  });
  $("oct-label").textContent = `C${base / 12 - 1}–C${base / 12 + 1}`;
  $("oct-down").disabled = base <= 24; $("oct-up").disabled = base >= 84;
}
(function buildKeys() {
  for (let i = 0; i <= 24; i++) {
    const key = document.createElement("button");
    key.type = "button"; key.className = [1, 3, 6, 8, 10].includes(i % 12) ? "key black" : "key";
    const off = () => { if (key.dataset.down) { note(Number(key.dataset.down), false); delete key.dataset.down; } };
    key.addEventListener("pointerdown", e => { e.preventDefault(); off(); key.dataset.down = String(base + i); note(base + i, true); });
    for (const ev of ["pointerup", "pointerleave", "pointercancel"]) key.addEventListener(ev, off);
    $("keys").append(key);
  }
  labelKeys();
})();
$("oct-down").addEventListener("click", () => { base = Math.max(24, base - 12); labelKeys(); });
$("oct-up").addEventListener("click", () => { base = Math.min(84, base + 12); labelKeys(); });

// ---- save to CHOMPI: the sound is sent first, then stored in a preset slot (opcode 4) ------------------
const BANKS = ["red", "orange", "yellow", "green", "teal", "blue", "purple", "pink"];   // presets 1-8, as the bank keys light
let occupied = {};
BANKS.forEach((c, i) => $("save-bank").add(new Option(`${c} (${i + 1})`, String(i + 1))));
function drawSlots() {
  const used = occupied[Number($("save-bank").value)] || [], keep = $("save-slot").value;
  $("save-slot").replaceChildren(...[...Array(15).keys()].map(i => new Option(`${i + 1}${used.includes(i + 1) ? " (used)" : ""}`, String(i + 1))));
  if (keep) $("save-slot").value = keep;
}
$("save-bank").addEventListener("change", drawSlots); drawSlots();
async function readSlots() { try { occupied = (await exchange(W.presetList(sequence()))).occupied; drawSlots(); } catch { /* the list is a nicety */ } }
$("save-to-chompi").addEventListener("click", async () => {
  const bank = Number($("save-bank").value), slot = Number($("save-slot").value), name = `${BANKS[bank - 1]} ${slot}`;
  try {
    if ((occupied[bank] || []).includes(slot) && !confirm(`Bank ${name} already holds a sound. Replace it with ${patch.name}?`)) return;
    clearTimeout(timer); while (sending) await new Promise(r => setTimeout(r, 30));
    notice(`Saving ${patch.name} to bank ${name}…`);
    await exchange(W.encodePatch(patch, sequence()));       // CHOMPI plays exactly this sound
    const done = await exchange(W.presetStore(sequence(), bank, slot), 6000);
    notice(`Saved ${patch.name} to bank ${BANKS[done.bank - 1]}, slot ${done.slot}. It's on CHOMPI's card now.`);
    await readSlots();
  } catch (error) { notice(error.message, true); }
});

// ---- easy: idea -> prompt -> chatbot -> reply ------------------------------------------------------
$("make-prompt").addEventListener("click", async () => {
  const idea = $("idea").value.trim();
  if (!idea) { notice("Write your idea first: a few words are enough.", true); return; }
  const text = DATA.promptPrefix + idea + DATA.promptSuffix, box = $("prompt-text");
  box.value = text;
  try { await navigator.clipboard.writeText(text); box.hidden = true; notice("Prompt copied. Paste it into any chatbot, then paste its whole reply below."); }
  catch { box.hidden = false; box.select(); notice("Copy the prompt from the box (Ctrl+C), paste it into any chatbot, then paste its reply below."); }
});
$("use-reply").addEventListener("click", () => {
  try {
    loadPatch(W.readSound($("reply").value)); $("preset").value = "";
    notice(connected() && $("live").checked ? `CHOMPI is playing ${patch.name}. Tweak it, or save it on the panel.` : `${patch.name} is ready. Tweak it or Save JSON.`);
  } catch (error) { notice(`${error.message}. Paste the chatbot's whole reply; if it keeps failing, paste this message back to the chatbot.`, true); }
});

// ---- tweak: the editor ------------------------------------------------------------------------------
function showJSON() { $("json").textContent = JSON.stringify(patch, null, 2); }
function changed() { showJSON(); audition(); }
function updateButtons() {
  $("download").disabled = !patch; $("editor").disabled = !patch; $("save-to-chompi").disabled = !patch || !connected();
  for (const c of CONTROLS) { const off = !patch || !path(c, 8); $(c.id).disabled = off; if ($(`${c.id}-range`)) $(`${c.id}-range`).disabled = off; }
}
function loadPatch(value, send = true) {
  patch = structuredClone(value); $("patch-name").value = patch.name;
  $("routing").replaceChildren(...ROUTES[8].map(([v, label]) => new Option(label, v))); $("routing").value = patch.routing;
  for (const c of CONTROLS) {
    const v = read(c);
    if (c.type === "check") $(c.id).checked = !!v;
    else if (c.type === "select") $(c.id).value = v ?? c.options[0];
    else { $(c.id).value = v ?? ""; $(`${c.id}-range`).value = toSlider(c, v); }
  }
  showJSON(); updateButtons(); if (send) audition(true);
}
(function build() {
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
      box.addEventListener("change", () => { if (patch) { write(c, box.checked); changed(); } });
      continue;
    }
    const label = document.createElement("label"); label.htmlFor = c.id; label.textContent = c.label;
    if (c.type === "select") {
      const select = document.createElement("select"); select.id = c.id;
      for (const option of c.options) select.add(new Option(c.labels ? c.labels[option] : option.replace("_", " & "), option));
      holder.append(label, select);
      select.addEventListener("change", () => { if (patch) { write(c, select.value); changed(); } });
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
    number.addEventListener("input", () => { if (!patch) return; write(c, number.value === "" ? null : Number(number.value)); range.value = toSlider(c, read(c)); changed(); });
    range.addEventListener("input", () => { number.value = fromSlider(c, range.value); number.dispatchEvent(new Event("input")); });
  }
})();
DATA.presets.forEach((p, i) => $("preset").add(new Option(p.name, String(i))));
$("preset").addEventListener("change", () => { if ($("preset").value !== "") { loadPatch(DATA.presets[Number($("preset").value)]); notice(`${patch.name} opened.`); } });
$("patch-name").addEventListener("input", () => { if (patch) { patch.name = $("patch-name").value; showJSON(); } });
$("routing").addEventListener("change", () => { if (patch) { patch.routing = $("routing").value; changed(); } });
$("fold-all").addEventListener("click", () => {
  const sets = [...document.querySelectorAll("#groups details")], open = !sets.every(d => d.open);
  sets.forEach(d => { d.open = open; }); $("fold-all").textContent = open ? "close all" : "open all";
});
$("download").addEventListener("click", () => {
  try {
    W.validate(patch);
    const url = URL.createObjectURL(new Blob([JSON.stringify(patch, null, 2) + "\n"], {type: "application/json"}));
    const link = document.createElement("a"); link.href = url; link.download = (patch.name.replace(/[^a-z0-9_-]/gi, "_").slice(0, 60) || "choowi-sound") + ".json";
    link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000); notice("Saved as a JSON file.");
  } catch (error) { notice(error.message, true); }
});
$("import").addEventListener("change", async () => {
  try {
    const file = $("import").files[0]; if (!file) return;
    if (file.size > 65536) throw new Error("That file is over 64 KiB: not a Choowi sound.");
    loadPatch(W.readSound(await file.text())); $("preset").value = ""; notice(`${patch.name} opened.`);
  } catch (error) { notice(error.message, true); } finally { $("import").value = ""; }
});
// Tabs: easy and tweak side by side on wide screens; one at a time on phones.
for (const [tab, panel, other, otherPanel] of [["tab-easy", "easy", "tab-tweak", "tweak"], ["tab-tweak", "tweak", "tab-easy", "easy"]])
  $(tab).addEventListener("click", () => {
    $(tab).setAttribute("aria-selected", "true"); $(other).setAttribute("aria-selected", "false");
    $(panel).classList.add("shown"); $(otherPanel).classList.remove("shown");
  });
$("easy").classList.add("shown");
updateButtons();
