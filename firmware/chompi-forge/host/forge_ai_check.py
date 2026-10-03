#!/usr/bin/env python3
"""Live OpenAI/Gemini check for Forge patch authoring (Python 3.10+).

Makes ONE real, possibly billed, provider request with your own key, validates the
result exactly as the webapp does, proves it encodes to the device wire format, and
appends a JSON-lines record you can paste into TEST_RESULTS.md. It never sends MIDI.

The key is read with a hidden prompt (or from FORGE_API_KEY if already set in your
shell). It is never printed, written to the record, or passed on the command line.
"""
import argparse
from datetime import datetime, timezone
import getpass
import json
import os
from pathlib import Path
import platform
import sys
import time

import forge_ai
import forge_host

DEFAULT_PROMPT = ("Warm rounded keys: triangle wave, soft 30 ms attack, medium decay, "
                  "sustain around 0.5, 600 ms release, darker tone, subtle short echo. Output at 25%.")


def run(provider, model, prompt, kind, api_key, generate=None, clock=time.monotonic):
    generate = generate or forge_ai.generate_patch
    record = {"utc": datetime.now(timezone.utc).isoformat(timespec="seconds"), "provider": provider,
              "model": model, "kind": kind, "prompt": prompt, "python": platform.python_version()}
    started = clock()
    try:
        patch = generate(provider, api_key, model, prompt, kind=kind)
        forge_host.encode_patch(patch, 1)  # wire-compatible with firmware 0.5
        record.update(ok=True, patch=patch)
    except (forge_ai.ProviderError, ValueError) as error:
        record.update(ok=False, error=str(error))  # safe messages only; never provider bodies
    record["seconds"] = round(clock() - started, 2)
    return record


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--provider", choices=("openai", "gemini"), required=True)
    parser.add_argument("--model", required=True, help="Model ID available to your API account")
    parser.add_argument("--kind", choices=("instrument", "delay"), default="instrument")
    parser.add_argument("--prompt", default=DEFAULT_PROMPT)
    parser.add_argument("--record", default="forge-ai-check.jsonl", help="JSON-lines file to append to")
    args = parser.parse_args(argv)
    api_key = os.environ.get("FORGE_API_KEY") or getpass.getpass(f"{args.provider} API key (hidden, not saved): ")
    try:
        record = run(args.provider, args.model, args.prompt, args.kind, api_key.strip())
    finally:
        api_key = None
    with Path(args.record).open("a", encoding="utf-8") as file:
        file.write(json.dumps(record, allow_nan=False) + "\n")
    print(json.dumps(record, indent=2))
    print(f"\n{'PASS' if record['ok'] else 'FAIL'}: appended to {Path(args.record).resolve()} (no key stored)")
    return 0 if record["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
