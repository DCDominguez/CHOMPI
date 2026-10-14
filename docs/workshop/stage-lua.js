"use strict";
// Stage scripts (2026-10-14; the owner: a norns-style instrument visualization, AI-scriptable). A small norns-like
// runtime: Lua 5.3 (Fengari, vendor/fengari-web.js, MIT) with only base / string / table / math / coroutine / utf8,
// a 128 x 64 screen with 16 levels, clock coroutines on CHOMPI's tempo, params, and MIDI back to CHOMPI. A script
// only sees what is handed to it here (no page, no network, no files); every call into it has an instruction budget,
// so a runaway loop stops with an error instead of freezing the page. The stage (stage.js) owns the input and the
// MIDI port and draws the screen as glowing dots.
(function () {
  const {lua, lauxlib, lualib, to_luastring} = window.fengari;
  const SW = 128, SH = 64, BUDGET = 400;                      // budget: x 1,000 Lua instructions per call
  const screenCanvas = document.createElement("canvas"); screenCanvas.width = SW; screenCanvas.height = SH;
  const sx = screenCanvas.getContext("2d", {willReadFrequently: true});
  const shown = new Float32Array(SW * SH);                    // the last screen.update(), 0..1 per pixel
  let L = null, running = false, error = "", steps = 0, lastRedraw = 0, cur = [0, 0], fontSize = 8;
  let send = () => {}, held = () => [], tempo = () => 120, colour = "white", logLine = () => {};
  let beat = 0, lastTick = 0, tickTimer = 0, sentAt = [], sounding = new Map();

  const PRELUDE = `
local _co, _next = {}, 1
clock = {}
function clock.run(f, ...) local id = _next; _next = _next + 1; _co[id] = {co = coroutine.create(f), args = table.pack(...), wake = -1, unit = "time"}; return id end
function clock.cancel(id) _co[id] = nil end
function clock.sleep(s) coroutine.yield("sleep", s) end
function clock.sync(b) coroutine.yield("sync", b) end
function clock.get_tempo() return _host.tempo() end
function clock.get_beats() return _beat end
function _clock_tick(now, beat)
  _beat = beat
  for id, c in pairs(_co) do
    if (c.unit == "time" and now >= c.wake) or (c.unit == "beat" and beat >= c.wake) then
      local ok, kind, v
      if c.args then ok, kind, v = coroutine.resume(c.co, table.unpack(c.args, 1, c.args.n)); c.args = nil
      else ok, kind, v = coroutine.resume(c.co) end
      if not ok then _co[id] = nil; error(kind, 0) end
      if coroutine.status(c.co) == "dead" then _co[id] = nil
      elseif kind == "sync" then local b = math.max(v or 1, 1/64); c.unit = "beat"; c.wake = (math.floor(beat / b + 1e-6) + 1) * b
      else c.unit = "time"; c.wake = now + math.max(v or 0, 0.001) end
    end
  end
end
params = {_v = {}, _d = {}}
function params:add_number(id, name, min, max, default) self._d[id] = {kind = "number", name = name or id, min = min, max = max}; self._v[id] = default or min end
function params:add_option(id, name, options, default) self._d[id] = {kind = "option", name = name or id, min = 1, max = #options, options = options}; self._v[id] = default or 1 end
function params:add_control(id, name, min, max, default) self._d[id] = {kind = "control", name = name or id, min = min or 0, max = max or 1}; self._v[id] = default or min or 0 end
function params:get(id) return self._v[id] end
function params:string(id) local d = self._d[id]; if d and d.kind == "option" then return d.options[self._v[id]] end; return tostring(self._v[id]) end
function params:set(id, v) local d = self._d[id]; if d then v = math.max(d.min, math.min(d.max, v)) end; self._v[id] = v end
function params:delta(id, n) local d = self._d[id]; if not d then return end
  local step = d.kind == "control" and (d.max - d.min) / 100 or 1; self:set(id, self._v[id] + n * step) end
util = {}
function util.clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end
function util.linlin(lo, hi, olo, ohi, v) return olo + (util.clamp(v, lo, hi) - lo) / (hi - lo) * (ohi - olo) end
function util.round(v, q) q = q or 1; return math.floor(v / q + 0.5) * q end
function util.wrap(v, lo, hi) local r = hi - lo + 1; return ((v - lo) % r) + lo end
musicutil = {}
local _scales = {major = {0,2,4,5,7,9,11}, minor = {0,2,3,5,7,8,10}, dorian = {0,2,3,5,7,9,10}, pentatonic = {0,2,4,7,9}, minor_pentatonic = {0,3,5,7,10}}
function musicutil.scale(root, name, count) local s, out = _scales[name or "major"] or _scales.major, {}
  for i = 0, (count or 8) - 1 do out[#out + 1] = root + s[i % #s + 1] + 12 * (i // #s) end; return out end
local _names = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}
function musicutil.note_name(n) return _names[n % 12 + 1] .. tostring(n // 12 - 1) end
midi = {}
function midi.note_on(n, v) _host.note_on(n, v or 100) end
function midi.note_off(n) _host.note_off(n) end
function midi.cc(c, v) _host.cc(c, v) end
function midi.note(n, v, dur) _host.note_on(n, v or 100); clock.run(function() clock.sleep(dur or 0.2); _host.note_off(n) end) end
chompi = {}
function chompi.key_x(note) return 2 + (util.clamp(note, 48, 72) - 48) / 24 * 123 end
function chompi.held() return _host.held() end
function chompi.colour(name) _host.colour(name) end
screen = _host.screen
print = _host.print
dofile, loadfile, load, require, collectgarbage = nil, nil, nil, nil, nil
`;

  function fail(message) {
    error = String(message).replace(/\[string "script"\]:(\d+):/g, "line $1:").replace(/\[string "stage"\]:\d+: ?/g, "").replace(/^_clock_tick: /, "clock: ");
    running = false; logLine("stopped: " + error);
  }
  function arg(Ls, i, fallback) { return lua.lua_isnoneornil(Ls, i) ? fallback : lauxlib.luaL_checknumber(Ls, i); }
  const fn = body => Ls => { body(Ls); return 0; };
  function table(Ls, entries) {
    lua.lua_createtable(Ls, 0, Object.keys(entries).length);
    for (const [name, f] of Object.entries(entries)) { lua.lua_pushcfunction(Ls, f); lua.lua_setfield(Ls, -2, to_luastring(name)); }
  }
  const grey = l => { const g = Math.round(Math.max(0, Math.min(15, l)) / 15 * 255); return `rgb(${g},${g},${g})`; };
  const SCREEN = {
    clear: fn(() => { sx.setTransform(1, 0, 0, 1, 0, 0); sx.fillStyle = "#000"; sx.fillRect(0, 0, SW, SH); sx.beginPath(); cur = [0, 0]; }),
    level: fn(Ls => { const c = grey(arg(Ls, 1, 15)); sx.strokeStyle = c; sx.fillStyle = c; }),
    line_width: fn(Ls => { sx.lineWidth = Math.max(0.5, Math.min(8, arg(Ls, 1, 1))); }),
    move: fn(Ls => { cur = [arg(Ls, 1, 0), arg(Ls, 2, 0)]; sx.moveTo(cur[0], cur[1]); }),
    move_rel: fn(Ls => { cur = [cur[0] + arg(Ls, 1, 0), cur[1] + arg(Ls, 2, 0)]; sx.moveTo(cur[0], cur[1]); }),
    line: fn(Ls => { cur = [arg(Ls, 1, 0), arg(Ls, 2, 0)]; sx.lineTo(cur[0], cur[1]); }),
    line_rel: fn(Ls => { cur = [cur[0] + arg(Ls, 1, 0), cur[1] + arg(Ls, 2, 0)]; sx.lineTo(cur[0], cur[1]); }),
    rect: fn(Ls => { sx.rect(arg(Ls, 1, 0), arg(Ls, 2, 0), arg(Ls, 3, 1), arg(Ls, 4, 1)); }),
    circle: fn(Ls => { const x = arg(Ls, 1, 0), y = arg(Ls, 2, 0), r = Math.max(0, arg(Ls, 3, 1)); sx.moveTo(x + r, y); sx.arc(x, y, r, 0, Math.PI * 2); }),
    arc: fn(Ls => { const x = arg(Ls, 1, 0), y = arg(Ls, 2, 0), r = Math.max(0, arg(Ls, 3, 1)); sx.arc(x, y, r, arg(Ls, 4, 0), arg(Ls, 5, Math.PI * 2)); }),
    stroke: fn(() => { sx.stroke(); sx.beginPath(); }),
    fill: fn(() => { sx.fill(); sx.beginPath(); }),
    pixel: fn(Ls => { sx.fillRect(Math.floor(arg(Ls, 1, 0)), Math.floor(arg(Ls, 2, 0)), 1, 1); }),
    font_size: fn(Ls => { fontSize = Math.max(5, Math.min(32, arg(Ls, 1, 8))); }),
    text: fn(Ls => text(Ls, "left")), text_center: fn(Ls => text(Ls, "center")), text_right: fn(Ls => text(Ls, "right")),
    update: fn(() => {
      const d = sx.getImageData(0, 0, SW, SH).data;
      for (let i = 0; i < shown.length; i++) shown[i] = d[i * 4] / 255;
    }),
  };
  function text(Ls, align) {
    const s = lauxlib.luaL_tolstring(Ls, 1); lua.lua_pop(Ls, 1);
    sx.font = `${fontSize}px "Space Mono", Consolas, monospace`; sx.textAlign = align; sx.textBaseline = "alphabetic";
    sx.fillText(new TextDecoder().decode(s).slice(0, 64), cur[0], cur[1]);
  }
  // MIDI back to CHOMPI: notes 0-127, at most 60 messages a second and 16 notes sounding; all of them end on stop.
  function allowed() {
    const now = performance.now(); sentAt = sentAt.filter(t => now - t < 1000);
    if (sentAt.length >= 60) return false; sentAt.push(now); return true;
  }
  const HOST = {
    note_on: fn(Ls => {
      const n = Math.round(arg(Ls, 1, 60)), v = Math.round(arg(Ls, 2, 100));
      if (n < 0 || n > 127 || (sounding.size >= 16 && !sounding.has(n)) || !allowed()) return;
      sounding.set(n, (sounding.get(n) || 0) + 1); send([0x90, n, Math.max(1, Math.min(127, v))]);
    }),
    note_off: fn(Ls => {
      const n = Math.round(arg(Ls, 1, 60)); if (!sounding.has(n)) return;
      const c = sounding.get(n) - 1; if (c <= 0) sounding.delete(n); else sounding.set(n, c);
      send([0x80, n, 0]);
    }),
    cc: fn(Ls => { const c = Math.round(arg(Ls, 1, 0)), v = Math.round(arg(Ls, 2, 0)); if (c >= 0 && c < 120 && allowed()) send([0xB0, c, Math.max(0, Math.min(127, v))]); }),
    tempo: Ls => { lua.lua_pushnumber(Ls, tempo()); return 1; },
    held: Ls => { const h = held(); lua.lua_createtable(Ls, h.length, 0); h.forEach((n, i) => { lua.lua_pushinteger(Ls, n); lua.lua_rawseti(Ls, -2, i + 1); }); return 1; },
    colour: fn(Ls => { colour = lua.lua_tojsstring(Ls, 1) || "white"; }),
    print: Ls => {
      const parts = []; for (let i = 1; i <= lua.lua_gettop(Ls); i++) { parts.push(new TextDecoder().decode(lauxlib.luaL_tolstring(Ls, i))); lua.lua_pop(Ls, 1); }
      logLine(parts.join(" ")); return 0;
    },
  };

  function call(name, ...args) {                              // a global function of the script, if it has one
    if (!running) return;
    lua.lua_getglobal(L, to_luastring(name));
    if (lua.lua_type(L, -1) !== lua.LUA_TFUNCTION) { lua.lua_pop(L, 1); return; }
    for (const a of args) lua.lua_pushnumber(L, a);
    steps = 0;
    if (lua.lua_pcall(L, args.length, 0, 0) !== lua.LUA_OK) { fail(name + ": " + lua.lua_tojsstring(L, -1)); lua.lua_pop(L, 1); }
  }
  function tick() {                                            // the clock: every 5 ms, on CHOMPI's tempo
    if (!running) return;
    const now = performance.now() / 1000, dt = lastTick ? Math.min(0.1, now - lastTick) : 0;
    lastTick = now; beat += dt * tempo() / 60;
    lua.lua_pushnumber(L, beat); lua.lua_setglobal(L, to_luastring("_beat"));
    call("_clock_tick", now, beat);
  }
  function stop() {
    if (running) call("cleanup");
    running = false; clearInterval(tickTimer);
    for (const n of sounding.keys()) send([0x80, n, 0]);
    sounding.clear();
  }
  function load(source) {
    stop(); error = ""; colour = "white"; beat = 0; lastTick = 0; fontSize = 8;
    L = lauxlib.luaL_newstate();
    for (const [name, open] of [["_G", lualib.luaopen_base], ["coroutine", lualib.luaopen_coroutine], ["table", lualib.luaopen_table],
                                ["string", lualib.luaopen_string], ["math", lualib.luaopen_math], ["utf8", lualib.luaopen_utf8]]) {
      lauxlib.luaL_requiref(L, to_luastring(name), open, 1); lua.lua_pop(L, 1);
    }
    lua.lua_sethook(L, (Ls) => { if (++steps > BUDGET) lauxlib.luaL_error(Ls, to_luastring("took too long (a loop that never ends?)")); }, lua.LUA_MASKCOUNT, 1000);
    table(L, HOST); table(L, SCREEN); lua.lua_setfield(L, -2, to_luastring("screen")); lua.lua_setglobal(L, to_luastring("_host"));
    running = true; steps = 0;
    for (const [chunk, label] of [[PRELUDE, "stage"], [source, "script"]]) {
      if (lauxlib.luaL_loadbuffer(L, to_luastring(chunk), null, to_luastring(label)) !== lua.LUA_OK || (steps = 0, lua.lua_pcall(L, 0, 0, 0)) !== lua.LUA_OK) {
        fail(lua.lua_tojsstring(L, -1)); return {ok: false, error};
      }
    }
    SCREEN.clear(L); call("init");
    if (!running) return {ok: false, error};
    tickTimer = setInterval(tick, 5);
    return {ok: true};
  }
  function frame(now) {                                        // redraw at most 30 times a second
    if (running && now - lastRedraw > 33) { lastRedraw = now; call("redraw"); }
  }
  // The script's code from a chatbot reply: the last \`\`\`lua block, or the whole text.
  function extract(text) {
    const blocks = [...text.matchAll(/```(?:lua)?\s*\n([\s\S]*?)```/g)];
    return (blocks.length ? blocks[blocks.length - 1][1] : text).trim();
  }
  window.StageScript = {
    load, stop, frame, call, extract,
    running: () => running, error: () => error,
    value: (x, y) => shown[y * SW + x],
    colour: () => colour,
    connect(options) { ({send, held, tempo, log: logLine} = {...{send, held, tempo, log: logLine}, ...options}); },
  };
})();
