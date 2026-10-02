# Forge control protocol — firmware 0.3

Transport version remains **1**; patch formats **1 and 2** are supported.
All lengths/indexes below exclude MIDI F0/F7 unless stated. USB and bidirectional
TRS MIDI use the non-commercial manufacturer ID 7D followed by ASCII FG.

## Envelope and operations

`F0 7D 46 47 01 OP SEQ_LO SEQ_HI DATA... CHECKSUM F7`

Every payload byte is 7-bit. Words are little-endian base 128. Sequence is
0–16383, echoed in replies. Sum of payload bytes including checksum is 0 modulo
128. This detects corruption, not malicious traffic; there is no authentication.

| OP | Length | Operation |
| --- | --- | --- |
| 01 | 18 for v1, 30 for v2 | Apply complete patch |
| 02 | 8 | Read current patch and status |
| 03 | 8 | Panic: silence all synth voices and clear old delay tail; return status |
| 40 | 30 for v1, 42 for v2 | Success/current targets plus diagnostics |
| 41 | 9 | Rejection; error code at index 7, checksum at 8 |

## Apply request layout

| Index | v1 and v2 |
| --- | --- |
| 0–6 | Header, operation and sequence |
| 7 | Patch version, 1 or 2 |
| 8–9 | Mix normalized 14-bit |
| 10–11 | Time normalized 14-bit |
| 12–13 | Feedback normalized 14-bit |
| 14–15 | Output level normalized 14-bit |
| 16 | Wet bypass, 0 or 1 |
| 17 | v1 checksum; v2 route: 0 aux→delay→output, 1 synth→delay→output |
| 18 | v2 waveform: 0 sine, 1 triangle, 2 saw, 3 square |
| 19–20 | v2 attack normalized 14-bit |
| 21–22 | v2 decay normalized 14-bit |
| 23–24 | v2 sustain normalized 14-bit |
| 25–26 | v2 release normalized 14-bit |
| 27–28 | v2 cutoff normalized 14-bit |
| 29 | v2 checksum |

For normalized value n = word / 16383:

| Control | Physical conversion |
| --- | --- |
| Mix, level, sustain | n |
| Time | 10 + 990 × n milliseconds |
| Feedback | 0.85 × n |
| Attack / decay | 1 + 1999 × n milliseconds |
| Release | 5 + 4995 × n milliseconds |
| Cutoff | 40 × 400^n Hz (logarithmic, 40–16000 Hz) |

v1 always selects aux input and restores default dormant synth settings. v2
selects one of two supported paths; all three named module settings are present
in JSON even on the aux path. No arbitrary edges, feedback routing, plugin code
or dynamic module creation is accepted. Names remain host-side.

## Success/status response

| Index | Content |
| --- | --- |
| 0–6 | Header, opcode 40, sequence |
| 7 | Success, 0 |
| 8 onward | Exact quantized patch DATA (request indexes 7 through before checksum) |

After patch DATA, diagnostics begin at offset **18 for v1**, **30 for v2**:

| Offset from diagnostics | Content |
| --- | --- |
| +0,+1 | Average audio callback load ×1000, 14-bit |
| +2,+3 | Peak audio callback load since boot ×1000, 14-bit |
| +4..+6 | Dropped ingress/control/reply count, saturated 21-bit |
| +7..+9 | Rejected recognized requests, saturated 21-bit |
| +10 | Firmware minor version, 3 |
| +11 | Checksum |

CPU resolution is 0.1 percentage point; max 1638.3%. Readings are from completed
callbacks before the snapshot, not total scheduling/interrupt-mask time. Offline
harness readings are synthetic zero and say nothing about device headroom.

Errors: 1 length, 2 protocol/patch version, 3 checksum, 4 patch fields, 5 opcode,
6 queue busy. Foreign SysEx/replies are ignored. Framing discards may be silent
and are not included in rejected recognized-request counts.

## Notes, controls and recovery

Channel 1 (zero-based 0): Note On/Off, including Note On velocity zero. Notes
0–127 accepted; keybed uses 48–72 at velocity 100. UART, USB and keybed have
separate source IDs. Matching source/note is retriggered; otherwise idle voices
are used, then the oldest of four is stolen. An old note-off cannot release a
replacement with a different note/source. No sustain pedal, pitch bend, octave
switching, clock sync, MPE, aftertouch, arpeggiator or MIDI note output yet.

CC20 mix, 21 time, 22 feedback, 23 level, 24 wet bypass (>=64 on), 25 cutoff.
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

Panic logically clears delay history with an O(1) reset marker; no full SDRAM
clear in the callback. Invalid patches never partially change targets or voices.
Queue overflow drops/counts controls. A full request queue on a note, or any
dropped ingress frame (the main loop cannot tell whether it was a note), triggers
global silence and discards queued notes/ingress to avoid stuck notes
(`RecoveryGate` in core/runtime.h, host-tested). Retrigger after
recovery. This logic is implemented; actual interrupt/transport behavior is
still hardware-unverified. Use SW5 press or host panic if an audible note hangs.

Replies are sent only by main loop. Largest response is 44 bytes with F0/F7:
14.08 ms at 31250 baud, within the UART 25 ms timeout. USB packet buffer is 64
bytes (44-byte SysEx uses 60 bytes). TX memory survives completion. Pending TX
is abandoned/counts a drop after 100 ms without progress.

Host checks full patch DATA, checksum and sequence. Timeout may mean applied
but reply lost; query status before retrying. No auto retry, deduplication,
subscriptions, sessions or authentication. One host, one acknowledged exchange
at a time. Old 0.2 hosts cannot decode v2 status; use the matching 0.3 host.
Old firmware rejects v2 patches and panic rather than executing them.
