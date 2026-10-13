"use strict";
// CHOMPI's panel as line art (the owner's choice, 2026-10-13, after the TAPE mini guide's panel diagram): the
// phone display draws it live and the illustrated manual draws its page figures with the same code (host/
// build_site.py copies this file into docs/forge/ILLUSTRATED_MANUAL.html, which must stand alone on the card;
// tests/test_site.py fails if that copy is stale). No DOM lookups: everything goes through the returned panel.
(function (root) {
  const NS = "http://www.w3.org/2000/svg";
  const WHITE = [48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72], BLACK = [49, 51, 54, 56, 58, 61, 63, 66, 68, 70];
  const LINE = "#e8939b", DIM = "#5a3d47", FACE = "Fredoka, Nunito, system-ui, sans-serif";
  // The view's light order (view page 0): 0-9 black keys C#3..A#4, 24 down to 10 white keys C3..C5, 25 CHOMPI,
  // 26 PLAY, 27 LOOP, 28-31 the knob lights SW4 SW1 SW2 SW3, 32 / 33 the transport pair, 34 SW6.
  const LIGHTS = {black: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], white: [24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10],
    chompi: 25, play: 26, loop: 27, rings: [28, 29, 30, 31], reverse: 32, forward: 33, sw6: 34};
  let panels = 0;
  function el(tag, attrs, parent) {
    const e = document.createElementNS(NS, tag);
    for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    if (parent) parent.append(e); return e;
  }
  // A 7-bit LED colour as the eye sees it: hue kept, dim lights lifted a little so they still read.
  function colour([r, g, b]) {
    const m = Math.max(r, g, b);
    if (m < 2) return null;
    const level = m / 127, lift = 255 * Math.pow(level, .55) / m;
    return {css: `rgb(${Math.round(r * lift)},${Math.round(g * lift)},${Math.round(b * lift)})`, level};
  }
  // Draws the panel into `svg` (viewBox 130 296 1432 424) and returns the handles to light and label it.
  function draw(svg) {
    const L = LIGHTS, lights = [], keyLabels = {}, knobLabels = [], glow = `panel-glow-${++panels}`;
    svg.setAttribute("viewBox", "130 296 1432 424");
    const defs = el("defs", {}, svg), f = el("filter", {id: glow, x: "-100%", y: "-100%", width: "300%", height: "300%"}, defs);
    el("feGaussianBlur", {stdDeviation: "8"}, f);
    const line = (tag, attrs) => el(tag, {fill: "none", stroke: LINE, "stroke-width": 3, ...attrs}, svg);
    const text = (x, y, s, size, colour = LINE, weight = 600) => {
      const e = el("text", {x, y, "text-anchor": "middle", fill: colour, "font-size": size, "font-family": FACE, "font-weight": weight}, svg);
      e.textContent = s; return e;
    };
    // A light: a halo (glow) under a dot or a window, both coloured from the view.
    const lamp = (index, shape, attrs) => {
      const halo = el(shape, {...attrs, fill: "#000", filter: `url(#${glow})`, opacity: 0}, svg);
      const dot = el(shape, {...attrs, fill: DIM, stroke: LINE, "stroke-width": 2}, svg);
      lights[index] = {dot, halo};
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
    const lever = el("circle", {cx: 222, cy: 462, r: 10, fill: LINE}, svg);
    text(222, 524, "toggle", 16);
    line("rect", {x: 262, y: 390, width: 98, height: 98, rx: 12}); line("rect", {x: 272, y: 400, width: 78, height: 78, rx: 9, "stroke-width": 2});
    text(311, 446, "CHOMPI", 17);
    // The knobs: SW4, SW1 and SW2 in their tape outline (r 42, about 13 units clear all round), SW3; each with
    // its control written under it.
    line("rect", {x: 538, y: 385, width: 257, height: 110, rx: 55, "stroke-width": 2});
    ["SW4", "SW1", "SW2", "SW3"].forEach((name, k) => {
      const cx = [446, 593, 740, 887][k], cy = 440;
      line("circle", {cx, cy, r: 42}); line("circle", {cx, cy, r: 25, "stroke-width": 2});
      text(cx, cy + 6, name, 16);
      knobLabels[k] = text(cx, 514, "", 19, "#f4f2ec");
    });
    // SW5 (r 80, centre 1052: clear of SW3 and of the PLAY / LOOP frame), PLAY and LOOP (lights inside), SW6.
    line("circle", {cx: 1052, cy: 440, r: 80}); line("circle", {cx: 1052, cy: 440, r: 64, "stroke-width": 2});
    line("circle", {cx: 1052, cy: 410, r: 22, "stroke-width": 2}); text(1052, 458, "SW5", 16);
    const sw5 = text(1052, 482, "", 18, "#f4f2ec");
    line("rect", {x: 1140, y: 392, width: 206, height: 106, rx: 12, "stroke-width": 2});
    [[L.play, 1153, "▶❙❙"], [L.loop, 1241, "↻"]].forEach(([index, x, symbol]) => {
      line("rect", {x, y: 403, width: 82, height: 84, rx: 10});
      lamp(index, "rect", {x: x + 21, y: 412, width: 40, height: 12, rx: 6});
      text(x + 41, 462, symbol, 24);
    });
    text(1194, 520, "PLAY", 15); text(1282, 520, "LOOP", 15);
    line("circle", {cx: 1458, cy: 444, r: 42}); line("circle", {cx: 1458, cy: 444, r: 25, "stroke-width": 2}); text(1458, 450, "SW6", 16);
    const sw6 = text(1458, 514, "", 19, "#f4f2ec");
    // The keys: the upper row (black keys) in groups of 2-3-2-3, the 15 white keys below, each with its light.
    const key = (note, index, x, y) => {
      line("rect", {x: x - 40, y, width: 80, height: 80, rx: 6, "stroke-width": 2});
      line("rect", {x: x - 34, y: y + 6, width: 68, height: 68, rx: 9, "stroke-width": 2});
      lamp(index, "rect", {x: x - 17, y: y + 12, width: 34, height: 12, rx: 6});
      keyLabels[note] = el("text", {x, y: y + 50, "text-anchor": "middle", fill: "#f4e6e9", "font-size": 16, "font-family": FACE, "font-weight": 600}, svg);
    };
    [265, 355, 531, 619, 707, 884, 972, 1149, 1237, 1325].forEach((x, j) => key(BLACK[j], L.black[j], x, 532));
    WHITE.forEach((note, i) => key(note, L.white[i], 224 + 88.07 * i, 622));

    // Touch / hover targets on top (tooltips): invisible shapes tagged data-tip, read by DisplayModel.tip.
    const hits = el("g", {fill: "transparent", style: "cursor:help"}, svg);
    const hit = (tag, attrs, id) => el(tag, {...attrs, "data-tip": id, "pointer-events": "all"}, hits);
    [265, 355, 531, 619, 707, 884, 972, 1149, 1237, 1325].forEach((x, j) => hit("rect", {x: x - 40, y: 532, width: 80, height: 80}, `key:${BLACK[j]}`));
    WHITE.forEach((note, i) => hit("rect", {x: 224 + 88.07 * i - 40, y: 622, width: 80, height: 80}, `key:${note}`));
    [446, 593, 740, 887].forEach((cx, k) => { hit("circle", {cx, cy: 440, r: 46}, `knob:${k}`); hit("circle", {cx, cy: 352, r: 20}, "light:knob"); });
    hit("circle", {cx: 1052, cy: 440, r: 80}, "sw5"); hit("circle", {cx: 1458, cy: 444, r: 46}, "sw6");
    hit("rect", {x: 1153, y: 403, width: 82, height: 84}, "play"); hit("rect", {x: 1241, y: 403, width: 82, height: 84}, "loop");
    hit("rect", {x: 262, y: 390, width: 98, height: 98}, "chompi"); hit("rect", {x: 196, y: 384, width: 52, height: 118}, "toggle");
    hit("circle", {cx: 310, cy: 352, r: 20}, "light:chompi"); hit("circle", {cx: 1458, cy: 352, r: 20}, "light:sw6");
    hit("rect", {x: 990, y: 326, width: 124, height: 34}, "light:transport");
    const short = (s, n) => s.length > n ? s.slice(0, n - 1) + "…" : s;
    return {
      // 35 lights as 7-bit RGB in the view's order (unlit: [0, 0, 0]).
      setLights(leds) {
        leds.forEach((rgb, i) => {
          const s = lights[i]; if (!s) return;
          const c = colour(rgb);
          s.dot.setAttribute("fill", c ? c.css : DIM);
          s.halo.setAttribute("fill", c ? c.css : "#000"); s.halo.setAttribute("opacity", c ? (.25 + .6 * c.level).toFixed(2) : 0);
        });
      },
      // A key's job: up to two lines of about eight letters inside the key (split at a space when there is one).
      setKeyLabel(note, label = "") {
        const e = keyLabels[note]; if (!e) return;
        let rows = [label];
        if (label.length > 8) { const cut = label.lastIndexOf(" ", 9); rows = cut > 0 ? [label.slice(0, cut), label.slice(cut + 1)] : [label.slice(0, 8), label.slice(8)]; }
        rows = rows.slice(0, 2).map(s => short(s, 9));
        e.replaceChildren(...rows.map((s, i) => {
          const span = document.createElementNS(NS, "tspan");
          span.setAttribute("x", e.getAttribute("x")); span.setAttribute("dy", i ? "18" : rows.length > 1 ? "-8" : "0");
          span.textContent = s; return span;
        }));
      },
      setKnob(k, label = "") { if (knobLabels[k]) knobLabels[k].textContent = short(label, 12); },
      setToggle(up) { lever.setAttribute("cy", up ? 424 : 462); },
      setSw5(label = "") { sw5.textContent = label; },
      setSw6(label = "") { sw6.textContent = label; },
    };
  }
  root.PanelArt = {draw, colour, LIGHTS, WHITE, BLACK};
})(typeof window !== "undefined" ? window : globalThis);
