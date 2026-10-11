"use strict";
// Choowi's sample pack: two TAPE banks made with Web Audio (OfflineAudioContext), rendered by
// host/make_sample_pack.py in Chromium. Chromatic bank d plays at its own pitch on middle C
// (C4, as Forge and TAPE map a chromatic sample); kit bank d is one hit per white key.
// Seeded noise, so the same script gives the same sound (Chromium may differ by one 16-bit step). CC0.
const SR = 48000, C4 = 261.6256;

function rng(seed) { let s = seed >>> 0 || 1; return () => { s ^= s << 13; s ^= s >>> 17; s ^= s << 5; return (s >>> 0) / 4294967296 * 2 - 1; }; }
function noise(ctx, seconds, seed) {
  const b = ctx.createBuffer(1, Math.ceil(seconds * SR), SR), d = b.getChannelData(0), r = rng(seed);
  for (let i = 0; i < d.length; i++) d[i] = r();
  const n = ctx.createBufferSource(); n.buffer = b; return n;
}
function osc(ctx, type, freq, t0 = 0, t1 = 8) { const o = ctx.createOscillator(); o.type = type; o.frequency.value = freq; o.start(t0); o.stop(t1); return o; }
function gain(ctx, value = 1) { const g = ctx.createGain(); g.gain.value = value; return g; }
function filter(ctx, type, freq, q = 0.7) { const f = ctx.createBiquadFilter(); f.type = type; f.frequency.value = freq; f.Q.value = q; return f; }
// Envelope on an AudioParam: [[time, value], ...]; exponential moves where both ends are positive.
function env(param, points) {
  param.cancelScheduledValues(0); param.setValueAtTime(points[0][1], points[0][0]);
  for (let i = 1; i < points.length; i++) {
    const [t, v] = points[i], prev = points[i - 1][1];
    if (v > 0 && prev > 0) param.exponentialRampToValueAtTime(v, t); else param.linearRampToValueAtTime(v, t);
  }
}
function chain(...nodes) { for (let i = 0; i < nodes.length - 1; i++) nodes[i].connect(nodes[i + 1]); return nodes[nodes.length - 1]; }
function adsr(ctx, a, d, s, r, hold, peak = 1) {
  const g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [a, peak], [a + d, Math.max(s * peak, 0.0001)], [hold, Math.max(s * peak, 0.0001)], [hold + r, 0.0001]]); return g;
}
// Plucked string: a noise burst into a tuned feedback delay (Karplus-Strong).
function pluck(ctx, out, f, damp, bright, seed, length) {
  const burst = noise(ctx, 0.012, seed), bg = gain(ctx, 0.9); burst.start(0);
  // A delay inside a feedback loop adds one 128-frame render block, so subtract it (the pluck is
  // rendered at 96 kHz so the remainder stays above the 128-frame minimum, then resampled).
  const delay = ctx.createDelay(1); delay.delayTime.value = 1 / f - 128 / ctx.sampleRate - 31e-6;   // 31 us: the loop filter's own delay (measured)
  const lp = filter(ctx, "lowpass", bright, -3), fb = gain(ctx, damp);   // Q is in dB here: -3 = no resonant bump, so the loop decays
  chain(burst, bg, delay, lp, fb, delay);
  const shape = gain(ctx, 1); env(shape.gain, [[0, 1], [length - 0.05, 1], [length, 0.0001]]);
  chain(delay, shape, out);
}
function fm(ctx, out, f, ratio, index, decay, length, indexDecay = decay) {
  const car = osc(ctx, "sine", f, 0, length), mod = osc(ctx, "sine", f * ratio, 0, length), depth = gain(ctx, 0);
  env(depth.gain, [[0, f * index], [indexDecay, f * index * 0.05], [length, 0.0001]]);
  chain(mod, depth); depth.connect(car.frequency);
  const amp = gain(ctx, 0); env(amp.gain, [[0, 0.0001], [0.004, 1], [decay, 0.05], [length, 0.0001]]);
  chain(car, amp, out);
}

