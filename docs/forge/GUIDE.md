# Choowi: how do I…?

*A hands-on guide to Forge · Choowi (firmware 0.20.0) on CHOMPI. Hold CHOMPI, find what you
want to do, follow the steps. The full reference is the [manual](MANUAL.md).*

Forge is independent community firmware, not affiliated with or endorsed by CHOMPI Club. CHOMPI is a trademark of CHOMPI Club. Keep a backup of your SD card.

---

## before you start: names used here

**The toggle** has two positions:

- **down** = the **menu position**. This is where you play, and where CHOMPI opens the menu.
- **up** = the **record position**. Holding CHOMPI here records a sample.

**The keys.** The 25 keys run from C3 (left) to C5 (right). This guide gives each key its
panel name and its note, so you can find it either way.

| Name | Keys |
| --- | --- |
| KEY_1 … KEY_15 | the white keys, C3 to C5 (KEY_1 = C3, KEY_8 = C4, KEY_15 = C5) |
| KEY_16, KEY_17 | the first two black keys: C#3, D#3 |
| KEY_18, KEY_19, KEY_20 | the next three black keys: F#3, G#3, A#3 |
| KEY_21, KEY_22 | the first two black keys of the upper octave: C#4, D#4 |
| KEY_23, KEY_24, KEY_25 | the last three black keys: F#4, G#4, A#4 (KEY_25 is the rightmost black key) |
| PLAY, LOOP | the looper keys (KEY_27, KEY_28) |
| CHOMPI | the big CHOMPI key |

**The knobs.** SW4, SW1, SW2, SW3 are the four sound knobs. SW5 is the filter (and the loop's
speed). SW6 is the volume. Knobs can be pressed as well as turned.

---

## how do I switch on and play?

1. Plug headphones in, toggle **down**, switch CHOMPI on with the power switch.
2. Turn **SW6** up a little. That's the volume.
3. A rainbow runs across the keys. If you have used a preset before, CHOMPI loads the last
   one you recalled and plays a little chime through it. Its event recorder loop (if it
   has one) stays quiet until you recall that slot yourself.
4. Play the keys.

**The very first start** (no preset ever recalled) is a plain pass-through: the input (line
in, or the built-in mic when nothing is plugged in) goes straight to the output, and the keys
play nothing yet. Give them a sound one of these ways:

