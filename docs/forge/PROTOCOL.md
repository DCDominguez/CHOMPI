# Forge control protocol — firmware 0.14

Transport version remains **1**; patch formats **1–7** are supported.
Firmware 0.4 added patch v3 (second oscillator, noise, resonant filter with
envelope, LFO, voice limit, glide, reverb); 0.5 added patch v4 (the sampler,
up to 7 voices) and sample requests (opcodes 08/09); 0.6 adds patch v5 (what
knobs 1–4 control; [KNOBS.md](KNOBS.md)) and the panel's knob pages; 0.7 adds USB
file transfer and firmware install (opcode 0C, below); 0.13 adds patch v6
(harmony mode, [HARMONY_BRIEF.md](HARMONY_BRIEF.md)) and Inspector page 8; 0.14 adds patch v7
(clock, arpeggiator, bass), MIDI clock in/out and Inspector page 9. v1–v3 patches render
bit-exactly as on 0.4 (checked against the previous core in simulation).
All lengths/indexes below exclude MIDI F0/F7 unless stated. USB and bidirectional
TRS MIDI use the non-commercial manufacturer ID 7D followed by ASCII FG.

## Envelope and operations

`F0 7D 46 47 01 OP SEQ_LO SEQ_HI DATA... CHECKSUM F7`

Every payload byte is 7-bit. Words are little-endian base 128. Sequence is
0–16383, echoed in replies. Sum of payload bytes including checksum is 0 modulo
128. This detects corruption, not malicious traffic; there is no authentication.

| OP | Length | Operation |
| --- | --- | --- |
| 01 | 18 for v1, 30 for v2, 69 for v3, 84 for v4, 88 for v5, 91 for v6, 97 for v7 | Apply complete patch |
| 02 | 8 or 9 | Read current patch and status. Optional byte 7 (firmware 0.7): 1 = after this reply, start a new CPU peak (per-step measurements); 0 = report only |
| 03 | 8 | Panic: silence all synth voices and clear old delay tail; return status |
| 04 | 10 | Store the current device patch to SD preset (bank 0–7 at 7, slot 0–14 at 8); reply 42 |
| 05 | 10 | Recall SD preset (bank, slot): applied like 01; reply 40 with the recalled patch |
| 06 | 10 | Erase SD preset (bank, slot); reply 42 |
| 07 | 8 | List occupied SD presets; reply 43 |
| 08 | 8 | List samples on the card and the recording; reply 44 |
| 09 | 15 | Sample job: 7 action (0 save recording, 1 erase, 2 copy), 8 mode, 9 bank, 10 slot, 11–13 copy destination mode/bank/slot; reply 45 |
| 40 | 30 for v1, 42 for v2, 81 for v3, 96 for v4, 100 for v5, 103 for v6, 109 for v7 | Success/current targets plus diagnostics |
| 41 | 9 | Rejection; error code at index 7, checksum at 8 |
| 42 | 12 | Preset done: index 8 = 1 stored / 2 erased, 9 bank, 10 slot |
| 43 | 33 | Occupancy: per bank (0–7) 3 bytes at 8 + 3·bank = 15-bit slot mask, 7 + 7 + 1 bits |
| 44 | 36 | Samples: 14-bit slot masks at 8 + 2·(mode·5 + bank) (mode 0 chromatic, 1 kit; bank 0–4 = a–e; bit s = slot s+1); 28 flags (1 card, 2 recording, 4 loader busy); 29–31 recording length ms, 32–34 capacity ms (21-bit) |
| 45 | 13 | Sample job done: 8 action, 9 mode, 10 bank, 11 slot (the destination for a copy) |

## Apply request layout

