# Forge v1 patch/control protocol

Implemented by firmware candidate 0.2. Transport is USB or TRS MIDI SysEx.
MIDI channel 1 CC20–24 remains available for individual controls. SysEx is
channel-independent and uses the non-commercial/development manufacturer ID
`7D`, followed by ASCII `FG`. This is an experimental private protocol.

## Envelope

`F0 7D 46 47 01 OP SEQ_LO SEQ_HI DATA... CHECKSUM F7`

All payload bytes are 7-bit. Words are least-significant 7 bits first. Sequence
numbers are 0–16383, chosen by the host and echoed by the device. The checksum
makes the sum of payload bytes (from `7D` through checksum) zero modulo 128.
It detects accidental corruption; it is not authentication.

| OP | Payload length, excluding F0/F7 | DATA |
| --- | --- | --- |
| `01` apply patch | 18 | Patch version `01`, four 14-bit words, bypass `00` or `01` |
| `02` query status | 8 | Empty |
| `40` accepted/status reply | 30 | Described below |
| `41` rejected reply | 9 | Error code |

Four words: mix, delay time, feedback, output level, each normalized to
0–16383. Physical time = `10 + 990 * word / 16383` ms. Physical feedback =
`0.85 * word / 16383`. Bypass is a separate byte, not a word.
The host JSON format carries patch version, name, engine, and physical values;
the name remains on the host. The device has one active volatile patch.

## Status layout

Zero-based indexes refer to payload, excluding F0/F7.

| Index | Content |
| --- | --- |
| 0–6 | Header including reply opcode and sequence |
| 7 | `00` success |
| 8 | Patch version `01` |
| 9–16 | Four 14-bit normalized parameters |
| 17 | Bypass boolean |
| 18–19 | Smoothed average callback load, fraction × 1000 |
| 20–21 | Peak callback load since boot, fraction × 1000 |
| 22–24 | Dropped ingress/control/reply count, saturated to 21 bits on wire |
| 25–27 | Rejected recognized request count, saturated to 21 bits on wire |
| 28 | Firmware minor version (`02` for candidate 0.2) |
| 29 | Checksum |

CPU reporting has 0.1 percentage-point resolution and saturates at 1638.3%.
CPU readings are from completed callbacks before the snapshot; they do not
measure all interrupt masking or total wall-clock scheduling delays. Zero at
startup means no completed sample yet. The offline harness reports synthetic
zero CPU load and cannot predict device performance.

Errors: 1 length, 2 unsupported protocol/patch version, 3 checksum, 4 invalid
patch, 5 unknown operation, 6 request queue busy. Foreign SysEx and replies are
ignored. Malformed/truncated/oversized envelopes may be discarded without a
reply; byte-framing discards are not included in rejected-request counts.

## Atomicity, acknowledgements, and overload

Main-loop parsing validates a whole patch before queueing it. The audio owner
assigns the complete parameter set between blocks, then queues the resulting
snapshot. Delay state and smoothing survive recall. Invalid patch fields never
partially update the engine. Acknowledgement means the targets were accepted;
it does not certify audible results. Encoders and later commands may change
the targets afterwards.

One producer/consumer per ingress stream; one main-to-audio request queue and
one audio-to-main response queue. Each callback handles at most 16 requests.
It stops draining requests while response capacity is unavailable. Incoming
overflow drops the newest item and counts it. Main-loop transmission keeps USB
buffers alive until completion; UART replies use a timeout longer than their
wire duration. Pending TX is abandoned after 100 ms if it cannot progress.
No transport transmission occurs in the audio callback.

The host matches sequences and checks the acknowledged parameters. A timeout
does not prove failure: the patch may already be active. Query status before
retrying. There is no sequence deduplication, authentication, batch transaction,
automatic retry, parameter subscription, or graph loading in this protocol.
Use one host and one outstanding acknowledged request at a time.

MIDI real-time bytes can interrupt SysEx/CC and are ignored by Forge's framing
layer. Oversized SysEx is discarded in full. USB packetization preserves F7 in
the final USB-MIDI packet; this avoids the bundled transport's split-ending
behavior without changing upstream library sources.