const CHROMATIC = [
  ["Glass Pluck", 2.0, (ctx, out) => pluck(ctx, out, C4, 0.996, 6000, 11, 2.0), 96000],
  ["Soft Keys", 2.5, (ctx, out) => {   // electric-piano FM: a bright attack that mellows
    fm(ctx, out, C4, 1, 1.6, 2.2, 2.5, 0.35);
    const tine = gain(ctx, 0); env(tine.gain, [[0, 0.0001], [0.002, 0.25], [0.25, 0.0001]]); chain(osc(ctx, "sine", C4 * 14, 0, 0.3), tine, out);
  }],
  ["FM Bell", 3.5, (ctx, out) => fm(ctx, out, C4, 3.5, 3, 3.3, 3.5, 1.2)],
  ["Warm Saw Pad", 3.5, (ctx, out) => {
    const lp = filter(ctx, "lowpass", 400, 1.2); env(lp.frequency, [[0, 350], [1.6, 2600], [3.5, 1800]]);
    const amp = adsr(ctx, 0.6, 0.6, 0.8, 0.8, 2.6, 0.35);
    for (const cents of [-12, 0, 9]) { const o = osc(ctx, "sawtooth", C4, 0, 3.5); o.detune.value = cents; o.connect(lp); }
    chain(lp, amp, out);
  }],
  ["Round Bass", 1.8, (ctx, out) => {
    const lp = filter(ctx, "lowpass", 900, 2); env(lp.frequency, [[0, 1800], [0.25, 500], [1.8, 400]]);
    const amp = adsr(ctx, 0.005, 0.4, 0.55, 0.3, 1.4, 0.8);
    chain(osc(ctx, "sawtooth", C4, 0, 1.8), lp); chain(osc(ctx, "sine", C4 / 2, 0, 1.8), gain(ctx, 0.7), lp);
    chain(lp, amp, out);
  }],
  ["Acid Squelch", 1.4, (ctx, out) => {
    const lp = filter(ctx, "lowpass", 300, 14); env(lp.frequency, [[0, 220], [0.02, 3200], [0.35, 380], [1.4, 300]]);
    const amp = adsr(ctx, 0.003, 0.3, 0.6, 0.2, 1.1, 0.5);
    chain(osc(ctx, "sawtooth", C4, 0, 1.4), lp, amp, out);
  }],
  ["Marimba", 1.6, (ctx, out) => {
    for (const [mult, level, decay] of [[1, 1, 1.4], [4, 0.35, 0.25], [9.9, 0.12, 0.08]]) {
      const g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [0.002, level], [decay, 0.0001]]); chain(osc(ctx, "sine", C4 * mult, 0, 1.6), g, out);
    }
  }],
  ["Ooh Choir", 3.2, (ctx, out) => {   // a sawtooth through "oo" formants, with a slow vibrato
    const src = osc(ctx, "sawtooth", C4, 0, 3.2), vib = osc(ctx, "sine", 5.2, 0, 3.2), vd = gain(ctx, 0);
    env(vd.gain, [[0, 0.0001], [0.8, 0.0001], [1.6, 4]]); chain(vib, vd); vd.connect(src.detune);
    const amp = adsr(ctx, 0.35, 0.3, 0.85, 0.7, 2.5, 0.9);
    for (const [f, q, level] of [[320, 8, 1], [800, 10, 0.45], [2400, 12, 0.18]]) chain(src, filter(ctx, "bandpass", f, q), gain(ctx, level), amp);
    amp.connect(out);
  }],
  ["Music Box", 2.6, (ctx, out) => {
    for (const [mult, level, decay] of [[2, 1, 2.4], [5.98, 0.3, 0.9], [11.6, 0.12, 0.3]]) {
      const g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [0.002, level], [decay, 0.0001]]); chain(osc(ctx, "sine", C4 * mult, 0, 2.6), g, out);
    }
  }],
  ["Brass Stab", 1.3, (ctx, out) => {
    const lp = filter(ctx, "lowpass", 600, 1.5); env(lp.frequency, [[0, 500], [0.06, 3500], [0.5, 1400], [1.3, 900]]);
    const amp = adsr(ctx, 0.03, 0.25, 0.6, 0.25, 0.9, 0.5);
    for (const cents of [-6, 6]) { const o = osc(ctx, "sawtooth", C4, 0, 1.3); o.detune.value = cents; o.connect(lp); }
    chain(lp, amp, out);
  }],
  ["Kalimba", 2.0, (ctx, out) => {
    const g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [0.002, 1], [1.8, 0.0001]]); chain(osc(ctx, "sine", C4, 0, 2), g, out);
    const t = gain(ctx, 0); env(t.gain, [[0, 0.0001], [0.001, 0.4], [0.06, 0.0001]]); chain(osc(ctx, "sine", C4 * 5.4, 0, 0.1), t, out);
    const k = noise(ctx, 0.01, 23); k.start(0); chain(k, filter(ctx, "bandpass", 3000, 2), gain(ctx, 0.3), out);
  }],
  ["Lofi Organ", 2.4, (ctx, out) => {
    const wob = osc(ctx, "sine", 0.7, 0, 2.4), wd = gain(ctx, 9); chain(wob, wd);
    const amp = adsr(ctx, 0.02, 0.1, 0.9, 0.25, 2.1, 0.32), lp = filter(ctx, "lowpass", 2500, 0.7);
    for (const [mult, level] of [[1, 1], [2, 0.6], [3, 0.35], [4, 0.2]]) { const o = osc(ctx, "sine", C4 * mult, 0, 2.4); wd.connect(o.detune); chain(o, gain(ctx, level), lp); }
    chain(osc(ctx, "square", C4, 0, 2.4), gain(ctx, 0.12), lp);
    chain(lp, amp, out);
  }],
  ["Wobble Lead", 2.0, (ctx, out) => {
    const o = osc(ctx, "square", C4, 0, 2), vib = osc(ctx, "sine", 6, 0, 2), vd = gain(ctx, 0);
    env(vd.gain, [[0, 0.0001], [0.3, 0.0001], [0.9, 25]]); chain(vib, vd); vd.connect(o.detune);
    const lp = filter(ctx, "lowpass", 2200, 3), amp = adsr(ctx, 0.01, 0.2, 0.7, 0.3, 1.7, 0.35);
    chain(o, lp, amp, out);
  }],
  ["Sparkle Pad", 4.0, (ctx, out) => {
    const amp = adsr(ctx, 1.0, 0.5, 0.8, 1.2, 2.8, 0.3);
    for (const [mult, cents] of [[1, -7], [1, 7], [2, 0], [3, 3], [4, -4]]) { const o = osc(ctx, "sine", C4 * mult, 0, 4); o.detune.value = cents; chain(o, gain(ctx, 1 / mult), amp); }
    const air = noise(ctx, 4, 31); air.start(0); chain(air, filter(ctx, "bandpass", 6000, 3), gain(ctx, 0.05), amp);
    amp.connect(out);
  }],
];

