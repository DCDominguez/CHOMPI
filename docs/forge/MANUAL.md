# Forge for CHOMPI — user manual

Firmware **0.15.1** (development builds; experimental community firmware, not an
official CHOMPI release). Forge turns CHOMPI into a playable synthesizer,
TAPE-compatible sampler and looper whose sounds you can design on a computer —
by hand or by describing them to an AI — and send over USB.

What has been checked on real hardware is listed in
[TEST_RESULTS.md](TEST_RESULTS.md); anything not listed there is software-tested
only. Keep a backup of your SD card.

---

## 1. Quick start

1. Install Forge on CHOMPI (section 2).
2. Plug headphones into CHOMPI and switch it on. Turn **SW6** (volume) up a little.
3. Forge starts as a clean pass-through effect: line in (or the built-in mic when
   nothing is plugged in) goes to the output. **The keys play nothing until a
   sound is loaded.** Load one of these ways:
   - **Record:** toggle **up**, **hold CHOMPI** through the short red count-in, make a sound, release — the keys
     now play your recording (section 7).
   - **Recall a saved preset:** toggle **down**, hold CHOMPI (the menu opens on TAPE's
     page), **hold KEY_22 for 1 s** (Forge's presets page), press a lit white key, release
     (section 6).
   - **Send one from the computer:** connect USB, open Forge Bridge, choose a
     patch, *Send* (section 10).
4. Play the 25 keys. Hold **SW4 and SW3** together for a second to stop all sound (panic).

---

## 2. Installing and updating

### First install (SD card, once)
1. Back up the card (copy everything to the computer).
2. In the card's top folder, rename or remove **every file whose name contains
   `.bin`** — the bootloader installs the first one it finds, even
   `something.bin.old`. Example: `CHOMPI_TAPEv2_0.bin` → `CHOMPI_TAPEv2_0_bin.old`.
3. Copy `FORGE.bin` (from the Forge Bridge download, section 10) to the card's top
   folder. Also copy your TAPE samples (`jammi_…`, `cubbi_…`) there if you want them.
4. Card into CHOMPI, plugged into a **USB-C charger**, power on: rainbow lights
   while the bootloader installs, then Forge starts.

**Power before any install (SD card or USB):** charge first. The battery light (hold
SW6 for 2 s) must be **green or white**, or CHOMPI must be on a USB-C to USB-C charger
(2 A or more). CHOMPI has **one USB port**: a USB install needs the computer's cable in
it, so it cannot sit on a charger at the same time — charge until the battery light is
green or white, then connect a **computer USB-C port with a USB-C to USB-C cable** (a
USB-A port or a C-to-A cable is a weak "legacy" supply). Forge refuses an install
otherwise (section 3). With a low battery on a computer's USB port, CHOMPI's bootloader
waits with every light off after the restart and stays dark until the power switch is
turned off and on (DC, 2026-10-05). If that happens: plug in a USB-C charger, switch
off, wait 5 s, switch on.

### Updates (over USB — no card swap; firmware 0.7 or newer)
1. Open **Forge Bridge.exe** (it contains the matching firmware), **Connect CHOMPI**.
2. **Card & firmware → Install this kit's firmware.** From 0.11 CHOMPI refuses
   (before anything is copied) unless the battery reads green or white, or the USB
   supply is strong (a USB-C port that never hit its current limit, not a USB-A /
   legacy source); the bridge says so. Then disconnect, charge on a USB-C charger until
   the battery light is green or white, and reconnect the computer (above).
3. When CHOMPI's key **blinks white**, press it within 15 s.
4. CHOMPI restarts, the bootloader installs (rainbow), Forge starts. *Connect
   CHOMPI* shows the new version.

The new file only replaces the old one after it arrived complete and checked; a
failed transfer changes nothing. Other `.bin` files on the card are renamed so the
bootloader ignores them (`NAME.bin` → `NAME_bin.old`).

### Back to stock TAPE
Rename `CHOMPI_TAPEv2_0_bin.old` back to `CHOMPI_TAPEv2_0.bin`, rename or remove
`FORGE.bin`, power on. (Factory card contents: `firmware/card-profiles/`.)

---

## 3. Power and battery

- **On/off: the power switch.** Charging still works with it off.
- **Charging:** plug USB into a wall charger or computer. A wall charger is faster.
- **Battery level:** hold the **SW6** knob down for 2 s — its light shows
  **white** = charged (on the charger), **green** = good, **yellow** = low
  (below about 3.3 V). Release to hide it.
- **Low battery:** the knob and CHOMPI lights flash amber for 15 s, then CHOMPI
  shuts itself down. Plugging in during the flashing cancels it. After this shutdown the switch
  alone won't wake it — **plug in USB**.
- **Storage mode** (as stock): hold **CHOMPI + PLAY + LOOP** while switching on.
  The battery is disconnected for long storage; plugging in USB should bring it
  back (expected from the charger chip; not yet tried on Forge).
- On a weak USB port (a computer, or a USB-A cable) with a low battery, CHOMPI's
  stock protection switches the lights off and stops it **at once**, without the
  amber warning; after an install or a restart it stays dark until the power switch
  is turned off and on (DC, 2026-10-05). Use a USB-C to USB-C charger (2 A or more):
  plug it in, switch off, wait 5 s, switch on.
- **Warning (0.12):** while the battery is yellow and CHOMPI is not on a strong
  charger, the **SW6 light blinks yellow twice every 4 s**; when a reading drops
  below the shut-off mark it **blinks fast** — charge now. Nothing blinks on a
  USB-C charger (it is charging). *Check setup* reports the same, and firmware
  installs are refused then (section 2).

---

## 4. The panel

Forge keeps TAPE's panel workflow (firmware 0.10). The toggle has two positions:
**down = menu position**, **up = record position**.

| Control | Normal play | In the menu (toggle down + hold CHOMPI) |
| --- | --- | --- |
| 25 keys (C3–C5) | Play notes (full velocity, as TAPE) | The menu opens on **TAPE's page** (section 7: sample slots, KEY_21 / KEY_22 taps place the effects). Hold KEY_22 1 s: Forge's **presets page** (white keys pick slots, black keys are save / copy / erase / bank). Hold KEY_21 1 s: the **harmony page**, then KEY_21: the **parts page** (sections 8a, 8b). KEY_22 goes back to TAPE's page |
| CHOMPI key | Toggle **up** + hold = record (after a 1.5 s count-in) | Toggle **down** + hold = open the menu; press = confirm |
| SW4, SW1, SW2, SW3 | Turn: the knob's current page. Press and release: next page. Hold 1.5 s: reset that control | SW4 turns the bank |
| SW4 + SW3 held 1 s | **Panic** (stops all sound) | — |
| SW5 | Turn: filter cutoff. Push and turn (with a loop): loop speed, scrub when paused. Click: loop speed back to 1× | — |
| SW6 | Turn: volume. Short press: switch to input gain (light blue → red) and back. Hold 2 s: battery light | — |
| PLAY (KEY_27), LOOP (KEY_28) | Looper (section 8) | Overdub feedback − / + |
| Line in jack | Plugging in selects line in for recording; unplugging selects the mic | — |

Lights (as TAPE): a held key lights white. In a sampler patch the keys also glow
dim in the bank's colour: in kit mode the keys that hold a sample (the recording
key pink when there is a recording), in chromatic mode C3, C4 and C5 as guides
(pink while playing the recording). The knob lights show each knob's value in
TAPE's colours (section 5). In the **record position** the knob lights are off,
PLAY/LOOP are dimmed and the CHOMPI light is the **input level meter** (dim white
at silence, green → yellow → pink with level); in the menu position the CHOMPI
light is purple while you hold it and otherwise shows the event recorder (section 8c:
orange blinking on the beat when armed, orange recording, dim green playing, yellow
overdubbing; off when it is empty or stopped). The CHOMPI light also shows recording
(red), the count-in (red blinks), saving (pink blink), install (white blink) and
results (green/red flash).

