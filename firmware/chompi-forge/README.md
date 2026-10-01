# Forge — foundation

Experimental community firmware for CHOMPI. A standalone stereo delay and
live-control foundation, not a continuation of TAPE's sampler UI.
See the [project brief](../../docs/forge/PROJECT.md) and
[current handoff](../../docs/forge/HANDOFF.md).

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

Archive SHA-256 verified for this build:
`97dbb4f019ad1650b732faffcc881689cedc14e2b7ee863d390e0a41ef16c9a3`.

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
| Wet/dry | 20 | SW1 | 0–100%; starts dry |
| Delay time | 21 | SW2 | 10–1000 ms; starts 257.5 ms |
| Feedback | 22 | SW3 | 0–85%; starts 21.25% |
| Output level | 23 | SW4 and SW6 | 0–1 gain; fades up to 0.25 |
| Wet bypass | 24 | MIDI only | 0–63 off, 64–127 on |

Encoder IDs follow `hardware.h`; confirm printed-panel correspondence during
bring-up. SW5 and keybed actions are unassigned. CC values use `value / 127`.
The same mapping is accepted over USB and TRS MIDI; other channels and unknown
CCs are ignored. No MIDI output, clock, note processing, or parameter feedback.

Bypass fades the wet mix to zero while retaining output level and the delay
state. It is software wet bypass, not a hardware relay or unity-gain bypass.
Delay tails keep circulating. Delay-time modulation glides in pitch.

DSP clips input/output to [-1, 1] and silences non-finite input; this is a final
numeric bound, not a transparent mastering limiter. Feedback is limited to
0.85. Controls are smoothed with a 20 ms time constant. A dim cyan panel LED
indicates initialization completed; red indicates failed engine initialization.
Battery warning/shutdown handling is inherited from WAVE and needs bench testing.

## Implementation boundaries

`core/` is hardware-independent and allocation-free. Main-loop MIDI processing
feeds a single-producer/single-consumer queue with 63 usable entries; at most
16 commands are consumed per audio block. Overflow drops the newest command and
increments `dropped_commands`, visible in a debugger. This prevents an unbounded
Forge control loop but is not a transport acknowledgement protocol.

DSP and parameter state belong to the audio callback after initialization;
physical encoder changes apply after MIDI changes in each block. SD, JSON,
runtime graph loading, persistent patches, AI, and networking are not implemented.
The stock bootloader is needed to load the app, but the app itself performs no
SD access. Building alone cannot establish CPU headroom, USB enumeration,
audio routing, electrical levels, or recovery on a physical unit.

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
