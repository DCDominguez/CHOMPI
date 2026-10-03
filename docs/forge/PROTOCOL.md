# Forge control protocol — firmware 0.4

Transport version remains **1**; patch formats **1, 2 and 3** are supported.
Firmware 0.4 adds patch v3 (second oscillator, noise, resonant filter with
envelope, LFO, voice limit, glide, reverb). v1/v2 patches render bit-exactly
as on 0.3 (checked against the previous core in simulation).
All lengths/indexes below exclude MIDI F0/F7 unless stated. USB and bidirectional
TRS MIDI use the non-commercial manufacturer ID 7D followed by ASCII FG.

## Envelope and operations

`F0 7D 46 47 01 OP SEQ_LO SEQ_HI DATA... CHECKSUM F7`

Every payload byte is 7-bit. Words are little-endian base 128. Sequence is
0–16383, echoed in replies. Sum of payload bytes including checksum is 0 modulo
128. This detects corruption, not malicious traffic; there is no authentication.

| OP | Length | Operation |
| --- | --- | --- |
| 01 | 18 for v1, 30 for v2, 69 for v3 | Apply complete patch |
| 02 | 8 | Read current patch and status |
| 03 | 8 | Panic: silence all synth voices and clear old delay tail; return status |
| 04 | 10 | Store the current device patch to SD preset (bank 0–7 at 7, slot 0–14 at 8); reply 42 |
| 05 | 10 | Recall SD preset (bank, slot): applied like 01; reply 40 with the recalled patch |
| 06 | 10 | Erase SD preset (bank, slot); reply 42 |
| 07 | 8 | List occupied SD presets; reply 43 |
| 40 | 30 for v1, 42 for v2, 81 for v3 | Success/current targets plus diagnostics |
| 41 | 9 | Rejection; error code at index 7, checksum at 8 |
| 42 | 12 | Preset done: index 8 = 1 stored / 2 erased, 9 bank, 10 slot |
| 43 | 33 | Occupancy: per bank (0–7) 3 bytes at 8 + 3·bank = 15-bit slot mask, 7 + 7 + 1 bits |

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
| 68 | v3 checksum |

v3 indexes 7–28 are identical to v2, except that the cutoff at 27–28 feeds the
v3 per-voice resonant filter instead of the shared one-pole filter.

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

v1 always selects aux input and restores default dormant synth settings. v2
selects one of two supported paths; all three named module settings are present
in JSON even on the aux path. v3 has the same two paths with reverb after the
delay; all six modules are present in JSON. Fields a format lacks take neutral
defaults (no osc2, noise, LFO, glide or reverb; four voices). No arbitrary edges, feedback routing, plugin code
or dynamic module creation is accepted. Names remain host-side.

## Success/status response

| Index | Content |
| --- | --- |
| 0–6 | Header, opcode 40, sequence |
| 7 | Success, 0 |
| 8 onward | Exact quantized patch DATA (request indexes 7 through before checksum) |

After patch DATA, diagnostics begin at offset **18 for v1**, **30 for v2**, **69 for v3**:

| Offset from diagnostics | Content |
| --- | --- |
| +0,+1 | Average audio callback load ×1000, 14-bit |
| +2,+3 | Peak audio callback load since boot ×1000, 14-bit |
| +4..+6 | Dropped ingress/control/reply count, saturated 21-bit |
| +7..+9 | Rejected recognized requests, saturated 21-bit |
| +10 | Firmware minor version, 4 |
| +11 | Checksum |

CPU resolution is 0.1 percentage point; max 1638.3%. Readings are from completed
callbacks before the snapshot, not total scheduling/interrupt-mask time. Offline
harness readings are synthetic zero and say nothing about device headroom.

Errors: 1 length, 2 protocol/patch version, 3 checksum, 4 patch fields or
preset address, 5 opcode, 6 queue busy, 7 empty preset slot, 8 SD card missing
or storage failed, 9 storage busy. Foreign SysEx/replies are ignored. Framing
discards may be silent and are not included in rejected recognized-request counts.

## Notes, controls and recovery

Channel 1 (zero-based 0): Note On/Off, including Note On velocity zero. Notes
0–127 accepted; keybed uses 48–72 at velocity 100. UART, USB and keybed have
separate source IDs. Matching source/note is retriggered; otherwise idle voices
are used, then the quietest releasing voice, then the oldest held voice of four.
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

CC20 mix, 21 time, 22 feedback, 23 level, 24 wet bypass (>=64 on), 25 cutoff,
26 resonance and 27 reverb mix (v3 patches only; ignored on v1/v2 so status
stays truthful), 64 sustain pedal, 121 reset controllers, 1 mod wheel (scales
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
still hardware-unverified. Use SW5 press or host panic if an audible note hangs.

Replies are sent only by main loop. Largest response is the 83-byte v3 status
(with F0/F7): 26.6 ms at 31250 baud; the UART timeout is computed per reply
(0.32 ms per byte + 5 ms). Incoming SysEx up to 72 bytes is accepted (v3 apply
is 71 with F0/F7). The USB buffer holds 112 bytes (28 USB-MIDI events); a v3
reply is larger than one 64-byte USB packet, relying on the USB stack's
multi-packet transfer, which is **unverified on hardware**. TX memory survives
completion. Pending TX is abandoned/counts a drop after 100 ms without progress.
While a UART reply blocks the main loop (up to ~27 ms), incoming MIDI waits in
the 16-frame ingress queues; overflow triggers the stuck-note recovery.

Host checks full patch DATA, checksum and sequence. Timeout may mean applied
but reply lost; query status before retrying. No auto retry, deduplication,
subscriptions, sessions or authentication. One host, one acknowledged exchange
at a time. Old 0.2 hosts cannot decode v2 status and 0.3 hosts cannot decode
v3 status; use the matching 0.4 host. Old firmware rejects newer patch versions
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
  Black KEY_16 / KEY_17 step the bank down / up, and turning encoder 1 also
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

