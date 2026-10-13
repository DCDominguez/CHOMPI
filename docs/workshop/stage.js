"use strict";
// Choowi stage, pattern channel (prototype 4, 2026-10-13; the owner, after Patternflow: "something like this",
// moving away from the cartoon). A dot-matrix screen computed live every frame: a pattern (waves, interference,
// plasma) shaped by CHOMPI's four knobs (speed, scale, twist, colour), notes dropping ripples where their key
// sits, chords tinting it, the loop sweeping across as a scanline. Each effect look changes the panel. Original
// Forge code; demo plays on its own, "connect CHOMPI" listens to its notes and knob moves (TAPE's MIDI out).
const cv = document.getElementById("stage"), cx = cv.getContext("2d");
const statusEl = document.getElementById("status"), bar = document.getElementById("bar"), knobBox = document.getElementById("knobs");
const GW = 128, GH = 64;                                       // the panel: 128 x 64 dots
const grid = document.createElement("canvas"); grid.width = GW; grid.height = GH;
const gx = grid.getContext("2d"), img = gx.createImageData(GW, GH);
const prev = new Float32Array(GW * GH * 3), history = [];      // cloud's trails, tape echo's past frames
const dead = new Uint8Array(GW * GH);                          // broken: dead pixels
// CHOMPI's nine colours, and chord colours as the panel shows them.
const C = {white: [244, 242, 236], green: [47, 191, 85], yellow: [245, 196, 0], orange: [255, 138, 42], red: [236, 59, 59],
  pink: [255, 94, 160], purple: [145, 80, 240], blue: [47, 124, 246], teal: [25, 201, 182]};
const WHEEL = [C.purple, C.blue, C.teal, C.green, C.yellow, C.orange, C.red, C.pink];
let pattern = "forge", look = "clean", bpm = 90, beatAt = performance.now(), loopStart = performance.now(), loopSeconds = 60 / 90 * 16;
const knobs = [50, 60, 30, 40].map(v => v / 127);
let tint = C.purple, tintAmount = 0;
const ripples = [];
let W = 0, H = 0, dpr = 1, mask = null;
function resize() {
  dpr = Math.min(2, window.devicePixelRatio || 1); W = cv.width = innerWidth * dpr; H = cv.height = innerHeight * dpr;
  // The LED look: a mask of round holes over the scaled-up panel.
  const cell = Math.min(W / GW, H / GH); mask = {cell, ox: (W - cell * GW) / 2, oy: (H - cell * GH) / 2};
  const m = document.createElement("canvas"); m.width = Math.ceil(cell); m.height = Math.ceil(cell);
  const mc = m.getContext("2d"); mc.fillStyle = "#050507"; mc.fillRect(0, 0, m.width, m.height);
  mc.globalCompositeOperation = "destination-out"; mc.beginPath(); mc.arc(cell / 2, cell / 2, cell * .36, 0, Math.PI * 2); mc.fill();
  mask.pattern = cx.createPattern(m, "repeat");
}
addEventListener("resize", resize); resize();

