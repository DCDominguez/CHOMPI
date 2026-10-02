#!/usr/bin/env python3
"""Build a reproducible-layout, checksummed hardware-test bundle from a clean tree."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_host


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output")
    parser.add_argument("--source-commit", help="Remote commit when uploaded through the GitHub connector")
    args = parser.parse_args()
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=REPO, text=True).strip():
        raise SystemExit("Commit the tested source before packaging")
    source_tree = subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], cwd=REPO, text=True).strip()
    commit = args.source_commit or subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
    if len(commit) != 40 or any(c not in "0123456789abcdef" for c in commit):
        raise SystemExit("Invalid source commit")
    elf, binary = ROOT / "src/build/FORGE.elf", ROOT / "src/build/FORGE.bin"
    sources = [*(ROOT / "core").glob("*.h"), ROOT / "src/forge_main.cpp", ROOT / "src/Makefile"]
    if not binary.is_file() or binary.stat().st_mtime < max(p.stat().st_mtime for p in sources):
        raise SystemExit("FORGE.bin is missing or older than firmware sources; run make firmware first")
    # Record the compiler that actually built this binary (GCC writes it into .comment).
    marker = elf.read_bytes().find(b"GCC: (")
    if marker < 0:
        raise SystemExit("Cannot identify the compiler in FORGE.elf")
    compiler = elf.read_bytes()[marker:marker + 120].split(b"\0")[0].decode("ascii", "replace")
    files = {
        "README.md": (REPO / "docs/forge/TEST_SESSION.md").read_bytes(),
        "firmware/FORGE.bin": binary.read_bytes(),
        "verify_bundle.py": (ROOT / "host/verify_bundle.py").read_bytes(),
        "docs/LIVE_AI_TEST.md": (REPO / "docs/forge/LIVE_AI_TEST.md").read_bytes(),
        "docs/PROTOCOL.md": (REPO / "docs/forge/PROTOCOL.md").read_bytes(),
        "docs/HANDOFF.md": (REPO / "docs/forge/HANDOFF.md").read_bytes(),
        "LICENSE": (REPO / "LICENSE").read_bytes(),
        "THIRD_PARTY.md": (REPO / "THIRD_PARTY.md").read_bytes(),
        "TRADEMARKS.md": (REPO / "TRADEMARKS.md").read_bytes(),
    }
    for name in ("forge_host.py", "forge_ai.py", "forge_ai_check.py", "forge_web.py", "requirements.txt", "README.md"):
        files["host/" + name] = (ROOT / "host" / name).read_bytes()
    for path in sorted((ROOT / "host/web").iterdir()):
        if path.is_file():
            files["host/web/" + path.name] = path.read_bytes()
    files["host/instrument.schema.json"] = (json.dumps(forge_host.SCHEMA2, indent=2) + "\n").encode()
    files["docs/CONTINUE.md"] = (REPO / "docs/forge/CONTINUE.md").read_bytes()
    files["host/patch.schema.json"] = (json.dumps(forge_host.SCHEMA, indent=2) + "\n").encode()
    for path in sorted((ROOT / "presets").glob("*.json")):
        patch = forge_host.load_patch(path)
        files["presets/" + path.name] = path.read_bytes()
        render = ROOT / "build" / (path.stem + "-simulated.wav")
        midi = bytes([0xF0, *forge_host.encode_patch(patch, 1), 0xF7]).hex(" ")
        subprocess.run([str(ROOT / "build/forge_probe"), "--render", str(render)],
                       input=midi, text=True, check=True, capture_output=True, timeout=10)
        files["audio-reference/" + render.name] = render.read_bytes()
    manifest = {"candidate": "Forge 0.3", "hardware_verified": False,
                "source_commit": commit, "source_tree": source_tree,
                "source_url": f"https://github.com/DCDominguez/CHOMPI/tree/{commit}",
                "compiler": compiler,
                "pinned_compiler": "GNU Arm Embedded 10.3-2021.10 (Arm archive)",
                "built_with_pinned_compiler": "GNU Arm Embedded Toolchain 10.3-2021.10" in compiler,
                "audio_reference": "Simulated aux plucks and synth chords; not CHOMPI recordings",
                "files": {name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                          for name, data in sorted(files.items())}}
    files["manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    with zipfile.ZipFile(args.output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(f"Forge-0.3-test-{commit[:7]}/" + name, date_time=(2026, 10, 2, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    print(json.dumps({"file": str(Path(args.output).resolve()), "source_tree": source_tree,
                      "firmware_sha256": manifest["files"]["firmware/FORGE.bin"]["sha256"]}))


if __name__ == "__main__": main()
