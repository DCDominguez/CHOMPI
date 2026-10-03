# Forge 0.5 — consolidated test candidate

Experimental community firmware for CHOMPI: a playable synth (up to four voices,
two oscillators, noise, resonant filter, LFO, glide), a TAPE-compatible sampler
with recording (up to seven voices), stereo delay and reverb, live patch
control, device and host presets, and diagnostics. Sampler design:
[SAMPLING.md](../../docs/forge/SAMPLING.md). Start with the [documentation index](../../docs/forge/README.md).
See the [project brief](../../docs/forge/PROJECT.md) and
[current handoff](../../docs/forge/HANDOFF.md),
[host controller](host/README.md), [protocol](../../docs/forge/PROTOCOL.md), and
[single test session](../../docs/forge/TEST_SESSION.md).

## Build and test

From the repository root:

```sh
make -C firmware/chompi-forge test
make -C firmware/chompi-forge sanitize
make -C firmware/chompi-forge firmware GCC_PATH=/path/to/gcc-arm-none-eabi-10.3-2021.10/bin
```

Host tests require a C++14 compiler and pthreads. The sanitizer target uses
AddressSanitizer and UndefinedBehaviorSanitizer. In containers where
LeakSanitizer cannot inspect `/proc`, use
`ASAN_OPTIONS=detect_leaks=0 make -C firmware/chompi-forge sanitize` and record
that leak checking was disabled; do not interpret that as a leak-test pass.

Use GNU Arm Embedded **10.3-2021.10**, per the upstream firmware quickstart.
Linux x86-64 distribution:

https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2

Expected archive SHA-256 (recorded at an earlier checkpoint; agent sandboxes
could not download the archive, so no current build used it):
`97dbb4f019ad1650b732faffcc881689cedc14e2b7ee863d390e0a41ef16c9a3`.
Current agent builds use the xPack fallback described in
[DEVELOPMENT.md](../../docs/forge/DEVELOPMENT.md); bundles record which compiler was used.

The firmware target builds the vendored WAVE libDaisy source into
`build/libdaisy/`, then links the Forge app. It does not rewrite the upstream
archive or any bootloader. Artifacts: `src/build/FORGE.bin`, `.elf`, `.hex`,
and `.map`. They are generated, ignored files; no binaries are committed.
The Forge Makefile rejects `program*` and `flash*` targets during this stage.

## Audio and controls

48 kHz, 24-frame blocks, as configured by the upstream hardware class.
Aux L/R (`in[2]`/`in[3]`) feed the effect; stereo output is copied to headphone
(`out[0]`/`out[1]`) and master (`out[2]`/`out[3]`). No microphone input is mixed.

| Parameter | MIDI CC, channel 1 | Encoder ID | Range / startup |
| --- | --- | --- | --- |
| Wet/dry | 20 | Knob 1 = hw SW4 | 0–100%; starts dry |
| Delay time | 21 | Knob 2 = hw SW1 | 10–1000 ms; starts 257.5 ms |
| Feedback | 22 | Knob 3 = hw SW2 | 0–85%; starts 21.25% |
| Output level | 23, 25 | Knob 4 = hw SW3, and SW6 | 0–1 gain; fades up to 0.25 |
| Wet bypass | 85 | MIDI only | 0–63 off, 64–127 on |
| Synth cutoff | 24 (SW5), 74 | SW5 turn | Logarithmic 40–16000 Hz; v2/v3 patches |
| Filter resonance | 71 | MIDI only | 0–1; v3 patches only |
| Reverb mix | 91 | MIDI only | 0–1; v3 patches only |
| Mod wheel | 1 | MIDI only | Scales LFO depth when the v3 patch enables it |
| Panic | 120/123 | SW5 press | Silence all voices and old delay/reverb tails |

