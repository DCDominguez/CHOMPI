"use strict";
// Forge's USB wire format in the browser (Choowi 0.20), for the static "take it to CHOMPI" page on
// GitHub Pages: a port of firmware/chompi-forge/host/forge_host.py (encode_patch for v7 / v8 patches,
// message, the status reply). The host test tests/test_wire_js.py checks it byte for byte against the
// Python code for every preset and random patches; change both together.
(function (root) {
  const PREFIX = [0x7D, 0x46, 0x47, 1];
  const WAVEFORMS = ["sine", "triangle", "saw", "square"], LFO_WAVEFORMS = ["sine", "triangle", "square", "sample_hold"];
  const ROUTES4 = ["aux>delay>reverb>output", "synth>delay>reverb>output", "sampler>delay>reverb>output"];
  const SAMPLE_MODES = ["chromatic", "kit"], SAMPLE_BANKS = ["a", "b", "c", "d", "e"];
  const KNOB_TARGETS = {"delay.mix": 0, "delay.time_ms": 1, "delay.feedback": 2, "output.level": 3, "filter.cutoff_hz": 5,
    "filter.resonance": 6, "reverb.mix": 7, "sampler.pitch_semitones": 8, "sampler.start": 9, "sampler.end": 10,
    "filter.env_octaves": 15, "synth.attack_ms": 16, "synth.decay_ms": 17, "synth.sustain": 18, "synth.release_ms": 19,
    "lfo.rate_hz": 20, "lfo.pitch_cents": 21, "lfo.filter_octaves": 22, "lfo.amp_depth": 23, "synth.osc2_level": 24,
    "synth.osc2_detune_cents": 25, "synth.noise": 26, "synth.glide_ms": 27, "reverb.size": 28, "reverb.damping": 29,
    "sampler.crossfade_ms": 30};
  const TONICS = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const MODES = ["major", "natural_minor", "harmonic_minor", "melodic_minor", "dorian", "phrygian", "lydian", "mixolydian", "locrian"];
  const EXTENSIONS = ["triad", "7th", "9th", "11th", "13th", "fifth"];          // firmware enum order
  const PATTERNS = ["off", "up", "down", "updown", "order", "random"], RATES = ["1/4", "1/8", "1/8t", "1/16", "1/16t", "1/32"];
  const BASS_MODES = ["off", "root", "fifth", "alternate", "octave"], BASS_RATES = ["chord", "1/2", "1/4", "1/8"];
  // v8 (0.20): the FX section (forge_host FX_*): four modules in any order, one effect each, amounts 0-1.
  const FX_MODULES = ["space", "colour", "texture", "motion"], FX_CLASSIC = ["colour", "motion", "texture", "space"];
  const FX_CHOICES = {space: ["echo_room", "cloud", "tape_echo"], colour: ["saturation", "fold", "crush"],
    texture: ["cassette", "broken", "squash"], motion: ["chorus", "phaser", "tremolo"]};
  const FX_AMOUNTS = ["colour_amount", "texture_amount", "motion_amount", "motion_rate", "colour_character", "texture_character"];
  const FX_DEFAULTS = {order: [...FX_CLASSIC], side_by_side: false, space: "echo_room", colour: "saturation", texture: "cassette",
    motion: "chorus", colour_amount: 0, texture_amount: 0, motion_amount: 0, motion_rate: 0.3, colour_character: 0, texture_character: 0};
  // Wire order after the shared fields (forge_host V3_FIELDS / V4_SAMPLER); codec as in Python.
  const V3_FIELDS = [
    ["synth", "osc2_waveform", ["enum", WAVEFORMS]], ["synth", "osc2_level", ["lin", 0, 1]], ["synth", "osc2_semitones", ["int", -24, 24, 24]],
    ["synth", "osc2_detune_cents", ["lin", -50, 50]], ["synth", "noise", ["lin", 0, 1]], ["filter", "resonance", ["lin", 0, 1]],
    ["filter", "env_octaves", ["lin", -6, 6]], ["filter", "attack_ms", ["lin", 1, 2000]], ["filter", "decay_ms", ["lin", 1, 2000]],
    ["filter", "sustain", ["lin", 0, 1]], ["filter", "release_ms", ["lin", 5, 5000]], ["lfo", "waveform", ["enum", LFO_WAVEFORMS]],
    ["lfo", "rate_hz", ["log", 0.05, 20]], ["lfo", "pitch_cents", ["lin", 0, 200]], ["lfo", "filter_octaves", ["lin", 0, 4]],
    ["lfo", "amp_depth", ["lin", 0, 1]], ["lfo", "mod_wheel", ["bool"]], ["synth", "voices", ["int", 1, 7, 0]],
    ["synth", "glide_ms", ["lin", 0, 2000]], ["reverb", "mix", ["lin", 0, 1]], ["reverb", "size", ["lin", 0, 1]], ["reverb", "damping", ["lin", 0, 1]]];
  const V4_SAMPLER = [["mode", ["enum", SAMPLE_MODES]], ["bank", ["enum", SAMPLE_BANKS]], ["slot", ["int", 1, 15, -1]],
    ["pitch_semitones", ["lin", -24, 24]], ["start", ["lin", 0, 1]], ["end", ["lin", 0, 1]], ["loop", ["bool"]], ["hold", ["bool"]],
    ["reverse", ["bool"]], ["crossfade_ms", ["lin", 0, 250]]];
  const AMP = [["attack_ms", 1, 2000], ["decay_ms", 1, 2000], ["sustain", 0, 1], ["release_ms", 5, 5000]];
  const MODULES = {synth: ["waveform", "attack_ms", "decay_ms", "sustain", "release_ms", "osc2_waveform", "osc2_level", "osc2_semitones",
                           "osc2_detune_cents", "noise", "voices", "glide_ms"],
    filter: ["cutoff_hz", "resonance", "env_octaves", "attack_ms", "decay_ms", "sustain", "release_ms"],
    lfo: ["waveform", "rate_hz", "pitch_cents", "filter_octaves", "amp_depth", "mod_wheel"],
    sampler: V4_SAMPLER.map(([k]) => k), delay: ["mix", "time_ms", "feedback", "bypass"], reverb: ["mix", "size", "damping"], output: ["level"]};
  const ERRORS = {1: "invalid length", 2: "unsupported version", 3: "checksum mismatch", 4: "invalid patch or preset address",
    5: "unknown operation", 6: "device queue busy", 7: "that slot is empty", 8: "SD card missing or storage failed", 9: "storage busy",
    10: "battery low on a weak supply"};
  ERRORS[2] = "unsupported version (sounds from this page need Choowi 0.20 or newer on CHOMPI)";
  const STATUS_LENGTHS = {1: 30, 2: 42, 3: 81, 4: 96, 5: 100, 6: 103, 7: 109, 8: 123}, STATUS_EXTRA = 9;
  const OFFSETS = {1: 18, 2: 30, 3: 69, 4: 84, 5: 88, 6: 91, 7: 97, 8: 111};
  const BATTERY = ["full", "high", "medium", "low", "unknown"];

  const word14 = v => [v & 127, (v >> 7) & 127];
  const toUnit = (v, c) => c[0] === "log" ? Math.log(v / c[1]) / Math.log(c[2] / c[1]) : (v - c[1]) / (c[2] - c[1]);
  const unitWord = (v, c) => Math.floor(Math.min(Math.max(toUnit(v, c), 0), 1) * 16383 + 0.5);   // Python int(x + .5), x >= 0
  const checksum = data => (-data.reduce((a, b) => a + b, 0)) & 127;
  function message(opcode, sequence, data = []) {
    if (!Number.isInteger(sequence) || sequence < 0 || sequence > 16383) throw new Error("Sequence must be 0-16383");
    const payload = [...PREFIX, opcode, ...word14(sequence), ...data];
    return [...payload, checksum(payload)];
  }
  const frame = payload => [0xF0, ...payload, 0xF7];

  function fail(where, why) { throw new Error(`Not a Choowi sound: ${where} ${why}`); }
  function exactKeys(object, keys, where) {
    if (!object || typeof object !== "object" || Array.isArray(object)) fail(where, "is missing");
    const have = Object.keys(object).sort().join(","), want = [...keys].sort().join(",");
    if (have !== want) fail(where, `should have exactly: ${keys.join(", ")}`);
  }
  function check(value, codec, where) {
    if (codec[0] === "enum") { if (!codec[1].includes(value)) fail(where, `must be one of ${codec[1].join(", ")}`); return; }
    if (codec[0] === "bool") { if (typeof value !== "boolean") fail(where, "must be true or false"); return; }
    if (typeof value !== "number" || !Number.isFinite(value)) fail(where, "must be a number");
    if (codec[0] === "int" && !Number.isInteger(value)) fail(where, "must be a whole number");
    if (value < codec[1] || value > codec[2]) fail(where, `must be ${codec[1]}-${codec[2]}`);
  }
  // The v7 / v8 checks forge_host.validate_patch makes (structure, ranges, start < end); older versions are
  // converted by the workshop first.
  function validate(patch) {
    if (!patch || typeof patch !== "object") fail("the file", "is not a JSON object");
    if (patch.version !== 7 && patch.version !== 8) fail("version", `is ${patch.version}: open it in Choowi's workshop first (it converts older sounds)`);
    exactKeys(patch, ["version", "name", "engine", "routing", "modules", "knobs", "harmony", "parts", ...(patch.version === 8 ? ["fx"] : [])], "the sound");
    if (typeof patch.name !== "string" || !patch.name.length || patch.name.length > 80) fail("name", "must be 1-80 characters");
    if (patch.engine !== "instrument") fail("engine", "must be instrument");
    if (!ROUTES4.includes(patch.routing)) fail("routing", `must be one of ${ROUTES4.join(", ")}`);
    exactKeys(patch.modules, Object.keys(MODULES), "modules");
    for (const [m, keys] of Object.entries(MODULES)) exactKeys(patch.modules[m], keys, `modules.${m}`);
    const s = patch.modules.synth, f = patch.modules.filter, d = patch.modules.delay;
    check(s.waveform, ["enum", WAVEFORMS], "synth.waveform");
    for (const [k, lo, hi] of AMP) check(s[k], ["lin", lo, hi], `synth.${k}`);
    check(f.cutoff_hz, ["log", 40, 16000], "filter.cutoff_hz");
    for (const [m, k, c] of V3_FIELDS) check(patch.modules[m][k], c, `${m}.${k}`);
    for (const [k, c] of V4_SAMPLER) check(patch.modules.sampler[k], c, `sampler.${k}`);
    if (patch.modules.sampler.start >= patch.modules.sampler.end) fail("sampler.start", "must be below sampler.end");
    check(d.mix, ["lin", 0, 1], "delay.mix"); check(d.time_ms, ["lin", 10, 1000], "delay.time_ms");
    check(d.feedback, ["lin", 0, 0.85], "delay.feedback"); check(d.bypass, ["bool"], "delay.bypass");
    check(patch.modules.output.level, ["lin", 0, 1], "output.level");
    if (!Array.isArray(patch.knobs) || patch.knobs.length !== 4 || patch.knobs.some(k => k !== "default" && !(k in KNOB_TARGETS)))
      fail("knobs", "must be four knob choices");
    const h = patch.harmony;
    exactKeys(h, ["enabled", "tonic", "mode", "extension", "inversion", "open", "voice_leading", "layout"], "harmony");
    check(h.enabled, ["bool"], "harmony.enabled"); check(h.tonic, ["enum", TONICS], "harmony.tonic"); check(h.mode, ["enum", MODES], "harmony.mode");
    check(h.extension, ["enum", EXTENSIONS], "harmony.extension"); check(h.inversion, ["int", 0, 3, 0], "harmony.inversion");
    check(h.open, ["bool"], "harmony.open"); check(h.voice_leading, ["bool"], "harmony.voice_leading"); check(h.layout, ["enum", ["static", "real"]], "harmony.layout");
    const p = patch.parts; exactKeys(p, ["arp", "bass", "clock"], "parts");
    exactKeys(p.arp, ["pattern", "rate", "octaves", "gate", "latch"], "parts.arp");
    exactKeys(p.bass, ["mode", "rate", "octave"], "parts.bass"); exactKeys(p.clock, ["bpm", "seed", "send_clock"], "parts.clock");
    check(p.arp.pattern, ["enum", PATTERNS], "arp.pattern"); check(p.arp.rate, ["enum", RATES], "arp.rate");
    check(p.arp.octaves, ["int", 1, 4, 0], "arp.octaves"); check(p.arp.gate, ["int", 5, 100, 0], "arp.gate");
    if (p.arp.gate % 5) fail("arp.gate", "must be a multiple of 5");
    check(p.arp.latch, ["bool"], "arp.latch"); check(p.bass.mode, ["enum", BASS_MODES], "bass.mode");
    check(p.bass.rate, ["enum", BASS_RATES], "bass.rate"); check(p.bass.octave, ["int", 1, 3, 0], "bass.octave");
    check(p.clock.bpm, ["int", 40, 300, 0], "clock.bpm"); check(p.clock.seed, ["int", 0, 2047, 0], "clock.seed");
    check(p.clock.send_clock, ["bool"], "clock.send_clock");
    if (patch.version === 8) {
      const x = patch.fx; exactKeys(x, Object.keys(FX_DEFAULTS), "fx");
      if (!Array.isArray(x.order) || x.order.length !== 4 || [...x.order].sort().join() !== [...FX_MODULES].sort().join())
        fail("fx.order", `must list ${FX_MODULES.join(", ")} each once`);
      check(x.side_by_side, ["bool"], "fx.side_by_side");
      for (const m of FX_MODULES) check(x[m], ["enum", FX_CHOICES[m]], `fx.${m}`);
      for (const k of FX_AMOUNTS) check(x[k], ["lin", 0, 1], `fx.${k}`);
    }
    return patch;
  }
  // A v7 sound as v8: the FX section at its defaults (forge_host.upgrade_patch to 8).
  const toV8 = patch => patch && patch.version === 7 ? {...patch, version: 8, fx: JSON.parse(JSON.stringify(FX_DEFAULTS))} : patch;
  function fxOrderIndex(order) {
    let index = 0; const left = [...FX_CLASSIC];
    order.forEach((m, i) => { const r = left.indexOf(m); index = index * (4 - i) + r; left.splice(r, 1); });
    return index;
  }
  const fxWord = f => FX_MODULES.reduce((w, m, i) => w | FX_CHOICES[m].indexOf(f[m]) << 2 * i, 0) | fxOrderIndex(f.order) << 8 | (f.side_by_side ? 1 : 0) << 13;
  const harmonyWord = h => TONICS.indexOf(h.tonic) | MODES.indexOf(h.mode) << 4 | EXTENSIONS.indexOf(h.extension) << 8 | h.inversion << 11
    | (h.open ? 1 : 0) << 13 | (h.voice_leading ? 0 : 1) << 14 | (h.layout === "real" ? 1 : 0) << 15 | (h.enabled ? 1 : 0) << 16;
  function partsWords(p) {
    const a = p.arp, b = p.bass, c = p.clock;
    return [PATTERNS.indexOf(a.pattern) | RATES.indexOf(a.rate) << 3 | (a.octaves - 1) << 6 | (a.gate / 5 - 1) << 8 | (a.latch ? 1 : 0) << 13
            | BASS_MODES.indexOf(b.mode) << 14 | BASS_RATES.indexOf(b.rate) << 17 | (b.octave - 1) << 19,
            c.bpm | c.seed << 9 | (c.send_clock ? 1 : 0) << 20];
  }
  const three = w => [w & 127, w >> 7 & 127, w >> 14 & 127];
  function encodeValue(value, codec) {
    if (codec[0] === "enum") return [codec[1].indexOf(value)];
    if (codec[0] === "int") return [value + codec[3]];
    if (codec[0] === "bool") return [value ? 1 : 0];
    return word14(unitWord(value, codec));
  }
  // forge_host.encode_patch for a v7 patch: the SysEx payload without F0 / F7.
  function encodePatch(patch, sequence) {
    validate(patch);
    const m = patch.modules, d = m.delay;
    const data = [patch.version];
    for (const v of [d.mix, (d.time_ms - 10) / 990, d.feedback / 0.85, m.output.level]) data.push(...word14(Math.floor(Math.min(Math.max(v, 0), 1) * 16383 + 0.5)));
    data.push(d.bypass ? 1 : 0);
    const route = ROUTES4.indexOf(patch.routing);
    data.push(Math.min(route, 1), WAVEFORMS.indexOf(m.synth.waveform));
    for (const [k, lo, hi] of AMP) data.push(...word14(unitWord(m.synth[k], ["lin", lo, hi])));
    data.push(...word14(unitWord(m.filter.cutoff_hz, ["log", 40, 16000])));
    for (const [mod, k, c] of V3_FIELDS) data.push(...encodeValue(m[mod][k], c));
    data.push(route === 2 ? 1 : 0);
    for (const [k, c] of V4_SAMPLER) data.push(...encodeValue(m.sampler[k], c));
    for (const k of patch.knobs) data.push(k === "default" ? 0 : KNOB_TARGETS[k] + 1);
    data.push(...three(harmonyWord(patch.harmony)));
    for (const w of partsWords(patch.parts)) data.push(...three(w));
    if (patch.version === 8) {
      const w = fxWord(patch.fx); data.push(w & 127, w >> 7 & 127);
      for (const k of FX_AMOUNTS) data.push(...word14(Math.floor(Math.min(Math.max(patch.fx[k], 0), 1) * 16383 + 0.5)));
    }
    return message(1, sequence, data);
  }
  const read14 = (data, i) => data[i] | data[i + 1] << 7;
  // 0.20 display (opcode 0D, forge_host.view_message / VIEW_WORDS): page 0 the 35 panel lights, 1 the state.
  const VIEW_LEDS = 35;
  const VIEW_WORDS = ["menu", "looper", "knob_state", "knob_values", "knob_targets", "fx_lights", "fx_knobs", "synth_lights",
    "synth_knobs", "harmony_lights", "parts_lights", "parts_clock", "sequence_lights", "chord", "flags", "dream"];
  function decodeView(data) {
    const page = data[8], size = page === 0 ? 9 + 3 * VIEW_LEDS + 1 : 9 + 5 * VIEW_WORDS.length + 9;
    if (data[7] !== 0 || (page !== 0 && page !== 1) || data.length !== size) throw new Error("Unexpected view reply");
    if (page === 0) return {page, leds: [...Array(VIEW_LEDS).keys()].map(i => data.slice(9 + 3 * i, 12 + 3 * i))};
    const words = {};
    VIEW_WORDS.forEach((name, i) => { words[name] = [0, 1, 2, 3, 4].reduce((w, b) => w + data[9 + 5 * i + b] * 2 ** (7 * b), 0) % 2 ** 32; });
    const at = 9 + 5 * VIEW_WORDS.length;
    return {page, words, chordNotes: data.slice(at, at + 5), power: {battery: data[at + 5], flags: data[at + 6], chargeState: data[at + 7]}};
  }
  // CHOMPI's reply (payload without F0 / F7) to the request `sent` (payload without F0 / F7).
  function decodeReply(data, sent) {
    if (data.length < 8 || PREFIX.some((b, i) => data[i] !== b) || data.some(x => !Number.isInteger(x) || x < 0 || x > 127))
      throw new Error("Not a reply from Forge");
    if (read14(data, 5) !== read14(sent, 5)) throw new Error("Reply to another request");
    if (checksum(data) !== 0) throw new Error("Reply damaged on the way (checksum)");
    if (data[4] === 0x41 && data.length === 9) throw new Error(`CHOMPI said no: ${ERRORS[data[7]] || "unknown error"}`);
    if (data[4] === 0x49) return decodeView(data);
    if (data[4] === 0x42) {                                  // preset stored / erased (forge_host decode_response)
      if (data.length !== 12 || data[7] !== 0 || ![1, 2].includes(data[8]) || data[9] > 7 || data[10] > 14) throw new Error("Unexpected preset reply");
      return {action: data[8] === 1 ? "stored" : "erased", bank: data[9] + 1, slot: data[10] + 1};
    }
    if (data[4] === 0x43) {                                  // which preset slots are used
      if (data.length !== 33 || data[7] !== 0) throw new Error("Unexpected preset list");
      const occupied = {};
      for (let b = 0; b < 8; b++) {
        const bits = data[8 + 3 * b] | data[9 + 3 * b] << 7 | data[10 + 3 * b] << 14;
        occupied[b + 1] = [...Array(15).keys()].filter(s => bits >> s & 1).map(s => s + 1);
      }
      return {occupied};
    }
    const version = data[8];
    if (data[4] !== 0x40 || data.length < 18 || data[7] !== 0 || !(version in STATUS_LENGTHS)
        || ![0, STATUS_EXTRA].includes(data.length - STATUS_LENGTHS[version])) throw new Error("Unexpected reply from CHOMPI");
    if (sent[4] === 1 && data.slice(8, sent.length).join() !== sent.slice(7, -1).join())
      throw new Error("CHOMPI acknowledged a different sound; send it again");
    const o = OFFSETS[version], identity = data.length !== STATUS_LENGTHS[version];
    const status = {patchVersion: version, firmware: `0.${data[o + 10]}${identity ? "." + data[o + 11] : ""}`, build: null, safeMode: false, power: null};
    if (identity) {
      const commit = [0, 1, 2, 3].reduce((a, i) => a + data[o + 13 + i] * 2 ** (7 * i), 0);
      status.build = commit ? commit.toString(16).padStart(7, "0") : null; status.safeMode = !!(data[o + 12] & 4);
      const [level, flags, charge] = data.slice(o + 17, o + 20);
      status.power = {battery: BATTERY[level] || "unknown", usbPower: !!(flags & 1), charging: charge >= 1 && charge <= 4, chargeDone: charge === 5};
    }
    return status;
  }
  // A JSON sound from a file or a chatbot's reply: the last ```json block, else the outermost braces.
  function readSound(text) {
    if (typeof text !== "string" || !text.trim()) throw new Error("Nothing to read");
    const blocks = [...text.matchAll(/```(?:json)?\s*(\{[\s\S]*?\})\s*```/g)].map(m => m[1]);
    const raw = blocks.length ? blocks[blocks.length - 1] : text.includes("{") ? text.slice(text.indexOf("{"), text.lastIndexOf("}") + 1) : "";
    let patch; try { patch = JSON.parse(raw); } catch { throw new Error("That isn't valid JSON"); }
    return validate(toV8(patch));
  }
  // forge_host.preset_message: store (4) saves the sound CHOMPI is playing into bank 1-8, slot 1-15; list (7).
  function presetStore(sequence, bank, slot) {
    if (!Number.isInteger(bank) || !Number.isInteger(slot) || bank < 1 || bank > 8 || slot < 1 || slot > 15) throw new Error("Bank must be 1-8 and slot 1-15");
    return message(4, sequence, [bank - 1, slot - 1]);
  }
  const api = {encodePatch, validate, readSound, decodeReply, statusMessage: s => message(2, s), stopMessage: s => message(3, s),
               presetStore, presetList: s => message(7, s), frame, message, toV8,
               viewMessage: (s, page) => { if (page !== 0 && page !== 1) throw new Error("View page must be 0 or 1"); return message(0x0D, s, [page]); },
               FX: {MODULES: FX_MODULES, CLASSIC: FX_CLASSIC, CHOICES: FX_CHOICES, AMOUNTS: FX_AMOUNTS, DEFAULTS: FX_DEFAULTS}};
  if (typeof module !== "undefined") module.exports = api; else root.ForgeWire = api;
})(typeof window !== "undefined" ? window : globalThis);
