#!/usr/bin/env python3
"""Check every file in an unpacked Forge test bundle against manifest.json.
Run from anywhere: python3 verify_bundle.py  (Python 3.8+, no dependencies)."""
import hashlib
import json
from pathlib import Path
import sys


def verify(folder):
    folder = Path(folder)
    manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    problems = []
    listed = set(manifest["files"])
    for name, expected in sorted(manifest["files"].items()):
        path = folder / name
        if not path.is_file():
            problems.append(f"missing: {name}"); continue
        data = path.read_bytes()
        if len(data) != expected["bytes"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
            problems.append(f"changed: {name}")
    extra = {p.relative_to(folder).as_posix() for p in folder.rglob("*") if p.is_file()} - listed - {"manifest.json"}
    problems += [f"unlisted: {name}" for name in sorted(extra)
                 if "__pycache__" not in name and not name.startswith(("host/.bridge-venv/", "reports/"))]
    return manifest, problems


if __name__ == "__main__":
    # In the bundle this script sits next to manifest.json.
    here = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent
    manifest, problems = verify(here)
    print(f"{manifest['candidate']} · source {manifest['source_commit'][:12]} · compiler: {manifest['compiler']}")
    print(f"firmware/FORGE.bin sha256 {manifest['files']['firmware/FORGE.bin']['sha256']}")
    for problem in problems: print("FAIL", problem)
    print("FAIL: do not flash this bundle" if problems else f"OK: all {len(manifest['files'])} files match the manifest")
    sys.exit(1 if problems else 0)