function hit(ctx, out, { type = "sine", from, to, sweep, decay, level = 1 }) {
  const o = osc(ctx, type, from, 0, decay + 0.05); env(o.frequency, [[0, from], [sweep, to]]);
  const g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [0.002, level], [decay, 0.0001]]); chain(o, g, out);
}
function hiss(ctx, out, { seed, type, freq, q = 0.7, attack = 0.001, decay, level = 1, delay = 0 }) {
  const n = noise(ctx, decay + delay + 0.05, seed); n.start(0);
  const g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [delay + 0.0001, 0.0001], [delay + attack, level], [delay + decay, 0.0001]]);
  chain(n, filter(ctx, type, freq, q), g, out);
}
function metal(ctx, out, { base, decay, level = 0.5, hp = 7000 }) {   // six detuned squares, the classic hat source
  const h = filter(ctx, "highpass", hp, 0.7), g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [0.001, level], [decay, 0.0001]]);
  for (const r of [2, 3, 4.16, 5.43, 6.79, 8.21]) chain(osc(ctx, "square", base * r, 0, decay + 0.05), gain(ctx, 0.2), h);
  chain(h, g, out);
}
const KIT = [
  ["Kick", 0.7, (ctx, out) => { hit(ctx, out, { from: 160, to: 42, sweep: 0.12, decay: 0.6 }); hiss(ctx, out, { seed: 1, type: "lowpass", freq: 3000, decay: 0.01, level: 0.4 }); }],
  ["Snare", 0.45, (ctx, out) => { hit(ctx, out, { from: 240, to: 170, sweep: 0.05, decay: 0.15, level: 0.6 }); hiss(ctx, out, { seed: 2, type: "highpass", freq: 1500, decay: 0.35, level: 0.8 }); }],
  ["Closed Hat", 0.12, (ctx, out) => metal(ctx, out, { base: 330, decay: 0.08 })],
  ["Open Hat", 0.6, (ctx, out) => metal(ctx, out, { base: 330, decay: 0.55 })],
  ["Clap", 0.5, (ctx, out) => { for (const [d, l] of [[0, 0.7], [0.011, 0.6], [0.023, 0.8]]) hiss(ctx, out, { seed: 3 + d * 1000, type: "bandpass", freq: 1200, q: 1.5, decay: d + 0.02, delay: d, level: l }); hiss(ctx, out, { seed: 9, type: "bandpass", freq: 1100, q: 1.2, decay: 0.4, delay: 0.03, level: 0.6 }); }],
  ["Low Tom", 0.7, (ctx, out) => hit(ctx, out, { from: 140, to: 85, sweep: 0.3, decay: 0.6 })],
  ["High Tom", 0.5, (ctx, out) => hit(ctx, out, { from: 230, to: 150, sweep: 0.25, decay: 0.45 })],
  ["Rim", 0.12, (ctx, out) => { hit(ctx, out, { from: 820, to: 800, sweep: 0.01, decay: 0.05, level: 0.7 }); hiss(ctx, out, { seed: 4, type: "bandpass", freq: 1700, q: 4, decay: 0.03 }); }],
  ["Cowbell", 0.6, (ctx, out) => { const bp = filter(ctx, "bandpass", 1000, 1.5), g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [0.002, 0.5], [0.05, 0.25], [0.55, 0.0001]]); for (const f of [545, 815]) chain(osc(ctx, "square", f, 0, 0.6), bp); chain(bp, g, out); }],
  ["Shaker", 0.25, (ctx, out) => hiss(ctx, out, { seed: 5, type: "highpass", freq: 5000, attack: 0.04, decay: 0.2, level: 0.6 })],
  ["Crash", 2.5, (ctx, out) => { metal(ctx, out, { base: 410, decay: 2.3, level: 0.35, hp: 4000 }); hiss(ctx, out, { seed: 6, type: "highpass", freq: 3500, decay: 2.4, level: 0.5 }); }],
  ["Zap", 0.3, (ctx, out) => hit(ctx, out, { type: "triangle", from: 2400, to: 70, sweep: 0.18, decay: 0.25, level: 0.8 })],
  ["Choowi Chirp", 0.35, (ctx, out) => { hit(ctx, out, { from: 700, to: 1800, sweep: 0.08, decay: 0.12, level: 0.6 }); const g = gain(ctx, 0); env(g.gain, [[0, 0.0001], [0.14, 0.0001], [0.145, 0.5], [0.3, 0.0001]]); const o = osc(ctx, "sine", 1200, 0, 0.35); env(o.frequency, [[0.14, 1200], [0.22, 2600]]); chain(o, g, out); }],
  ["Anvil (Choowi's hammer)", 2.2, (ctx, out) => { fm(ctx, out, 620, 2.76, 4, 2.0, 2.2, 0.15); fm(ctx, out, 1710, 1.41, 2, 1.1, 2.2, 0.05); hiss(ctx, out, { seed: 7, type: "bandpass", freq: 4000, q: 1, decay: 0.03, level: 0.7 }); }],
];

