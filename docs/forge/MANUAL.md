# Forge for CHOMPI — user manual

Firmware **0.8** (development builds; experimental community firmware, not an
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
   - **Record:** toggle **down**, **hold CHOMPI**, make a sound, release — the keys
     now play your recording (section 7).
   - **Recall a saved preset:** toggle **up**, hold CHOMPI, press a lit white key,
     release (section 6).
   - **Send one from the computer:** connect USB, open Forge Bridge, choose a
     patch, *Send* (section 10).
4. Play the 25 keys. **SW5** press = stop all sound (panic) — while a loop exists,
   SW5 belongs to the looper; use CC 120 or the bridge's Panic instead.

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

| Control | Normal play | In the menu (toggle up + hold CHOMPI) |
| --- | --- | --- |
| 25 keys (C3–C5) | Play notes (velocity 100) | White keys pick slots; black keys are menu functions |
| CHOMPI key | Toggle **down** + hold = record | Toggle **up** + hold = open the menu; press = confirm |
| Toggle | Down = record position | Up = menu position |
| SW4, SW1, SW2, SW3 (knobs 1–4) | Turn: the knob's current page. Press: next page | Knob 1 turns the bank |
| SW5 | Turn: filter cutoff (or looper speed/scrub when a loop exists). Press: **panic** (stops all sound) / looper speed back to 1× | — |
| SW6 | Turn: output volume. Hold 2 s: battery light | — |
| PLAY (KEY_27), LOOP (KEY_28) | Looper (section 8) | Overdub feedback − / + |
| Line in jack | Plugging in selects line in for recording; unplugging selects the mic | — |

Lights: a held key lights white. In a sampler patch the keys also glow dim in
the bank's colour: in kit mode the keys that hold a sample (the recording key
pink when there is a recording), in chromatic mode C3, C4 and C5 as guides
(pink while playing the recording). The four knob lights show each knob's page; PLAY/LOOP show the looper;
the CHOMPI light shows recording (red), saving (pink blink), install (white blink)
and results (green/red flash).

---

## 5. Knobs and knob pages

Press a knob to step its page (1 → 2 → 3 → 4 → 1). Its light shows the page:

| Page | Light | Knob 1 (SW4) | Knob 2 (SW1) | Knob 3 (SW2) | Knob 4 (SW3) |
| --- | --- | --- | --- | --- | --- |
| 1 | dim white | the patch's knob 1 | the patch's knob 2 | the patch's knob 3 | the patch's knob 4 |
| 2 | red | filter cutoff | attack | LFO rate | delay mix |
| 3 | green | resonance | decay | LFO filter depth | delay feedback |
| 4 | blue | filter envelope amount | release | osc 2 detune (sampler: loop crossfade) | reverb mix |

Page 1 is chosen by the patch. If the patch doesn't choose, it is delay mix,
delay time, feedback and output level (sampler patches: pitch, start, end, delay
mix, like TAPE). Pages reset at power-on. Details: [KNOBS.md](KNOBS.md).

---

## 6. Presets on CHOMPI

8 banks × 15 slots on the SD card. They start empty — save sounds into them first.

Open the menu: **toggle up, hold CHOMPI.** While it is open, occupied slots are
lit dim, the last recalled one white, the bank keys show the bank's colour.

| To | Do |
| --- | --- |
| **Recall** | Hold CHOMPI, press a white key, release CHOMPI |
| **Change bank** | KEY_16 / KEY_17 (down / up), or turn knob 1 |
| **Save** the current sound | KEY_25, release CHOMPI, press a white key (turns blue), press CHOMPI → green flash |
| **Copy** | KEY_24, source white key (green), change bank if you like, destination key (blue), CHOMPI |
| **Erase** | KEY_23, white key (red), CHOMPI |
| Cancel a save/copy/erase | Press the same function key again |
| Close the menu | Release CHOMPI with nothing pending, or move the toggle down |

All white keys red = no usable SD card. **MIDI program change** (channel 1)
recalls slots too: program = (bank − 1) × 15 + (slot − 1).

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
- Samples go through the filter, envelopes, LFO, delay and reverb. Knob page 1 on
  a sampler patch: pitch, start, end, delay mix.

### Samples page (menu → KEY_22)
| Key | Does |
| --- | --- |
| KEY_16 | Chromatic (press again: next bank a–e) |
| KEY_17 | Kit (press again: next bank a–e) |
| White key | Chromatic: load and play that slot. Kit: pick a slot for save/copy/erase |
| KEY_18 / 19 / 20 | Record from mic / line in / resample (the instrument's own output) |
| KEY_25 | Save the recording into a slot (white key, CHOMPI) |
| KEY_24 | Copy a sample (source, destination, CHOMPI); press LOOP as the source to save the loop |
| KEY_23 | Erase a sample (white key, CHOMPI) |

### Recording
Toggle **down**, **hold CHOMPI**: records from the chosen source (CHOMPI light red;
you hear the input). Release to stop — the keys play the recording at once,
normalised. Up to about 87 s. It stays in memory until power-off; save it to a slot
(KEY_25) to keep it.

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
| **SW5** turn / press | Playing: loop speed (below zero = reverse) / back to 1×. Paused: scrub |

Up to about 83 s. Everything you hear goes into the loop. In the menu: PLAY/LOOP
lower/raise the overdub feedback; KEY_21 = effects before the loop (default),
KEY_20 = effects after it. Panic pauses the loop without losing it. Save the loop
as a sample: Samples page, KEY_24, LOOP, a white key, CHOMPI.

---

## 9. MIDI

Channel **1**, over USB or the MIDI jack.

| Message | Does |
| --- | --- |
| Notes 0–127 | Play (keys send nothing out) |
| Pitch bend | ±2 semitones |
| CC 1 | Mod wheel (LFO depth, when the patch uses it) |
| CC 20–23 | Knobs 1–4, page 1 controls |
| CC 24 | SW5 (cutoff, or looper speed while a loop exists) |
| CC 25 | SW6 (output level) |
| CC 26 / 27 | Looper PLAY / LOOP (≥ 85 press, ≤ 41 release) |
| CC 64 | Sustain pedal |
| CC 71 / 74 / 91 | Resonance / cutoff / reverb mix |
| CC 85 | Delay bypass (≥ 64 on) |
| CC 120, 123 | Panic |
| CC 121 | Reset controllers |
| Program change 0–119 | Recall a device preset (section 6) |

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

Forge never changes TAPE's `presets.json` or `options.json`.

---

## 12. Troubleshooting

| Problem | Try |
| --- | --- |
| No sound from the keys | Load a sound first (section 1); turn SW6 up; cable in the headphone or main out |
| Howling / feedback | The built-in mic is live when nothing is in line in; use headphones or lower the volume |
| Stuck notes or runaway echo | Press **SW5** (panic); with a loop present, clear or pause the loop first, or use the bridge's Panic |
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