| Index | v1, v2 and v3 |
| --- | --- |
| 0–6 | Header, operation and sequence |
| 7 | Patch version, 1 or 2 |
| 8–9 | Mix normalized 14-bit |
| 10–11 | Time normalized 14-bit |
| 12–13 | Feedback normalized 14-bit |
| 14–15 | Output level normalized 14-bit |
| 16 | Wet bypass, 0 or 1 |
| 17 | v1 checksum; v2/v3 route: 0 aux→delay(→reverb)→output, 1 synth→delay(→reverb)→output |
| 18 | v2 waveform: 0 sine, 1 triangle, 2 saw, 3 square |
| 19–20 | v2 attack normalized 14-bit |
| 21–22 | v2 decay normalized 14-bit |
| 23–24 | v2 sustain normalized 14-bit |
| 25–26 | v2 release normalized 14-bit |
| 27–28 | v2 cutoff normalized 14-bit |
| 29 | v2 checksum; v3 osc2 waveform 0–3 |
| 30–31 | v3 osc2 level |
| 32 | v3 osc2 interval byte 0–48 (semitones = byte − 24) |
| 33–34 | v3 osc2 detune |
| 35–36 | v3 noise level |
| 37–38 | v3 filter resonance |
| 39–40 | v3 filter envelope amount |
| 41–48 | v3 filter attack, decay, sustain, release (2 bytes each) |
| 49 | v3 LFO waveform: 0 sine, 1 triangle, 2 square, 3 sample-and-hold |
| 50–51 | v3 LFO rate |
| 52–53 | v3 LFO pitch depth |
| 54–55 | v3 LFO filter depth |
| 56–57 | v3 LFO amplitude depth |
| 58 | v3 mod wheel gates LFO depth: 0 or 1 |
| 59 | v3 voices 1–4 |
| 60–61 | v3 glide |
| 62–67 | v3 reverb mix, size, damping (2 bytes each) |
| 68 | v3 checksum; v4 source: 0 oscillators, 1 sampler (with route 1) |
| 69 | v4 sample mode: 0 chromatic (TAPE JAMMI), 1 kit (TAPE CUBBI) |
| 70 | v4 sample bank 0–4 (a–e) |
| 71 | v4 sample slot 0–14 (slot 15 = index 14 = the recording; kit mode ignores it) |
| 72–73 | v4 sample pitch |
| 74–75, 76–77 | v4 sample start, end (start < end, else error 4) |
| 78, 79, 80 | v4 loop, gate (1 = sound while held, TAPE "sustain"; 0 = trigger), reverse: 0 or 1 |
| 81–82 | v4 loop crossfade |
| 83 | v4 checksum; v5 knob 1 (SW4) page-1 control |
| 84, 85, 86 | v5 knobs 2, 3, 4 (SW1, SW2, SW3) |
| 87 | v5 checksum; v6 harmony word bits 0–6 |
| 88, 89 | v6 harmony word bits 7–13, 14–16 (byte 89 at most 7) |
| 90 | v6 checksum; v7 arp word bits 0–6 |
| 91, 92 | v7 arp word bits 7–13, 14–20 |
| 93, 94, 95 | v7 clock word bits 0–6, 7–13, 14–20 |
| 96 | v7 checksum |

v5 knob bytes: 0 = the source's default (mix, time, feedback, level; sampler:
pitch, start, end, mix), else firmware Parameter id + 1 (`core/parameters.h`;
host names in `forge_host.KNOB_TARGETS`): 1 delay mix, 2 time, 3 feedback,
4 output level, 6 cutoff, 7 resonance, 8 reverb mix, 9 sample pitch,
10 start, 11 end, 16 filter envelope amount, 17 attack, 18 decay, 19 sustain,
20 release, 21 LFO rate, 22 LFO pitch depth, 23 LFO filter depth, 24 LFO
amplitude depth, 25 osc 2 level, 26 osc 2 detune, 27 noise, 28 glide,
29 reverb size, 30 reverb damping, 31 loop crossfade. 5 (bypass), 12–15
(the knobs themselves) and anything above 31 are error 4. v5 indexes 7–82 are
identical to v4. v6 indexes 7–86 are identical to v5.

v6 harmony word (17 bits; `harmony::Pack` in core/harmony.h, host
`forge_host.harmony_word`): bits 0–3 tonic 0–11 (C..B), 4–7 mode 0–8 (major,
natural minor, harmonic minor, melodic minor, dorian, phrygian, lydian,
mixolydian, locrian), 8–10 chord size 0–5 (triad, 7th, 9th, 11th, 13th, fifth),
11–12 inversion 0–3, 13 open spread, 14 voice leading off, 15 layout (0 Static,
1 Real), 16 enabled. Anything out of range is error 4. Panel-only Shift (bit 17
in Inspector page 8) is never in a patch. A v6 patch also sets everything a v5
patch does; v1–v5 patches leave the harmony settings as they are.
v7 parts words (core/parts.h `PackArp` / `PackClock`, host `forge_host.parts_words`):
arp word bits 0–2 pattern (0 off, 1 up, 2 down, 3 up-down, 4 as played, 5 random), 3–5
rate (1/4, 1/8, 1/8 triplet, 1/16, 1/16 triplet, 1/32), 6–7 octaves − 1, 8–12 gate in 5 %
steps − 1 (0–19), 13 latch, 14–16 bass (0 off, 1 root, 2 root + fifth, 3 alternate root /
fifth, 4 alternate root / octave), 17–18 bass rate (once per chord change, 1/2, 1/4, 1/8),
19–20 bass octave (0 C1, 1 C2, 2 C3). Clock word bits 0–8 tempo 40–300 BPM, 9–19 random
seed 0–2047, 20 send MIDI clock. Out-of-range values are error 4. v7 indexes 7–89 are
identical to v6.

