"""Cloud patch authoring. Credentials are used for one request, never persisted."""
import copy
import json
import re
import urllib.error
import urllib.request

from forge_host import SCHEMA, SCHEMA3, parse_json, validate_patch

SYSTEM = (
    "Author a Forge v1 stereo_delay JSON preset matching the schema. Only mix, "
    "time_ms, feedback, level and bypass exist. Do not invent effects or code. "
    "Translate descriptions within these delay controls. Default level to 0.25 "
    "unless explicitly requested. Return only the preset."
)


class ProviderError(RuntimeError):
    """A safe public error without provider bodies, credentials or request headers."""


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ProviderError("Provider redirect refused. Check the API documentation.")


UNITS = {"time_ms": "milliseconds", "attack_ms": "milliseconds", "decay_ms": "milliseconds",
         "release_ms": "milliseconds", "cutoff_hz": "hertz, resonant low-pass cutoff", "mix": "wet fraction",
         "feedback": "echo feedback fraction", "level": "output gain fraction", "sustain": "envelope level fraction",
         "osc2_level": "second oscillator level relative to the first", "osc2_semitones": "second oscillator interval in semitones",
         "osc2_detune_cents": "second oscillator detune in cents", "noise": "white noise level",
         "voices": "polyphony; 1 = monophonic", "glide_ms": "portamento time in milliseconds, 0 = off",
         "resonance": "filter resonance fraction", "env_octaves": "filter envelope amount in octaves (negative closes)",
         "rate_hz": "LFO rate in hertz", "pitch_cents": "LFO vibrato depth in cents",
         "filter_octaves": "LFO filter sweep depth in octaves", "amp_depth": "LFO tremolo depth fraction",
         "size": "reverb size/decay fraction", "damping": "reverb high-frequency damping fraction"}


def describe(node, key=None):
    """Mirror numeric ranges into descriptions. Providers whose strict mode ignores or
    limits range keywords still see them; local validation remains the authority."""
    if isinstance(node, dict):
        if node.get("type") in ("number", "integer") and "minimum" in node and "maximum" in node:
            node["description"] = f"{UNITS.get(key, 'value')}; must be between {node['minimum']} and {node['maximum']}"
        for name, child in node.get("properties", {}).items():
            describe(child, name)
    return node


def generate_patch(provider, api_key, model, prompt, opener=None, kind="delay"):
    if provider not in ("openai", "gemini"):
        raise ValueError("Choose OpenAI or Gemini")
    if not isinstance(api_key, str) or not 1 <= len(api_key) <= 512 or not all(
            33 <= ord(c) <= 126 for c in api_key):
        raise ValueError("Enter an API key without spaces or control characters")
    if not isinstance(model, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}", model):
        raise ValueError("Enter a model ID using letters, numbers, dots, dashes or underscores")
    if not isinstance(prompt, str) or not prompt.strip() or len(prompt) > 4000:
        raise ValueError("Describe your sound in 1–4000 characters")
    if kind not in ("delay", "instrument"): raise ValueError("Unknown authoring mode")
    schema = copy.deepcopy(SCHEMA3 if kind == "instrument" else SCHEMA)
    schema.pop("$schema", None)
    system = SYSTEM if kind == "delay" else (
        "Author a Forge v3 instrument patch. Installed modules only: synth (up to four voices; main "
        "oscillator sine/triangle/saw/square; second oscillator with its own waveform, level, semitone "
        "interval and detune; white noise; amplitude ADSR; voices 1-4; glide), filter (resonant low-pass "
        "with its own ADSR and envelope amount in octaves), lfo (sine/triangle/square/sample_hold to pitch, "
        "filter and amplitude; mod_wheel true makes the wheel control depth), stereo delay, reverb and "
        "output gain. Use synth>delay>reverb>output for playable sounds, aux>delay>reverb>output for "
        "external audio. All settings are required; set unused modules neutral (levels/depths 0, "
        "env_octaves 0). No sampler, FM, arbitrary routing or custom code exists. Approximate the request "
        "only with these modules. Default output level 0.25. Return JSON only.")
    describe(schema)
    # Explicit types and enums work across both providers' JSON Schema subsets.
    schema["properties"]["version"] = {"type": "integer", "enum": [3 if kind == "instrument" else 1]}
    schema["properties"]["engine"] = {"type": "string", "enum": ["instrument" if kind == "instrument" else "stereo_delay"]}
    headers = {"Content-Type": "application/json"}
    if provider == "openai":
        url = "https://api.openai.com/v1/responses"
        headers["Authorization"] = "Bearer " + api_key
        body = {"model": model, "store": False, "instructions": system, "input": prompt,
                "max_output_tokens": 4096,
                "text": {"format": {"type": "json_schema", "name": "forge_patch",
                                    "strict": True, "schema": schema}}}
    else:
        url = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent"
        headers["x-goog-api-key"] = api_key
        body = {"systemInstruction": {"parts": [{"text": system}]},
                "contents": [{"role": "user", "parts": [{"text": prompt}]}],
                "generationConfig": {"maxOutputTokens": 4096,
                    "responseFormat": {"text": {"mimeType": "application/json", "schema": schema}}}}
    request = urllib.request.Request(url, json.dumps(body).encode(), headers, method="POST")
    try:
        with (opener or urllib.request.build_opener(NoRedirect()).open)(request, timeout=90) as response:
            raw = response.read(65537)
    except urllib.error.HTTPError as error:
        messages = {400: "Request rejected; check that your model supports structured JSON output.",
                    401: "API key was not accepted.", 403: "Key or project lacks access to this model.",
                    404: "Model was not found; check its ID and your account access.",
                    429: "Provider quota or rate limit reached; check your API account."}
        raise ProviderError(messages.get(error.code, "Provider unavailable; try again later.")) from None
    except (urllib.error.URLError, TimeoutError, OSError):
        raise ProviderError("Could not reach the provider or the request timed out.") from None
    try:
        if len(raw) > 65536:
            raise ValueError("Oversized response")
        envelope = parse_json(raw.decode("utf-8"))
        if provider == "openai":
            if envelope.get("status") != "completed":
                raise ValueError("Incomplete response")
            parts = [part for item in envelope["output"] if item.get("type") == "message"
                     for part in item.get("content", [])]
            if any(part.get("type") == "refusal" for part in parts):
                raise ValueError("Refusal")
            content = "".join(part["text"] for part in parts if part.get("type") == "output_text")
        else:
            candidate = envelope["candidates"][0]
            if candidate.get("finishReason") != "STOP":
                raise ValueError("Incomplete or blocked response")
            content = "".join(part["text"] for part in candidate["content"]["parts"]
                              if "text" in part and not part.get("thought"))
        patch = validate_patch(parse_json(content))
        if patch["version"] != (3 if kind == "instrument" else 1):
            raise ValueError("Wrong patch format for authoring mode")
        return patch
    except (ValueError, KeyError, TypeError, IndexError, AttributeError):
        raise ProviderError("Provider returned an incomplete, refused, or invalid patch. Try revising the prompt.") from None
