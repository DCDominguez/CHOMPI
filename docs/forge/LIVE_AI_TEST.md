# Forge — live OpenAI / Gemini test

Status: **not yet run.** Request formats were checked against the providers'
public docs on 2026-10-03 (OpenAI Responses `text.format` json_schema strict;
Gemini `generationConfig.responseFormat.text`) and are covered by mocked tests.
No real request has been made by any agent. Only DC runs this, with his own keys.

## Rules

- Type the key only into the hidden prompt or the webapp key field on your own
  computer. Never paste it into chat, issues, commits, screenshots or records.
- Each request may be billed by the provider.
- Generation never sends anything to CHOMPI. Review before sending.

## Step 1 — command-line preflight (one request per provider)

From `firmware/chompi-forge/host` (or `host/` in the test bundle):

```sh
python3 forge_ai_check.py --provider openai --model YOUR_OPENAI_MODEL_ID
python3 forge_ai_check.py --provider gemini --model YOUR_GEMINI_MODEL_ID
```

The key is read with a hidden prompt (or from `FORGE_API_KEY` if you set it in
your shell yourself). Each run appends a key-free record to
`forge-ai-check.jsonl`: provider, model, prompt, seconds, pass/fail, safe error
text, and the validated patch. PASS means the reply was complete, matched the
schema, passed Forge's own validation and encodes to the firmware 0.15 wire format
(instrument mode asks for a v7 patch: seven modules including the sampler, four knob choices,
harmony and the arp / bass / clock parts,
~50 fields, integers, booleans and enums as well as numbers; ranges are
repeated in field descriptions). The command-line check has no device, so the
model is told no sample list was read; in the webapp, Read samples first and
the AI may use only the samples CHOMPI reported.

Optional: `--kind delay` checks external-audio delay authoring; `--prompt "..."`
tries your own description.

## Step 2 — webapp

`python3 forge_web.py`, open the printed URL (or `http://localhost:8765`).
Select provider, enter model ID and key, describe a sound, Generate. Confirm the
patch loads into the editor, then press Clear on the key field.

## If it fails

| Message | Likely cause |
| --- | --- |
| API key was not accepted | Wrong/revoked key or wrong provider selected |
| Key or project lacks access / Model was not found | Model ID not available to that account |
| Request rejected; check structured JSON output | Model lacks structured output, or the provider rejected a schema keyword. Record the model; an agent can then relax the schema (e.g. move ranges to descriptions only) |
| Provider returned an incomplete, refused, or invalid patch | Output cut off (reasoning models may need more tokens), refused, or out of range. Retry once with a clearer prompt; record it |
| Could not reach the provider | Network, proxy or firewall |

Record results in `TEST_RESULTS.md` (provider, model, pass/fail, seconds, any
message). Never record keys.
