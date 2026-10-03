# Forge host controller

Python 3.10+ on the computer connected to CHOMPI. Commands below assume the
current directory is `firmware/chompi-forge/`, or the extracted test bundle.
Use `python3` instead of `python` if that is your Python executable.

## Webapp — OpenAI or Gemini with your key

Run this on the computer connected to CHOMPI, using Python 3.10 or newer:

```sh
python -m pip install -r host/requirements.txt
python host/forge_web.py
```

Open **http://127.0.0.1:8765** (or **http://localhost:8765**) in that computer's browser. The server binds only
to this computer; it is not a hosted site or a phone-accessible LAN service.
Use `--port 8766` if the default port is occupied, and open the printed URL.
Stop with Ctrl+C. AI authoring, preset editing and downloads also work without
installing MIDI dependencies; those are needed only for hardware control.

1. Choose **OpenAI** or **Gemini**. Enter an API key from that provider and a
   model ID available to your API account that supports structured JSON output.
   Model IDs are editable rather than tied to a particular model's lifecycle.
2. Choose **Playable instrument** or **External-audio delay**, describe the sound and choose **Generate a patch**. A validated patch opens
   in the editor. Provider refusal, truncated output and invalid settings fail
   without replacing the current patch. There is no automatic retry or fallback.
3. Adjust every module: oscillators and noise, amplitude envelope and voices,
   filter, LFO, delay, reverb and output. Controls a v1 or v2 preset does not
   have are greyed out; **Convert to v3 instrument** upgrades it (new modules
   start neutral). Start from any of the ten included presets or import a JSON file. **Save JSON** validates and
   downloads a preset through your browser; normal browser download rules apply.
4. Connect CHOMPI, select **Refresh ports**, then explicitly select the MIDI
   input and output. **Read device status** reports firmware, CPU and counters.
   **Capture to editor** reads current device targets, including knob changes.
5. Choose **Send to CHOMPI** when ready. It validates the patch, sends it once,
   and waits for an acknowledgement matching the requested values. Generation
   and editing alone never send MIDI. Do not run another MIDI host concurrently.

Only the prompt, fixed authoring instruction and patch schema go to the selected
provider; no audio is uploaded. API usage may incur charges under your provider
account. A consumer chat subscription is not used by this integration.

Forge keeps the entered key in the page's memory and the active local request.
It does not write keys to presets, browser storage, source files or logs. Clear
the key with **Clear**; switching providers also clears the key and model field.
Do not paste keys into chat or commit them. The browser calls only the local
bridge, which sends credentials in HTTPS headers to fixed provider URLs and
refuses redirects. OpenAI requests set `store: false`; provider data policies
still apply. There is no persistent login or key vault in this first version.

The local bridge checks Host, Origin, fetch metadata and a session token, serves
only its three frontend assets, and does not enable cross-origin access. It is
intended for a trusted local computer, not public deployment or shared hosting.
One AI request and one MIDI exchange can be active per server; no automatic
patch resend occurs after a timeout. Read status before deciding to retry.

The provider adapters and MIDI path have automated mock coverage. Live API-key
requests and physical CHOMPI operation still need the consolidated test session.
Real-Chromium interaction tests (`make browser-test`) drive the webapp against a
simulated device; they are not hardware or live-provider tests.

