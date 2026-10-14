"use strict";
// Stage scripts: the examples and the prompt a chatbot gets (the forge page's way: describe it, paste the reply).
window.STAGE_EXAMPLES = {
  "rain garden": `-- rain garden: every note you play plants a seed; a grown plant blooms and sings its note back, an octave up
-- SW1 growth speed · SW2 how many plants · LOOP clears the garden
plants, drops, sparks = {}, {}, {}

function init()
  params:add_number("growth", "growth", 1, 10, 4)
  params:add_number("max", "plants", 1, 12, 8)
  chompi.palette("forest")
  chompi.afterglow(0.8)
end

function note_on(note, vel)
  if #plants >= params:get("max") then table.remove(plants, 1) end
  table.insert(plants, {x = chompi.key_x(note), h = 0, note = note, sway = math.random() * 6})
end

function enc(n, d)
  if n == 1 then params:delta("growth", d) end
  if n == 2 then params:delta("max", d) end
end

function key(n, z) if n == 2 and z == 1 then plants = {} end end

clock.run(function()                             -- the garden grows on CHOMPI's sixteenths
  while true do
    clock.sync(1/4)
    for _, p in ipairs(plants) do
      p.h = p.h + params:get("growth")
      if p.h >= 44 then
        midi.note(p.note + 12, 55, 0.3)
        for i = 1, (#sparks < 150 and 10 or 0) do table.insert(sparks, {x = p.x, y = 63 - p.h, vx = math.random() * 2 - 1, vy = -math.random() * 1.5, life = 12}) end
        p.h = 6
      end
    end
  end
end)

function redraw()
  screen.clear()
  if #drops < 60 and math.random() < 0.6 then table.insert(drops, {x = math.random(0, 127), y = 0, v = 1 + math.random() * 2}) end
  for i = #drops, 1, -1 do                        -- soft rain, always falling
    local d = drops[i]; d.y = d.y + d.v
    screen.level(3); screen.move(d.x, d.y); screen.line(d.x, d.y + 2); screen.stroke()
    if d.y > 63 then table.remove(drops, i) end
  end
  local t = clock.get_beats()
  for _, p in ipairs(plants) do                   -- stems sway with the beat, brighter as they grow
    local tip = p.x + math.sin(t * 1.5 + p.sway) * p.h / 12
    screen.level(math.floor(4 + p.h / 4)); screen.line_width(1.5)
    screen.move(p.x, 63); screen.line(tip, 63 - p.h); screen.stroke()
    screen.level(15); screen.circle(tip, 63 - p.h, 1.5 + p.h / 20); screen.fill()
  end
  screen.line_width(1)
  for i = #sparks, 1, -1 do                       -- a bloom bursts into petals
    local s = sparks[i]; s.x, s.y, s.vy, s.life = s.x + s.vx, s.y + s.vy, s.vy + 0.08, s.life - 1
    screen.level(s.life + 3); screen.pixel(s.x, s.y)
    if s.life <= 0 then table.remove(sparks, i) end
  end
  screen.level(6); screen.move(2, 8); screen.text("growth " .. params:get("growth") .. "  plants " .. params:get("max"))
  screen.update()
end
`,
  fireworks: `-- fireworks: each note launches a rocket from its key; at the top it bursts and plays the note, two octaves up
-- SW1 burst size · SW2 gravity · LOOP: a finale
rockets, sparks = {}, {}

function init()
  params:add_number("size", "burst", 6, 40, 22)
  params:add_number("gravity", "gravity", 1, 10, 4)
  chompi.palette("ember")
  chompi.afterglow(0.88)
end

function note_on(note, vel)
  table.insert(rockets, {x = chompi.key_x(note), y = 63, vy = -2.2 - vel / 90, note = note, top = 8 + math.random(0, 18)})
end

function enc(n, d)
  if n == 1 then params:delta("size", d) end
  if n == 2 then params:delta("gravity", d) end
end

function key(n, z)
  if n == 2 and z == 1 then for i = 1, 6 do note_on(48 + math.random(0, 24), 100) end end
end

local function burst(r)
  midi.note(r.note + 24, 70, 0.4)
  local n = params:get("size")
  if #sparks > 400 then return end
  for i = 1, n do
    local a, s = i / n * 2 * math.pi, 0.6 + math.random() * 1.4
    table.insert(sparks, {x = r.x, y = r.y, vx = math.cos(a) * s, vy = math.sin(a) * s, life = 22 + math.random(0, 10)})
  end
end

function redraw()
  screen.clear()
  local g = params:get("gravity") / 100
  for i = #rockets, 1, -1 do
    local r = rockets[i]; r.y = r.y + r.vy; r.vy = r.vy + g
    screen.level(15); screen.circle(r.x, r.y, 1); screen.fill()
    if r.y <= r.top or r.vy >= 0 then burst(r); table.remove(rockets, i) end
  end
  for i = #sparks, 1, -1 do
    local s = sparks[i]; s.x, s.y = s.x + s.vx, s.y + s.vy; s.vy = s.vy + g; s.vx = s.vx * 0.98; s.life = s.life - 1
    screen.level(math.min(15, s.life // 2 + 2)); screen.pixel(s.x, s.y)
    if s.life <= 0 or s.y > 64 then table.remove(sparks, i) end
  end
  screen.level(2); screen.move(0, 63); screen.line(127, 63); screen.stroke()
  screen.update()
end
`,
  orbits: `-- orbits: four rings turn at their own speeds; the last notes you played ride them as comets and sound
-- whenever a ring brings its comet to the top. SW1-SW4: each ring's steps per bar · LOOP clears
rings = {{steps = 3}, {steps = 4}, {steps = 5}, {steps = 7}}
stars, pos = {}, {0, 0, 0, 0}

function init()
  chompi.palette("ocean")
  chompi.afterglow(0.93)                          -- long trails: the comets
end

function note_on(note, vel)
  table.insert(stars, 1, note)
  if #stars > 4 then table.remove(stars) end
end

function enc(n, d) if rings[n] then rings[n].steps = util.clamp(rings[n].steps + d, 1, 16) end end

function key(n, z) if n == 2 and z == 1 then stars = {} end end

for i = 1, 4 do
  clock.run(function()
    while true do
      clock.sync(4 / rings[i].steps)
      pos[i] = (pos[i] + 1) % rings[i].steps
      if pos[i] == 0 and stars[i] then midi.note(stars[i], 70, 0.15); rings[i].flash = 10 end
    end
  end)
end

function redraw()
  screen.clear()
  local b = clock.get_beats()
  for i, r in ipairs(rings) do
    local radius = 6 + i * 6
    local a = (b / 4 * r.steps % r.steps) / r.steps * 2 * math.pi - math.pi / 2
    screen.level(stars[i] and 4 or 2); screen.circle(64, 32, radius); screen.stroke()
    screen.level(stars[i] and 15 or 6); screen.circle(64 + math.cos(a) * radius, 32 + math.sin(a) * radius, stars[i] and 2.5 or 1.5); screen.fill()
    if (r.flash or 0) > 0 then screen.level(15); screen.circle(64, 32 - radius, r.flash / 3 + 1); screen.stroke(); r.flash = r.flash - 1 end
  end
  screen.level(15); screen.circle(64, 32, 2 + (1 - b % 1) * 2); screen.fill()   -- the sun pulses on the beat
  screen.level(7); screen.move(2, 62); screen.text(rings[1].steps .. ":" .. rings[2].steps .. ":" .. rings[3].steps .. ":" .. rings[4].steps)
  screen.update()
end
`,
};

