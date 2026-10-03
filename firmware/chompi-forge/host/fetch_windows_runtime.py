#!/usr/bin/env python3
"""Download the portable Windows runtime for the development kit into a cache folder,
verifying every file against host/windows-runtime.json (SHA-256). Packaging only."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

LOCK = Path(__file__).resolve().parent / "windows-runtime.json"


def files():
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    return [lock["python"], *lock["wheels"], lock["msvc"]]


def verify(folder):
    """Paths of all runtime files in `folder`; raises if any is missing or differs from the lock."""
    paths = []
    for entry in files():
        path = Path(folder) / entry["file"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            raise SystemExit(f"{entry['file']} is missing or does not match windows-runtime.json; run fetch_windows_runtime.py")
        paths.append(path)
    return paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder")
    folder = Path(parser.parse_args().folder); folder.mkdir(parents=True, exist_ok=True)
    for entry in files():
        path = folder / entry["file"]
        if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]: continue
        with urllib.request.urlopen(entry["url"], timeout=120) as response: data = response.read()
        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise SystemExit(f"{entry['file']}: SHA-256 mismatch, not saved")
        path.write_bytes(data); print(f"{entry['file']}: {len(data):,} bytes, verified")
    verify(folder); print("OK")


if __name__ == "__main__": main()
