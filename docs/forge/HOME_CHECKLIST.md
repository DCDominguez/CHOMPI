# First time home with Forge 0.7

One page, in order. About 45 minutes. After step 2 you should never need to
take the SD card out for firmware or samples again.

## 1. Get the files

GitHub → DCDominguez/CHOMPI → **Actions** → **Forge Bridge (Windows exe)** →
the newest green run → **Artifacts** → **Forge-Bridge-exe**. Unzip it. You get:

- `Forge Bridge.exe`: the bridge (setup check, panel walk, automatic checks,
  card and firmware loader). Nothing to install. If Windows warns about an
  unsigned program: **More info → Run anyway**.
- `FORGE.bin`: the same development firmware that is inside the exe, for the
  card flash below.
- this checklist.

## 2. Flash 0.7 from the card (the last time)

1. Back up the card (copy everything to a folder on the PC).
2. In the card's top folder, **every file whose name contains `.bin` must go
   or be renamed** (the bootloader takes the first one it finds, even
   `something.bin.old`). Rename e.g. `CHOMPI_TAPEv2_0.bin` →
   `CHOMPI_TAPEv2_0_bin.old`.
3. Copy `FORGE.bin` to the card's top folder.
4. While the card is out anyway: copy the TAPE samples (`jammi_a1.wav` …,
   `cubbi_a1.wav` …) into the top folder too. From now on the bridge can add
   more over USB.
5. Card into CHOMPI, flash the way you flashed 0.6 (rainbow lights), Forge
   starts.

If anything looks wrong: put your backup back and flash 0.6 the same way.

## 3. Fix the rig (from the last session's results)

- CHOMPI **right** main output → interface **Input 2** (only the left was wired).
- Interface **Input 1 gain down about 10 dB** (it clipped).
- The 144 Hz hum on Input 1: plug the interface into a different USB port
  (not the monitor's hub), or move its cable away from the monitor.
- Interface **outputs 1/2 → CHOMPI line in** (line-in checks need it).

## 4. Run the bridge

1. CHOMPI to the PC with a USB **data** cable. Close other MIDI programs.
2. Double-click `Forge Bridge.exe`; the browser opens it. Keep the black
   window open.
3. **Connect CHOMPI**. It must say firmware **0.7**.
4. **Check setup**. Fix whatever it lists (each item says how) and run it
   again until it is all green.
5. **Start panel walk**. It asks you to press each key, turn each knob and look at
   the lights (the camera can take photos as evidence).
6. **Run automatic checks**. CPU figures are now per step.
7. Try the USB loader once: **Card & firmware** → *Install this kit's
   firmware* → press the CHOMPI key while it blinks white. CHOMPI restarts
   (the firmware is the same, so it may start without the rainbow lights).
   Then put one TAPE-named sample in the `card` folder next to the exe (the
   section shows the path; *Refresh list* creates it), *Copy selected to
   CHOMPI*, and play it.

## 5. Send the results back

The reports land in the `reports` folder next to `Forge Bridge.exe`. Zip that
folder and upload it in the chat. Say which steps felt wrong, even if they
passed.
