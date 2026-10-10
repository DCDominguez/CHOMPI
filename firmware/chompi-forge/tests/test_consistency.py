"""Copies that must not drift: the bridge checklist vs TEST_SESSION.md, the automatic
check ids, the Inspector's key->note table vs the firmware's, and the linker script's
boot_info placement (the "64 MHz" boot bug)."""
import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import check_firmware_layout
import forge_inspector

SESSION = (ROOT.parents[1] / "docs/forge/TEST_SESSION.md").read_text(encoding="utf-8")


def session_ids():
    """Table rows ("| 3.17 |") plus the numbered steps of section 1A ("1A.1"...)."""
    ids = set(re.findall(r"^\| (\d+\.\d+[a-z]?) \|", SESSION, re.M))
    section = SESSION.split("## 1A.", 1)[1].split("\n## ", 1)[0]
    ids |= {f"1A.{n}" for n in re.findall(r"^(\d+)\. ", section, re.M)}
    return ids


class ConsistencyTests(unittest.TestCase):
    def test_version_strings_agree(self):
        # 0.15.1: one version everywhere a person reads it (the docs had 0.5, 0.7 and 0.10).
        import forge_host
        version = forge_host.FIRMWARE_VERSION
        minor = int(re.search(r"kFirmwareMinor = (\d+);", (ROOT / "core/protocol.h").read_text()).group(1))
        self.assertEqual(int(version.split(".")[1]), minor)
        patch = int(re.search(r"kFirmwarePatch = (\d+);", (ROOT / "core/protocol.h").read_text()).group(1))
        self.assertEqual(int(version.split(".")[2]), patch)   # 0.15.2: the patch is on the wire too
        docs = ROOT.parents[1] / "docs/forge"
        self.assertTrue((docs / "CHANGELOG.md").read_text(encoding="utf-8").split("\n## ", 1)[1].startswith(version + " "))
        for name, pattern in (("MANUAL.md", r"Firmware \*\*([0-9.]+)\*\*"), ("TEST_SESSION.md", r"^# Forge ([0-9.]+) "),
                              ("HOME_CHECKLIST.md", r"^# First time home with Forge ([0-9.]+)")):
            found = re.search(pattern, (docs / name).read_text(encoding="utf-8"), re.M)
            self.assertTrue(found and found.group(1) == version, f"docs/forge/{name} names another firmware version")

    def test_bridge_checklist_matches_test_session(self):
        checks = json.loads((ROOT / "host/bridge_checks.json").read_text(encoding="utf-8"))
        bridge = [c["id"] for c in checks]
        self.assertEqual(len(bridge), len(set(bridge)))
        self.assertEqual(set(bridge), session_ids(),
                         "host/bridge_checks.json and docs/forge/TEST_SESSION.md list different steps; update both")

    def test_automatic_checks_name_real_steps(self):
        plan = json.loads((ROOT / "host/auto_checks.json").read_text(encoding="utf-8"))
        ids = session_ids()
        for step in plan["steps"]:
            if step["id"].startswith("0."): continue                  # bridge-only steps (noise floor)
            # exact step (6.2c), or an extension of one (3.1k, 3.30c -> 3.1, 3.30)
            self.assertTrue(step["id"] in ids or step["id"].rstrip("abcdefghijklmnopqrstuvwxyz") in ids, step["id"])

    def test_inspector_key_notes_match_firmware(self):
        header = (ROOT / "core/panel_controller.h").read_text(encoding="utf-8")
        table = re.search(r"kKeyNotes\[40\] = \{([^}]*)\}", header).group(1)
        self.assertEqual(tuple(int(v) for v in table.replace("\n", " ").split(",")), forge_inspector.KEY_NOTES)

    def test_boot_info_is_placed_in_backup_sram(self):
        script = (ROOT / "src/forge_sram.lds").read_text(encoding="utf-8")
        self.assertRegex(script, r"BACKUP_SRAM \(RWX\)\s*: ORIGIN = 0x38800000")
        self.assertRegex(script, r"\.backup_sram \(NOLOAD\)[^}]*\*\(\.backup_sram\)[^}]*\} > BACKUP_SRAM")

    def test_layout_checker_rejects_bad_images(self):
        import struct
        for stack, entry, size, why in ((0x20020000, 0x24000101, 1024, None),
                                        (0x30000000, 0x24000101, 1024, "stack"),
                                        (0x20020000, 0x24000100, 1024, "entry"),               # not Thumb
                                        (0x20020000, 0x24200001, 1024, "entry"),               # outside the image
                                        (0x20020000, 0x24000101, 480 * 1024, "exceeds")):
            problems = check_firmware_layout.image_problems(struct.pack("<II", stack, entry) + bytes(size))
            if why: self.assertTrue(any(why in p for p in problems), (why, problems))
            else: self.assertEqual(problems, [])


if __name__ == "__main__":
    unittest.main()
