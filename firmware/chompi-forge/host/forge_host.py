#!/usr/bin/env python3
"""Forge patch authoring, persistence and acknowledged MIDI control (Python 3.10+)."""
import argparse
import json
import math
from pathlib import Path
import secrets
import sys
import time
import urllib.parse
import urllib.request

PREFIX = [0x7D, 0x46, 0x47, 1]
ERRORS = {1: "invalid length", 2: "unsupported version", 3: "checksum mismatch",
          4: "invalid patch", 5: "unknown operation", 6: "device queue busy"}
LIMITS = {"mix": (0, 1), "time_ms": (10, 1000), "feedback": (0, 0.85), "level": (0, 1)}
SCHEMA = {
    "$schema": "https://json-schema.org/draft/2020-12/schema", "type": "object",
    "additionalProperties": False, "required": ["version", "name", "engine", "parameters"],
    "properties": {
        "version": {"const": 1}, "name": {"type": "string", "minLength": 1, "maxLength": 80},
        "engine": {"const": "stereo_delay"},
        "parameters": {"type": "object", "additionalProperties": False,
            "required": [*LIMITS, "bypass"], "properties": {
                **{key: {"type": "number", "minimum": low, "maximum": high}
                   for key, (low, high) in LIMITS.items()}, "bypass": {"type": "boolean"}}}}}


def validate_patch(patch):
    if not isinstance(patch, dict) or set(patch) != {"version", "name", "engine", "parameters"}:
        raise ValueError("Patch requires exactly version, name, engine and parameters")
    if type(patch["version"]) is not int or patch["version"] != 1:
        raise ValueError("Only patch version 1 is supported")
    if patch["engine"] != "stereo_delay":
        raise ValueError("Only stereo_delay is available in this firmware")
    if not isinstance(patch["name"], str) or not 1 <= len(patch["name"].strip()) <= 80:
        raise ValueError("Patch name must contain 1–80 characters")
    if len(patch["name"]) > 80:
        raise ValueError("Patch name exceeds 80 characters")
    p = patch["parameters"]
    if not isinstance(p, dict) or set(p) != {*LIMITS, "bypass"}:
        raise ValueError("Parameter keys must match the schema exactly")
    for key, (low, high) in LIMITS.items():
        value = p[key]
        if type(value) not in (float, int) or not low <= value <= high or not math.isfinite(value):
            raise ValueError(f"{key} must be a finite number in [{low}, {high}]")
    if type(p["bypass"]) is not bool:
        raise ValueError("bypass must be a boolean")
    return patch


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_json(text):
    if len(text.encode("utf-8")) > 65536:
        raise ValueError("JSON exceeds 64 KiB")
    return json.loads(text, object_pairs_hook=unique_keys,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(f"Invalid JSON: {value}")))


def load_patch(path):
    with Path(path).open("rb") as file:
        raw = file.read(65537)
    if len(raw) > 65536:
        raise ValueError("Patch file exceeds 64 KiB")
    return validate_patch(parse_json(raw.decode("utf-8")))


def save_patch(patch, path, overwrite=False):
    validate_patch(patch)
    # Exclusive creation by default: AI output cannot silently replace a preset.
    with Path(path).open("w" if overwrite else "x", encoding="utf-8") as file:
        file.write(json.dumps(patch, indent=2, allow_nan=False) + "\n")


def word14(value):
    return [value & 127, (value >> 7) & 127]


def read14(data, index):
    return data[index] | data[index + 1] << 7


def checksum(data):
    return (-sum(data)) & 127


def message(opcode, sequence, data=()):
    if type(sequence) is not int or not 0 <= sequence <= 16383:
        raise ValueError("Sequence must be 0–16383")
    payload = PREFIX + [opcode] + word14(sequence) + list(data)
    return payload + [checksum(payload)]


def encode_patch(patch, sequence):
    p = validate_patch(patch)["parameters"]
    values = [p["mix"], (p["time_ms"] - 10) / 990, p["feedback"] / 0.85, p["level"]]
    data = [1]
    for value in values:
        data.extend(word14(int(value * 16383 + 0.5)))
    data.append(int(p["bypass"]))
    return message(1, sequence, data)


def decode_response(data, sequence):
    data = list(data)
    if len(data) < 8 or data[:4] != PREFIX or any(type(x) is not int or not 0 <= x < 128 for x in data):
        raise ValueError("Invalid Forge reply header")
    if read14(data, 5) != sequence:
        raise ValueError("Unrelated reply sequence")
    if checksum(data) != 0:
        raise ValueError("Invalid reply checksum")
    if data[4] == 0x41 and len(data) == 9:
        raise RuntimeError("Device rejected request: " + ERRORS.get(data[7], "unknown error"))
    if len(data) != 30 or data[4] != 0x40 or data[7] != 0 or data[8] != 1 or data[17] > 1:
        raise ValueError("Invalid Forge status payload")
    patch = {"version": 1, "name": "Captured from Forge", "engine": "stereo_delay", "parameters": {
        "mix": read14(data, 9) / 16383,
        "time_ms": 10 + 990 * read14(data, 11) / 16383,
        "feedback": 0.85 * read14(data, 13) / 16383,
        "level": read14(data, 15) / 16383, "bypass": bool(data[17])}}
    return {"sequence": sequence, "firmware": f"0.{data[28]}", "patch": validate_patch(patch),
            "cpu_average_percent": read14(data, 18) / 10,
            "cpu_max_percent": read14(data, 20) / 10,
            "dropped": read14(data, 22) | data[24] << 14,
            "rejected": read14(data, 25) | data[27] << 14}