// ---- the patterns (original, Choowi's own): each prepares once a frame, then gives 0..1 per dot ------------------
// The knobs nudge, the music leads (the owner: "we can use the knobs somewhat"): SW4 a little speed, SW1 density,
// SW2 spread, SW3 colour.
const field = new Float32Array(GW * GH);                       // particle patterns draw into this
const embers = [];
function splat(x, y, r, amount) {                              // a soft dot into the field (grid coordinates)
  const x0 = Math.max(0, Math.floor(x - r * 2)), x1 = Math.min(GW - 1, Math.ceil(x + r * 2));
  const y0 = Math.max(0, Math.floor(y - r * 2)), y1 = Math.min(GH - 1, Math.ceil(y + r * 2));
  for (let yy = y0; yy <= y1; yy++) for (let xx = x0; xx <= x1; xx++) {
    const d2 = ((xx - x) ** 2 + (yy - y) ** 2) / (r * r); if (d2 < 4) field[yy * GW + xx] += amount * Math.exp(-d2);
  }
}
const keyCol = note => 2 + (Math.max(48, Math.min(72, note)) - 48) / 24 * (GW - 5);   // C3 .. C5 across the panel
const columns = new Float32Array(25), rain = [];
let lastBeat = 0, reelAngle = 0;
const PATTERNS = {
  forge: {                                                     // an anvil glowing; the beat strikes, notes throw sparks
    prepare(t, k, now, beatPulse) {
      field.fill(0);
      if (beatPulse > .97 && now - lastBeat > 200) { lastBeat = now; for (let i = 0; i < 14 + k.scale * 20; i++) embers.push(ember(GW / 2 + (Math.random() - .5) * 8, GH - 14, 1.6)); }
      for (let i = embers.length - 1; i >= 0; i--) {
        const e = embers[i]; e.x += e.vx; e.y += e.vy; e.vy += .025; e.vx *= .99; e.life -= .012 / (1 + k.twist);
        if (e.life <= 0 || e.y > GH) { embers.splice(i, 1); continue; }
        splat(e.x, e.y, 1.1, e.life * 1.4);
      }
      for (let x = GW / 2 - 16; x <= GW / 2 + 16; x++) for (let y = GH - 12; y < GH - 6; y++)  // the anvil
        field[y * GW + Math.round(x)] += .35 + .25 * beatPulse;
      for (let x = GW / 2 - 6; x <= GW / 2 + 6; x++) for (let y = GH - 6; y < GH - 2; y++) field[y * GW + Math.round(x)] += .3;
    },
    value(x, y) { return Math.min(1, field[y * GW + x]); },
    note(note, velocity) { for (let i = 0; i < 4 + velocity / 20; i++) embers.push(ember(keyCol(note), GH - 16, 1)); },
  },
  reels: {                                                     // two tape reels turning with the loop, the tape between
    prepare(t, k) { reelAngle += (.02 + k.speed * .06) * loopSpeed; },
    value(x, y, u, v, t, k) {
      let out = 0;
      for (const cxr of [GW * .28, GW * .72]) {
        const dx = x - cxr, dy = y - GH * .45, r = Math.hypot(dx, dy), a = Math.atan2(dy, dx) - reelAngle * (cxr < GW / 2 ? 1 : 1.12);
        const R = GH * (.3 + k.scale * .08);
        if (Math.abs(r - R) < 1.6) out = Math.max(out, .9);                                  // rim
        if (r < R * .25) out = Math.max(out, .7);                                           // hub
        if (r < R && r > R * .25 && Math.abs(Math.sin(a * 3)) < .12 + k.twist * .15) out = Math.max(out, .55);   // spokes
        if (r < R * .82 && r > R * .3) out = Math.max(out, .12);                            // wound tape
      }
      const tapeY = GH * .45 + GH * (.3 + k.scale * .08);                                   // the tape ribbon below, moving
      if (Math.abs(y - tapeY) < 1.2 && x > GW * .28 && x < GW * .72) out = Math.max(out, .35 + .45 * (Math.sin(x * .6 - reelAngle * 12) > .6 ? 1 : 0));
      return out;
    },
    note(note) { for (let i = 0; i < 3; i++) embers.push(ember(keyCol(note), GH * .85, .5)); },
  },
  keyfall: {                                                   // the keys as columns: held notes rise, released ones rain
    prepare(t, k) {
      field.fill(0);
      for (let c = 0; c < 25; c++) {
        const target = held.has(48 + c) ? (held.get(48 + c) / 127) * (.6 + .4 * k.scale) : 0;
        if (columns[c] > .05 && target === 0 && Math.random() < .3) rain.push({c, y: GH * (1 - columns[c]), v: .2});
        columns[c] += (target - columns[c]) * (target > columns[c] ? .35 : .08);
      }
      for (let i = rain.length - 1; i >= 0; i--) { const r = rain[i]; r.y += r.v; r.v += .04 * (1 + k.speed); if (r.y > GH) rain.splice(i, 1); else splat(keyCol(48 + r.c) + Math.sin(r.y) * k.twist * 3, r.y, .8, .9); }
    },
    value(x, y) {
      const c = Math.round((x - 2) / (GW - 5) * 24), col = c >= 0 && c < 25 ? columns[c] : 0;
      const inCol = Math.abs(x - keyCol(48 + c)) < 1.6, top = GH * (1 - col);
      return Math.min(1, field[y * GW + x] + (inCol && y >= top ? .45 + .55 * (1 - (y - top) / GH) : 0) + (inCol && y === GH - 1 ? .15 : 0));
    },
    note() {},
  },
  tide: {                                                      // slow drifting bands of light, for Dream
    prepare() {},
    value(x, y, u, v, t, k) {
      const s = .15 + k.speed * .4, f = 1.5 + k.scale * 3;
      const band = Math.sin(v * f * 3 + Math.sin(u * 2 + t * s) * (1 + k.twist * 2) + t * s * 1.3);
      const band2 = Math.sin(v * f * 5 - Math.cos(u * 3 - t * s * .7) * 1.5 - t * s);
      return Math.max(0, .5 * band + .35 * band2) * .9;
    },
    note() {},
  },
};
function ember(x, y, power) {
  return {x, y, vx: (Math.random() - .5) * 1.4 * power, vy: -(.6 + Math.random() * 1.2) * power, life: .7 + Math.random() * .3};
}
let loopSpeed = 1;
function palette(value, colourKnob, out) {                      // the colour knob walks the wheel; the chord tints it
  const pos = (colourKnob * WHEEL.length + value * 2.2) % WHEEL.length, i = Math.floor(pos), f = pos - i;
  const a = WHEEL[i], b = WHEEL[(i + 1) % WHEEL.length], br = Math.pow(value, 1.6);
  for (let c = 0; c < 3; c++) {
    const base = a[c] + (b[c] - a[c]) * f;
    out[c] = (base + (tint[c] - base) * tintAmount) * br;
  }
}

