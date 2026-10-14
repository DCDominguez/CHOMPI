# Stage scripts (developer note; the stage is a secret, keep it out of the player manuals)

Status: **step 1 built and software-tested in the browser (2026-10-14); not yet played with CHOMPI.** The owner's
idea: turn the stage into a norns-style instrument screen that anyone can program by describing it to a chatbot.

## What it is

The stage's dot screen is 128 x 64, exactly norns' screen. A **stage script** is a small Lua 5.3 program in the
shape of a norns script (`init`, `redraw`, `enc`, `key`, `note_on` / `note_off`, `cleanup`, `clock` coroutines,
`params`). It draws on the screen with 16 brightness levels (shown as glowing dots in one CHOMPI colour), hears
CHOMPI (keys, knobs SW1-SW4, PLAY / LOOP) and can **play CHOMPI** over MIDI: its notes go through Choowi's sound,
harmony and effects. The player writes nothing: the editor's **copy the AI prompt** button puts a prompt on the
clipboard (the whole API, the controls, the rules and a complete example); paste it into any chatbot, describe the
idea, paste the reply back and press **run**. If a script fails, **copy the error for the chatbot** gives the error
and the script back to the chatbot to fix.

## Using it

Stage (behind its word) → **script** (runs the script in the editor; first time: the rain garden) → **✎** opens the
editor: examples (rain garden, orbits), run, stop, the AI prompt, the error. The script and its state live in this
browser (localStorage, per viewer). **connect CHOMPI** first for the knobs, buttons and MIDI back.

Controls a script gets: `note_on(note, vel)` / `note_off(note)` from CHOMPI's keys (and Dream's keys through the
view); `enc(n, d)` from SW1-SW4 (CC 21, 22, 23, 20; CHOMPI sends absolute values, so `d` is the change since the
last value and stops at the knob's ends); `key(1, z)` PLAY, `key(2, z)` LOOP (CC 26 / 27). The sliders do the same
in the browser.

## The API (all a script can see)

- `screen.clear / level(0-15) / move / line / move_rel / line_rel / rect / circle / arc / stroke / fill / pixel /
  line_width / font_size / text / text_center / text_right / update`; `chompi.colour(name)` (the nine panel colours).
- `clock.run(f, ...)`, `clock.sync(beats)` (on CHOMPI's tempo, read from Choowivision's view), `clock.sleep(s)`,
  `clock.cancel(id)`, `clock.get_tempo()`, `clock.get_beats()`.
- `params:add_number / add_option / add_control / get / set / delta / string`.
- `midi.note(note, vel, seconds)`, `midi.note_on`, `midi.note_off`, `midi.cc` (channel 1).
- `util.clamp / linlin / round / wrap`, `musicutil.scale / note_name`, `chompi.key_x(note)`, `chompi.held()`,
  `print` (to the editor's log); Lua's `string`, `table`, `math`, `coroutine`, `utf8`.

Not there: `os`, `io`, `load`, `require`, `debug`, `package`, Fengari's JavaScript bridge, softcut, engines (CHOMPI is
the engine), grid / arc.

## How it's built

- `docs/workshop/vendor/fengari-web.js`: Fengari 0.1.4 (Lua 5.3 in JavaScript, MIT; licence beside it). Kept on the
  site, not loaded from a CDN.
- `docs/workshop/stage-lua.js`: the runtime. A fresh Lua state per run with only the safe libraries; a Lua prelude
  (clock scheduler, params, util, musicutil, midi, chompi); `screen` drawn on a hidden 128 x 64 canvas, copied on
  `screen.update()`; **an instruction budget per call** (400,000 Lua instructions via a count hook) so a runaway loop
  stops with "took too long" (also inside coroutines); the clock ticks every 5 ms; `redraw` at most 30 times a
  second. MIDI out: notes 0-127, at most 60 messages a second and 16 notes sounding; stop / a new run / a failure end
  every sounding note.
- `docs/workshop/stage-scripts.js`: the examples and the AI prompt (the prompt embeds the rain garden as its example).
- `docs/workshop/stage.js`: the "script" pattern (a script owns the screen: no ripples or scanline; Dream doesn't
  switch away from it), input routing, the tempo from the view (`parts_clock >>> 8 & 511`), the editor.

## Checked (browser, 2026-10-14)

Rain garden and orbits run and draw; demo notes plant seeds; the endless-loop guard fires in `init` and in a clock
coroutine; the sandbox has none of the globals above; runtime errors stop the script with "redraw: line 1: ..."; MIDI
out sends the expected notes (an octave up, off after 0.3 s) and stop ends them; a chatbot reply's last ```lua block is
extracted. Also fixed: the stage threw at start-up in a 0 x 0 window (a hidden tab), which stopped the page.

**Not yet:** playing a script with CHOMPI connected (knobs, buttons, MIDI back, tempo), the editor on a phone, a
chatbot-written script end to end.

## Next steps

1. Play it with CHOMPI; tune the prompt with real chatbot replies.
2. Port keyfall and tide to Lua (more examples for the prompt, and proof the API covers them).
3. Save / share scripts (a file, a link), a small library.
4. Maybe: knob labels on screen by convention, a params page on the stage, more musicutil.