---

## 5. Knobs and knob pages

The knobs follow TAPE: TAPE's pages first, then Forge's extra synth controls. Press
and **release** a knob to go to its next page; **hold it 1.5 s** (without turning)
to put its control back to the preset's value (its light flashes white).

| Knob | Sampler patch | Synth patch |
| --- | --- | --- |
| SW4 | pitch · gain · resonance · filter envelope | same |
| SW1 | sample start · attack · LFO speed | attack · decay · LFO speed |
| SW2 | sample end · release · LFO filter · loop crossfade | release · sustain · LFO filter · osc 2 detune |
| SW3 | reverb + delay · saturation · DJ filter | same |

- **Pitch** (TAPE): the light is green-yellow at normal speed; turning left slows
  down, past the centre the sample plays **backwards**, turning right speeds up to 2×.
- **Reverb + delay** (TAPE): one knob for both. **Saturation**: TAPE's "lofi"
  drive. **DJ filter**: left of centre low-pass, right of centre high-pass.
- One click moves 3 % on SW1–SW3 (TAPE's step), so single clicks are audible.
- A patch can add its own control as each knob's **last page** (dim white light;
  the webapp's *Panel knobs*, or the AI). Effects-only sounds (Dry, Slap, Long
  echoes) put the delay on the knobs instead: SW4 delay mix / level, SW1 delay
  time, SW2 feedback, SW3 TAPE's effects on the line input.
