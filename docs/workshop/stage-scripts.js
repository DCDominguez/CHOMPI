"use strict";
// Stage scripts: the examples and the prompt a chatbot gets (the forge page's way: describe it, paste the reply).
window.STAGE_EXAMPLES = {
  "rain garden": `-- rain garden: every note you play plants a seed; a grown plant sings its note back, an octave up
-- SW1 growth speed · SW2 how many plants · LOOP clears the garden
plants = {}

function init()
  params:add_number("growth", "growth", 1, 10, 4)
  params:add_number("max", "plants", 1, 12, 6)
  chompi.colour("green")
end

function note_on(note, vel)
  if #plants >= params:get("max") then table.remove(plants, 1) end
  table.insert(plants, {x = chompi.key_x(note), h = 0, note = note})
end

function enc(n, d)
  if n == 1 then params:delta("growth", d) end
  if n == 2 then params:delta("max", d) end
end

function key(n, z)
  if n == 2 and z == 1 then plants = {} end
end

clock.run(function()
  while true do
    clock.sync(1/4)
    for _, p in ipairs(plants) do
      p.h = p.h + params:get("growth")
      if p.h >= 40 then midi.note(p.note + 12, 50, 0.3); p.h = 0; p.bloom = 6 end
    end
  end
end)

function redraw()
  screen.clear()
  for _, p in ipairs(plants) do
    screen.level(math.floor(p.h / 3) + 2)
    screen.move(p.x, 63); screen.line(p.x, 63 - p.h); screen.stroke()
    local b = p.bloom or 0
    screen.level(15); screen.circle(p.x, 63 - p.h, 1 + b / 2); screen.fill()
    if b > 0 then p.bloom = b - 1 end
  end
  screen.level(4); screen.move(2, 7); screen.text("growth " .. params:get("growth") .. "  plants " .. params:get("max"))
  screen.update()
end
`,
  orbits: `-- orbits: four rings turn at their own speeds; the last notes you played sit on them as stars and sound
-- each time a ring brings its star to the top. SW1-SW4 set the rings' steps per bar · LOOP clears
rings = {{steps = 3}, {steps = 4}, {steps = 5}, {steps = 7}}
stars, pos = {}, {0, 0, 0, 0}

function init() chompi.colour("teal") end

function note_on(note, vel)
  table.insert(stars, 1, note)
  if #stars > 4 then table.remove(stars) end
end

function enc(n, d)
  if rings[n] then rings[n].steps = util.clamp(rings[n].steps + d, 1, 16) end
end

function key(n, z) if n == 2 and z == 1 then stars = {} end end

for i = 1, 4 do
  clock.run(function()
    while true do
      clock.sync(4 / rings[i].steps)
      pos[i] = (pos[i] + 1) % rings[i].steps
      if pos[i] == 0 and stars[i] then midi.note(stars[i], 70, 0.15); rings[i].flash = 8 end
    end
  end)
end

function redraw()
  screen.clear()
  for i, r in ipairs(rings) do
    local radius = 6 + i * 6
    screen.level(stars[i] and 3 or 1); screen.circle(64, 32, radius); screen.stroke()
    local a = (pos[i] / r.steps) * 2 * math.pi - math.pi / 2
    screen.level(stars[i] and 15 or 5); screen.circle(64 + math.cos(a) * radius, 32 + math.sin(a) * radius, 2); screen.fill()
    if (r.flash or 0) > 0 then screen.level(r.flash + 7); screen.circle(64, 32 - radius, 3); screen.fill(); r.flash = r.flash - 1 end
  end
  screen.level(6); screen.move(2, 62); screen.text(rings[1].steps .. ":" .. rings[2].steps .. ":" .. rings[3].steps .. ":" .. rings[4].steps)
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
  chompi.colour(name): the dots' colour: white, green, yellow, orange, red, pink, purple, blue or teal

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

STYLE: like a good norns script: minimal, readable at a glance, the screen shows what the instrument is doing.
Label the knobs you use on screen (small text) if it helps. Comment the top with what each control does.

EXAMPLE (a complete, working script):
\`\`\`lua
${window.STAGE_EXAMPLES["rain garden"].trim()}
\`\`\`
`;