The apply reply echoes the patch as sent. Status replies and presets saved on
CHOMPI report the live harmony settings (changed on the panel's harmony page):
a v3–v5 patch is reported as v6 when harmony is on, and a v3–v6 patch as v7 while the arp
or bass is on (with the live tempo); v1 and v2 stay as they are.

v3 indexes 7–28 are identical to v2, except that the cutoff at 27–28 feeds the
v3 per-voice resonant filter instead of the shared one-pole filter. v4 indexes
7–67 are identical to v3 except that byte 59 (voices) allows 1–7.

For normalized value n = word / 16383:

| Control | Physical conversion |
| --- | --- |
| Mix, level, sustain | n |
| Time | 10 + 990 × n milliseconds |
| Feedback | 0.85 × n |
| Attack / decay | 1 + 1999 × n milliseconds |
| Release | 5 + 4995 × n milliseconds |
| Cutoff | 40 × 400^n Hz (logarithmic, 40–16000 Hz) |
| v3 osc2 level, noise, sustains, LFO amplitude, reverb mix/size/damping | n |
| v3 osc2 detune | −50 + 100 × n cents |
| v3 resonance | Q = 0.707 × 16^n (0.707–11.3), output gain-compensated |
| v3 filter envelope amount | −6 + 12 × n octaves added to cutoff × envelope |
| v3 filter attack / decay / release | as amplitude attack / decay / release |
| v3 LFO rate | 0.05 × 400^n Hz (logarithmic, 0.05–20 Hz) |
| v3 LFO pitch / filter depth | 200 × n cents / 4 × n octaves |
| v3 glide | 2000 × n ms (0 = off; ~98 % of the interval after that time) |
| v3 reverb size | decay RT60 = 0.2 × 50^n seconds (0.2–10 s) |
| v3 reverb damping | in-loop low-pass 16000 × (1500/16000)^n Hz |
| v4 sample pitch | −24 + 48 × n semitones (n = 0.5 is the sample's own pitch) |
| v4 sample start / end | n × sample length (minimum window 1024 frames) |
| v4 loop crossfade | 250 × n ms (limited to half the window and the room before the start) |

v1 always selects aux input and restores default dormant synth settings. v2
selects one of two supported paths; all three named module settings are present
in JSON even on the aux path. v3 has the same two paths with reverb after the
delay; all six modules are present in JSON. v4 adds a third path,
sampler→delay→reverb→output (route 1 with source 1), and a seventh module,
`sampler`. Fields a format lacks take neutral defaults (no osc2, noise, LFO,
glide, reverb or sampler; four voices). No arbitrary edges, feedback routing, plugin code
or dynamic module creation is accepted. Names remain host-side.

## Success/status response

| Index | Content |
| --- | --- |
| 0–6 | Header, opcode 40, sequence |
| 7 | Success, 0 |
| 8 onward | Exact quantized patch DATA (request indexes 7 through before checksum) |

After patch DATA, diagnostics begin at offset **18 for v1**, **30 for v2**, **69 for v3**, **84 for v4**, **88 for v5**, **91 for v6**, **97 for v7**:

| Offset from diagnostics | Content |
| --- | --- |
| +0,+1 | Average audio callback load ×1000, 14-bit |
| +2,+3 | Peak audio callback load since boot ×1000, 14-bit |
| +4..+6 | Dropped ingress/control/reply count, saturated 21-bit |
| +7..+9 | Rejected recognized requests, saturated 21-bit |
| +10 | Firmware minor version (13 for 0.13) |
| +11 | Checksum |

CPU resolution is 0.1 percentage point; max 1638.3%. Readings are from completed
callbacks before the snapshot, not total scheduling/interrupt-mask time. Offline
harness readings are synthetic zero and say nothing about device headroom.

Errors: 1 length, 2 protocol/patch version, 3 checksum, 4 patch fields or
preset/sample address, 5 opcode, 6 queue busy (also: recording in progress
when saving it), 7 empty slot (preset, copy source, or no recording to save),
8 SD card missing or storage failed, 9 storage busy, 10 power (0.11: a firmware
install refused, low battery on a weak or missing supply). Foreign SysEx/replies are ignored. Framing
discards may be silent and are not included in rejected recognized-request counts.

## MIDI clock (firmware 0.14)

MIDI real-time bytes are accepted on USB and the MIDI jack whatever the channel: F8
clock (24 per quarter note), FA start, FB continue, FC stop; active sensing (FE) and the
rest are ignored. They may arrive inside any other message. The arp and bass follow the
clock while it arrives (start restarts the phrase on its downbeat; stop pauses them);
after 0.5 s without a tick CHOMPI's own tempo takes over. A clock tick lost to a full
ingress queue counts separately and never triggers the stuck-note recovery.

MIDI out (with the panel's notes): arp notes on the "Midi Out Channel", bass notes on
the next channel (16 wraps to 1), and, while the arp or bass plays on CHOMPI's own tempo
with "send clock" on, FA start, F8 at 24 per beat and FC stop.

## Notes, controls and recovery

Channel 1 (zero-based 0): Note On/Off, including Note On velocity zero. Notes
0–127 accepted; keybed uses 48–72 at velocity 100. UART, USB and keybed have
separate source IDs. Matching source/note is retriggered; otherwise idle voices
are used, then the quietest releasing voice, then the oldest held voice (four voices; up to seven on v4).
A reused voice keeps its current level and waveform phase and glides to the new
velocity over about 2 ms, so retrigger/steal does not jump to silence. An old note-off cannot release a
replacement with a different note/source.

Sustain pedal CC64 (>=64 down) and pitch bend (14-bit, ±2 semitones, smoothed
over ~5 ms) are tracked **per source**, so a USB pedal or bend does not affect
UART or keybed notes. Note-off while that source's pedal is down marks the
voice sustained; pedal-up releases only sustained voices, not keys still held.
Steal order: idle, quietest releasing, oldest pedal-sustained, oldest held.
CC121 (reset all controllers) lifts that source's pedal and centres its bend.
Panic, CC120/123 and route/waveform changes clear pedal and bend for every
source. The keybed has no pedal or bend input. Not yet: octave switching,
clock sync, MPE, aftertouch, arpeggiator or MIDI note output. v3 voices = 1
is monophonic with last-note priority but no return to a still-held earlier note.

Stock CHOMPI convention, CC20+n = absolute position of logical knob n:
CC20–23 = knobs 1–4 on their first page: the v5 patch's knob controls, else
mix, time, feedback, level (or, on a sampler patch, TAPE's page: sample pitch,
start, end, mix), whatever page the panel shows; 24 cutoff (SW5),
25 level (SW6). While a loop exists, CC 24 sets the looper speed instead
(−2…+2×, as TAPE's transport knob). CC 26 / 27 are TAPE's looper PLAY / LOOP
(≥ 85 press, ≤ 41 release, the middle third ignored). Stock's virtual-key and
second-page CCs (14, 15, 28–33) are ignored. General MIDI
extras: 74 cutoff, 85 wet bypass (>=64 on), 71 resonance and 91 reverb mix
(71/91 on v3 patches only; ignored on v1/v2 so status stays truthful), 64 sustain pedal, 121 reset controllers, 1 mod wheel (scales
LFO depth when the v3 patch sets mod_wheel; global, reset by panic/CC121).
Pitch bend (status E0) on channel 1.
CC120 and CC123 silence **all** sources/tails as a global recovery action;
CC123 deliberately uses panic semantics rather than an envelope release.
Real-time bytes may interrupt all supported frames/running status and are ignored.
MIDI clock is tolerated, not used for tempo. Unsupported channel messages cancel
running status; oversized SysEx is discarded in full.

## Atomicity and real-time boundary

Audio owns voices, envelope/filter/delay and parameters. Main loop validates and
queues patches; callback consumes at most 16 requests per block, stopping when
reply capacity is unavailable. Accepted patch targets change atomically. Delay
state survives ordinary parameter changes. Route or waveform changes silence
voices and clear the old tail; held keys must be released/retriggered. Other
synth changes affect active voices, except an already-running release retains
its previously calculated release slope. Delay-time changes glide in pitch.

Panic logically clears delay and reverb history with O(1) reset markers; no
full SDRAM clear in the callback. The reverb is skipped while its mix is zero
and restarts from silence. Changing between a v1/v2 and a v3 patch silences
voices like a route change. Invalid patches never partially change targets or voices.
Queue overflow drops/counts controls and pitch bend (the next bend message
corrects pitch). A full request queue on a note, pedal or CC121, or any
dropped ingress frame (the main loop cannot tell whether it was a note), triggers
global silence to avoid stuck notes. The main loop counts these emergencies (and
CC120/123) and stamps every queued request with the count; the audio callback
silences once per new count and discards only notes, pedal, bend and CC121
queued before it. Notes
queued after the emergency play immediately, even under continuous traffic.
Patches, status, panic and controls always execute (`RecoveryGate` in
core/runtime.h, host-tested). Keys held through a recovery must be retriggered. This logic is implemented; actual interrupt/transport behavior is
still hardware-unverified. Use the panel panic (SW4 + SW3 held 1 s) or host panic if an audible note hangs.

Replies are sent only by main loop. Largest response is the 111-byte v7 status
(with F0/F7): 35.5 ms at 31250 baud; the UART timeout is computed per reply
(0.32 ms per byte + 5 ms). Incoming SysEx up to 288 bytes is framed (v7 apply
is 99 with F0/F7; file-transfer requests are larger). The USB buffer holds 136 bytes (34 USB-MIDI events); a v3–v7
reply is larger than one 64-byte USB packet, relying on the USB stack's
multi-packet transfer, which is **unverified on hardware**. TX memory survives
completion. Pending TX is abandoned/counts a drop after 100 ms without progress.
While a UART reply blocks the main loop (up to ~27 ms), incoming MIDI waits in
the 16-frame ingress queues; overflow triggers the stuck-note recovery.

Host checks full patch DATA, checksum and sequence. Timeout may mean applied
but reply lost; query status before retrying. No auto retry, deduplication,
subscriptions, sessions or authentication. One host, one acknowledged exchange
at a time. Old 0.2 hosts cannot decode v2 status and 0.3 hosts cannot decode
v3 status, 0.4 hosts cannot decode v4 status, 0.5 hosts cannot decode v5
status, hosts before 0.13 cannot decode v6 status and hosts before 0.14 cannot decode v7
status; use the matching host. Old firmware rejects newer patch versions
(error 2) and 0.2 rejects panic rather than executing them.

## Device presets (SD card), firmware 0.4

8 banks × 15 slots. Panel numbers are 1-based (bank 1–8, slot 1–15); the wire
uses 0-based bytes. One file per slot: `FORGE/B1S01.FPR` … `FORGE/B8S15.FPR`
(never `.bin`, so the bootloader ignores them). Record: `'F' 'P'`, format 1,
DATA length, the patch DATA exactly as in an apply request (indexes 7 up to the
checksum), then CRC-16/CCITT (big-endian) over everything before it. A file
that fails the CRC or the patch decoder reads as an empty slot. Writes go to
`FORGE/TMP.FPR`, are synced, then renamed over the slot, and are read back
before success is reported. Names are not stored; they stay in host JSON.

Ways to recall the same slots:
- **Panel menu (as in TAPE):** with the toggle in TAPE's menu position, press
  the CHOMPI key. While the menu is open, keys select presets instead of
  playing. White keys 1–15 recall slot 1–15 of the current bank (hold CHOMPI).
  Black KEY_16 / KEY_17 step the bank down / up, and turning knob 1 (hardware SW4) also
  selects the bank. Save is KEY_25, erase KEY_23, copy KEY_24 (source, then
  destination, any bank). Choose the mode, press a white key, press CHOMPI to
  confirm; pressing the mode key again cancels. Releasing CHOMPI with nothing
  pending, or moving the toggle back, closes the menu.
- **MIDI program change** on channel 1: program = bank × 15 + slot (0-based),
  0–119; higher programs are ignored. No reply.
- **Host opcodes 04–07** above (`forge_host.py store|recall|erase|slots`, webapp
  Device presets).

Key LEDs while the menu is open: occupied slots dim, the last recalled slot
white, empty slots off, bank keys in the bank's colour (8 colours), save blue,
erase red, copy green (bright while that mode is active). Selections show
blue (save/copy target), red (erase) or green (copy source); occupied slots
blink while choosing. All white keys red: no usable SD card. Panel LED 0
flashes green or red after a panel/program-change action.

SD access runs only in the main loop, never in the audio callback. A store first
takes the audio owner's current patch through the request queue. Saving blocks
the main loop for the SD write (typically a few to tens of ms); incoming MIDI
waits in the 16-frame ingress queues meanwhile. A recalled patch is applied
atomically like any patch request. **Hardware-unverified:** SD timing,
card-swap remount, LED colours and positions.

## USB file transfer and firmware install, firmware 0.7

Opcode **0C** (all builds; reply **48**, or 41 with an error). Byte 7 is the
operation. **W35** = unsigned 32 bits in five 7-bit chunks, low first (fifth ≤ 15).

| Op | Request (after the 7-byte header) | Meaning |
| --- | --- | --- |
| 0 begin | 8–12 size W35, 13 name length n (1–24), 14.. name (ASCII 33–126, no `/ \ :`) | Start a file: `FORGE.bin` (≤ 480 KB) or a TAPE sample `jammi_`/`cubbi_` + `a`–`e` + `1`–`14` + `.wav` (≤ 64 MB). Data goes to `FORGE/UPLOAD.TMP`; an unfinished upload is discarded |
| 1 data | 8–12 offset W35, 13.. packed data | Up to 224 bytes as 32 groups of 8 SysEx bytes (a high-bit byte, bit i = byte i's bit 7, then up to 7 low-7-bit bytes; a final partial group has n+1 bytes). The offset must equal the bytes received so far, else error 4 (status gives the offset to resume from). Request ≤ 270 bytes |
| 2 end | 8–12 CRC-32 W35 (IEEE, as zlib.crc32) | All bytes and the CRC match: the file replaces the target (FatFS sync, then rename). Else error 1 (short) or 3 (CRC), and the card is unchanged. `FORGE.bin` must also pass the bootloader's image test (stack pointer in DTCM/D1 SRAM, Thumb entry point inside the image; else error 4) and is read back from the card and its CRC compared before it replaces the old one (else error 8). Repeating the End of the file just written (its reply was lost) succeeds again; a sample's End is refused with error 9 while the sample loader is busy (retry) |
| 3 abort | — | Discard the upload |
| 4 install | — | Needs `FORGE.bin` on the card and no upload in progress (error 7 / 9; an upload untouched for 5 s is abandoned). The CHOMPI bootloader flashes the first visible root file whose name *contains* `.bin` or `.BIN`, so every other such file is renamed: each `.bin` becomes `_bin`, and a name that ended in it gains `.old` (`CHOMPI_TAPEv2_0.bin` → `CHOMPI_TAPEv2_0_bin.old`; `1_` … `9_` in front if taken; nothing is overwritten). Error 8 if any rename fails. Then it waits 15 s for a CHOMPI key press on the panel (its light blinks white; the press never reaches the menu or record gesture). On the press CHOMPI sends its replies and restarts; the bootloader flashes the new `FORGE.bin` from the card. From 0.11: error 10 (before anything is renamed) unless the power is safe for a restart into the bootloader (`core/power.h` `InstallPowerOk`: battery green/white with no low reading in the last 8, or USB power on a supply that is neither legacy nor at its current limit in the last 8 readings); the power is checked again at the press (if it failed, the gate closes and flag 16 says why) |
| 5 status | — | Report only |

Reply 48 (16 bytes): 7 zero, 8 operation, 9 flags (1 upload active, 2 waiting
for the CHOMPI press, 4 `FORGE.bin` on the card, 8 restarting, 16 an install
would be refused now for power, 0.11), 10–14 bytes
received W35, 15 checksum. Errors: 1 length, 3 CRC, 4 bad name/size/offset or
packing, 7 nothing to finish/install, 8 no card or a write failed, 9 an upload
is in progress, 10 power (install only). The host checks flag 16 with a status
request before uploading `FORGE.bin`. One request at a time per transport is the contract; the host
keeps at most 4 data requests in flight (CHOMPI queues 16 frames per port). The
MIDI framer accepts SysEx up to 288 bytes. Host tools: `host/forge_card.py`,
the bridge's *Card & firmware* section.

## Sampler, firmware 0.5

Design and TAPE comparison: [SAMPLING.md](SAMPLING.md).

**Files.** TAPE's names in the SD root: `jammi_<a-e><1-14>.wav` (chromatic)
and `cubbi_<a-e><1-14>.wav` (kit). Forge reads PCM 8/16/24-bit or 32-bit
float, mono or stereo, 8–96 kHz (pitch-corrected). It writes TAPE's format
(44-byte header, 16-bit stereo, 48 kHz), deletes the slot's stale
`_double.wav` (TAPE regenerates it at its next boot) and never touches
`options.json`; from 0.12 a save/copy/erase also moves the slot's TAPE settings in
`presets.json` (main loop, not on the wire). Saves and copies go to `FORGE_TMP.WAV`, then
replace the slot file.

**Memory.** The chromatic slot, or every file of a kit bank, is loaded into a
32 MiB SDRAM pool (~174 s stereo); a file that does not fit is loaded partly
(the rest plays silent). Loading happens in 16 KB steps in the main loop;
notes can start before a file has fully loaded. The recording lives in its own
16 MB buffer (~87 s stereo).

**Playing.** Chromatic: MIDI note 60 plays the sample at its own pitch (±
semitones across the keys). Kit: the white keys/notes C3–C5 (MIDI 48–72) play
slots 1–15 of the bank (C5 = the recording); black notes are silent. Pitch,
start, end, loop (with crossfade), gate/trigger, reverse; samples pass through
the per-voice filter, filter envelope, LFO, glide, delay and reverb.
Interpolation is 4-point Hermite when pitched down, linear otherwise.

**Recording (as TAPE).** Toggle down, hold the CHOMPI key: records from the
selected source (mic ×5 with DC blocking, line ×3, or resample = the output);
CHOMPI LED red; the input is monitored. Releasing it stops; the take becomes
chromatic slot 15 and plays at once. 5 ms fades at both ends; playback and
saving are normalised to −1 dBFS (at most +24 dB). Jack insertion selects line
in, removal the mic.

**Panel menu.** Toggle up + CHOMPI key, then KEY_22 switches to the Samples
page: KEY_16 chromatic / KEY_17 kit (press again for the next bank a–e; the
instrument follows), white keys play a slot (chromatic) or the bank (kit),
KEY_18/19/20 choose mic / line / resample, KEY_25 save the recording, KEY_23
erase, KEY_24 copy (any mode/bank), each confirmed with CHOMPI. Knob 1 turns
the shown bank. Bank keys light in TAPE's bank colours; occupied slots dim,
the playing slot white, the recording pink, file slots red without a card.
CHOMPI LED blinks pink while a save/copy runs.

**Host.** Opcodes 08/09 above (`forge_host.py samples | sample-save |
sample-erase | sample-copy`, webapp Device samples). A host save first locks
the recording (refused with error 6 while recording, error 7 with no take);
the reply comes when the file is written.

**Hardware-unverified:** SD read/write speed and main-loop stalls during
saves, SDRAM cache behaviour of sample reads (device CPU), recording levels,
monitoring, jack detection, LED colours.

## Development probe and Inspector schema 1

Only `FORGE_TEST_HOOKS` firmware accepts `0A`/`0B`. Release firmware rejects
both with error 5, including all new pages. Protocol version remains 1,
and the patch/status layouts are those above. This is an
additive development extension; the Inspector has its own explicit schema byte.
Unknown pages return error 4. Unknown schemas must be rejected by clients.

`0A` is the existing 11-byte panel injection request: bytes 7 kind, 8 id,
9 value biased by +64, 10 checksum. Kinds: 0 key (id 0–39, value 0/1), 1 turn
(id 0–5, −63..63), 2 SW5 press (id 4, value 0), 3 toggle and 4 jack (id 0,
−1 hardware / 0 off / 1 on), 5 release overrides (id 0, value 0). Reply `47`
is 9 bytes: byte 7 zero, byte 8 checksum. Ack means queued, not yet applied.

`0B` reads a page. Normal request size is 9 bytes: byte 7 page, 8 checksum.
Page 6 additionally accepts a 14-byte request with cursor at 8–12 and checksum
at 13; without the cursor it starts at serial 0. Cursor is unsigned 32-bit in
five little-endian 7-bit chunks, with the fifth chunk at most 15. All sizes
exclude F0/F7. Largest reply still fits `kMaxReply = 109` and the
existing USB/UART buffers. No subscription, background push or new transport.

Page 0 (the original 24-byte state page) was retired on 2026-10-03: the
Inspector pages carry the same state and no host read it any more. A page-0
request now returns error 4 like any unknown page.

- Page 1, 88 bytes: byte 8 page, 9–86 LED RGB triples, 87 checksum. Indices
  0–24 are key LEDs in renderer order, 25 is CHOMPI. Each component 0–127.

New `46` pages 2–9 (page 8 from 0.13, page 9 from 0.14): byte 7 zero, 8 page, 9 schema (1), 10–14 snapshot
generation (unsigned 32-bit); body starts at 15, checksum is the last byte.
The notation **U32** below means five 7-bit chunks, unsaturated, wrapping at
32 bits. **N14** means 0–1 normalized in two 7-bit chunks. All fields are in
listed order, with no struct padding on the wire.

| Page | Size | Body from byte 15 |
| --- | --- | --- |
| 2 SYSTEM | 98 (91 in 0.8–0.9, 88 before) | firmware minor, protocol, simulated flag (3 bytes); uptime ms, audio state timestamp ms, audio block count (3 U32); CPU average and peak ×1000 (2 14-bit words); UART then USB: RX complete accepted frames, TX accepted submissions, TX errors, ingress drops (4 U32 each); aggregate drops and rejections (2 U32); from 0.8: battery level (0 full, 1 high, 2 medium, 3 low, 4 unknown), power flags (1 USB power, 2 charger fault, 4 USB lines handed to the charger IC; from 0.12: 8 weak supply = legacy source or at its current limit in the last 8 readings, 16 a battery reading below the 3.0 V mark, 32 a firmware install would be refused now; hosts before 0.12 reject values above 7), charge state (charger CHG_STAT 0–7; 5 = done); from 0.10: reset flags (1 power-on, 2 brown-out, 4 reset pin, 8 software, 16 watchdog, 32 window watchdog, 64 low-power), crashed (0/1), crash PC (U32; details in FORGE/RESTARTS.TXT) |
| 3 PANEL | 97 | physical and merged key masks (6 7-bit chunks each, 40 bits); physical then merged flags (2 bytes); packed menu U32; six physical encoder accumulators then six merged accumulators (12 U32, signed two's complement); knob pages (two 7-bit chunks: knob n's page 0–4 in bits 3n..3n+2; firmware 0.9 and older: 0–3 in bits 2n, 2n+1) |
| 4 ENGINE | 88 | seven voices (7 bytes each: note, source 0 UART/1 USB/2 panel, stage 0 off/1 attack/2 decay/3 sustain/4 release, sample slot 0–14 or 127 none, flags sampled 1/sustained 2/reverse 4, envelope N14); smoothed cutoff normalized, (LFO+1)/2, mod wheel (3 N14); pedal-source bit mask byte; three smoothed bend ratios ×4096 (3 14-bit words); resolved mix, feedback/0.85, level, delay samples/48000, reverb mix (5 N14) |
| 5 STORAGE | 94 | flags, recording source 0 mic/1 line/2 resample, queued sample-job count, active job 0 save/1 copy/2 erase/127 none, last generic error code, partial-file-slot count (6 bytes); actual loaded file selection, file readable frames, file allocated frames, pool reserved bytes, pool capacity bytes, recording frames, recording capacity frames, storage error count, audio event drops, emergency count, panel queue drops, sample queue drops (12 U32); looper: flags (state 0 empty/1 armed/2 first take/3 playing/4 paused, 8 overdub, 16 effects before the loop, 32 locked for saving), length frames (U32), position, speed ((s+2)/4) and feedback (N14) |
| 6 EVENTS | 27 + 17 × count, count 0–3 | latest event serial U32, total retention overwrites U32, count byte; records: serial U32, timestamp ms U32, kind byte, id byte, value U32 |
| 7 PATCH | 26/38/77/92/96/99/105 for v1–v7 | existing patch DATA, exactly as `EncodePatchData` and status use |
| 8 HARMONY | 38 | harmony word U32 (the v6 bits above, plus 17 Shift held); last chord: root 0–11, degree 0–6, kind (0 none, 1 diatonic, 2 secondary dominant, 3 borrowed, 4 Shift key, 5 modal interchange), quality (0 scale, 1 major, 2 minor, 3 dominant, 4 sus4), Shift (0/1), note count 0–5 (6 bytes); five MIDI notes, unused 0 (5 bytes); notes sounding from the harmony player (byte); chords played since start U32 |
| 9 PARTS | 50 | arp word, clock word (U32 each, the v7 words; live settings); tempo in use ×10 (14-bit: MIDI's measured tempo while following); flags (1 clock running, 2 following MIDI clock, 4 latched, 8 parts active for this patch); clock ticks U32 (24 per beat); notes in the set (byte, 0–16) and the first eight (8 bytes, unused 0); arp note sounding, bass note sounding (0 none); dropped part events U32 |

Page 3 physical flags: toggle up 1, line jack 2, SW5 press edge 4. Merged flags
add overridden 8. Masks include menu/control keys; the map in
`panel::kKeyNotes` identifies musical notes. Raw encoder accumulators are
debounced hardware increments, not analogue positions or electrical pins.
Page 4 cutoff uses the existing logarithmic 40×400^n mapping; delay n×1000 ms;
feedback n×0.85. Resolved values are approximate due to quantization. Patch
targets carry the existing pitch/window/loop/gate/reverse and modulation settings.

Page 5 flags: card driver ready 1, mount configured 2, busy 4, loading 8,
recording 16, take locked 32. Actual file selection uses PackSelection; kit
selection omits slot bits, and `FFFFFFFF` means no completed file selection.
Wanted selection is separately derived from page 7. Aggregate frame counts
exclude RAM slot 15; recording frames come from the audio publication. Error 8
includes unreadable/unsupported WAV headers and file IO failures; no FatFS
detail is claimed. Queue count includes audio-to-main and loader jobs, not
synchronous preset operations or host requests still waiting for audio.

Page 2 latches a main-loop snapshot from the latest audio publication (at most
20 Hz refresh) plus current main-loop storage/counters. Pages 3,4,5,7 then read
that same generation, without relatching. Before any audio publication, requests
return error 6. Read page 2 first and compare generations; another client reading
page 2 can replace the latch. Page 1 is independently live LED shadow; page 6
is an independently live retained log. Audio timestamp makes stale data visible;
these pages do not claim simultaneous physical measurements. No callback load
measurement exists in the offline simulation; clients must label it unavailable.

Event kinds 1–14: key down, key up, knob, voice start, voice stop, patch apply,
sample selection, recording start, recording stop, card, queue error, storage
error, sample loaded, sample job done. Key id is physical switch index; value
bit 1 physical, bit 2 injected. Knob id is hardware encoder index, value is signed
increment. Voice id is voice index; value has note in bits 0–7, source in 8–15,
sample slot/127 in 16–23. Patch value is revision, selection/load value is
PackSelection, record value is frames. Card value bits are present 1/usable
mount 2. Queue-error value is newly observed loss count; storage-error id 0 is
a generic error code, id 1 is a newly observed loader-error count. Job id uses
the storage job enum; value 1 success/0 failure.

The log retains 64 records, read non-destructively after an exclusive cursor.
Only three records fit a page; continue until cursor equals latest or no records
remain. A reader falling behind gets the oldest retained records and detects a
serial gap. Audio observations can also drop from their bounded queue; those
losses are a separate page-5 counter. Block-level voice/patch/record transitions
may collapse multiple changes in one block. See [INSPECTOR.md](INSPECTOR.md) for
ownership, limitations, host use and the exact next physical test.

