# Forge for CHOMPI — user manual

Firmware **0.10** (development builds; experimental community firmware, not an
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
   - **Recall a saved preset:** toggle **down**, hold CHOMPI, press a lit white key,
     release (section 6).
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
4. Card into CHOMPI, power on: rainbow lights while the bootloader installs, then
   Forge starts.

### Updates (over USB — no card swap; firmware 0.7 or newer)
1. Open **Forge Bridge.exe** (it contains the matching firmware), **Connect CHOMPI**.
2. **Card & firmware → Install this kit's firmware.**
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
- On a weak USB port with a flat battery, CHOMPI may look dead while it charges.
  Give it time, preferably on a wall charger.

---

## 4. The panel

Forge keeps TAPE's panel workflow (firmware 0.10). The toggle has two positions:
**down = menu position**, **up = record position**.

| Control | Normal play | In the menu (toggle down + hold CHOMPI) |
| --- | --- | --- |
| 25 keys (C3–C5) | Play notes (full velocity, as TAPE) | White keys pick slots; black keys are menu functions |
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
light is off, purple while you hold it. The CHOMPI light also shows recording
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
| KEY_21 / KEY_22 (tap) | Effects before / after the looper (TAPE). Hold KEY_22 1 s: Forge's presets page |

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

**MIDI out (as TAPE):** the keys send notes (velocity 127); turning a knob on TAPE's
pages sends TAPE's CC (SW4/SW1/SW2/SW3 page 1: CC 20–23, page 2: CC 28–31, SW3
page 3: CC 33), SW5 CC 24, SW6 CC 25 (volume) or 32 (input gain); PLAY CC 26 and LOOP
CC 27 (127 pressed, 0 released); CHOMPI CC 21 in the record position. Forge's extra
pages and the menu send nothing.

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
| `jammi_…wav`, `cubbi_…wav` | Samples (TAPE's names and format; shared with TAPE) |
| `FORGE/UPLOAD.TMP`, `FORGE/TMP.FPR`, `FORGE_TMP.WAV` | Temporary files while writing (safe to delete when CHOMPI is off) |
| `NAME_bin.old` | Other firmware set aside by a USB install |
| `FORGE/RESTARTS.TXT` | One line per start: why CHOMPI started (power-on, brown-out, reset, software) and any crash Forge recorded |

Forge never changes TAPE's `presets.json` or `options.json`. It **reads**
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
| CHOMPI switched itself off or restarted | *Check setup* shows **Last start** (power-on, brown-out = the supply dipped, or a crash); the card keeps a line per start in `FORGE/RESTARTS.TXT`. Send both with your report |
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