- Pitch, gain, saturation and the DJ filter are performance settings, as on TAPE:
  they stay when you change presets and reset when CHOMPI is switched on. They are
  not saved in presets.

Details: [KNOBS.md](KNOBS.md).

---

## 6. Presets on CHOMPI

8 banks × 15 slots on the SD card. They start empty — save sounds into them first.

Open the menu: **toggle down, hold CHOMPI.** It opens on TAPE's page (section 7);
**hold KEY_22 for 1 s** to reach Forge's presets page (a tap on the presets page goes
back; the menu remembers the last page). On the presets page occupied slots are lit
dim, the last recalled one white, the bank keys show the bank's colour.

| To | Do |
| --- | --- |
| **Recall** | Hold CHOMPI, press a white key, release CHOMPI |
| **Change bank** | KEY_16 / KEY_17 (down / up), or turn knob 1 |
| **Save** the current sound | KEY_25, release CHOMPI, press a white key (turns blue), press CHOMPI → green flash |
| **Copy** | KEY_24, source white key (green), change bank if you like, destination key (blue), CHOMPI |
| **Erase** | KEY_23, white key (red), CHOMPI |
| Cancel a save/copy/erase | Press the same function key again |
| Close the menu | Release CHOMPI with nothing pending, or move the toggle up |

All white keys red = no usable SD card. **MIDI program change** (channel 1)
recalls slots too: program = (bank − 1) × 15 + (slot − 1).

**Starter presets:** Forge Bridge → *Card & firmware* → choose a bank →
*Load starter presets* writes 12 sounds into slots 1–12 of that bank: 1 Dry,
2 Slap echo, 3 Long echoes, 4 Glass Keys, 5 Soft Pad, 6 Saw Bass, 7 Warm Pad,
8 Acid Bass, 9 Bell Keys, 10 Recorded Keys (plays the recording), 11 TAPE Kit A
(needs the TAPE samples), 12 Knob Pad. Slots already holding a preset are kept,
never overwritten. No firmware update needed (it uses the normal store request).

CHOMPI always starts in the pass-through mode; recall a preset after switching on.
TAPE's own presets are not used by Forge.

---

## 7. Samples and recording

### Sample files
Forge plays TAPE's sample files from the card's top folder:
`jammi_<a–e><1–14>.wav` (chromatic) and `cubbi_<a–e><1–14>.wav` (kit), e.g.
`cubbi_a1.wav`. Names must be exact (capitals are fine). Any normal WAV works:
8/16/24-bit or float, mono or stereo, 8–96 kHz. Copy files with the card in a
computer, or over USB with Forge Bridge (*Card & firmware*, section 10).

### Playing samples
- **Chromatic:** one sample across the keys; the middle C key plays it at its own
  pitch.
- **Kit:** the white keys play slots 1–14 of the bank (top C = the recording);
  black keys are silent.
- Samples go through the filter, envelopes, LFO, TAPE's effects, delay and reverb.
  Knob page 1 on a sampler patch is TAPE's: pitch (SW4), start (SW1), end (SW2),
  reverb + delay (SW3).

**Each slot remembers its settings (0.12, as TAPE).** Pitch, gain, start, end,
attack, release, pan, auto-loop and sustain belong to the sample slot, saved the
moment you turn or press them, in TAPE's own `presets.json` on the card. TAPE and
Forge share it: what you set in one is there in the other.
- **Chromatic:** choosing a slot in the menu brings its settings back (a slot
  never set starts from TAPE's defaults). Recalling a Forge preset that uses the
  slot also brings the slot's settings; the preset sets everything else (filter,
  LFO, effects…).