// ---- what's playing ------------------------------------------------------------------------------------------
const held = new Map();
const CHORD_COLOUR = notes => {
  if (notes.length < 3) return null;
  const s = [...notes].sort((a, b) => a - b), root = s[0] % 12, pcs = new Set(s.map(n => (n - root + 120) % 12));
  if (pcs.has(4) && pcs.has(8) && !pcs.has(7)) return C.pink;
  if (pcs.has(3) && pcs.has(6) && !pcs.has(7)) return C.red;
  if (pcs.has(5) && !pcs.has(3) && !pcs.has(4)) return C.teal;
  return pcs.has(3) && !pcs.has(4) ? C.blue : C.orange;
};
function noteOn(note, velocity) {
  held.set(note, velocity);
  const low = [...held.keys()].filter(n => n < 60), c = CHORD_COLOUR(low);
  if (c && note < 60 && low.length >= 3) { tint = c; tintAmount = .55; }
  const x = -1 + 2 * Math.max(0, Math.min(1, (note - 48) / 24));            // C3 at the left edge, C5 at the right
  ripples.push({x: x * 2, y: note < 60 ? .55 : -.35, born: performance.now(), amp: .12 + velocity / 500});
  PATTERNS[pattern].note(note, velocity);
}
function noteOff(note) { held.delete(note); }

