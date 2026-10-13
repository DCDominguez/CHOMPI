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
  const SYNTH_KNOBS = [["Osc 2 level", "Vibrato"], ["Osc 2 detune", "Tremolo depth"], ["Noise", "LFO rate"], ["Glide", "Delay time"]];
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
      if (where === "synth") {                   // PanelController::SynthTarget: press flips page 1 / 2
        const page2 = words.synth_lights >>> 14 + k & 1;
        return {name, page: page2 + 1, label: SYNTH_KNOBS[k][page2], value: byte(words.synth_knobs, k) / 255};
      }
      if (k === 0 && (where === "presets" || where === "samples"))
        return {name, label: "Bank", text: where === "samples" ? "abcde"[words.menu >>> 4 & 7] || "?" : String((words.menu >>> 4 & 7) + 1),
                value: (words.menu >>> 4 & 7) / (where === "samples" ? 4 : 7), hint: ""};
      const target = byte(words.knob_targets, k);
      return {name, page: (words.knob_state >>> 3 * k & 7) + 1, patchPage: !!(words.knob_state >>> 12 + k & 1),
              label: PARAMETERS[target] || `control ${target}`, value: byte(words.knob_values, k) / 255};
    });
  }
  // SW5 (view flags): its cutoff (MANUAL: about 80 Hz to 8 kHz, so shown as an estimate) and the loop speed.
  function sw5(words) {
    const f = words.flags, cutoff = (f >>> 8 & 255) / 255, speed = Math.round(((f >>> 16 & 1023) / 255 - 2) * 100) / 100;
    const hz = 80 * Math.pow(100, cutoff);
    return {cutoff, turned: !!(f >>> 5 & 1), speed, hz: hz >= 1000 ? `≈${(hz / 1000).toFixed(1)} kHz` : `≈${Math.round(hz)} Hz`};
  }
  function volume(words) { return {label: words.knob_state >>> 20 & 1 ? "Input gain" : "Volume", value: (words.knob_state >>> 24) / 255}; }
  // Music spelling (the owner, 2026-10-13: the chord read wrong): letters from the key, flats for borrowed roots,
  // lower-case numerals for minor and diminished chords. core/harmony.h kScale, in Mode order.
  const SCALES = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 8, 11], [0, 2, 3, 5, 7, 9, 11],
    [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10], [0, 2, 4, 6, 7, 9, 11], [0, 2, 4, 5, 7, 9, 10], [0, 1, 3, 5, 6, 8, 10]];
  const LETTERS = ["C", "D", "E", "F", "G", "A", "B"], NATURAL = [0, 2, 4, 5, 7, 9, 11];
  const TONIC_LETTER = [0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6];        // C D♭ D E♭ E F F#(G♭ → F#) G A♭ A B♭ B
  const TONIC_FLAT = [0, 1, 0, 1, 0, 0, -1, 0, 1, 0, 1, 0];         // black-key tonics as flats, F# as a sharp
  function spell(pc, letter) {                                     // the pitch class written on this letter
    const diff = ((pc - NATURAL[letter]) % 12 + 18) % 12 - 6;
    return LETTERS[letter] + (diff > 0 ? "♯".repeat(diff) : "♭".repeat(-diff));
  }
  const tonicLetter = tonic => TONIC_FLAT[tonic] === 1 ? (TONIC_LETTER[tonic] + 1) % 7 : TONIC_LETTER[tonic];
  // A root's degree (0-6) and accidental against the key: in the scale as it is, else the nearest lowered degree.
  function degreeOf(interval, scale) {
    const d = scale.indexOf(interval);
    if (d >= 0) return {degree: d, flat: ""};
    const up = scale.findIndex(s => s === (interval + 1) % 12);
    return up >= 0 ? {degree: up, flat: "♭"} : {degree: scale.findIndex(s => s === (interval + 11) % 12), flat: "♯"};
  }
  function diatonicLower(degree, scale) {                          // that degree's triad is minor or diminished
    const third = (scale[(degree + 2) % 7] - scale[degree] + 12) % 12;
    return third === 3;
  }
  function numeral(degree, lower, dim) { const r = ROMAN[degree] || "?"; return (lower ? r.toLowerCase() : r) + (dim ? "°" : ""); }
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
  function chordSuffix(root, notes) { return chordName(root, notes).slice(NOTES[root].length); }
  // Engine::ChordView: root 0-3, quality 4-6, kind 7-9, degree 10-12, shifted 13, count 14-16, counter 17-23.
  function chord(words, chordNotes) {
    const w = words.chord, count = Math.min(w >>> 14 & 7, 5), root = w & 15, kind = w >>> 7 & 7, target = w >>> 10 & 7;
    if (!count) return null;
    const notes = chordNotes.slice(0, count), h = words.harmony_lights || 0, tonic = h & 15, scale = SCALES[(h >>> 4 & 15) % 9];
    const suffix = chordSuffix(root, notes), lower = /^(m(?!aj)|dim)/.test(suffix), dim = suffix.startsWith("dim");
    const at = degreeOf((root - tonic + 12) % 12, scale), letter = (tonicLetter(tonic) + at.degree) % 7;
    // Secondary dominants name their target (V/ii); everything else its own degree, flats for borrowed roots.
    const roman = kind === 2 ? `V/${numeral(target, diatonicLower(target, scale), false)}` : kind ? at.flat + numeral(at.degree, lower, dim) : null;
    // The chord's notes on letters counted from its root (third, fifth, seventh...), so A7 reads A C♯ E G.
    const STEP = [0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6];
    const noteName = n => {                       // the octave follows the letter (B♯4 = C5, C♭5 = B4)
      const l = (letter + STEP[(n - root + 120) % 12]) % 7, diff = ((n % 12 - NATURAL[l]) % 12 + 18) % 12 - 6;
      return spell(n % 12, l) + (Math.floor((n - diff) / 12) - 1);
    };
    const spelled = spell(root, letter);
    return {name: spelled + suffix, numeral: roman, shifted: !!(w >>> 13 & 1), counter: w >>> 17 & 127, notes: notes.map(noteName)};
  }
  // PackLooper: state 0-2, overdub 3, effects before 4, has a loop 5, position 6-15 (x1023), length 16-31 (10 ms).
  function looper(words) {
    const l = words.looper;
    return {state: LOOPER[l & 7] || "?", overdub: !!(l >>> 3 & 1), fxBefore: !!(l >>> 4 & 1), hasLoop: !!(l >>> 5 & 1),
            position: (l >>> 6 & 1023) / 1023, seconds: (l >>> 16) / 100};
  }
  function recorder(words) { const s = words.sequence_lights; return {state: RECORDER[s & 7] || "?", overdub: !!(s >>> 3 & 1)}; }
  function power(p) { return {battery: BATTERY[p.battery] || "unknown", usb: !!(p.flags & 1), charging: p.chargeState >= 1 && p.chargeState <= 4, full: p.chargeState === 5}; }
  // ---- page context: what each key and control does on the page you're on (MANUAL; firmware key handlers) ----
  const noteName = n => NOTES[n % 12] + (Math.floor(n / 12) - 1);
  const NAV = {61: "next ▸", 63: "TAPE"};          // C#4 (KEY_21) and D#4 (KEY_22) on Forge's own pages
  const FX_KEYS = {48: "echo", 50: "cloud", 52: "tape echo", 53: "satur.", 55: "fold", 57: "crush", 60: "cassette", 62: "broken",
    64: "squash", 65: "chorus", 67: "phaser", 69: "tremolo", 58: "order", 59: "side by side", 54: "before", 56: "after", ...NAV};
  const SYNTH_KEYS = {48: "sine", 50: "triangle", 52: "saw", 53: "square", 55: "sine", 57: "triangle", 59: "saw", 60: "square",
    62: "unison", 64: "+5th", 65: "+oct", 67: "−oct", 69: "+4th", 71: "+oct 5th", 49: "LFO sin", 51: "LFO tri", 54: "LFO sqr",
    56: "LFO S&H", 58: "wheel", 66: "4 voices", 68: "7 voices", 70: "mono", 72: "listen", ...NAV};
  const PARTS_KEYS = {48: "arp off", 50: "up", 52: "down", 53: "up-down", 55: "as played", 57: "random", 59: "latch",
    60: "1/4", 62: "1/8", 64: "1/8t", 65: "1/16", 67: "1/16t", 69: "1/32", 71: "run / stop", 49: "no bass", 51: "root",
    54: "fifth", 56: "alt", 58: "octave", 66: "record", 68: "overdub", 70: "clear", 72: "tap tempo", ...NAV};
  const STATIC_BLACK = {49: "V/ii", 51: "V/iii", 54: "V/V", 56: "V/vi", 58: "♭VII", 61: "iv", 63: "♭III", 66: "♭VI", 68: "V/IV", 70: "♭II"};
  const WHITE = [48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72];
  function harmonyOn(words) { return !!(words.harmony_lights >>> 16 & 1); }
  // keys: {midi note: short label}; legend: [[what, does], ...]; controls: labels for the panel's other controls.
  function guide(where, words) {
    const keys = {}, legend = [];
    const h = words.harmony_lights, tonic = h & 15, real = !!(h >>> 15 & 1);
    let controls = {chompi: "toggle down + hold: menu", sw5: "cutoff · push+turn: loop speed", sw6: "volume · hold: battery",
                    play: "looper", loop: "looper"};
    if (where === "play" || where === "record") {
      if (where === "play" && harmonyOn(words) && !real) {
        WHITE.forEach((n, i) => { keys[n] = i === 14 ? "shift" : ROMAN[i % 7]; });
        Object.assign(keys, STATIC_BLACK);
        legend.push(["white keys", "chords I–VII, two octaves (C5: shift)"], ["black keys", "V/ii V/iii V/V V/vi ♭VII · iv ♭III ♭VI V/IV ♭II"],
                    ["key", `${NOTES[tonic]} ${MODES[(h >>> 4 & 15) % 9]}`]);
      } else {
        for (let n = 48; n <= 72; n++) keys[n] = noteName(n);
        legend.push(["keys", where === "play" && harmonyOn(words) ? "each key plays the chord built on it (real layout)" : "play notes"]);
      }
      if (where === "record") { controls.chompi = "hold: record (1.5 s count-in)"; legend.push(["record", "CHOMPI light = input meter; hold CHOMPI to record"]); }
      else legend.push(["SW4+SW3", "panic"], ["SW1+SW2", "harmony on / off"], ["SW1+SW3", "surprise me"], ["SW2+SW3", "mutate"], ["SW4+SW1", "Dream"]);
    } else if (where === "presets" || where === "samples") {
      WHITE.forEach((n, i) => { keys[n] = where === "samples" && i === 14 ? "recording" : `slot ${i + 1}`; });
      Object.assign(keys, {66: "erase", 68: "copy", 70: "save"});
      controls = {chompi: "release: close · press: confirm", sw4: "bank"};
      if (where === "presets") {
        Object.assign(keys, {49: "bank −", 51: "bank +", 63: "TAPE"});
        legend.push(["bank", String((words.menu >>> 4 & 7) + 1)], ["recall", "white key, then release CHOMPI"], ["save", "A#4, release CHOMPI, a white key, press CHOMPI"],
                    ["copy / erase", "G#4 / F#4, release CHOMPI, slot(s), press CHOMPI"]);
      } else {
        Object.assign(keys, {49: "chrom", 51: "kit", 54: "mic", 56: "line", 58: "resamp", 61: "before", 63: "after"});
        legend.push(["mode", (words.menu >>> 22 & 1 ? "kit" : "chromatic") + ` · bank ${"abcde"[words.menu >>> 4 & 7] || "?"}`],
                    ["white keys", "pick a sample slot (C5: the recording)"], ["F#3 G#3 A#3", "record from mic / line / resample"], ["C#4 / D#4", "effects before / after the looper · hold: harmony / presets page"],
                    ["save the recording", "A#4, release CHOMPI, a white key, press CHOMPI"]);
      }
    } else if (where === "harmony") {
      WHITE.concat([49, 51, 54, 56, 58, 66, 68, 70]).forEach(n => { keys[n] = noteName(n); });
      Object.assign(keys, NAV);
      legend.push(["any key", `sets the key's tonic (now ${NOTES[tonic]})`], ["C#4", "next page: arp · bass"], ["D#4", "back to TAPE's page"]);
      controls = {...controls, sw5: "cutoff", chompi: "release: close the menu"};
    } else if (where === "parts") {
      Object.assign(keys, PARTS_KEYS); controls.chompi = "release: close the menu";
      legend.push(["C3–A3", "arp pattern"], ["C4–A4", "arp rate"], ["C#3–A#3", "bass part"], ["F#4 G#4 A#4", "event recorder: record, overdub, clear"],
                  ["B3 / B4 / C5", "latch · run / stop · tap tempo"], ["C#4", "next page: synth"]);
    } else if (where === "synth") {
      Object.assign(keys, SYNTH_KEYS); controls.chompi = "release: close the menu";
      legend.push(["C3–F3", "oscillator 1 wave"], ["G3–C4", "oscillator 2 wave"], ["D4–B4", "oscillator 2 interval"], ["C#3–G#3", "LFO shape"],
                  ["A#3", "mod wheel → LFO"], ["F#4 G#4 A#4", "4 / 7 voices, mono with glide"], ["C5", "listen"], ["knobs", "press: page 1 / page 2"], ["C#4", "next page: FX"]);
    } else if (where === "fx") {
      Object.assign(keys, FX_KEYS); controls.chompi = "release: close the menu";
      legend.push(["C3 D3 E3", "Space: echo + room, cloud, tape echo (SW4)"], ["F3 G3 A3", "Colour: saturation, fold, crush (SW1)"],
                  ["C4 D4 E4", "Texture: cassette, broken, squash (SW2)"], ["F4 G4 A4", "Motion: chorus, phaser, tremolo (SW3)"],
                  ["A#3", "tap: next order · hold + tap modules: your own"], ["B3", "side by side"], ["F#3 / G#3", "effects before / after the looper"],
                  ["knobs", "turn: amount · hold + turn: character"], ["C#4", "next page: harmony"]);
    }
    if (words.menu & 1) Object.assign(controls, {play: "overdub feedback −", loop: "overdub feedback +"});
    return {keys, legend, controls};
  }
  // The whole picture for one state reply.
  function describe(view) {
    const w = view.words, where = page(w);
    return {page: where, pageName: PAGES[where][0], pageColour: PAGES[where][1], knobs: knobs(w, where), volume: volume(w), sw5: sw5(w),
            chord: chord(w, view.chordNotes), looper: looper(w), recorder: recorder(w), power: power(view.power),
            dream: !!(w.dream >>> 26 & 1), recording: !!(w.flags & 2), safeMode: !!(w.flags & 4), busy: !!(w.flags & 8),
            guide: guide(where, w)};
  }
  // The panel's lights (view page 0) in place: 0-9 black keys C#3..A#4, 24 down to 10 white keys C3..C5,
  // 25 CHOMPI, 26 PLAY, 27 LOOP, 28-31 the rings SW4 SW1 SW2 SW3, 32 / 33 the transport pair, 34 SW6.
  const LIGHTS = {black: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], white: [24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10],
    chompi: 25, play: 26, loop: 27, rings: [28, 29, 30, 31], reverse: 32, forward: 33, sw6: 34};
  root.DisplayModel = {describe, page, knobs, chord, chordName, looper, sw5, guide, LIGHTS, PARAMETERS, NOTES};
})(typeof window !== "undefined" ? window : globalThis);
if (typeof module !== "undefined") module.exports = globalThis.DisplayModel;