def midi_module():
    try:
        import mido
    except ImportError as error:
        raise RuntimeError("Install MIDI dependencies: python -m pip install -r host/requirements.txt") from error
    return mido


def exchange(payload, input_name, output_name, timeout=2.0, midi=None):
    if not math.isfinite(timeout) or timeout <= 0 or timeout > 30:
        raise ValueError("Timeout must be greater than 0 and at most 30 seconds")
    midi = midi or midi_module()
    sequence = read14(payload, 5)
    # Explicit names: never send a Forge packet to an automatically selected synth.
    if input_name not in midi.get_input_names() or output_name not in midi.get_output_names():
        raise ValueError("Port not found; run ports and copy the exact input/output names")
    with midi.open_input(input_name) as source, midi.open_output(output_name) as destination:
        destination.send(midi.Message("sysex", data=payload))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for _ in range(64):
                reply = source.receive(block=False)
                if reply is None:
                    break
                if reply.type != "sysex":
                    continue
                data = list(reply.data)
                if len(data) < 7 or data[:3] != PREFIX[:3] or read14(data, 5) != sequence:
                    continue
                result = decode_response(data, sequence)
                if payload[4] == 1 and data[8:18] != list(payload[7:17]):
                    raise RuntimeError("Acknowledgement does not match the requested patch; query status")
                return result
            time.sleep(0.002)
    raise TimeoutError("No matching acknowledgement. The device may have applied the patch; query status before retrying.")


def generate_patch(prompt, model, endpoint="http://127.0.0.1:11434/api/chat", opener=None):
    if not prompt.strip() or len(prompt) > 4000 or not model.strip():
        raise ValueError("Provide a model and a prompt of 1–4000 characters")
    url = urllib.parse.urlparse(endpoint)
    if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password:
        raise ValueError("Use an HTTP(S) Ollama chat endpoint without embedded credentials")
    if url.scheme == "http" and url.hostname not in ("localhost", "127.0.0.1", "::1"):
        raise ValueError("Plain HTTP is limited to a local Ollama server; use HTTPS for a remote endpoint")
    system = ("Author a JSON Forge v1 stereo_delay patch matching the supplied schema. "
              "Only mix, time_ms, feedback, level and bypass exist. Do not invent algorithms, filters, "
              "reverb or code. Translate the request only within these delay controls. "
              "Default level to 0.25 unless explicitly requested. Return only the JSON object.")
    body = {"model": model, "stream": False, "format": SCHEMA,
            "messages": [{"role": "system", "content": system}, {"role": "user", "content": prompt}],
            "options": {"temperature": 0}}
    request = urllib.request.Request(endpoint, json.dumps(body).encode(),
                                     {"Content-Type": "application/json"}, method="POST")
    with (opener or urllib.request.urlopen)(request, timeout=90) as response:
        raw = response.read(65537)
    if len(raw) > 65536:
        raise ValueError("Model response exceeds 64 KiB")
    envelope = parse_json(raw.decode("utf-8"))
    try:
        content = envelope["message"]["content"]
    except (KeyError, TypeError) as error:
        raise ValueError("Ollama response has no message content") from error
    if not isinstance(content, str):
        raise ValueError("Model content must be JSON text")
    return validate_patch(parse_json(content))


def cli(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("schema", help="Print the authoring JSON schema")
    commands.add_parser("ports", help="List MIDI ports")
    validate = commands.add_parser("validate"); validate.add_argument("patch")
    encode = commands.add_parser("encode", help="Print SysEx bytes without using MIDI")
    encode.add_argument("patch"); encode.add_argument("--sequence", type=int, default=1)
    for name in ("send", "status", "capture"):
        command = commands.add_parser(name)
        if name == "send": command.add_argument("patch")
        if name == "capture": command.add_argument("file")
        command.add_argument("--input", required=True); command.add_argument("--output", required=True)
        command.add_argument("--timeout", type=float, default=2.0)
    ai = commands.add_parser("ai", help="Ask a local Ollama model for a validated patch; does not send MIDI")
    ai.add_argument("prompt"); ai.add_argument("--model", required=True)
    ai.add_argument("--endpoint", default="http://127.0.0.1:11434/api/chat")
    ai.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    if args.command == "schema": print(json.dumps(SCHEMA, indent=2))
    elif args.command == "ports":
        midi = midi_module()
        print(json.dumps({"inputs": midi.get_input_names(), "outputs": midi.get_output_names()}, indent=2))
    elif args.command == "validate": print(json.dumps(load_patch(args.patch), indent=2))
    elif args.command == "encode":
        print(bytes([0xF0, *encode_patch(load_patch(args.patch), args.sequence), 0xF7]).hex(" "))
    elif args.command == "ai":
        patch = generate_patch(args.prompt, args.model, args.endpoint)
        save_patch(patch, args.out)
        print(json.dumps(patch, indent=2))
    else:
        sequence = secrets.randbelow(16384)
        payload = encode_patch(load_patch(args.patch), sequence) if args.command == "send" else message(2, sequence)
        result = exchange(payload, args.input, args.output, args.timeout)
        if args.command == "capture": save_patch(result["patch"], args.file)
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(cli())
    except (ValueError, RuntimeError, OSError, TimeoutError) as error:
        print(f"Forge: {error}", file=sys.stderr)
        sys.exit(1)