// ---- one frame -----------------------------------------------------------------------------------------------
const rgb = [0, 0, 0];
let frozenUntil = 0, glitchRow = -1;
function frame(now) {
  requestAnimationFrame(frame);
  if (look === "broken" && now < frozenUntil) return;
  if (look === "broken" && Math.random() < .01) frozenUntil = now + 80 + Math.random() * 180;
  const t = now / 1000, k = {speed: knobs[0], scale: knobs[1], twist: knobs[2], colour: knobs[3]};
  const beat = Math.pow(1 - ((now - beatAt) % (60000 / bpm)) / (60000 / bpm), 3);
  const scan = ((now - loopStart) / 1000 % loopSeconds) / loopSeconds;
  tintAmount *= .995;
  for (let i = ripples.length - 1; i >= 0; i--) if (now - ripples[i].born > 2600) ripples.splice(i, 1);
  const P = PATTERNS[pattern]; P.prepare(t, k, now, beat);
  const d = img.data, persist = look === "cloud" ? .9 : 0, pulse = look === "tremolo" ? .35 + .65 * beat : 1;
  if (look === "broken" && Math.random() < .02) { dead.fill(0); for (let i = 0; i < 90; i++) dead[Math.floor(Math.random() * dead.length)] = 1; glitchRow = Math.floor(Math.random() * GH); }
  for (let y = 0; y < GH; y++) {
    const v = (y / (GH - 1)) * 2 - 1, shift = look === "broken" && Math.abs(y - glitchRow) < 3 ? 9 : 0;
    for (let x = 0; x < GW; x++) {
      const u = (((x + shift) % GW) / (GW - 1)) * 2 - 1;
      let val = P.value(x, y, u, v, t, k);
      for (const r of ripples) {                                 // each note: a ring spreading from its key
        const age = (now - r.born) / 1000, dist = Math.hypot(u * 2 - r.x, v - r.y), front = age * 1.6;
        val += r.amp * Math.exp(-Math.pow((dist - front) * 6, 2)) * (1 - age / 2.6);
      }
      if (Math.abs(u - (scan * 2 - 1)) < .012) val = Math.max(val, .95);   // the loop's scanline
      val = Math.max(0, Math.min(1, val)) * pulse;
      palette(val, k.colour, rgb);
      const i = y * GW + x, j = i * 3;
      for (let c = 0; c < 3; c++) { const out = Math.max(rgb[c], prev[j + c] * persist); prev[j + c] = out; d[i * 4 + c] = dead[i] && look === "broken" ? 0 : out; }
      d[i * 4 + 3] = 255;
    }
  }
  gx.putImageData(img, 0, 0);
  // Draw the panel scaled up (crisp), then the LED holes over it.
  const {cell, ox, oy} = mask;
  cx.fillStyle = "#050507"; cx.fillRect(0, 0, W, H); cx.imageSmoothingEnabled = false;
  if (look === "tape") {                                         // tape echo: older frames, lagging and wobbling
    history.push(gx.getImageData(0, 0, GW, GH)); if (history.length > 24) history.shift();
    [[23, .35], [12, .5]].forEach(([back, alpha]) => {
      const old = history[history.length - 1 - back]; if (!old) return;
      const tmp = document.createElement("canvas"); tmp.width = GW; tmp.height = GH; tmp.getContext("2d").putImageData(old, 0, 0);
      cx.globalAlpha = alpha; cx.drawImage(tmp, ox + Math.sin(t * 3 + back) * cell * 2, oy, cell * GW, cell * GH);
    });
    cx.globalAlpha = 1;
  } else history.length = 0;
  cx.globalCompositeOperation = "lighter";
  cx.drawImage(grid, ox, oy, cell * GW, cell * GH);
  if (look === "chorus") { cx.globalAlpha = .55; cx.drawImage(grid, ox + Math.sin(t * 1.3) * cell * 3, oy + Math.cos(t) * cell, cell * GW, cell * GH); cx.globalAlpha = 1; }
  cx.globalCompositeOperation = "source-over";
  cx.save(); cx.translate(ox, oy); cx.fillStyle = mask.pattern; cx.fillRect(0, 0, cell * GW, cell * GH); cx.restore();
  // A soft bloom so the dots glow.
  cx.globalCompositeOperation = "lighter"; cx.globalAlpha = .35; cx.filter = `blur(${cell * .9}px)`;
  cx.drawImage(grid, ox, oy, cell * GW, cell * GH); cx.filter = "none"; cx.globalAlpha = 1; cx.globalCompositeOperation = "source-over";
}
requestAnimationFrame(frame);

