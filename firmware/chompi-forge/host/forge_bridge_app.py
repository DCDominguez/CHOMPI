"""Forge Bridge.exe: the hardware test bridge in one Windows program.

  Forge Bridge.exe                     starts the bridge and opens it in the browser
  Forge Bridge.exe checks [options]    automatic checks without the browser (forge_audio.py run)
  Forge Bridge.exe card sync FOLDER    USB card and firmware tool (forge_card.py)
Reports and the `card` folder live next to the program.
"""
import os
from pathlib import Path
import sys

if getattr(sys, "frozen", False):                       # PyInstaller: bundled files are unpacked here
    os.environ.setdefault("FORGE_HOST_DIR", str(Path(sys._MEIPASS) / "host"))


def main():
    command, rest = (sys.argv[1], sys.argv[2:]) if len(sys.argv) > 1 else (None, [])
    if command == "checks":
        import forge_audio
        if not any(a == "--report" for a in rest):
            import time
            exe = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path.cwd()
            rest += ["--report", str(exe / "reports" / time.strftime("%Y%m%d-%H%M%S"))]
        return forge_audio.cli(["run", *rest])
    if command == "card":
        import forge_card
        return forge_card.cli(rest)
    if command in ("-h", "--help"):
        print(__doc__); return 0
    import forge_web
    sys.argv = [sys.argv[0], *(sys.argv[1:] or ["--open"])]
    return forge_web.main()


if __name__ == "__main__":
    sys.exit(main())
