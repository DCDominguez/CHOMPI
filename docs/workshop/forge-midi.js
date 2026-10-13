"use strict";
// CHOMPI over Web MIDI for the forge site's pages (0.20): find CHOMPI's ports (remembered per browser), send
// one request at a time and wait for its reply (decoded by forge-wire.js). Used by the display page; the
// workshop page (app.js) has its own copy of the same steps.
(function (root) {
  const W = root.ForgeWire;
  let access = null, input = null, output = null, busy = Promise.resolve();
  const sequence = () => Math.floor(Math.random() * 16384);
  function remember(kind, port) { try { localStorage.setItem(`forge.${kind}`, port.name); } catch { /* storage blocked */ } }
  function remembered(kind) { try { return localStorage.getItem(`forge.${kind}`); } catch { return null; } }
  function pick(ports, kind) {
    const list = [...ports.values()], name = remembered(kind);
    return list.find(p => p.name === name) || list.find(p => /chompi/i.test(p.name)) || null;
  }
  async function connect(onChange) {
    if (!navigator.requestMIDIAccess) throw new Error("This browser has no Web MIDI. Use Chrome on Android or a computer (not an iPhone).");
    try { access = await navigator.requestMIDIAccess({sysex: true}); }
    catch { throw new Error("MIDI permission was refused. Allow MIDI devices for this page (the icon by the address), then Connect again."); }
    const find = () => {
      input = pick(access.inputs, "input"); output = pick(access.outputs, "output");
      if (input) remember("input", input); if (output) remember("output", output);
      if (onChange) onChange(!!(input && output));
    };
    access.onstatechange = find; find();
    if (!input || !output) throw new Error("No CHOMPI found. Plug it in with a USB-C (OTG) cable, switch it on, then Connect again.");
  }
  // One request in flight: each exchange waits for the one before (the reply is matched by its sequence).
  function exchange(makePayload, timeout = 1500) {
    const run = () => new Promise((resolve, reject) => {
      if (!input || !output) { reject(new Error("CHOMPI isn't connected.")); return; }
      const payload = makePayload(sequence());
      const t = setTimeout(() => { input.onmidimessage = null; reject(new Error("CHOMPI didn't answer.")); }, timeout);
      input.onmidimessage = event => {
        const data = Array.from(event.data);
        if (data.length < 10 || data[0] !== 0xF0 || data[1] !== 0x7D || data[6] !== payload[5] || data[7] !== payload[6]) return;
        clearTimeout(t); input.onmidimessage = null;
        try { resolve(W.decodeReply(data.slice(1, -1), payload)); } catch (error) { reject(error); }
      };
      output.send(W.frame(payload));
    });
    const next = busy.then(run, run); busy = next.catch(() => {}); return next;
  }
  root.ForgeMidi = {connect, exchange, connected: () => !!(input && output), portName: () => (output && output.name) || ""};
})(window);
