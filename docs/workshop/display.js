"use strict";
// Choowi display (0.20): CHOMPI's panel on a phone, live. Polls the view (opcode 0D: page 0 the lights, page 1
// the state) as fast as CHOMPI answers, and the status every 2 s (battery, firmware). display-model.js names
// things; this file draws them. A demo shows the layout without a CHOMPI.
const $ = id => document.getElementById(id);
const W = window.ForgeWire, M = window.ForgeMidi, D = window.DisplayModel, L = D.LIGHTS;
const NS = "http://www.w3.org/2000/svg";
let running = false, wake = null;

// ---- the panel drawing: CHOMPI as line art (after the TAPE mini guide's panel diagram, the owner's choice,
// 2026-10-13), viewBox 130 296 1432 424. One element per light, coloured from view page 0; every key and knob
// carries its job on the current page. ---------------------------------------------------------------------
const lights = [], keyLabels = {}, knobLabels = [];
let toggleLever = null;
const WHITE = [48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72], BLACK = [49, 51, 54, 56, 58, 61, 63, 66, 68, 70];
const LINE = "#e8939b", DIM = "#5a3d47", FACE = "Fredoka, Nunito, system-ui, sans-serif";
function el(tag, attrs, parent) {
  const e = document.createElementNS(NS, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  if (parent) parent.append(e); return e;
}
(function buildPanel() {
  const svg = $("panel");
  svg.setAttribute("viewBox", "130 296 1432 424");
  const defs = el("defs", {}, svg), f = el("filter", {id: "glow", x: "-100%", y: "-100%", width: "300%", height: "300%"}, defs);
  el("feGaussianBlur", {stdDeviation: "8"}, f);
  const line = (tag, attrs) => el(tag, {fill: "none", stroke: LINE, "stroke-width": 3, ...attrs}, svg);
  const text = (x, y, s, size, colour = LINE, weight = 600) => {
    const e = el("text", {x, y, "text-anchor": "middle", fill: colour, "font-size": size, "font-family": FACE, "font-weight": weight}, svg);
    e.textContent = s; return e;
  };
  // A light: a halo (glow) under a dot or a window, both coloured from the view.
  const lamp = (index, shape, attrs) => {
    const halo = el(shape, {...attrs, fill: "#000", filter: "url(#glow)", opacity: 0}, svg);
    const dot = el(shape, {...attrs, fill: DIM, stroke: LINE, "stroke-width": 2}, svg);
    lights[index] = {kind: "dot", dot, halo, off: DIM};
  };
  line("rect", {x: 132, y: 298, width: 1428, height: 418, rx: 24});
  // The lights along the top: CHOMPI, the four knobs, SW6; the transport pair around SW5.
  lamp(L.chompi, "circle", {cx: 310, cy: 352, r: 14});
  [446, 593, 740, 887].forEach((x, k) => lamp(L.rings[k], "circle", {cx: x, cy: 352, r: 14}));
  lamp(L.sw6, "circle", {cx: 1458, cy: 352, r: 14});
  lamp(L.reverse, "circle", {cx: 1010, cy: 342, r: 9}); lamp(L.forward, "circle", {cx: 1094, cy: 342, r: 9});
  text(1052, 349, "«|»", 22);
  line("circle", {cx: 1381, cy: 352, r: 24});                         // the microphone
  [-10, 0, 10].forEach(dx => line("line", {x1: 1381 + dx, y1: 336, x2: 1381 + dx, y2: 368, "stroke-width": 4}));
  // The toggle (up: record position, down: menu position) and the CHOMPI key.
  line("rect", {x: 196, y: 384, width: 52, height: 118, rx: 10});
  line("rect", {x: 210, y: 404, width: 24, height: 78, rx: 12, "stroke-width": 2});
  toggleLever = el("circle", {cx: 222, cy: 462, r: 10, fill: LINE}, svg);
  text(222, 524, "toggle", 16);
  line("rect", {x: 262, y: 390, width: 98, height: 98, rx: 12}); line("rect", {x: 272, y: 400, width: 78, height: 78, rx: 9, "stroke-width": 2});
  text(311, 446, "CHOMPI", 17);
  // The knobs: SW4, SW1 and SW2 in their tape outline, SW3; each with its control written under it.
  line("rect", {x: 545, y: 390, width: 244, height: 104, rx: 44, "stroke-width": 2});
  ["SW4", "SW1", "SW2", "SW3"].forEach((name, k) => {
    const cx = [446, 593, 740, 887][k], cy = 440;
    line("circle", {cx, cy, r: 47}); line("circle", {cx, cy, r: 28, "stroke-width": 2});
    text(cx, cy + 6, name, 16);
    knobLabels[k] = text(cx, 514, "", 19, "#f4f2ec");
  });
  // SW5, PLAY and LOOP (their lights glow inside them), SW6.
  // SW5 (r 80, centre 1052): clear of SW3 (right edge 934) and of the PLAY / LOOP frame (left edge 1140).
  line("circle", {cx: 1052, cy: 440, r: 80}); line("circle", {cx: 1052, cy: 440, r: 64, "stroke-width": 2});
  line("circle", {cx: 1052, cy: 410, r: 22, "stroke-width": 2}); text(1052, 472, "SW5", 18);
  line("rect", {x: 1140, y: 392, width: 206, height: 106, rx: 12, "stroke-width": 2});
  [[L.play, 1153, "▶❙❙"], [L.loop, 1241, "↻"]].forEach(([index, x, symbol]) => {
    line("rect", {x, y: 403, width: 82, height: 84, rx: 10});
    lamp(index, "rect", {x: x + 21, y: 412, width: 40, height: 12, rx: 6});
    text(x + 41, 462, symbol, 24);
  });
  text(1194, 520, "PLAY", 15); text(1282, 520, "LOOP", 15);
  line("circle", {cx: 1458, cy: 444, r: 47}); line("circle", {cx: 1458, cy: 444, r: 28, "stroke-width": 2}); text(1458, 450, "SW6", 16);
  // The keys: the upper row (black keys) in groups of 2-3-2-3, the 15 white keys below, each with its light.
  const key = (note, index, x, y) => {
    line("rect", {x: x - 40, y, width: 80, height: 80, rx: 6, "stroke-width": 2});
    line("rect", {x: x - 34, y: y + 6, width: 68, height: 68, rx: 9, "stroke-width": 2});
    lamp(index, "rect", {x: x - 17, y: y + 12, width: 34, height: 12, rx: 6});
    keyLabels[note] = el("text", {x, y: y + 50, "text-anchor": "middle", fill: "#f4e6e9", "font-size": 16, "font-family": FACE, "font-weight": 600}, svg);
  };
  [265, 355, 531, 619, 707, 884, 972, 1149, 1237, 1325].forEach((x, j) => key(BLACK[j], L.black[j], x, 532));
  WHITE.forEach((note, i) => key(note, L.white[i], 224 + 88.07 * i, 622));
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
  // Page context: every key's job on this page, each knob's control under it, the page's notes below.
  for (const n of WHITE.concat(BLACK)) {
    const label = v.guide.keys[n] || "", e = keyLabels[n];
    // Up to two lines of about eight letters inside the key (split at a space when there is one).
    let lines = [label];
    if (label.length > 8) { const cut = label.lastIndexOf(" ", 9); lines = cut > 0 ? [label.slice(0, cut), label.slice(cut + 1)] : [label.slice(0, 8), label.slice(8)]; }
    lines = lines.slice(0, 2).map(s => s.length > 9 ? s.slice(0, 8) + "…" : s);
    e.replaceChildren(...lines.map((s, i) => {
      const span = document.createElementNS(NS, "tspan");
      span.setAttribute("x", e.getAttribute("x")); span.setAttribute("dy", i ? "18" : lines.length > 1 ? "-8" : "0");
      span.textContent = s; return span;
    }));
  }
  if (toggleLever) toggleLever.setAttribute("cy", v.page === "record" ? 424 : 462);
  v.knobs.forEach((k, i) => { const s = k.text || k.label || ""; knobLabels[i].textContent = s.length > 12 ? s.slice(0, 11) + "…" : s; });
  drawGuide(v);
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
let guideKey = "";
function drawGuide(v) {
  const c = v.guide.controls;
  const rows = v.guide.legend.concat([["CHOMPI", c.chompi], ["SW5", c.sw5], ["SW6", c.sw6], ["SW4", c.sw4], ["PLAY", c.play], ["LOOP", c.loop]]
    .filter(([, d]) => d));
  const key = v.page + JSON.stringify(rows);
  if (key === guideKey) return;                                   // rebuild only when the page's notes change
  guideKey = key;
  $("guide-title").textContent = `on this page · ${v.pageName}`;
  $("guide").replaceChildren(...rows.map(([what, does]) => {
    const row = document.createElement("div"); row.className = "guide-row";
    const a = document.createElement("b"); a.textContent = what; const b = document.createElement("span"); b.textContent = does;
    row.append(a, b); return row;
  }));
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
