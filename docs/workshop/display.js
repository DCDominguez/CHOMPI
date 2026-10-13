"use strict";
// Choowi display (0.20): CHOMPI's panel on a phone, live. Polls the view (opcode 0D: page 0 the lights, page 1
// the state) as fast as CHOMPI answers, and the status every 2 s (battery, firmware). display-model.js names
// things; this file draws them. A demo shows the layout without a CHOMPI.
const $ = id => document.getElementById(id);
const W = window.ForgeWire, M = window.ForgeMidi, D = window.DisplayModel, L = window.PanelArt.LIGHTS;
let running = false, wake = null;

// ---- the panel drawing: panel-art.js (shared with the illustrated manual) ----------------------------------
const P = window.PanelArt.draw($("panel")), colour = window.PanelArt.colour;
const WHITE = window.PanelArt.WHITE, BLACK = window.PanelArt.BLACK;
const drawLights = leds => P.setLights(leds);

// ---- the readout ------------------------------------------------------------------------------------------
const knobTiles = ["SW4", "SW1", "SW2", "SW3"].map(name => {
  const tile = document.createElement("div"); tile.className = "tile knob";
  tile.innerHTML = `<span class="label"><span class="nm">${name}</span><span class="pg"></span></span><b>—</b><div class="meter"><i></i></div><span class="detail hint"></span>`;
  $("knobs").append(tile);
  return {tile, title: tile.querySelector(".nm"), page: tile.querySelector(".pg"), name: tile.querySelector("b"), bar: tile.querySelector(".meter i"), hint: tile.querySelector(".hint")};
});
let lastLeds = null;
function drawState(v) {
  lastView = v;
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
  for (const n of WHITE.concat(BLACK)) P.setKeyLabel(n, v.guide.keys[n] || "");
  P.setToggle(v.page === "record");
  v.knobs.forEach((k, i) => P.setKnob(i, k.text || k.label || ""));
  drawGuide(v);
  const s5 = v.sw5, speedText = s5.speed === 1 ? "speed 1×" : s5.speed === 0 ? "stopped" : `speed ${s5.speed.toFixed(2)}×${s5.speed < 0 ? " (reverse)" : ""}`;
  $("sw5-value").textContent = s5.hz; $("sw5").style.width = `${Math.round(s5.cutoff * 100)}%`;
  $("sw5-speed").textContent = s5.turned ? "turning" : "";
  $("sw5-detail").textContent = v.looper.hasLoop ? `loop ${speedText} · push + turn` : "push + turn with a loop: speed";
  const vol = Math.round(v.volume.value * 100);
  $("vol-label").textContent = `SW6 · ${v.volume.label.toLowerCase()}`; $("vol-value").textContent = `${vol} %`; $("vol").style.width = `${vol}%`;
  P.setSw5(v.looper.hasLoop && s5.speed !== 1 ? `${s5.speed.toFixed(2)}×` : s5.hz.replace("≈", ""));
  P.setSw6(`${v.volume.label.toLowerCase()} ${vol}%`);
  $("chord-name").textContent = v.chord && v.chord.name ? v.chord.name : "—";
  $("chord-detail").textContent = v.chord ? [v.chord.numeral, v.chord.shifted ? "shift" : "", v.chord.notes.join(" ")].filter(Boolean).join(" · ") : "harmony plays chords here";
  const l = v.looper;
  $("loop-state").textContent = l.state + (l.overdub ? " · overdub" : "");
  $("loop-pos").style.width = `${Math.round(l.position * 100)}%`;
  $("loop-detail").textContent = l.hasLoop || l.seconds ? `${(l.position * l.seconds).toFixed(1)} / ${l.seconds.toFixed(1)} s · fx ${l.fxBefore ? "before" : "after"}` : "";
  $("rec-state").textContent = `event recorder: ${v.recorder.state}${v.recorder.overdub ? " · overdub" : ""}`;
  const p = v.power;
  $("battery").hidden = false;
  $("battery").textContent = `${p.charging ? "⚡ " : ""}battery ${p.battery}${p.full ? " · charged" : ""}`;
}
let guideKey = "", lastView = null;
// Tooltips (the owner, 2026-10-13): hover a key, knob, button or light on a computer, tap it on a phone. The text
// is DisplayModel.tip for the page CHOMPI is on now.
(function tooltips() {
  const svg = $("panel"), box = $("tip"); let timer = 0;
  const hide = () => { box.hidden = true; clearTimeout(timer); };
  const show = (event, sticky) => {
    const target = event.target.closest && event.target.closest("[data-tip]");
    if (!target || !lastView) { if (!sticky) hide(); return; }
    box.textContent = D.tip(target.getAttribute("data-tip"), lastView); if (!box.textContent) { hide(); return; }
    box.hidden = false;
    const card = $("panel-card").getBoundingClientRect(), w = box.offsetWidth, h = box.offsetHeight;
    let x = event.clientX - card.left - w / 2, y = event.clientY - card.top - h - 14;
    if (y < 4) y = event.clientY - card.top + 18;
    box.style.left = `${Math.max(4, Math.min(card.width - w - 4, x))}px`; box.style.top = `${Math.max(4, y)}px`;
    clearTimeout(timer); if (sticky) timer = setTimeout(hide, 5000);
  };
  svg.addEventListener("pointermove", e => { if (e.pointerType === "mouse") show(e, false); });
  svg.addEventListener("pointerleave", e => { if (e.pointerType === "mouse") hide(); });
  svg.addEventListener("pointerdown", e => { if (e.pointerType !== "mouse") show(e, true); });
  document.addEventListener("pointerdown", e => { if (!svg.contains(e.target)) hide(); });
})();
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
function show(on) { $("start").hidden = on; $("panel-card").hidden = !on; $("readout").hidden = !on; $("guide-card").hidden = !on; }
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
      chord: root | 1 << 7 | degree << 10 | notes.length << 14 | (Math.floor(t / 60) & 127) << 17, flags: 160 << 8 | 765 * 65536, dream: 0},
      chordNotes: [...notes, 0, 0, 0, 0].slice(0, 5), power: {battery: 1, flags: 1, chargeState: 3}}));
    $("status-line").textContent = "demo: made-up lights and state";
    requestAnimationFrame(frame);
  })();
});
$("full").addEventListener("click", () => {
  if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
  else document.documentElement.requestFullscreen().catch(() => {});
});
