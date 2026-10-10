"""Writes the bridge's starter presets (as CHOMPI stores them: upgraded to v5) as
encoded patch requests, one per line: "<file> <bytes...>". Input of knob_audio_test."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import forge_bridge as bridge
import forge_host as host

for slot, name in bridge.STARTER_PRESETS:
    patch = host.load_patch(ROOT / "presets" / name)
    if patch["version"] < 5: patch = host.upgrade_patch(patch, 5)
    print(name, " ".join(str(b) for b in host.encode_patch(patch, 1)))
