"use strict";
// Choowi display (0.20): CHOMPI's panel on a phone, live. Polls the view (opcode 0D: page 0 the lights, page 1
// the state) as fast as CHOMPI answers, and the status every 2 s (battery, firmware). display-model.js names
// things; this file draws them. A demo shows the layout without a CHOMPI.
const $ = id => document.getElementById(id);
const W = window.ForgeWire, M = window.ForgeMidi, D = window.DisplayModel, L = D.LIGHTS;
const NS = "http://www.w3.org/2000/svg";
let running = false, wake = null;

// ---- the panel drawing (viewBox 1200 x 470): one element per light, coloured from view page 0 -------------
const lights = [];
function el(tag, attrs, parent) {
  const e = document.createElementNS(NS, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  if (parent) parent.append(e); return e;
}
(function buildPanel() {
  const svg = $("panel");
  const defs = el("defs", {}, svg), f = el("filter", {id: "glow", x: "-80%", y: "-80%", width: "260%", height: "260%"}, defs);
  el("feGaussianBlur", {stdDeviation: "7"}, f);
  el("rect", {x: 6, y: 6, width: 1188, height: 458, rx: 26, fill: "#2b1f27", stroke: "#4a3341", "stroke-width": 2}, svg);
  const light = (index, shape) => { lights[index] = shape; };
  const glowDot = (index, cx, cy, r) => {
    const g = el("g", {}, svg);
    const halo = el("circle", {cx, cy, r: r * 1.9, fill: "#000", filter: "url(#glow)", opacity: 0}, g);
    const dot = el("circle", {cx, cy, r, fill: "#111"}, g);
    light(index, {kind: "dot", dot, halo}); return g;
  };
  const text = (x, y, s, size = 22) => { const t = el("text", {x, y, "text-anchor": "middle", fill: "#b9a9b3", "font-size": size, "font-family": "Space Mono, monospace"}, svg); t.textContent = s; };
  // CHOMPI key (its light glows through it).
  el("rect", {x: 60, y: 46, width: 118, height: 118, rx: 16, fill: "#3a2a31", stroke: "#5a4250", "stroke-width": 2}, svg);
  glowDot(L.chompi, 119, 105, 26); text(119, 192, "CHOMPI", 20);
  // The four knobs and their rings: SW4 SW1 SW2 SW3, left to right.
  ["SW4", "SW1", "SW2", "SW3"].forEach((name, k) => {
    const cx = 290 + 125 * k, cy = 100;
    const halo = el("circle", {cx, cy, r: 50, fill: "none", stroke: "#000", "stroke-width": 18, filter: "url(#glow)", opacity: 0}, svg);
    const ring = el("circle", {cx, cy, r: 44, fill: "none", stroke: "#222", "stroke-width": 9}, svg);
    el("circle", {cx, cy, r: 31, fill: "#d9d6cf"}, svg);
    light(L.rings[k], {kind: "ring", ring, halo}); text(cx, 172, name);
  });
  // SW5, the transport pair above it, PLAY and LOOP.
  el("circle", {cx: 838, cy: 104, r: 58, fill: "#4c3f9e", stroke: "#2c2560", "stroke-width": 4}, svg); text(838, 186, "SW5");
  glowDot(L.reverse, 800, 28, 9); glowDot(L.forward, 876, 28, 9);
  const button = (index, x, colour, label) => {
    el("rect", {x, y: 66, width: 78, height: 78, rx: 12, fill: colour, opacity: .55}, svg);
    glowDot(index, x + 39, 105, 16); text(x + 39, 172, label, 19);
  };
  button(L.play, 918, "#5aa6d6", "PLAY"); button(L.loop, 1008, "#e7c02c", "LOOP");
  // SW6 and its ring.
  {
    const cx = 1138, cy = 100;
    const halo = el("circle", {cx, cy, r: 36, fill: "none", stroke: "#000", "stroke-width": 14, filter: "url(#glow)", opacity: 0}, svg);
    const ring = el("circle", {cx, cy, r: 31, fill: "none", stroke: "#222", "stroke-width": 7}, svg);
    el("circle", {cx, cy, r: 21, fill: "#d9d6cf"}, svg); light(L.sw6, {kind: "ring", ring, halo}); text(cx, 162, "SW6", 19);
  }
  // 25 keys C3..C5: 15 white, 10 black between them, each with its light.
  const kw = 1120 / 15, x0 = 40;
  for (let i = 0; i < 15; i++) {
    const x = x0 + i * kw;
    el("rect", {x: x + 3, y: 282, width: kw - 6, height: 160, rx: 10, fill: "#ece9e2"}, svg);
    const halo = el("rect", {x: x + kw / 2 - 20, y: 290, width: 40, height: 22, rx: 8, fill: "#000", filter: "url(#glow)", opacity: 0}, svg);
    const dot = el("rect", {x: x + kw / 2 - 15, y: 294, width: 30, height: 14, rx: 6, fill: "#bdb9b0"}, svg);
    light(L.white[i], {kind: "dot", dot, halo, off: "#bdb9b0"});
    if (i % 7 === 0) text(x + kw / 2, 430, `C${3 + i / 7}`, 16);
  }
  [0, 1, 3, 4, 5, 7, 8, 10, 11, 12].forEach((after, j) => {
    const x = x0 + (after + 1) * kw;
    el("rect", {x: x - 27, y: 208, width: 54, height: 62, rx: 8, fill: "#151215"}, svg);
    const halo = el("rect", {x: x - 18, y: 216, width: 36, height: 18, rx: 7, fill: "#000", filter: "url(#glow)", opacity: 0}, svg);
    const dot = el("rect", {x: x - 13, y: 220, width: 26, height: 11, rx: 5, fill: "#2a2529"}, svg);
    light(L.black[j], {kind: "dot", dot, halo, off: "#2a2529"});
  });
})();
// A 7-bit LED colour as the eye sees it: hue kept, dim lights lifted a little so they still read.
function colour([r, g, b]) {
  const m = Math.max(r, g, b);
  if (m < 2) return null;
  const level = m / 127, lift = 255 * Math.pow(level, .55) / m;
  return {css: `rgb(${Math.round(r * lift)},${Math.round(g * lift)},${Math.round(b * lift)})`, level};
}
function drawLights(leds) {
  leds.forEach((rgb, i) => {
    const s = lights[i]; if (!s) return;
    const c = colour(rgb), target = s.kind === "ring" ? s.ring : s.dot;
    target.setAttribute(s.kind === "ring" ? "stroke" : "fill", c ? c.css : s.off || (s.kind === "ring" ? "#222" : "#111"));
    s.halo.setAttribute(s.kind === "ring" ? "stroke" : "fill", c ? c.css : "#000");
    s.halo.setAttribute("opacity", c ? (.25 + .6 * c.level).toFixed(2) : 0);
  });
}

// ---- the readout ------------------------------------------------------------------------------------------
const knobTiles = ["SW4", "SW1", "SW2", "SW3"].map(name => {
  const tile = document.createElement("div"); tile.className = "tile knob";
  tile.innerHTML = `<span class="label"><span class="nm">${name}</span><span class="pg"></span></span><b>—</b><div class="meter"><i></i></div><span class="detail hint"></span>`;
  $("knobs").append(tile);
  return {tile, title: tile.querySelector(".nm"), page: tile.querySelector(".pg"), name: tile.querySelector("b"), bar: tile.querySelector(".meter i"), hint: tile.querySelector(".hint")};
});
let lastLeds = null;
function drawState(v) {
  $("where-name").textContent = v.dream ? "Dream" : v.pageName;
  $("where-dot").style.background = v.pageColour; $("where-dot").style.color = v.pageColour;
  v.knobs.forEach((k, i) => {
    const t = knobTiles[i];
    // A page's own setting: its name in the small row, its value big; a play knob: the control it moves big.
    t.title.textContent = k.text ? `${k.name} · ${k.label}` : k.name; t.name.textContent = k.text || k.label;
    t.page.textContent = k.page ? `page ${k.page}${k.patchPage ? " · sound's" : ""}` : "";
    t.hint.textContent = k.hint || ""; t.hint.hidden = !k.hint;
    t.bar.style.width = `${Math.round(k.value * 100)}%`;
    const ring = lastLeds && colour(lastLeds[L.rings[i]]);
    t.tile.style.setProperty("--ring", ring ? ring.css : "#888");
  });
  $("vol-label").textContent = v.volume.label; $("vol").style.width = `${Math.round(v.volume.value * 100)}%`;
  $("chord-name").textContent = v.chord && v.chord.name ? v.chord.name : "—";
  $("chord-detail").textContent = v.chord ? [v.chord.numeral, v.chord.shifted ? "shift" : "", v.chord.notes.join(" ")].filter(Boolean).join(" · ") : "harmony plays chords here";
  const l = v.looper;
  $("loop-state").textContent = l.state + (l.overdub ? " · overdub" : "");
  $("loop-pos").style.width = `${Math.round(l.position * 100)}%`;
  $("loop-detail").textContent = l.hasLoop || l.seconds ? `${(l.position * l.seconds).toFixed(1)} / ${l.seconds.toFixed(1)} s · fx ${l.fxBefore ? "before" : "after"}` : "";
  $("rec-state").textContent = v.recorder.state + (v.recorder.overdub ? " · overdub" : "");
  const p = v.power;
  $("battery").hidden = false;
  $("battery").textContent = `${p.charging ? "⚡ " : ""}battery ${p.battery}${p.full ? " · charged" : ""}`;
}
function show(on) { $("start").hidden = on; $("panel-card").hidden = !on; $("readout").hidden = !on; }
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function keepAwake() {
  try { if (navigator.wakeLock && document.visibilityState === "visible") wake = await navigator.wakeLock.request("screen"); } catch { /* not allowed: fine */ }
}
document.addEventListener("visibilitychange", () => { if (running && document.visibilityState === "visible") keepAwake(); });

// ---- live: poll CHOMPI --------------------------------------------------------------------------------------
async function live() {
  running = true; show(true); keepAwake();
  let lastStatus = 0, misses = 0;
  while (running) {
    try {
      const lightsReply = await M.exchange(s => W.viewMessage(s, 0));
      lastLeds = lightsReply.leds; drawLights(lastLeds);
      drawState(D.describe(await M.exchange(s => W.viewMessage(s, 1))));
      if (Date.now() - lastStatus > 2000) {
        lastStatus = Date.now();
        const s = await M.exchange(W.statusMessage);
        $("status-line").textContent = `Forge ${s.firmware}${s.build ? ` · build ${s.build}` : ""}${s.safeMode ? " · SAFE MODE" : ""} · ${M.portName()}`;
      }
      misses = 0; await sleep(20);
    } catch (error) {
      if (/unknown operation/.test(error.message)) {
        running = false; show(false); $("notice").textContent = "This CHOMPI's firmware has no display yet: it needs Choowi 0.20 or newer."; return;
      }
      if (++misses > 5) $("where-name").textContent = "CHOMPI isn't answering…";
      await sleep(400);
    }
  }
}
$("connect").addEventListener("click", async () => {
  $("notice").textContent = "";
  try { await M.connect(ok => { if (!ok && running) $("where-name").textContent = "CHOMPI unplugged"; }); live(); }
  catch (error) { $("notice").textContent = error.message; }
});

// ---- demo: the layout without a CHOMPI (made-up state, the real drawing code) ----------------------------
$("demo").addEventListener("click", () => {
  running = true; show(true); keepAwake();
  const chords = [[9, [57, 60, 64, 67], 5, 1], [5, [53, 57, 60, 64], 3, 1], [0, [48, 52, 55], 0, 1], [7, [55, 59, 62, 65], 4, 1]];
  const pages = [["play", 0, 0], ["harmony", 1 | 7 << 1, 0], ["fx", 1 | 5 << 1, 2 ** 31]];
  let t = 0;
  (function frame() {
    if (!running) return;
    t += 1;
    const [pageName, menu, fxBit] = pages[Math.floor(t / 180) % pages.length];
    const [root, notes, degree] = chords[Math.floor(t / 60) % chords.length];
    const pcs = new Set(notes.map(n => n % 12)), keyNotes = [49, 51, 54, 56, 58, 61, 63, 66, 68, 70];
    const whiteNotes = [48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72];
    const leds = Array.from({length: 35}, () => [0, 0, 0]);
    whiteNotes.forEach((n, i) => { leds[L.white[i]] = pcs.has(n % 12) ? [127, 70, 20] : [12, 6, 2]; });
    keyNotes.forEach((n, j) => { leds[L.black[j]] = pcs.has(n % 12) ? [30, 50, 127] : [3, 4, 10]; });
    leds[L.chompi] = pageName === "fx" ? [30, 127, 40] : pageName === "harmony" ? [90, 40, 127] : [110, 110, 110];
    const wave = k => .5 + .5 * Math.sin(t / 25 + k);
    [[40, 127, 50], [127, 120, 10], [127, 20, 15], [10, 120, 110]].forEach((c, k) => { leds[L.rings[k]] = c.map(x => Math.round(x * (.2 + .8 * wave(k)))); });
    const pos = (t % 240) / 240;
    leds[L.play] = [0, Math.round(90 * (1 - pos)), Math.round(76 * (1 - pos))]; leds[L.loop] = [127, 127, 127].map(x => Math.round(x * pos));
    leds[L.sw6] = [60, 60, 60];
    lastLeds = leds; drawLights(leds);
    const knobBytes = [0, 1, 2, 3].reduce((w, k) => w + Math.round(255 * wave(k)) * 2 ** (8 * k), 0);
    drawState(D.describe({words: {menu, looper: 3 | 32 | Math.round(pos * 1023) << 6 | 800 * 65536, knob_state: 1 << 3 | 200 * 2 ** 24,
      knob_values: knobBytes, knob_targets: 0 | 1 << 8 | 2 << 16 | 5 * 2 ** 24, fx_lights: fxBit, fx_knobs: knobBytes, synth_lights: 0, synth_knobs: 0,
      harmony_lights: 9 | 1 << 8 | 1 << 16, parts_lights: 1 | 3 << 3 | 1 << 6 | 9 << 8 | 2 << 17, parts_clock: 1 | 120 << 8, sequence_lights: 0,
      chord: root | 1 << 7 | degree << 10 | notes.length << 14 | (Math.floor(t / 60) & 127) << 17, flags: 0, dream: 0},
      chordNotes: [...notes, 0, 0, 0, 0].slice(0, 5), power: {battery: 1, flags: 1, chargeState: 3}}));
    $("status-line").textContent = "demo: made-up lights and state";
    requestAnimationFrame(frame);
  })();
});
$("full").addEventListener("click", () => {
  if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
  else document.documentElement.requestFullscreen().catch(() => {});
});
