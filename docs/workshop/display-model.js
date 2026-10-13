"use strict";
// What the display shows, from CHOMPI's view reply (opcode 0D; docs/forge/DISPLAY.md) and its status: plain
// names for the menu page, the knobs, the chord, the looper and the battery. No DOM here, so the host tests
// (tests/test_display_js.py) check it against the firmware's own tables.
(function (root) {
  const NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const ROMAN = ["I", "II", "III", "IV", "V", "VI", "VII"];
  // core/parameters.h Parameter, in enum order (a knob's target id indexes this).
  const PARAMETERS = ["Delay mix", "Delay time", "Feedback", "Level", "Bypass", "Cutoff", "Resonance", "Reverb",
    "Pitch", "Start", "End", "Knob 1", "Knob 2", "Knob 3", "Knob 4",
    "Filter envelope", "Attack", "Decay", "Sustain", "Release", "LFO rate", "Vibrato", "LFO filter", "Tremolo depth",
    "Osc 2 level", "Osc 2 detune", "Noise", "Glide", "Room size", "Damping", "Crossfade",
    "Speed", "Voice gain", "Pan", "Space", "Saturation", "Warble", "DJ filter", "DJ resonance", "Compressor",
    "Input gain", "Motion", "Motion speed", "Colour character", "Texture character"];
  const KNOBS = ["SW4", "SW1", "SW2", "SW3"];                 // knob 0..3, left to right on the panel
  const FX_MODULES = ["Space", "Colour", "Texture", "Motion"];  // the FX page's knobs SW4 SW1 SW2 SW3
  // The harmony and parts pages' knobs (MANUAL 8a / 8b): harmony::Pack and parts::PackArp, as those pages light them.
  const MODES = ["major", "natural minor", "harmonic minor", "melodic minor", "dorian", "phrygian", "lydian", "mixolydian", "locrian"];
  const EXTENSIONS = ["triad", "7th", "9th", "11th", "13th", "fifth"];        // firmware enum order
  const SIZE_ORDER = ["fifth", "triad", "7th", "9th", "11th", "13th"];        // SW1's order, for its bar
  const BASS_RATES = ["per chord", "1/2", "1/4", "1/8"];
  const LOOPER = ["empty", "armed", "recording", "playing", "paused"];
  const RECORDER = ["empty", "armed", "recording", "playing", "stopped"];
  const BATTERY = ["full", "high", "medium", "low", "unknown"];
  // The CHOMPI light's colour per page (MANUAL: TAPE teal, presets blue, harmony purple, parts orange, synth pink, FX green).
  const PAGES = {play: ["Play", "#ffffff"], record: ["TAPE · record", "#ec3b3b"], presets: ["Presets", "#2f7cf6"],
    samples: ["TAPE page", "#19c9b6"], harmony: ["Harmony", "#9150f0"], parts: ["Arp · bass", "#ff8a2a"],
    synth: ["Synth", "#ff5ea0"], fx: ["FX", "#2fbf55"]};

  // core/preset_menu.h Packed: bit 0 open, 1-3 the mode (7 harmony, 6 parts, 5 synth / FX), 21 the samples page;
  // the FX page is told apart from the synth page by the FX lights' bit 31.
  function page(words) {
    const m = words.menu;
    if (!(m & 1)) return words.flags & 1 ? "record" : "play";
    const mode = m >>> 1 & 7;
    if (mode === 7) return "harmony";
    if (mode === 6) return "parts";
    if (mode === 5) return words.fx_lights >>> 31 & 1 ? "fx" : "synth";
    return m >>> 21 & 1 ? "samples" : "presets";
  }
  const byte = (word, i) => word >>> 8 * i & 255;
  // The four knobs as the panel moves them. Normal play: each knob's page (1-based), its target and value;
  // FX page: the module amounts; synth page: that page's knobs.
  function harmonyKnobs(w) {
    const mode = (w >>> 4 & 15) % 9, size = EXTENSIONS[(w >>> 8 & 7) % 6], inversion = w >>> 11 & 3;
    return [
      {label: "Mode", text: MODES[mode], value: mode / 8, hint: `harmony ${w >>> 16 & 1 ? "on" : "off"}`},
      {label: "Chord size", text: size, value: SIZE_ORDER.indexOf(size) / 5, hint: `${w >>> 15 & 1 ? "real" : "static"} layout`},
      {label: "Inversion", text: String(inversion), value: inversion / 3, hint: `voice leading ${w >>> 14 & 1 ? "off" : "on"}`},
      {label: "Spread", text: w >>> 13 & 1 ? "open" : "close", value: w >>> 13 & 1, hint: ""}];
  }
  function partsKnobs(arp, clock) {
    const bpm = clock >>> 8 & 511, octaves = (arp >>> 6 & 3) + 1, gate = ((arp >>> 8 & 31) + 1) * 5, rate = arp >>> 17 & 3;
    return [
      {label: "Tempo", text: `${bpm} BPM`, value: (bpm - 40) / 260, hint: clock & 2 ? "MIDI clock" : "tap tempo"},
      {label: "Arp octaves", text: String(octaves), value: (octaves - 1) / 3, hint: ""},
      {label: "Arp gate", text: `${gate} %`, value: gate / 100, hint: ""},
      {label: "Bass rate", text: BASS_RATES[rate], value: rate / 3, hint: `bass in C${(arp >>> 19 & 3) + 1}`}];
  }
  function knobs(words, where) {
    if (where === "harmony") return harmonyKnobs(words.harmony_lights).map((k, i) => ({name: KNOBS[i], ...k}));
    if (where === "parts") return partsKnobs(words.parts_lights, words.parts_clock).map((k, i) => ({name: KNOBS[i], ...k}));
    return KNOBS.map((name, k) => {
      if (where === "fx") return {name, label: FX_MODULES[k], value: byte(words.fx_knobs, k) / 255};
      if (where === "synth") return {name, label: `Synth ${words.synth_lights >>> 14 + k & 1 ? "page 2" : "page 1"}`, value: byte(words.synth_knobs, k) / 255};
      const target = byte(words.knob_targets, k);
      return {name, page: (words.knob_state >>> 3 * k & 7) + 1, patchPage: !!(words.knob_state >>> 12 + k & 1),
              label: PARAMETERS[target] || `control ${target}`, value: byte(words.knob_values, k) / 255};
    });
  }
  function volume(words) { return {label: words.knob_state >>> 20 & 1 ? "Input gain" : "Volume", value: (words.knob_state >>> 24) / 255}; }
  // A chord's name from the notes that sound (the Inspector's chord_label, host/forge_inspector.py).
  function chordName(root, notes) {
    if (!notes.length) return null;
    const pcs = new Set(notes.map(n => ((n - root) % 12 + 12) % 12)), has = i => pcs.has(i);
    let third = has(3) && !has(4) ? "m" : has(5) && !has(4) && !has(3) ? "sus4" : "";
    if (has(3) && has(6) && !has(7)) third = "dim";
    let ext = "";
    if (has(10)) ext = "7";
    if (has(11)) ext = "maj7";
    if (has(2) && (has(10) || has(11))) ext = ext === "7" ? "9" : ext === "maj7" ? "maj9" : ext;
    if (has(9) && (has(10) || has(11))) ext = "13";
    if (has(5) && third !== "sus4" && (has(10) || has(11)) && ext !== "13") ext = "11";
    return NOTES[root] + (third !== "m" || !ext.startsWith("maj") ? third : "m") + ext;
  }
  // Engine::ChordView: root 0-3, quality 4-6, kind 7-9, degree 10-12, shifted 13, count 14-16, counter 17-23.
  function chord(words, chordNotes) {
    const w = words.chord, count = Math.min(w >>> 14 & 7, 5), root = w & 15, kind = w >>> 7 & 7, degree = w >>> 10 & 7;
    if (!count) return null;
    const notes = chordNotes.slice(0, count);
    const numeral = kind === 1 || kind === 5 ? ROMAN[degree] : kind === 2 ? `V/${ROMAN[degree]}` : null;
    return {name: chordName(root, notes), numeral, shifted: !!(w >>> 13 & 1), counter: w >>> 17 & 127,
            notes: notes.map(n => NOTES[n % 12] + (Math.floor(n / 12) - 1))};
  }
  // PackLooper: state 0-2, overdub 3, effects before 4, has a loop 5, position 6-15 (x1023), length 16-31 (10 ms).
  function looper(words) {
    const l = words.looper;
    return {state: LOOPER[l & 7] || "?", overdub: !!(l >>> 3 & 1), fxBefore: !!(l >>> 4 & 1), hasLoop: !!(l >>> 5 & 1),
            position: (l >>> 6 & 1023) / 1023, seconds: (l >>> 16) / 100};
  }
  function recorder(words) { const s = words.sequence_lights; return {state: RECORDER[s & 7] || "?", overdub: !!(s >>> 3 & 1)}; }
  function power(p) { return {battery: BATTERY[p.battery] || "unknown", usb: !!(p.flags & 1), charging: p.chargeState >= 1 && p.chargeState <= 4, full: p.chargeState === 5}; }
  // The whole picture for one state reply.
  function describe(view) {
    const w = view.words, where = page(w);
    return {page: where, pageName: PAGES[where][0], pageColour: PAGES[where][1], knobs: knobs(w, where), volume: volume(w),
            chord: chord(w, view.chordNotes), looper: looper(w), recorder: recorder(w), power: power(view.power),
            dream: !!(w.dream >>> 26 & 1), recording: !!(w.flags & 2), safeMode: !!(w.flags & 4), busy: !!(w.flags & 8)};
  }
  // The panel's lights (view page 0) in place: 0-9 black keys C#3..A#4, 24 down to 10 white keys C3..C5,
  // 25 CHOMPI, 26 PLAY, 27 LOOP, 28-31 the rings SW4 SW1 SW2 SW3, 32 / 33 the transport pair, 34 SW6.
  const LIGHTS = {black: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], white: [24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10],
    chompi: 25, play: 26, loop: 27, rings: [28, 29, 30, 31], reverse: 32, forward: 33, sw6: 34};
  root.DisplayModel = {describe, page, knobs, chord, chordName, looper, LIGHTS, PARAMETERS, NOTES};
})(typeof window !== "undefined" ? window : globalThis);
if (typeof module !== "undefined") module.exports = globalThis.DisplayModel;
