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

Open **http://127.0.0.1:8765** in that computer's browser. The server binds only
to this computer; it is not a hosted site or a phone-accessible LAN service.
Use `--port 8766` if the default port is occupied, and open the printed URL.
Stop with Ctrl+C. AI authoring, preset editing and downloads also work without
installing MIDI dependencies; those are needed only for hardware control.

1. Choose **OpenAI** or **Gemini**. Enter an API key from that provider and a
   model ID available to your API account that supports structured JSON output.
   Model IDs are editable rather than tied to a particular model's lifecycle.
2. Describe the delay and choose **Generate a patch**. A validated patch opens
   in the editor. Provider refusal, truncated output and invalid settings fail
   without replacing the current patch. There is no automatic retry or fallback.
3. Adjust mix, delay time, feedback, level and wet bypass. Start from any of the
   three included presets or import a JSON file. **Save JSON** validates and
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
JavaScript syntax and static asset serving are checked; actual browser interaction
and visual checks remain pending because the development browser download failed.

Provider references:
[OpenAI structured outputs](https://developers.openai.com/api/docs/guides/structured-outputs),
[Gemini structured outputs / Generate Content](https://ai.google.dev/gemini-api/docs/generate-content/structured-output).
The adapters use OpenAI Responses `text.format` and Gemini Generate Content
`generationConfig.responseFormat.text` respectively. Both responses also pass
Forge's independent local validator before reaching the editor.

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