Encoder IDs follow `hardware.h`; confirm printed-panel correspondence during
bring-up. SW5 turns synth cutoff and its press panics. Keybed notes 48–72
play v2/v3 synth patches at velocity 100. CC values use `value / 127`.
Knobs use the stock logical order (TAPE/TEMPO/WAVE `encoder_map`): knob 1–4
are hardware SW4, SW1, SW2, SW3; SW5 and SW6 keep their positions. CC20+n
turns knob n+1, as on stock CHOMPI firmware.
The same mapping is accepted over USB and TRS MIDI; other channels and unknown
CCs are ignored. SysEx patch/status requests receive replies on the same transport.
Channel-1 notes play the synth; CC64 sustain and pitch bend (±2 semitones)
apply per MIDI source; CC121 resets them; CC1 sets the (global) mod wheel;
CC120/123 globally panic. No clock output or unsolicited parameter streaming.

Bypass fades the wet mix to zero while retaining output level and the delay
state. It is software wet bypass, not a hardware relay or unity-gain bypass.
Delay tails keep circulating. Delay-time modulation glides in pitch.

DSP clips input/output to [-1, 1] and silences non-finite input; this is a final
numeric bound, not a transparent mastering limiter. Feedback is limited to
0.85. Controls are smoothed with a 20 ms time constant. A dim cyan panel LED
indicates initialization completed; red indicates failed engine initialization.
Battery warning/shutdown handling is inherited from WAVE and needs bench testing.

## Synth and module patches

Version-3 patches (firmware 0.4) contain synth, filter, lfo, delay, reverb and
output modules plus a route selector: synth or stereo aux → delay → reverb →
output. The synth is mono, duplicated to L/R before the delay. Per voice: main
oscillator and a second oscillator (sine, polyBLAMP triangle, polyBLEP
saw/square; ±24 semitones, ±50 cents), white noise, amplitude ADSR, and a
resonant state-variable low-pass with its own ADSR. One shared LFO
(sine/triangle/square/sample-and-hold) modulates pitch, cutoff and amplitude;
the mod wheel can scale it. Voices 1–4 per patch (1 = mono, the CPU fallback),
glide 0–2000 ms. The reverb is a 4-line feedback delay network in DTCM (as the
stock apps place their reverbs); the delay stays in SDRAM.

Version-2 patches (synth, delay, output) and v1 delay patches still work and
render bit-exactly as on firmware 0.3 (simulation check): v1/v2 keep the original
shared one-pole low-pass and have no v3 modules. See host guide for physical
ranges, JSON examples and `upgrade` (v1/v2 → v3).

Voice allocation uses idle, then the quietest releasing, then the oldest voice.
A reused voice continues from its current level and phase (simulated steal step
1.1x steady state, was 5x). Note ownership distinguishes keybed, USB and UART.
Note-on velocity zero is note-off. Route/waveform switching, switching between
v1/v2 and v3 patches, and panic stop voices/tails abruptly; held notes must be
retriggered. Mono mode (voices 1) uses last-note priority and does not return
to a still-held earlier note. Triangle uses polyBLAMP
corners; saw/square use 2-point polyBLEP, so some high-note aliasing remains.
No claim of click-free sound or hardware CPU headroom before the listening session;
TEST_SESSION 6.2b measures the v3 worst case (`presets/10-cpu-stress.json`).

Startup remains dry aux v1 for compatibility (DC, 2026-10-03). Send an
instrument preset, recall a device preset, pick a sample or record to play.
Knobs 1–4 control delay/output; on a sampler patch they follow TAPE's first
page instead: pitch, start, end, delay mix.

## Sampler and recording (v4 patches)

Plays TAPE's sample files from the SD card root: `jammi_<a-e><1-14>.wav`
(chromatic: one sample across the keys, middle C at original pitch) and
`cubbi_<a-e><1-14>.wav` (kit: white keys = slots 1–15 of a bank). Slot 15 is
the recording. Per patch: pitch ±24 semitones, start/end, loop with crossfade,
hold or one-shot, reverse; samples run through the filter, filter envelope,
LFO, glide, delay and reverb, up to 7 voices. Accepted files: 8/16/24-bit PCM
or 32-bit float, mono or stereo, 8–96 kHz.

