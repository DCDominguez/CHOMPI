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
    parser.add_argument("--development", action="store_true", help="Package the Inspector development firmware")
    parser.add_argument("--include-probe", action="store_true", help="Include this platform's simulation executable")
    parser.add_argument("--windows-runtime", type=Path, metavar="CACHE",
                        help="Bundle portable Python + MIDI/audio packages for Windows (folder from fetch_windows_runtime.py)")
    args = parser.parse_args()
    if args.windows_runtime and not args.development: raise SystemExit("--windows-runtime is for the development kit")
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=REPO, text=True).strip():
        raise SystemExit("Commit the tested source before packaging")
    source_tree = subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], cwd=REPO, text=True).strip()
    commit = args.source_commit or subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
    if len(commit) != 40 or any(c not in "0123456789abcdef" for c in commit):
        raise SystemExit("Invalid source commit")
    build_dir = ROOT / ("src/build-dev" if args.development else "src/build")
    elf, binary = build_dir / "FORGE.elf", build_dir / "FORGE.bin"
    sources = [*(ROOT / "core").glob("*.h"), ROOT / "src/forge_main.cpp", ROOT / "src/Makefile",
               ROOT / "src/forge_sram.lds", ROOT / "src/fatfs_storage.h"]
    if not binary.is_file() or binary.stat().st_mtime < max(p.stat().st_mtime for p in sources):
        raise SystemExit("FORGE.bin is missing or older than firmware sources; run make firmware first")
    # Record the compiler that actually built this binary (GCC writes it into .comment).
    marker = elf.read_bytes().find(b"GCC: (")
    if marker < 0:
        raise SystemExit("Cannot identify the compiler in FORGE.elf")
    compiler = elf.read_bytes()[marker:marker + 120].split(b"\0")[0].decode("ascii", "replace")
    files = {
        "README.md": (REPO / ("docs/forge/BRIDGE.md" if args.development else "docs/forge/TEST_SESSION.md")).read_bytes(),
        "firmware/FORGE.bin": binary.read_bytes(),
        "verify_bundle.py": (ROOT / "host/verify_bundle.py").read_bytes(),
        "docs/LIVE_AI_TEST.md": (REPO / "docs/forge/LIVE_AI_TEST.md").read_bytes(),
        "docs/PROTOCOL.md": (REPO / "docs/forge/PROTOCOL.md").read_bytes(),
        "docs/HANDOFF.md": (REPO / "docs/forge/HANDOFF.md").read_bytes(),
        "docs/COMPATIBILITY.md": (REPO / "docs/forge/COMPATIBILITY.md").read_bytes(),
        "docs/SAMPLING.md": (REPO / "docs/forge/SAMPLING.md").read_bytes(),
        "LICENSE": (REPO / "LICENSE").read_bytes(),
        "THIRD_PARTY.md": (REPO / "THIRD_PARTY.md").read_bytes(),
        "TRADEMARKS.md": (REPO / "TRADEMARKS.md").read_bytes(),
    }
    for name in ("forge_host.py", "forge_ai.py", "forge_ai_check.py", "forge_web.py", "forge_bridge.py",
                 "forge_inspector.py", "bridge_checks.json", "start_bridge.cmd", "requirements.txt", "README.md",
                 "forge_audio.py", "forge_walk.py", "forge_card.py", "check_firmware_layout.py", "auto_checks.json", "bridge-requirements.txt", "windows-runtime.json"):
        files["host/" + name] = (ROOT / "host" / name).read_bytes()
    files["card/README.txt"] = (b"Put files for CHOMPI's SD card here: TAPE samples named jammi_<a-e><1-14>.wav or\r\n"
                                b"cubbi_<a-e><1-14>.wav. The bridge's Card & firmware section copies them over USB\r\n"
                                b"(firmware 0.7 or newer); so does: python\\python.exe host\\forge_card.py sync card\r\n")
    if args.development: files["CLAUDE.md"] = (ROOT / "host/KIT_CLAUDE.md").read_bytes()   # for an agent on the test PC
    files["MANUAL.md"] = (REPO / "docs/forge/MANUAL.md").read_bytes()
    if args.development: files["HOME_CHECKLIST.md"] = (REPO / "docs/forge/HOME_CHECKLIST.md").read_bytes()
    for name in ("BRIDGE.md", "INSPECTOR.md", "TEST_SESSION.md", "KNOBS.md", "TEST_RESULTS.md"):
        files["docs/" + name] = (REPO / "docs/forge" / name).read_bytes()
    probe = ROOT / "build" / ("forge_probe.exe" if sys.platform == "win32" else "forge_probe")
    if args.include_probe:
        files["build/" + probe.name] = probe.read_bytes()
    for path in sorted((ROOT / "host/web").iterdir()):
        if path.is_file():
            files["host/web/" + path.name] = path.read_bytes()
    files["host/instrument.schema.json"] = (json.dumps(forge_host.SCHEMA5, indent=2) + "\n").encode()
    files["host/instrument-v4.schema.json"] = (json.dumps(forge_host.SCHEMA4, indent=2) + "\n").encode()
    files["host/instrument-v3.schema.json"] = (json.dumps(forge_host.SCHEMA3, indent=2) + "\n").encode()
    files["host/instrument-v2.schema.json"] = (json.dumps(forge_host.SCHEMA2, indent=2) + "\n").encode()
    files["docs/CONTINUE.md"] = (REPO / "docs/forge/CONTINUE.md").read_bytes()
    files["host/patch.schema.json"] = (json.dumps(forge_host.SCHEMA, indent=2) + "\n").encode()
    for path in sorted((ROOT / "presets").glob("*.json")):
        patch = forge_host.load_patch(path)
        files["presets/" + path.name] = path.read_bytes()
        render = ROOT / "build" / (path.stem + "-simulated.wav")
        midi = bytes([0xF0, *forge_host.encode_patch(patch, 1), 0xF7]).hex(" ")
        subprocess.run([str(probe), "--render", str(render)],
                       input=midi, text=True, check=True, capture_output=True, timeout=10)
        files["audio-reference/" + render.name] = render.read_bytes()
    runtime = None
    if args.windows_runtime:
        runtime = windows_runtime(args.windows_runtime, files)
        files["Start Forge bridge.cmd"] = b'@echo off\r\ncall "%~dp0host\\start_bridge.cmd"\r\n'
    manifest = {"candidate": "Forge Bridge 0.10 development" if args.development else "Forge 0.10", "hardware_verified": False,
                "development_hooks": args.development,
                "simulation_platform": sys.platform if args.include_probe else None,
                "windows_runtime": runtime,
                "source_commit": commit, "source_tree": source_tree,
                "source_url": f"https://github.com/DCDominguez/CHOMPI/tree/{commit}",
                "compiler": compiler,
                "pinned_compiler": "GNU Arm Embedded 10.3-2021.10 (Arm archive)",
                "built_with_pinned_compiler": "GNU Arm Embedded Toolchain 10.3-2021.10" in compiler,
                "audio_reference": "Simulated aux plucks, synth chords and sampler notes (synthetic sine samples); not CHOMPI recordings",
                "files": {name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                          for name, data in sorted(files.items())}}
    files["manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    with zipfile.ZipFile(args.output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            prefix = "Forge-Bridge-dev" if args.development else "Forge-0.9-test"
            info = zipfile.ZipInfo(f"{prefix}-{commit[:7]}/" + name, date_time=(2026, 10, 3, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o100755 if name.startswith("build/") else 0o100644) << 16
            archive.writestr(info, data)
    print(json.dumps({"file": str(Path(args.output).resolve()), "source_tree": source_tree,
                      "firmware_sha256": manifest["files"]["firmware/FORGE.bin"]["sha256"]}))


def windows_runtime(cache, files):
    """Portable Python (NuGet `python`, tools/ -> python/) with the bridge's packages unpacked into
    python/Lib/site-packages, all verified against host/windows-runtime.json."""
    import fetch_windows_runtime as fetch
    paths = fetch.verify(cache)
    skip = ("tools/Lib/site-packages/", "tools/Lib/ensurepip/", "tools/Lib/venv/", "tools/include/", "tools/libs/")
    with zipfile.ZipFile(paths[0]) as nupkg:
        for entry in nupkg.infolist():
            if entry.filename.startswith("tools/") and not entry.is_dir() and not entry.filename.startswith(skip):
                files["python/" + entry.filename[len("tools/"):]] = nupkg.read(entry)
    lock = json.loads((ROOT / "host/windows-runtime.json").read_text(encoding="utf-8"))
    with zipfile.ZipFile(paths[-1]) as msvc:                        # only the C++ runtime DLL rtmidi needs
        for member, target in lock["msvc"]["extract"].items(): files[target] = msvc.read(member)
    for wheel in paths[1:-1]:
        with zipfile.ZipFile(wheel) as archive:
            for entry in archive.infolist():
                name = entry.filename
                if entry.is_dir() or name.startswith("numpy/") and "/tests/" in name: continue   # numpy's own test suite
                top, _, rest = name.partition("/")
                if top.endswith(".data"):                                   # <dist>.data/<kind>/...
                    kind, _, name = rest.partition("/")
                    if kind not in ("purelib", "platlib"): continue         # scripts/headers are not needed
                files["python/Lib/site-packages/" + name] = archive.read(entry)
    return {"python": lock["python"]["file"], "packages": [w["file"] for w in lock["wheels"]],
            "msvc": lock["msvc"]["file"],
            "note": "python.exe/python312.dll signed by the Python Software Foundation, msvcp140.dll by Microsoft; files verified by SHA-256"}


if __name__ == "__main__": main()
