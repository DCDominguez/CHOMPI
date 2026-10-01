# Forge host controller

Python 3.10+ on the computer connected to CHOMPI. Commands below assume the
current directory is `firmware/chompi-forge/`, or the extracted test bundle.
Use `python3` instead of `python` if that is your Python executable.

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
and recall do not write the CHOMPI SD card. Reboot returns to firmware defaults;
send a saved patch again to restore it. Patch transfer uses 14-bit quantization.

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

## Offline DSP reference

For developers, `make test` builds `build/forge_probe`. It accepts hex MIDI bytes
on stdin and prints reply payloads using the same decoder, runtime and engine
as firmware. Add `--render output.wav` to render a four-second synthetic pluck
through the active patch. Rendered examples are simulations, not CHOMPI recordings.

API references: [Mido ports](https://mido.readthedocs.io/en/stable/ports/) and
[Ollama structured outputs](https://docs.ollama.com/capabilities/structured-outputs).