- **Kit:** every pad has its own. Play a pad, then turn: the knobs edit that pad
  (its lights show its values). A pad never set follows the preset.
- Saving the recording to a slot gives the slot the recording's settings; copying
  a slot copies them; erasing clears them (TAPE's menu).
- The first time Forge writes `presets.json` it copies the card's original to
  `FORGE/presets_backup.json`.

### TAPE's menu page (the menu opens here)
| Key | Does |
| --- | --- |
| KEY_16 | Chromatic (press again: next bank a–e) |
| KEY_17 | Kit (press again: next bank a–e) |
| White key | Chromatic: load and play that slot. Kit: pick a slot for save/copy/erase |
| KEY_18 / 19 / 20 | Record from mic / line in / resample (the instrument's own output) |
| KEY_25 | Save the recording into a slot (white key, CHOMPI) |
| KEY_24 | Copy a sample (source, destination, CHOMPI); press LOOP as the source to save the loop |
| KEY_23 | Erase a sample (white key, CHOMPI) |
| KEY_21 / KEY_22 (tap) | Effects before / after the looper (TAPE). Hold KEY_22 1 s: Forge's presets page. Hold KEY_21 1 s: the harmony page (section 8a); from there KEY_21 = the parts page (section 8b) |

While the menu is open the knobs are TAPE's second layer:

| Knob | Turn | Press |
| --- | --- | --- |
| SW4 | Pitch in fifths and octaves (4 clicks per step); on page 2: pan | Pitch back to 1× (page 2: gain and pan back) |
| SW1 / SW2 | Move the start–end window together (synth: attack and release together) | SW1: auto-loop on/off · SW2: sustain on/off (lights white when on) |
| SW3 | Delay time (page 2: warble, page 3: DJ filter resonance) | Every effect back to its default |
| SW5 | Loop speed, also while paused | Speed back to 1× |
| SW6 | Output compressor | Next monitor position: orange headphones, blue both, yellow send/return |

**Monitor positions (TAPE):** *headphones* (default) — in the record position you
hear the input in the headphones only; *both* — the input is always heard, through
the effects and the looper, on both outputs; *send/return* — the mic goes through
the effects in the record position, line in always returns to the headphones.

### Recording
1. Toggle **up** (the record position). The CHOMPI light becomes the input meter
   and you hear the input in the **headphones** (TAPE's default monitoring).
2. **Hold CHOMPI.** CHOMPI and the white keys blink red three times (1.5 s
   count-in); letting go during the count-in records nothing.
3. Recording starts when the blinking stops (CHOMPI light red). Release to stop —
   the keys play the recording at once, chromatically, normalised, with pitch,
   gain, start and end back to normal (as TAPE).

Too quiet or distorted? Short-press **SW6** and turn it: input gain (light blue →
red; 75 % at power-on, as TAPE); short-press again for the volume. Up to about
87 s. The take stays in memory until power-off; save it to a slot (KEY_25) to keep it.

### Samples on the card from the computer
- **Add WAV files:** put them, named as above, in the `card` folder next to
  `Forge Bridge.exe` (the bridge shows the folder; *Refresh list* creates it), then
  *Card & firmware* → *Copy selected to CHOMPI*.
- **See, play, save or erase what is on the card:** open the sound designer
  (section 10), scroll to **Device samples · SD card**, press **Read samples**.
  Choose chromatic or kit and a bank a–e; the slots show what is on the card.
  *Use in patch* plays a slot; *Save recording here* stores your last CHOMPI
  recording into the selected slot; *Erase sample* deletes it.

---

## 8. Looper

| Do | Result |
| --- | --- |
| Tap **LOOP** | Start recording the first take (PLAY light teal, LOOP red) |
| Tap **PLAY** | End the take; the loop plays |
| Tap **LOOP** instead | End the take and go straight into overdub (TAPE habit) |
| Tap **LOOP** while playing | Overdub on/off (LOOP light yellow) |
| Tap **PLAY** while playing | Pause / resume |
| Hold **PLAY** 2 s while paused | Back to the start |
| Hold **PLAY + LOOP** 2 s | Clear the loop |
| PLAY + LOOP with no loop, then play a key | Armed: recording starts with the first note |
| **SW5** push and turn / click | Playing: loop speed (below zero = reverse) / back to 1×. Paused: scrub. A plain turn is always the filter cutoff |

Up to about 83 s. Everything you hear goes into the loop. In the menu: PLAY/LOOP
lower/raise the overdub feedback; on TAPE's page KEY_21 = effects before the loop
(default), a KEY_22 tap = after it (on Forge's presets page: KEY_21 / KEY_20). Panic pauses the loop without losing it. Save the loop
as a sample: TAPE's menu page, KEY_24, LOOP, a white key, CHOMPI.

---

## 8a. Harmony (one key plays a chord) — firmware 0.13

With harmony on, each key plays a whole chord in a key you choose, through the
instrument's own sound and to MIDI out (the chord's notes, not the key you
pressed). It works with synth patches and the chromatic sampler; kit mode and
the stereo-delay (v1) patches play keys as usual. Chords have up to 5 notes and
never more than the patch's voices. *Software-tested only; not yet heard on
CHOMPI (TEST_SESSION 3.65–3.67).*

**Open the harmony page:** toggle down, hold CHOMPI (TAPE's menu page), then
**hold KEY_21 for 1 s**. A tap on KEY_22 goes back to TAPE's page. On the harmony
page the keys play nothing; **a key sets the key (tonic)** to its note. The key
lights show the key: the tonic white, the rest of its scale blue (dimmer while
harmony is off).

| Knob | Turn (3 clicks per step, stops at the ends) | Press | Light |
| --- | --- | --- | --- |
| SW4 | Mode: major, natural minor, harmonic minor, melodic minor, dorian, phrygian, lydian, mixolydian, locrian | Harmony on / off | green on, red off |
| SW1 | Chord size: fifth (power chord), triad, 7th, 9th, 11th, 13th | Layout Static / Real | white Static, orange Real |
| SW2 | Inversion 0–3 (used when voice leading is off) | Voice leading on / off | white on, purple off |
| SW3 | — | Open spread on / off | white on, dim off |

**Static layout** (the default): the white keys C3–B4 are the scale's chords I to
vii, twice (lower and upper register), whatever the key; **C5 is Shift**. The black
keys are colour chords: lower octave V/ii, V/iii, V/V, V/vi and the parallel
mode's vii (in major: ♭VII); upper octave the parallel mode's iv, iii and vi (in
major: iv, ♭III, ♭VI), V/IV and ♭II. Hold **Shift** while playing a key to change its
chord: a dominant becomes its tritone substitute, a major chord sus4, a minor
chord the dominant on the same root (ii → V/V), a diminished chord the key's V7.

**Real layout:** the key you press is the chord's root. Notes in the scale play
that degree's chord; other notes play the parallel mode's chord on that root,
else a dominant that falls a fifth to a degree, else a major chord. Real also
applies to notes arriving over MIDI; Static maps only C3–C5 (keys or MIDI), and
notes outside that range play as usual.

**Voice leading** (on by default) picks each chord's inversion so it moves as
little as possible from the last one. Turn it off to choose the inversion with SW2.
**Open spread** lifts the second-lowest note an octave (chords of 3 or more notes). Bigger chords drop notes in this
order when the voices run out: the 5th first, then the middle extensions; the
root, 3rd, 7th and top extension stay.

The settings are part of a **v6 patch** (the webapp's *Harmony* group, the AI, or
`forge_host.py`) and are saved with presets stored on CHOMPI. Panic ends held
chords. The Inspector shows the last chord by name and number
(e.g. *Am7 · I · tonic*).

---

## 8b. Arp, bass and tempo — firmware 0.14

An arpeggiator plays the held keys (or, with harmony on, the chord) one note at a time
on CHOMPI's clock; a bass part plays the chord's root under it. Both use the instrument's
own sound and also go to MIDI out: the arp (and the keys) on the MIDI out channel, the
bass on the next channel up (channel 2 by default). Synth patches and the chromatic
sampler; kit patches play as before. *Software-tested only (TEST_SESSION 3.68–3.72).*

**Open the parts page:** open the harmony page (section 8a: toggle down, hold CHOMPI,
hold KEY_21 1 s), then **tap KEY_21**. KEY_21 switches between the harmony and parts
pages; KEY_22 goes back to TAPE's page. The keys light what is chosen.

| Keys | Choose |
| --- | --- |
| C3 · D3 · E3 · F3 · G3 · A3 (lit blue) | Arp off · up · down · up-down · as played · random |
| B3 (white = on) | Latch: the arp keeps playing after you let go, until the next chord (on by default) |
| C4 · D4 · E4 · F4 · G4 · A4 (lit green) | Arp rate 1/4 · 1/8 · 1/8 triplet · 1/16 · 1/16 triplet · 1/32 |
| B4 (green / red) | Clock running / stopped |
| C#3 · D#3 · F#3 · G#3 · A#3 (lit orange; brighter = higher octave) | Bass off · root · root + fifth · root then fifth · root then octave |
| F#4 · G#4 · A#4 | Event recorder: record / play, overdub, clear (section 8c) |
| C5 (blinks on the beat) | **Tap tempo**: tap it on the beat (two taps or more) |

| Knob | Turn | Press |
| --- | --- | --- |
| SW4 | **Tempo**, 1 BPM a click (40–300) | Tap tempo |
| SW1 | Arp octaves 1–4 (3 clicks a step; white, green, yellow, red) | — |
| SW2 | Arp gate (each note's length) 5–100 %, 5 % a click (brighter = longer) | — |
| SW3 | Bass rate: once per chord change (purple), 1/2 (blue), 1/4 (green), 1/8 (yellow) | Bass octave C1 → C2 → C3 (0.15; the bass key gets brighter) |

**Playing:** with the arp on, hold keys (or a chord key with harmony on): the notes cycle
from the first one at once, then on the clock. Adding keys while holding adds notes; after
letting go of all keys, the next key starts a new set. Random uses a fixed seed, so the
same keys play the same "random" line every time (the webapp can choose the seed). With
the arp off, the keys play as usual and the bass (if on) plays under them. Panic (SW4 + SW3
held 1 s with the menu closed) stops the arp, the bass and any latched notes.

**Tempo:** the tempo is set here (SW4, tap) or by a v7 patch, and is saved with presets
stored on CHOMPI. When MIDI clock arrives (USB or the MIDI jack, any channel), the parts
follow it, with MIDI start / stop / continue; the SW4 light blinks blue then. Half a second
without clock and CHOMPI's own tempo takes over again. While the parts play on CHOMPI's
own tempo it sends MIDI clock (start, 24 ticks per beat, stop) so other gear can follow
(the webapp can turn that off).

---

## 8c. Event recorder and projects — firmware 0.15

The event recorder records what you play — keys, MIDI notes coming in and knob moves —
as notes and control changes on CHOMPI's clock (not audio), and loops it over whole bars
(1–8). It plays back like a second player: through harmony, the arp and the bass, and out
of MIDI. The audio looper (PLAY / LOOP) is separate and works as before. *Software-tested
only (TEST_SESSION 3.73–3.76).*

On the parts page (section 8b):

| Key | Does |
| --- | --- |
| F#4 | **Record**: arms (red, blinking on the beat); recording starts at the next bar line. Press again while recording: it closes at the next bar line and loops (green). While it plays: stop (dim green); again: play from the start |
| G#4 | **Overdub** on / off while it plays (yellow): what you play is added from the next pass |
| A#4 | **Clear**: press twice within 2 s (lights white after the first press) |

Playing is done with the menu closed: arm on the parts page, let go of CHOMPI, play; to
close the loop, open the menu again and press F#4. With the toggle down and the menu
closed, the CHOMPI light shows the recorder: orange blinking on the beat when armed,
orange while recording, dim green while it plays, yellow while overdubbing. A take closes
by itself after 8 bars. Notes still held when a take or overdub ends get their end just
before the loop point, so nothing hangs.

What is recorded: notes (with velocity) from the keys and MIDI in, and every knob / CC
move except the output volume and input gain. On sampler patches the per-slot sample
settings (pitch, start, end, attack, release, gain, pan) are not recorded, because playing
them back would keep rewriting the slot's saved values. Timing is on the clock's grid
(24 steps per beat). Panic stops the recorder (the loop is kept; F#4 plays it again).
MIDI start restarts it from its first bar. Up to 1,024 events (the Inspector counts any
beyond).

**Projects:** saving a preset on CHOMPI (section 6) also saves the recorded loop beside it
(`FORGE/BbSss.FSQ`), with the patch, harmony, arp, bass and tempo in the preset: together
that is a project. Recalling the slot brings the loop back and plays it. A slot without a
loop leaves the current loop playing, so you can change the sound under a running loop.
Copying a preset copies its loop; erasing it erases the loop. Saving with an empty
recorder removes the slot's old loop.

---

## 9. MIDI

Channel **1** in and out, over USB or the MIDI jack, unless TAPE's `options.json`
on the card sets other channels (section 11).

**MIDI in:**

| Message | Does |
| --- | --- |
| Notes 0–127 | Play |
| Pitch bend | ±2 semitones |
| CC 1 | Mod wheel (LFO depth, when the patch uses it) |
| CC 20–23 | The patch's knob controls (its last page), or the source's defaults |
| CC 24 | SW5 (cutoff, or looper speed while a loop exists) |
| CC 25 | SW6 (output level) |
| CC 26 / 27 | Looper PLAY / LOOP (≥ 85 press, ≤ 41 release) |
| CC 64 | Sustain pedal |
| CC 71 / 74 / 91 | Resonance / cutoff / reverb mix |
| CC 85 | Delay bypass (≥ 64 on) |
| CC 120, 123 | Panic |
| CC 121 | Reset controllers |
| Program change 0–119 | Recall a device preset (section 6) |
| Clock, start, continue, stop (any channel) | The arp and bass follow the clock (section 8b) |

**MIDI out (as TAPE):** the keys send notes (velocity 127); turning a knob on TAPE's
pages sends TAPE's CC (SW4/SW1/SW2/SW3 page 1: CC 20–23, page 2: CC 28–31, SW3
page 3: CC 33), SW5 CC 24, SW6 CC 25 (volume) or 32 (input gain); PLAY CC 26 and LOOP
CC 27 (127 pressed, 0 released, also while the menu is open); CHOMPI CC 21 in the
record position. Forge's extra knob pages send nothing, nor do the knobs on TAPE's menu
page (they set TAPE's shift layer); on the presets, harmony and parts pages a knob sends
its CC only if it changes that control. With the arp or bass on (section 8b): the arp notes on
the out channel, the bass on the next channel, and MIDI clock while CHOMPI keeps the tempo.

---

## 10. On the computer

### Forge Bridge (Windows)
Download: GitHub → *DCDominguez/CHOMPI* → **Actions** → *Forge Bridge (Windows
exe)* → newest green run → **Forge-Bridge-exe**. Unzip into its own folder and
double-click **Forge Bridge.exe** (if Windows warns: *More info → Run anyway*).
Your browser opens the bridge; keep the black window open.

- **Connect CHOMPI** — finds CHOMPI's USB MIDI ports and shows the firmware version.
- **Test patch → Send selected patch** — load any built-in sound.
- **Card & firmware** — copy samples from the `card` folder next to the exe onto
  CHOMPI's card, and install firmware (section 2).
- **Check setup**, **Start panel walk**, **Run automatic checks** — testing tools;
  reports are saved in the `reports` folder next to the exe.

Close other MIDI programs first; only one program can use CHOMPI's MIDI port.

### The sound designer (webapp)
The bridge's main page — the same address in the browser without `/inspector`
(normally **http://127.0.0.1:8765/**).

1. **Author:** choose *Playable instrument* (or *External-audio delay*), an AI
   provider (OpenAI or Gemini) and **your own API key**, describe the sound,
   *Generate a patch*. The key stays in that page only; it is never saved.
2. **Refine:** or *Start from a preset*; edit every module; *Save JSON* / *Import
   JSON* keeps sounds on the computer.
3. **Play:** choose CHOMPI's MIDI ports, **Send to CHOMPI**. *Device presets* and
   *Device samples* manage what is stored on the card.

Nothing is sent to CHOMPI until you press Send.

### Built-in sounds
Dry, Slap, Long Echo (delay effects for line in); Glass Keys, Soft Pad, Saw Bass,
Warm Pad, Acid Bass, Bell Keys, Knob Pad (synths); Recorded Keys, TAPE Kit A
(samplers); CPU Stress and Sampler Stress (test patches).

---

## 11. What Forge keeps on the SD card

| File | What |
| --- | --- |
| `FORGE.bin` | The firmware the bootloader installs |
| `FORGE/B<bank>S<slot>.FPR` | Device presets |
| `FORGE/B<bank>S<slot>.FSQ` | The recorded loop saved with that preset (0.15, section 8c) |
| `jammi_…wav`, `cubbi_…wav` | Samples (TAPE's names and format; shared with TAPE) |
| `FORGE/UPLOAD.TMP`, `FORGE/TMP.FPR`, `FORGE_TMP.WAV` | Temporary files while writing (safe to delete when CHOMPI is off) |
| `NAME_bin.old` | Other firmware set aside by a USB install |
| `FORGE/RESTARTS.TXT` | One line per start: why CHOMPI started (power-on, brown-out, reset, software) and any crash Forge recorded; from 0.11 also "battery low" when the stock protection switched it off |
| `presets.json` | TAPE's per-slot sample settings, shared with TAPE (Forge writes it from 0.12, in TAPE's format, via `presets_temp.json`) |
| `FORGE/presets_backup.json` | The card's `presets.json` as it was before Forge first wrote it |

Forge never changes TAPE's `options.json`. It **reads**
`options.json` at start-up (TAPE writes it with defaults the first time TAPE runs),
so one card sets both firmwares:

| Option | Forge |
| --- | --- |
| Record Latch | Press CHOMPI to start (after the count-in), press again to stop |
| Midi In / Out Channel | The channels above |
| Tape Slew On | Looper scrubbing glides like tape (on by default) |
| Monitor Position | 1 headphones, 2 both, 3 send/return (section 7; the menu's SW6 press changes it until power-off) |
| Pitch Quantize In Shift Menu | true (default): normal pages free, the menu in fifths/octaves; false: the other way round (pitch and loop speed) |
| Split Delay | SW3 page 1: left of centre = delay only, right = reverb only |

---

## 12. Troubleshooting

| Problem | Try |
| --- | --- |
| No sound from the keys | Load a sound first (section 1); turn SW6 up; cable in the headphone or main out |
| Howling / feedback | The built-in mic is live when nothing is in line in; use headphones or lower the volume |
| Stuck notes or runaway echo | Hold **SW4 + SW3** for a second (panic; a loop is paused, not lost), or the bridge's Panic |
| A knob seems to do nothing | Check its light: you may be on another page (press and release to step); hold it 1.5 s to reset it. Some pages need another control (e.g. LFO speed needs an LFO depth) |
| CHOMPI switched itself off or restarted | *Check setup* shows **Last start** (power-on, brown-out = the supply dipped, or a crash); the card keeps a line per start in `FORGE/RESTARTS.TXT`. Send both with your report. A low battery switches CHOMPI off as stock TAPE does: with no USB after 15 s of amber flashes; on a weak USB supply (computer port) at once, lights off. From 0.11 these leave a "battery low" line in `RESTARTS.TXT`; charge it, or play on a USB-C charger |
| Dark after an install or a restart; only the red charge light | The bootloader is waiting on a low battery with a weak USB supply. Plug in a USB-C to USB-C charger, switch off, wait 5 s, switch on |
| White keys all red in the menu | SD card missing or unreadable; reinsert it |
| A sample doesn't play | Exact name in the card's top folder (`cubbi_a1.wav`), not inside a folder |
| Bridge: *Select both MIDI ports* / CHOMPI not found | USB data cable (not charge-only); close other MIDI programs; Connect again |
| Bridge says numpy/sounddevice missing | Download the newest exe (fixed in the build after 2026-10-04) |
| Install: nothing happens | Press the CHOMPI key while it blinks white (15 s) |
| Dead after the battery ran out | Plug in USB (wall charger), wait, then switch on |
| Want stock TAPE back | Section 2, *Back to stock TAPE* |

---

## More

- What's been verified on hardware: [TEST_RESULTS.md](TEST_RESULTS.md)
- Knob pages in detail: [KNOBS.md](KNOBS.md) · Sampler: [SAMPLING.md](SAMPLING.md) ·
  Looper: [LOOPING.md](LOOPING.md)
- Differences from stock TAPE/WAVE/TEMPO: [COMPATIBILITY.md](COMPATIBILITY.md)
- The test bridge: [BRIDGE.md](BRIDGE.md) · Full hardware test list:
  [TEST_SESSION.md](TEST_SESSION.md)
- Wire protocol for developers: [PROTOCOL.md](PROTOCOL.md)