- hold **SW1 + SW3** for a second: a surprise sound ([get surprise sounds](#how-do-i-get-surprise-sounds-and-keep-one)),
- record something ([record and use samples](#how-do-i-record-and-use-samples)),
- recall a preset ([recall, copy and erase](#how-do-i-recall-copy-and-erase-presets)),
- or send one from a computer with Forge Bridge ([manual, section 10](MANUAL.md#10-on-the-computer)).

**Dream:** hold **SW4 + SW1** for a second and CHOMPI plays an ambient piece on its own, keys and
knobs glowing in the chord's colours. It follows your sound: pads get slow chords, plucky sounds
get drifting little clusters, a kit gets soft patter. Hold **SW4 + SW1** again (or press a key)
to stop it.
**Nap:** leave CHOMPI alone for two minutes and it hums quietly to itself until you touch
something (any MIDI arriving wakes it too). Details: [manual, section 5a](MANUAL.md#5a-surprise-me-and-mutate-017).

---

## how do I find my way around the menu?

The menu is where you save, recall, set chords, arps and design sounds.

1. Toggle **down**.
2. **Hold CHOMPI.** The menu is open while you hold it. The keys stop playing notes and show
   the page instead.
3. The **CHOMPI light shows which page** you are on:

| CHOMPI light | Page | What it's for |
| --- | --- | --- |
| teal | TAPE's page | samples, recording source, effects before / after the looper |
| blue | presets | save, recall, copy, erase |
| purple | harmony | chords |
| orange | parts | arp, bass, tempo, event recorder |
| pink | synth | design the sound |

4. **Let go of CHOMPI** to close the menu (unless a save, copy or erase is waiting for you to
   confirm; then the menu stays open).

**Moving between pages:**

- From **TAPE's page**, hold **KEY_22** (D#4) for 1 s: the **presets** page (blue).
- From **TAPE's page**, keep holding **KEY_21** (C#4): at 1 s **harmony** (purple), at 2 s
  **parts** (orange), at 3 s **synth** (pink). Let go when you reach the one you want.
- On the harmony, parts or synth page, a tap on **KEY_21** steps to the next of the three
  (harmony, parts, synth, harmony…).
- From any Forge page, a tap on **KEY_22** goes back to **TAPE's page**.

The menu **remembers the last page**: next time you hold CHOMPI it opens where you left it.

Careful on TAPE's page: a quick *tap* on KEY_21 or KEY_22 there is TAPE's own job for those
keys (effects before / after the looper). Hold them to change page.

To close the menu at any time: let go of CHOMPI, or move the toggle up.

---

## how do I save a sound I like?

**When to save:** whenever you hear something you want to keep. Changes made on the panel
(the synth page, the knobs, a surprise roll, harmony, the arp) change the sound you are playing
*now*. Nothing is kept until you save it into a preset slot. If you switch off, or recall
another preset, an unsaved sound is gone.

There are 8 banks of 15 slots. Each white key is one slot of the bank you are looking at.

### the presses

1. Toggle **down**, **hold CHOMPI**, and go to the **presets page** (CHOMPI light blue). From
   TAPE's page that's: hold **KEY_22** (D#4) for 1 s.
   - On the presets page, white keys that already hold a preset glow dim. The one you
     recalled last glows white.
   - The two bank keys, **KEY_16** and **KEY_17** (C#3, D#3), glow in the bank's colour.
2. *(Optional)* **Pick a bank:** **KEY_16** = bank down, **KEY_17** = bank up (they wrap
   round from 8 to 1). Or turn **SW4** (it stops at bank 1 and bank 8). Bank colours, 1 to 8:
   red, orange, yellow, green, teal, blue, purple, pink.
3. Press **KEY_25** (A#4, the rightmost black key). It lights bright blue: you are saving.
   Used slots start blinking, so you can see what you might overwrite.
4. **Let go of CHOMPI.** The menu stays open (the CHOMPI light stays on, dimmer), waiting for
   you to choose.
5. **Press a white key**: the slot. It turns **blue**. Changed your mind? Press another white
   key. You can still change bank with KEY_16 / KEY_17 here.
6. **Press CHOMPI** to confirm. The CHOMPI light **flashes green**: saved.
7. Let go of CHOMPI. The menu closes. Play on.

**Cancel** before step 6: press **KEY_25** again. Nothing is saved.

**A red flash** at step 6 means it didn't save (card missing, full or unreadable). If all
the white keys glow red on the presets page, CHOMPI can't read the SD card at all.

### check it worked

1. Hold CHOMPI (the menu opens on the presets page again; the menu remembers it).
2. Press another lit white key, let go of CHOMPI: you hear that other preset. (No other
   presets yet? Hold **SW1 + SW3** for a second instead, for a surprise sound.)
3. Hold CHOMPI, press **your** slot's white key, let go: your sound is back.

Do this check every time: it proves the sound is really on the card.

### what gets saved

A preset saves the whole "project" around the sound:

- the sound itself: oscillators, filter, envelopes, LFO, effects, and the knob changes you
  made on the knob pages,
- harmony (on or off, key, mode, chord size and the rest),
- the arp, bass and tempo,
- the **event recorder's** loop, if you recorded one (saved with an empty recorder, the slot's
  old loop is removed).

Not saved, as on TAPE: pitch, gain, saturation and the DJ filter (performance settings; they
reset at power-on), the volume knob's input gain, and the **audio looper** (PLAY / LOOP). To
keep an audio loop, save it as a sample ([loop](#how-do-i-loop)).

**If your sound plays your recording** (you recorded into CHOMPI, or used chomp to instrument),
the preset remembers "play the recording", not the recording itself. The recording lives in
memory and is gone at power-off. To keep it:

1. Save the recording to a sample slot first ([record and use samples](#how-do-i-record-and-use-samples), step "keep it").
2. On TAPE's page, with nothing else pressed, tap that same white key: the sound now plays the
   saved sample from the card, the rest of your instrument stays.
3. Then save the preset as above.

### good to know

- **Saving over a used slot replaces it.** There is no undo. The blinking keys in step 3 show
  which slots are taken.
- **The sound CHOMPI wakes up with** is the last preset you **saved or recalled** (by key, MIDI
  program change or the computer), remembered about 2 s later. (Before 0.19.2 only a recall
  counted; the check above covers both.)
- Saving needs the SD card. Forge reads every preset back after writing it, so a green flash
  means it is really on the card.

Details: [manual, section 6](MANUAL.md#6-presets-on-chompi).

---

## how do I recall, copy and erase presets?

All on the **presets page** (toggle down, hold CHOMPI, CHOMPI light blue; from TAPE's page hold
**KEY_22** for 1 s).

**Recall**

1. Hold CHOMPI.
2. Pick the bank if needed (**KEY_16** / **KEY_17**, or turn **SW4**).
3. Press a lit white key.
4. Let go of CHOMPI. The sound changes, and its event recorder loop (if any) comes back and
   plays. A slot without a loop leaves your current loop playing.

**Copy**

1. Hold CHOMPI. Press **KEY_24** (G#4). It lights green.
2. Let go of CHOMPI.
3. Press the **source** white key (turns green).
4. Change bank if you like. Press the **destination** white key (turns blue). It replaces
   whatever was there.
5. Press CHOMPI: green flash. The slot's event recorder loop is copied too.

**Erase**

1. Hold CHOMPI. Press **KEY_23** (F#4). It lights red.
2. Let go of CHOMPI. Press the white key to erase (turns red).
3. Press CHOMPI: green flash. The slot and its loop are gone.

To cancel a copy or erase, press its key (**KEY_24** or **KEY_23**) again. While one of
save / copy / erase is waiting, the other two keys do nothing: finish or cancel it first.

**Starter sounds:** the slots start empty. Forge Bridge on a computer can fill a bank with
15 starter presets (*Card & firmware → Load starter presets*); it never overwrites a used slot.
MIDI program change on channel 1 recalls slots too. See [manual, section 6](MANUAL.md#6-presets-on-chompi).

---

## how do I make a new sound on the panel?

The **synth page** changes the sound itself, no computer needed. It works on synth sounds.
On an effects-only sound (Dry, Slap, Long echoes) the keys glow dim red: there is nothing to
change. No synth sound yet? Hold **SW1 + SW3** for a second (menu closed) for a starting point.

1. Toggle **down**, hold **CHOMPI**.
2. On TAPE's page keep holding **KEY_21** (C#4) for 3 s, until the CHOMPI light is **pink**.
   (On the harmony or parts page: tap KEY_21 until it's pink.)
3. Press keys to choose. Each group lights its choice brightly, the rest of the group dim:

| Keys | Choose |
| --- | --- |
| C3 · D3 · E3 · F3 (pink) | oscillator 1: sine · triangle · saw · square |
| G3 · A3 · B3 · C4 (teal) | oscillator 2: sine · triangle · saw · square |
| D4 · E4 · F4 · G4 · A4 · B4 (yellow) | oscillator 2's interval: unison · +5th · +octave · −octave · +4th · +12th |
| C#3 · D#3 · F#3 · G#3 (purple) | LFO shape: sine · triangle · square · random |
| A#3 (white = on) | the mod wheel sets the LFO depth |
| F#4 · G#4 · A#4 (green) | 4 voices · 7 voices · mono with glide |
| C5 | **hear it**: a C major chord while you hold it |

4. Turn the knobs (press a knob to flip it between its two pages; pink = page 1, teal = page 2):

| Knob | Page 1 (pink) | Page 2 (teal) |
| --- | --- | --- |
| SW4 | oscillator 2 level | vibrato depth |
| SW1 | oscillator 2 detune | tremolo depth |
| SW2 | noise | LFO speed |
| SW3 | glide | delay time |

5. Hold **C5** to listen while you change things. The keys don't play notes on this page.
6. Let go of CHOMPI and play it properly.
7. **Like it? Save it now:** [save a sound I like](#how-do-i-save-a-sound-i-like). Every edit
   changes the sound you are playing; nothing is kept until it is in a slot.

The normal knob pages (menu closed) shape the sound too: attack, decay, release, filter,
reverb and more. See [manual, section 5](MANUAL.md#5-knobs-and-knob-pages) and
[section 8d](MANUAL.md#8d-the-synth-page-design-a-sound-on-chompi--firmware-019).

---

## how do I get surprise sounds and keep one?

With the menu closed:

- **Surprise me:** hold **SW1 + SW3** for a second. CHOMPI rolls a brand-new sound (sometimes
  with chords in your key, sometimes an arp, sometimes a gliding solo voice). The knob lights
  flash colours and the keys sparkle. **Keep holding** and it rolls another every 1.5 s. Let go
  on one you like. Your volume and key stay put.
  **Chords and the arp come and go, on purpose:** each roll also decides them (about 1 roll in
  4 turns harmony on, about 1 in 5 starts the arp) and switches them off when it doesn't pick
  them, even if you had them on. Want chords on a rolled sound? Switch harmony on yourself
  (SW1 + SW2) afterwards, then save.
- **Mutate:** hold **SW2 + SW3** for a second. The sound you have now drifts a little
  (brighter, darker, longer, a different echo…). The knob lights flash teal. Keep holding to
  drift further. Mutate never changes harmony or the arp.
- **Chomp to instrument:** record anything first
  ([record and use samples](#how-do-i-record-and-use-samples)), then hold **SW1 + SW3**. The
  roll keeps your recording and builds an instrument around it, often with chords and an arp.

**Keep one:** the next roll replaces the sound, so save the one you like straight away:
[save a sound I like](#how-do-i-save-a-sound-i-like). A chomped instrument plays your
recording: keep the recording too (see "If your sound plays your recording" in that section).

Details: [manual, section 5a](MANUAL.md#5a-surprise-me-and-mutate-017).

---

## how do I play chords?

With harmony on, each key plays a whole chord in a key you choose. It works on synth sounds and
the chromatic sampler (kit sounds play single keys as usual).

**Quick on / off:** with the menu closed, hold **SW1 + SW2** for a second. All four knob lights
flash **green** (on) or **red** (off).

**The keys show the chords:** with harmony on, each key glows dim in its chord's colour:
**orange** major, **blue** minor, **red** diminished, **pink** augmented, **teal** sus4.

**Shift:** in the standard (Static) layout, the white keys C3 to B4 are the scale's chords I to
vii, twice. **C5 is Shift** (it glows dim white). Hold it while playing a key for a twist on
that chord; the key colours change to show the shifted chords.

**Pick the key and the chord type (the harmony page):**

1. Toggle down, hold **CHOMPI**, on TAPE's page hold **KEY_21** (C#4) for 1 s: CHOMPI light
   **purple**.
2. **Press a key** to set the key (tonic) to that note. The keys show it: the tonic white, the
   rest of the scale blue.
3. The knobs:

| Knob | Turn | Press |
| --- | --- | --- |
| SW4 | mode: major, natural minor, harmonic minor, melodic minor, dorian, phrygian, lydian, mixolydian, locrian | harmony on / off |
| SW1 | chord size: fifth, triad, 7th, 9th, 11th, 13th | layout Static / Real |
| SW2 | inversion 0–3 (when voice leading is off) | voice leading on / off |
| SW3 | — | open spread on / off |

4. Let go of CHOMPI and play.

Harmony is saved with your preset. Details, the Real layout and the black keys' colour chords:
[manual, section 8a](MANUAL.md#8a-harmony-one-key-plays-a-chord--firmware-013).

---

## how do I use arps, bass and tempo?

The arp plays the keys you hold (or the chord, with harmony on) one note at a time; the bass
plays the chord's root under it.

1. Toggle down, hold **CHOMPI**, on TAPE's page keep holding **KEY_21** for 2 s: CHOMPI light
   **orange** (the parts page). (From the harmony page: tap KEY_21.)
2. Choose with the keys (the choices light up):

| Keys | Choose |
| --- | --- |
| C3 · D3 · E3 · F3 · G3 · A3 (blue) | arp off · up · down · up-down · as played · random |
| B3 (white = on) | latch: the arp keeps going after you let go, until the next chord |
| C4 · D4 · E4 · F4 · G4 · A4 (green) | arp rate 1/4 · 1/8 · 1/8 triplet · 1/16 · 1/16 triplet · 1/32 |
| B4 (green / red) | clock running / stopped |
| C#3 · D#3 · F#3 · G#3 · A#3 (orange) | bass off · root · root + fifth · root then fifth · root then octave |
| C5 (blinks on the beat) | tap tempo: tap it on the beat, two taps or more |

3. The knobs: **SW4** tempo (1 BPM a click, press = tap tempo), **SW1** arp octaves 1–4,
   **SW2** note length, **SW3** bass rate (press: bass octave).
4. Let go of CHOMPI and hold some keys.

Stop everything: hold **SW4 + SW3** for a second (panic). The arp, bass and tempo are saved with
your preset. MIDI clock from other gear takes over the tempo while it plays. Details:
[manual, section 8b](MANUAL.md#8b-arp-bass-and-tempo--firmware-014).

---

## how do I use the effects?

Choowi 0.20 has an FX section: four **modules** in a row, each with three effects.

| Module | Knob | Keys on the FX page |
| --- | --- | --- |
| Space | SW4 | C3 echo + room · D3 cloud · E3 tape echo |
| Colour | SW1 | F3 saturation · G3 fold · A3 crush |
| Texture | SW2 | C4 cassette · D4 broken · E4 squash |
| Motion | SW3 | F4 chorus · G4 phaser · A4 tremolo |

1. **Open the FX page:** toggle down, hold CHOMPI, then tap **KEY_21 (C#4)** until the CHOMPI
   light turns **green** (harmony → parts → synth → FX).
2. **Pick an effect:** press its white key. The chosen key in each module lights.
3. **Turn its knob up** to hear it. **Hold the knob and turn** for its character (for example
   the phaser's speed, or the hiss on cassette).
4. **Change the order** the sound goes through the modules: tap **A#3**. It and the effect keys
   blink. Tap one key of each module in the order you want; the **fourth tap locks it**. Tap
   A#3 again to finish early; **hold A#3 1 s** to go back to Classic.
5. **Take an effect out:** turn its knob down until its ring is dim. The order and the chosen
   effect stay put for when you turn it back up.
6. Let go of CHOMPI and play. **Save a preset** to keep the effects with the sound.

B3 puts the reverb side by side with the delay; F#3 / G#3 put the effects before or after the
looper. Details: [manual, section 8e](MANUAL.md#8e-the-fx-section--firmware-020).

---

## how do I play Choowi's samples?

Forge comes with its own sounds in **bank purple** (copy `choowi-samples.zip` from the release
page to the card's top folder first; it replaces TAPE's bank a files).

1. Toggle **down**, **hold CHOMPI**, go to **TAPE's page** (teal; from a Forge page tap **KEY_22**).
2. **Instruments:** press **KEY_16** until it glows **purple**, then press a white key: 1 Glass
   Pluck, 2 Soft Keys, 3 FM Bell, 4 Warm Saw Pad, 5 Round Bass, 6 Acid Squelch, 7 Marimba, 8 Ooh
   Choir, 9 Music Box, 10 Brass Stab, 11 Kalimba, 12 Lofi Organ, 13 Wobble Lead, 14 Sparkle Pad.
   Let go of CHOMPI and play it across the keys.
3. **Drums:** press **KEY_17** until it glows **purple**, let go: the white keys are Kick, Snare,
   Closed Hat, Open Hat, Clap, Low Tom, High Tom, Rim, Cowbell, Shaker, Crash, Zap, Choowi
   Chirp and Anvil.

Sample banks by colour: purple, orange, teal, red, lime (the files' a to e).

## how do I record and use samples?

**Record**

1. Toggle **up** (record position). The CHOMPI light becomes an input meter, and you hear the
   input in your headphones. Line in is used when a cable is plugged in, otherwise the mic.
2. **Hold CHOMPI.** It and the white keys blink red three times (a 1.5 s count-in). Letting go
   during the count-in records nothing.
3. When the blinking stops (CHOMPI light red), make your sound.
4. Let go of CHOMPI. Toggle **down** and play: the keys play your recording, across the keys.

Too quiet or too loud? Short-press **SW6** and turn it: input gain (light blue to red). Press
again to get the volume back. Up to about 87 s per take.

**Keep it** (the recording is lost at power-off):

1. Toggle down, hold CHOMPI, go to **TAPE's page** (teal; from a Forge page tap **KEY_22**).
2. Press **KEY_25** (A#4).
3. Let go of CHOMPI, press a white key (the sample slot; not C5, that one *is* the recording).
   It turns blue.
4. Press CHOMPI. The CHOMPI light blinks pink while it writes, then flashes green.

The slot goes into the bank shown on TAPE's page: KEY_16 / KEY_17 glow in its colour (purple,
orange, teal, red, lime).

**Play samples from the card** (TAPE's page): **KEY_16** = chromatic (one sample across the
keys; press again for the next bank), **KEY_17** = kit (each white key a different sample).
In chromatic, a white key loads and plays that slot. **KEY_18 / KEY_19 / KEY_20** choose what
to record from: mic, line in, or resample (CHOMPI's own output). **KEY_24** copies a sample
(source key, destination key, CHOMPI) and **KEY_23** erases one (a white key, then CHOMPI).

TAPE's sample files work as they are. Details, file names and the knobs on this page:
[manual, section 7](MANUAL.md#7-samples-and-recording).

---

## how do I loop?

The looper records everything you hear, as audio. Menu closed:

1. Tap **LOOP** to start recording (PLAY light teal, LOOP red).
2. Tap **PLAY** to end the take: the loop plays. (Tap **LOOP** instead to go straight into
   overdub.)
3. While it plays: tap **LOOP** = overdub on / off (LOOP yellow), tap **PLAY** = pause / resume.
4. Overdubbed something you don't like? **Tap PLAY + LOOP together**: the last overdub is gone
   (one step back, loops up to about 41 s). They needn't land at exactly the same moment.
5. Hold **PLAY + LOOP** for 2 s to clear it.

**SW5 on the loop:**

- **Turn** SW5: the filter, on your sound and on the loop together (darker to the left).
- **Push and turn** SW5: the loop's speed while it plays (below zero plays it backwards);
  scrub through it while paused.
- **Click** SW5: speed back to normal.

Up to about 83 s. Panic pauses the loop without losing it.

**Keep a loop:** a preset doesn't hold the audio loop. Save it as a sample instead: TAPE's page,
press **KEY_24**, press **LOOP**, let go of CHOMPI, press a white key, press CHOMPI.

Details: [manual, section 8](MANUAL.md#8-looper).

---

## how do I record what I play?

The event recorder records your playing as notes and knob moves (not audio), on CHOMPI's
clock, and loops it over whole bars. It plays back through harmony, the arp and the bass, and is
saved with your preset.

1. Toggle down, hold **CHOMPI**, go to the **parts page** (orange).
2. Press **F#4** (KEY_23). It blinks red on the beat: armed.
3. Let go of CHOMPI. Play. Recording starts at the next bar line.
4. To close the loop, **tap CHOMPI** (a quick tap, nothing else pressed). It closes at the next
   bar line and starts looping; a green ripple runs across the keys. (It also closes by itself
   after 8 bars.)
5. While it plays, a tap on CHOMPI stops it; another tap plays it from the start.

The CHOMPI light shows it (menu closed): orange blinking = armed, orange = recording, dim green =
playing, yellow = overdubbing. On the parts page: **G#4** (KEY_24) = overdub on / off, **A#4**
(KEY_25) pressed twice within 2 s = clear.

Nothing happens? Check that the clock runs (B4 green on the parts page).

Save the preset to keep the loop with your sound. Details:
[manual, section 8c](MANUAL.md#8c-event-recorder-and-projects--firmware-015).

---

## when something goes wrong

**Stuck notes, runaway echo, too much going on:** hold **SW4 + SW3** for a second (panic). All
sound stops; a loop is paused, not lost.

**No sound from the keys:**

- Is there a sound loaded? The very first start is a pass-through. Hold **SW1 + SW3**, record
  something, or recall a preset.
- Turn **SW6** up. Is SW6 on input gain (light blue to red)? Short-press it to get the volume
  back.
- Is the toggle down? Is the menu closed (let go of CHOMPI)?
- Check the cable is in the headphone or main out.

**Howling:** the built-in mic is live when nothing is in line in. Use headphones or turn down.

**A knob seems to do nothing:** you may be on another page of that knob. Press and release it
to step pages; hold it 1.5 s to put it back to the preset's value.

**If CHOMPI loses power:**

1. Power CHOMPI from a **USB-C charger** (2 A or more) or a **charged battery**, not a computer
   port.
2. **Save** sounds you like to a preset as you go.
3. If it goes off, switch it back on. Your last saved or recalled preset comes back (unsaved changes
   don't).

**Safe mode:** the knob lights flash magenta three times at start, then the CHOMPI light blinks
magenta every 2 s and the menu keys are red. Forge crashed three times just after starting, so
it started without the card's presets, samples and settings. Playing works and your card is
untouched. Switch off and on to leave it. If it keeps happening, copy `FORGE/RESTARTS.TXT` off
the card and report it.

**Battery:**

- Hold **SW6** down for 2 s to see the battery: **white** = charged (on the charger), **green**
  = good, **yellow** = getting low, **red** = nearly empty. While charging it doesn't climb; it
  turns white when full.
- The SW6 light blinking yellow means the battery is low; blinking fast means charge now.
- When it runs out, the knob and CHOMPI lights flash amber for 15 s, then CHOMPI switches off.
  Plugging in during the flashing cancels it. After that, plug in USB to wake it.
- Dark after an install or restart, only the charge light on: plug in a USB-C charger, switch
  off, wait 5 s, switch on.

More: [manual, section 12](MANUAL.md#12-troubleshooting) and
[section 3](MANUAL.md#3-power-and-battery).

---

## cheat sheet

| Gesture | What it does |
| --- | --- |
| Toggle down, hold CHOMPI | open the menu (let go to close) |
| In the menu: hold KEY_22 1 s (on TAPE's page) | presets page (blue) |
| In the menu: hold KEY_21 1 / 2 / 3 / 4 s (on TAPE's page) | harmony (purple) / parts (orange) / synth (pink) / FX (green) |
| In the menu: tap KEY_21 (harmony, parts, synth, FX) | next of those four pages |
| FX page: white key / knob / hold knob + turn | pick an effect / its amount / its character |
| FX page: tap A#3, then one key per module | your own effect order (the fourth tap locks it) |
| FX page: hold A#3 1 s | the Classic order again |
| In the menu: tap KEY_22 (any Forge page) | back to TAPE's page (teal) |
| Presets page: KEY_25, let go of CHOMPI, white key, CHOMPI | **save** the sound (green flash) |
| Presets page: hold CHOMPI, white key, let go | recall |
| Presets page: KEY_24, let go, source, destination, CHOMPI | copy |
| Presets page: KEY_23, let go, white key, CHOMPI | erase |
| Presets page: KEY_16 / KEY_17, or turn SW4 | bank down / up |
| Same function key again | cancel save / copy / erase |
| SW1 + SW3 held 1 s | surprise me (keep holding for more) |
| SW2 + SW3 held 1 s | mutate (keep holding to drift further) |
| SW4 + SW1 held 1 s | Dream on / off (an ambient auto-drone with lights) |
| SW1 + SW2 held 1 s | harmony on / off |
| SW4 + SW3 held 1 s | panic |
| Toggle up, hold CHOMPI | record (after the red count-in) |
| Toggle down, tap CHOMPI | event recorder: close the take / stop / play |
| LOOP, then PLAY | record a loop, then play it |
| PLAY + LOOP tapped together | undo the last overdub |
| PLAY + LOOP held 2 s | clear the loop |
| SW5 turn / push and turn / click | filter / loop speed or scrub / speed back to normal |
| SW6 turn / short press / hold 2 s | volume / switch to input gain and back / battery light |
| Knob press and release / hold 1.5 s | next knob page / reset that control |

## how do I install Choowi?

Download `choowi-0.20.0-rc3-card.zip` from the [download page](https://github.com/myth-world/Forge/releases/tag/choowi-0.20.0-rc3), back up the card and read **README-FIRST**. Copy the kit contents to the top folder, with no other filename containing `.bin` left for the bootloader to find. The kit supplies firmware, bank purple samples and `MANUAL.html`. Use a strong USB-C charger for installation. See [the full install instructions](MANUAL.md#2-installing-and-updating).

## how do I make a sound on the forge page?

1. Open [Choowi’s forge page](https://myth-world.github.io/Forge/workshop/) in Chrome or Edge on a computer. Connect CHOMPI and allow MIDI including SysEx.
2. Describe your sound → **Copy the prompt** → any chatbot → paste its whole reply → **Use this reply**.
3. Enable live audition, play, and tweak. Choose a preset bank and slot → **Save to CHOMPI**; confirm an occupied-slot replacement only if intended. Wait for the saved message, then recall the slot on the panel.
4. **Save JSON** keeps a file on the computer. Without a device, **make sounds anyway** lets you edit and keep JSON for later.

Any arriving MIDI stops dream mode. No API key is needed on the forge page.

## how do I see CHOMPI on my phone?

The forge page has a **phone display**: CHOMPI's panel drawn live, with every light, the page
you're on, what each key and knob does there, the chord you just played, the looper and the
battery. It's the same drawing as the illustrated manual, so you can check one against the other.

1. **Android phone, Chrome** (iPhones have no Web MIDI). Plug CHOMPI into the phone with a USB-C
   OTG cable or adapter and switch it on. A computer with Chrome or Edge works too.
2. Open the forge page, tap **phone display**, then **Connect**, and allow MIDI devices.
3. Turn the phone sideways for the biggest panel. It only watches; it never changes CHOMPI.
   Needs Choowi 0.20.

**Power reminder:** When using a power bank, charge with CHOMPI switched off. SW6 green, yellow and red are voltage bands, not a percentage; only white means full.