// ---- knobs: sliders in the demo, CHOMPI's knobs when connected ----------------------------------------------------
const sliders = [...document.querySelectorAll("[data-knob]")];
sliders.forEach(s => s.addEventListener("input", () => { knobs[Number(s.dataset.knob)] = s.value / 127; }));
function setKnob(i, value) { knobs[i] = value / 127; sliders[i].value = value; }

// ---- demo: I - vi - IV - V in C at 90 BPM, a melody on top, the knobs drifting -----------------------------------
const CHORDS = [[48, 52, 55], [45, 48, 52], [41, 45, 48], [43, 47, 50]], SCALE = [60, 62, 64, 65, 67, 69, 71, 72];
let demoOn = true, demoTimer = 0, step = 0;
function demoStep() {
  if (!demoOn) return;
  if (step % 8 === 0) { [...held.keys()].forEach(noteOff); CHORDS[Math.floor(step / 8) % 4].forEach(n => noteOn(n < 48 ? n + 12 : n, 80)); if (step % 32 === 0) loopStart = performance.now(); }
  if (step % 2 === 0) beatAt = performance.now();
  if (Math.random() < .45) { const n = SCALE[Math.floor(Math.random() * SCALE.length)]; noteOn(n, 60 + Math.random() * 50); setTimeout(() => noteOff(n), 220); }
  if (step % 16 === 8) setKnob(2, Math.round(20 + 60 * Math.random()));   // the twist wanders, as a player would
  step++; demoTimer = setTimeout(demoStep, 60000 / bpm / 2);
}
demoStep();

// ---- live: CHOMPI's notes and knob moves (CC 20-23 for SW4 SW1 SW2 SW3 on their first page) ----------------------
const connectButton = document.getElementById("connect");
connectButton.addEventListener("click", async () => {
  try {
    if (!navigator.requestMIDIAccess) throw new Error("no Web MIDI here (Chrome or Edge)");
    const access = await navigator.requestMIDIAccess();
    const inputs = [...access.inputs.values()], chompi = inputs.find(i => /chompi/i.test(i.name)) || inputs[0];
    if (!chompi) throw new Error("no MIDI input found: plug CHOMPI in");
    demoOn = false; clearTimeout(demoTimer); [...held.keys()].forEach(noteOff);
    document.getElementById("demo").setAttribute("aria-pressed", "false");
    chompi.onmidimessage = e => {
      const [s, a, b] = e.data, kind = s & 0xf0;
      if (kind === 0x90 && b > 0) noteOn(a, b); else if (kind === 0x80 || (kind === 0x90 && b === 0)) noteOff(a);
      else if (kind === 0xb0 && a >= 20 && a <= 23) setKnob(a - 20, b);
    };
    statusEl.textContent = `listening to ${chompi.name}: play, and turn SW4 SW1 SW2 SW3`;
  } catch (error) { statusEl.textContent = error.message; }
});
document.getElementById("demo").addEventListener("click", e => {
  demoOn = !demoOn; e.target.setAttribute("aria-pressed", String(demoOn));
  if (demoOn) demoStep(); else { clearTimeout(demoTimer); [...held.keys()].forEach(noteOff); }
});
const pick = (attr, set) => document.querySelectorAll(`[data-${attr}]`).forEach(b => b.addEventListener("click", () => {
  set(b.dataset[attr]); document.querySelectorAll(`[data-${attr}]`).forEach(x => x.setAttribute("aria-pressed", String(x === b)));
}));
pick("pattern", v => { pattern = v; }); pick("look", v => { look = v; prev.fill(0); dead.fill(0); });
document.getElementById("full").addEventListener("click", () => document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen().catch(() => {}));
// Opened from Choowivision (?live=1): connect straight away (MIDI was already allowed there; the button stays as a fallback).
if (new URLSearchParams(location.search).has("live")) connectButton.click();
let idle = 0;
function wake() { bar.classList.remove("hidden"); knobBox.classList.remove("hidden"); clearTimeout(idle); idle = setTimeout(() => { bar.classList.add("hidden"); knobBox.classList.add("hidden"); }, 4000); }
["pointermove", "pointerdown", "keydown"].forEach(e => addEventListener(e, wake)); wake();