function wav(samples) {
  const data = new Int16Array(samples.length);
  for (let i = 0; i < samples.length; i++) data[i] = Math.max(-32768, Math.min(32767, Math.round(samples[i] * 32767)));
  const buf = new ArrayBuffer(44 + data.length * 2), v = new DataView(buf); let p = 0;
  const str = s => { for (const c of s) v.setUint8(p++, c.charCodeAt(0)); }, u32 = x => { v.setUint32(p, x, true); p += 4; }, u16 = x => { v.setUint16(p, x, true); p += 2; };
  str("RIFF"); u32(36 + data.length * 2); str("WAVE"); str("fmt "); u32(16); u16(1); u16(1); u32(SR); u32(SR * 2); u16(2); u16(16);
  str("data"); u32(data.length * 2); new Int16Array(buf, 44).set(data);
  let s = ""; const bytes = new Uint8Array(buf);
  for (let i = 0; i < bytes.length; i += 32768) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 32768));
  return btoa(s);
}
// Peak-normalise to -1 dBFS, trim trailing near-silence, 5 ms fade at the end.
function finish(raw) {
  let peak = 0; for (const x of raw) peak = Math.max(peak, Math.abs(x));
  const scale = peak > 0 ? 0.891 / peak : 1;
  let end = raw.length; while (end > SR * 0.05 && Math.abs(raw[end - 1] * scale) < 0.0003) end--;
  const out = new Float32Array(end), fade = Math.min(end, SR * 0.005);
  for (let i = 0; i < end; i++) out[i] = raw[i] * scale * (i >= end - fade ? (end - i) / fade : 1);
  return out;
}
async function render(build, seconds, rate = SR) {
  const ctx = new OfflineAudioContext(1, Math.ceil(seconds * rate), rate);
  build(ctx, ctx.destination);
  let buffer = await ctx.startRendering();
  if (rate !== SR) {                                   // resample to the pack's 48 kHz
    const down = new OfflineAudioContext(1, Math.ceil(seconds * SR), SR), src = down.createBufferSource();
    src.buffer = buffer; src.connect(down.destination); src.start(0); buffer = await down.startRendering();
  }
  return finish(buffer.getChannelData(0));
}
async function renderPack() {
  const files = [];
  for (const [list, prefix] of [[CHROMATIC, "jammi_d"], [KIT, "cubbi_d"]])
    for (let i = 0; i < list.length; i++) {
      const [name, seconds, build, rate] = list[i], samples = await render(build, seconds, rate);
      files.push({file: `${prefix}${i + 1}.wav`, name, seconds: samples.length / SR, wav: wav(samples)});
    }
  return files;
}