| Gesture | Action |
| --- | --- |
| Toggle down, hold CHOMPI | Record (mic, line in or resample); release to stop and play it chromatically |
| Menu (toggle up + CHOMPI), KEY_22 | Samples page |
| Samples page: KEY_16 / KEY_17 | Chromatic / kit; again = next bank a–e |
| Samples page: white key | Play that slot (chromatic) or that bank (kit) |
| Samples page: KEY_18 / 19 / 20 | Record source mic / line / resample (jack insertion picks line) |
| Samples page: KEY_25 / 24 / 23 → white key → CHOMPI | Save recording / copy / erase a sample file |

The chromatic slot or the whole kit bank is loaded into a 32 MiB SDRAM pool in
16 KB steps (notes can start while it loads); the recording has its own
16 MB (~87 s). Forge writes TAPE's 16-bit stereo 48 kHz format and removes the
slot's stale `_double.wav`, which TAPE regenerates at boot. See
[PROTOCOL.md](../../docs/forge/PROTOCOL.md#sampler-firmware-05) for LEDs and
host opcodes 08/09.

## Device presets (SD card)

Same gestures as TAPE's preset menu: with the toggle in TAPE's menu position,
press the CHOMPI key. While the menu is open the keys select presets instead of
playing:

| Keys | Action |
| --- | --- |
| Hold CHOMPI + white key 1–15 | Recall that slot of the current bank |
| KEY_16 / KEY_17 (black), or turn knob 1 (hw SW4) | Bank down / up (8 banks, each with its own LED colour) |
| KEY_25 → white key → CHOMPI | Save the current sound to that slot |
| KEY_24 → source → destination → CHOMPI | Copy (destination may be another bank) |
| KEY_23 → white key → CHOMPI | Erase |
| The same mode key again | Cancel |

Release CHOMPI with nothing pending, or move the toggle back, to close the
menu. MIDI program change n (channel 1) recalls bank n / 15, slot n % 15.
Files live in `FORGE/` on the card (`B1S01.FPR` …), never `.bin`, so they
coexist with the firmware file the bootloader loads. The record format and
LED colours are in [PROTOCOL.md](../../docs/forge/PROTOCOL.md). All SD access
is in the main loop; the audio callback only runs the menu state machine.
Without a card the menu shows red keys and everything else works.

## Implementation boundaries

`core/` is hardware-independent and allocation-free. Main-loop MIDI processing
feeds a single-producer/single-consumer queue with 63 usable entries; at most
16 requests are consumed per audio block. Full patches are validated before
queueing and applied atomically by the audio owner. Responses include the active
targets and average/peak callback load; TX stays in the main loop. Overflow is
counted; a host timeout must not be interpreted as proof that a patch was not
applied. See the protocol document for backpressure and recovery details.

DSP and parameter state belong to the audio callback after initialization;
physical encoder changes apply after MIDI changes in each block. JSON files,
validation, saving/capture, an OpenAI/Gemini webapp and optional Ollama CLI
authoring live on the host. See the host guide for launch and key handling.
The active patch is volatile and the device boots into dry aux defaults
(samples on the card and the device presets persist; the recording does not). Sounds
can be stored on the SD card as device presets (8 banks × 15 slots) and
recalled from the panel, MIDI program change or the host; see "Device presets"
below. It does not run AI/networking or load new DSP/graphs. The stock
bootloader is still needed to load the app. Building alone cannot establish
CPU headroom, USB enumeration, routing, electrical levels, or device recovery.

## First hardware session

Per DC's 2026-10-01 preference, defer this to one consolidated acceptance
session for the assembled first candidate. M0 does not require a separate
user test now. Keep building and testing software in the meantime, and extend
the checklist below with patch/host integration checks as those are implemented.

This build has not been flashed or tested on DC's CHOMPI. Before first boot,
back up the working card and confirm the existing stock firmware/restore path;
use a separate test card and the installed CHOMPI bootloader. Do not install
the repository's beta bootloader as part of Forge bring-up.

Start with low external monitoring level and a known stereo line source. Check
dry left/right separation on headphones and main output, volume/mute, the
delay and bypass, all assigned encoders, and CCs over each MIDI transport.
Then measure callback CPU load under control traffic and check power/battery
handling. Record results in the handoff. These are pending acceptance tests,
not claims that the hardware has already passed.