window.STAGE_PROMPT = `You are writing a script for the Choowi stage: a norns-style instrument screen for CHOMPI (a small synth with
25 keys, 4 encoder knobs and buttons). The script is Lua 5.3 and runs in a sandbox with ONLY the functions below
(no io, os, require, load or files). Reply with the whole script in ONE \`\`\`lua code block, and nothing else matters.

THE IDEA (written by the player):
<describe what you want to see and hear here>

SCREEN: 128 x 64 pixels, 16 brightness levels (0 dark .. 15 bright), drawn as glowing dots in one colour.
  screen.clear()  screen.level(0-15)  screen.move(x, y)  screen.line(x, y)  screen.move_rel(dx, dy)  screen.line_rel(dx, dy)
  screen.rect(x, y, w, h)  screen.circle(x, y, r)  screen.arc(x, y, r, a1, a2)  screen.stroke()  screen.fill()
  screen.pixel(x, y)  screen.line_width(w)  screen.font_size(n)  screen.text(s) / text_center(s) / text_right(s) at the
  current point (baseline)  screen.update() (call at the end of redraw)
  THE LOOK (make it vivid; use these in init):
  chompi.palette(name): brightness walks dark -> colour -> white-hot: "neon" (purple-pink), "ember" (fire),
    "ocean" (blue-teal), "forest" (teal-green-yellow), "sun" (red-yellow), "aurora" (blue-purple-teal-green)
  chompi.colour(name): one colour to white instead (white green yellow orange red pink purple blue teal)
  chompi.afterglow(0-0.97): how long moving things leave glowing trails (0.8 default; 0.93 = comet tails)
  chompi.ripples(true/false): rings spread from each key you play (on by default); chompi.ambient(0-1): a faint shimmer

CALLBACKS (define the ones you need):
  init()                  once at the start
  redraw()                about 30 times a second: draw everything, then screen.update()
  note_on(note, vel)      a key played on CHOMPI (MIDI note 48 = C3 .. 72 = C5; Choowi's harmony may add chord notes)
  note_off(note)
  enc(n, d)               a CHOMPI knob turned: n = 1-4 (SW1-SW4), d = steps (+/-)
  key(n, z)               a CHOMPI button: n = 1 PLAY, n = 2 LOOP; z = 1 pressed, 0 released
  cleanup()               when the script stops

TIME: clock.run(function() ... end) starts a coroutine; inside it clock.sync(beats) waits for the next multiple of
  that many beats on CHOMPI's tempo (1/4 = a sixteenth), clock.sleep(seconds) waits; clock.get_tempo() (BPM),
  clock.get_beats(). Never write a loop without clock.sync or clock.sleep in it.

SOUND: the script plays CHOMPI itself (its current sound, through its harmony and effects):
  midi.note(note, velocity, seconds)   midi.note_on(note, vel)   midi.note_off(note)   midi.cc(cc, value)
  Keep it musical: at most a few notes per beat; notes 36-96.

HELPERS: params:add_number(id, name, min, max, default) / add_option(id, name, {..}, default) / add_control(id, name,
  min, max, default); params:get(id) / set(id, v) / delta(id, d) / string(id)
  util.clamp(v, lo, hi)  util.linlin(lo, hi, olo, ohi, v)  util.round(v, q)  util.wrap(v, lo, hi)
  musicutil.scale(root, "major" | "minor" | "dorian" | "pentatonic" | "minor_pentatonic", count)  musicutil.note_name(n)
  chompi.key_x(note): the x position (2-125) of a key C3..C5 across the screen;  chompi.held(): notes held now
  math.random, string, table, coroutine as in standard Lua. print(...) shows a line on the stage.

STYLE: vivid and alive, like a great norns script: something always moves (falling, swaying, pulsing on the beat with
clock.get_beats()), particles and bursts when things happen (cap particle lists, e.g. at 300), bright accents at level 15 over dimmer detail, and the
screen shows what the instrument is doing.
Label the knobs you use on screen (small text) if it helps. Comment the top with what each control does.

EXAMPLE (a complete, working script):
\`\`\`lua
${window.STAGE_EXAMPLES["rain garden"].trim()}
\`\`\`
`;