Provider references:
[OpenAI structured outputs](https://developers.openai.com/api/docs/guides/structured-outputs),
[Gemini structured outputs / Generate Content](https://ai.google.dev/gemini-api/docs/generate-content/structured-output).
The adapters use OpenAI Responses `text.format` and Gemini Generate Content
`generationConfig.responseFormat.text` respectively. Both responses also pass
Forge's independent local validator before reaching the editor.

## Instrument patches and playing

Firmware 0.5 plays v4 instrument patches (and still v1–v3). Load Warm Pad, Acid
Bass or Bell Keys; connect MIDI, send once, then play CHOMPI keys (MIDI 48–72)
or incoming channel 1 notes. Keybed velocity is fixed at 100; MIDI velocity
changes loudness. Synth output is mono duplicated to stereo through the delay
and reverb. No audio input is needed on the synth route. The aux route
processes external stereo audio.

v4 JSON has `version: 4`, `engine: "instrument"`, `routing`
(`synth>delay>reverb>output`, `sampler>delay>reverb>output` or
`aux>delay>reverb>output`) and seven modules, all keys required (v3 is the same
without `sampler`, with `voices` 1–4):

| Module | Keys and ranges |
| --- | --- |
| `synth` | `waveform` and `osc2_waveform` (sine/triangle/saw/square); `attack_ms`, `decay_ms` 1–2000; `sustain` 0–1; `release_ms` 5–5000; `osc2_level` 0–1; `osc2_semitones` integer −24…24; `osc2_detune_cents` −50…50; `noise` 0–1; `voices` integer 1–7 (v3: 1–4); `glide_ms` 0–2000 |
| `sampler` | `mode` chromatic/kit; `bank` a–e; `slot` integer 1–15 (15 = the recording; kit ignores it); `pitch_semitones` −24…24; `start`, `end` 0–1 with start < end; `loop`, `hold` (true: sounds while held; false: one-shot), `reverse` booleans; `crossfade_ms` 0–250 |
| `filter` | `cutoff_hz` 40–16000; `resonance` 0–1; `env_octaves` −6…6; `attack_ms`, `decay_ms` 1–2000; `sustain` 0–1; `release_ms` 5–5000 |
| `lfo` | `waveform` (sine/triangle/square/sample_hold); `rate_hz` 0.05–20; `pitch_cents` 0–200; `filter_octaves` 0–4; `amp_depth` 0–1; `mod_wheel` boolean |
| `delay` | `mix` 0–1; `time_ms` 10–1000; `feedback` 0–0.85; `bypass` boolean |
| `reverb` | `mix`, `size`, `damping` 0–1 |
| `output` | `level` 0–1 |

v3, v2 JSON (`synth`, `delay`, `output`; routes without reverb) and v1 delay files
still load, send and capture. `python host/forge_host.py upgrade old.json new.json`
writes a v3 copy (`--to 4` for v4) with the same settings and neutral new
modules; the v3 filter is steeper than v1/v2, so tone can differ slightly.
`schema --instrument` prints the v4 schema. No arbitrary graph, looper or FM
can be generated, and AI never creates audio: the sampler plays files that are
already on the card or the device's recording.

**Panic / stop sound** clears all voices and old delay and reverb tails; it requires both
MIDI ports and waits for a reply. SW5 press or channel-1 CC120/123 also panic.
SW5 turn/CC24/CC74 change cutoff; CC71 resonance and CC91 reverb mix (v3); CC85 wet bypass; CC1 mod
wheel scales LFO depth when the patch enables it. Route or waveform changes,
and switching between v1/v2 and v3/v4 patches or between oscillators and sampler, stop voices/tails; retrigger held notes. Up to the patch's `voices` (4, or 7 on v4); additional notes steal a releasing voice, else the oldest.
A patch is volatile; save JSON and resend after reboot. v1 delay files still work.
Firmware 0.4 rejects v4 patches, 0.3 also v3, 0.2 also v2 and panic. Use the matching 0.5 host tools.

```sh
python host/forge_host.py panic --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
```

Ollama CLI authoring currently emits v1 delay patches only. OpenAI/Gemini webapp
supports both authoring modes (v4 instrument, v1 delay); after Read samples
in the Device samples panel, instrument authoring may use only the samples
CHOMPI reported, and a patch pointing at a missing sample is refused. Instrument code has software and
real-Chromium test coverage, but real model and hardware acceptance remain pending.

## Command-line controller

```sh
python host/forge_host.py validate presets/03-long-echo.json
python host/forge_host.py encode presets/03-long-echo.json
python host/forge_host.py schema
```

These need no MIDI dependencies or hardware. JSON uses physical units and is
strict: unknown keys/engines/versions, duplicate keys, NaN, and out-of-range
values fail validation. `mix` and `level` are 0–1; `time_ms` is 10–1000;
`feedback` is 0–0.85; `bypass` is boolean. Name is 1–80 characters.

## Live control and saving

```sh
python -m pip install -r host/requirements.txt
python host/forge_host.py ports
python host/forge_host.py status --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
python host/forge_host.py send presets/03-long-echo.json --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
python host/forge_host.py capture my-patch.json --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
```

Use the exact names printed by `ports`, not the placeholders above. USB provides
both directions. With TRS, connect MIDI both ways through a computer MIDI
interface for replies; an input-only connection cannot acknowledge a request.
The library's USB device name may still identify itself as Daisy rather than
Forge. Verify the firmware version in `status`.

Send waits for a matching acknowledgement and verifies its patch values.
Status includes active targets, CPU average/peak, and drops/rejections. If a
reply times out, the command might have applied: query status before retrying.
Neither acknowledgement nor CPU values replace listening during the test.

Capture writes the current targets as a new host file, including physical
knob changes. Existing files are not overwritten. Recall with `send`. Saving
and recall use files on the computer. To keep sounds on CHOMPI itself, use device
presets (below). Reboot returns to firmware defaults;
send a saved patch again to restore it. Patch transfer uses 14-bit quantization.

## Samples on the SD card

TAPE's files (`jammi_…`, `cubbi_…`, banks a–e, slots 1–14) and the recording:

```sh
python host/forge_host.py samples --input "IN" --output "OUT"                          # what is on the card
python host/forge_host.py sample-save kit b 3 --input "IN" --output "OUT"              # recording -> cubbi_b3.wav
python host/forge_host.py sample-copy kit b 3 chromatic a 1 --input "IN" --output "OUT"
python host/forge_host.py sample-erase kit b 3 --input "IN" --output "OUT"            # also removes _double
```

Saving is refused while recording (busy) or with no take (empty). The webapp's
Device samples panel does the same plus "Use in patch" (sets the editor's
sampler module and routing). Record on the device: toggle down, hold CHOMPI.

## Device presets on the SD card

```sh
python host/forge_host.py slots --input "IN" --output "OUT"          # occupied slots per bank
python host/forge_host.py store 1 3 --input "IN" --output "OUT"      # save the device's current sound
python host/forge_host.py recall 1 3 --input "IN" --output "OUT"     # load it (prints the patch)
python host/forge_host.py erase 1 3 --input "IN" --output "OUT"
```

Banks 1–8 and slots 1–15, numbered as on the panel; MIDI program change n
recalls bank n / 15 + 1, slot n % 15 + 1. `store` saves what CHOMPI is playing
now, so `send` a patch first. The card keeps sounds, not names. In the webapp
the same actions are under **Device presets · SD card** (Erase needs a second
press within 4 seconds). On the instrument itself use the TAPE-style menu
(firmware guide).

## Optional local-model authoring

Requires a running Ollama server and a model you have already installed. Forge
does not install/download models, start a service, or require an API key.

```sh
python host/forge_host.py ai "Long echoes with gentle repeats, keep output at 25 percent" --model YOUR_INSTALLED_MODEL --out my-ai-patch.json
python host/forge_host.py send my-ai-patch.json --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
```

Default endpoint: `http://127.0.0.1:11434/api/chat`. `--endpoint` can specify an
Ollama-compatible HTTPS service. Only the instruction, prompt, and patch schema
are sent. AI generates a JSON preset for the existing delay; it does not create
new DSP code, filters, reverb or a graph. The model response must pass local
validation before being saved. Generation never sends MIDI automatically.
There is no fallback pretending that a deterministic preset was model-generated.

The adapter has been tested with mocked responses; model availability, latency,
and musical interpretation still need a real local-model test.

## Test traffic: CC and notes

For the hardware session or without a MIDI keyboard (channel 1, no reply exists):

```sh
python host/forge_host.py cc 85 127 --output "EXACT OUTPUT NAME"      # wet bypass on
python host/forge_host.py cc 123 0 --output "EXACT OUTPUT NAME"       # panic
python host/forge_host.py note 60 64 67 71 74 --hold 3 --output "EXACT OUTPUT NAME"
python host/forge_host.py note 60 --velocity 30 --zero-velocity-off --output "EXACT OUTPUT NAME"
python host/forge_host.py note 69 --bend 8191 --hold 2 --output "EXACT OUTPUT NAME"   # +2 semitones
python host/forge_host.py note 60 64 67 --sustain --hold 3 --output "EXACT OUTPUT NAME"
```

`note` plays the notes together, holds, then always releases them, even after
Ctrl+C. `--bend` (-8192..8191) bends after note-on and recentres at the end.
`--sustain` presses CC64, releases the keys at once, holds, then lifts the
pedal; the chord should keep sounding until the pedal lifts.

## Live provider check

`python host/forge_ai_check.py --provider openai|gemini --model ID` makes one
real request with a hidden key prompt and appends a key-free result record. See
[LIVE_AI_TEST.md](../../../docs/forge/LIVE_AI_TEST.md).

## Offline DSP reference

For developers, `make test` builds `build/forge_probe`. It accepts hex MIDI bytes
on stdin and prints reply payloads using the same decoder, runtime and engine
as firmware. Add `--render output.wav` to render a four-second synthetic pluck
through the active patch. Rendered examples are simulations, not CHOMPI recordings.

API references: [Mido ports](https://mido.readthedocs.io/en/stable/ports/) and
[Ollama structured outputs](https://docs.ollama.com/capabilities/structured-outputs).
