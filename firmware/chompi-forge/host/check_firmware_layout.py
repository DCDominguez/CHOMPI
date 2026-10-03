#!/usr/bin/env python3
"""Link-layout guard, run after every firmware build (make firmware / firmware-dev).

- libDaisy's boot_info must link into backup SRAM (0x38800000), where the
  bootloader writes its version. Anywhere else it is uninitialised RAM; a 0
  there makes DaisySeed::Init() skip clock and SDRAM setup ("64 MHz bug").
- FORGE.bin must pass the checks the CHOMPI bootloader and the community
  launcher apply: at most 480 KB, stack pointer in DTCM/D1 SRAM, Thumb entry
  point inside the image at 0x24000000.
Standard library only."""
from pathlib import Path
import struct
import sys

BOOT_INFO = "_ZN5daisy9boot_infoE"
BACKUP_SRAM = 0x38800000
APP_START, MAX_IMAGE = 0x24000000, 480 * 1024


def symbols(elf):
    """name -> value from an ELF32 little-endian symbol table."""
    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 1: raise ValueError("not an ELF32 little-endian file")
    shoff, = struct.unpack_from("<I", elf, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", elf, 0x2e)
    sections = [struct.unpack_from("<IIIIIIIIII", elf, shoff + i * shentsize) for i in range(shnum)]
    found = {}
    for _, kind, _, _, offset, size, link, _, _, entsize in sections:
        if kind != 2: continue                                     # SHT_SYMTAB
        strtab = sections[link]
        for i in range(size // entsize):
            name, value = struct.unpack_from("<II", elf, offset + i * entsize)
            end = elf.index(b"\0", strtab[4] + name)
            found[elf[strtab[4] + name:end].decode("ascii", "replace")] = value
    return found


def check(elf_path, bin_path):
    problems = []
    address = symbols(Path(elf_path).read_bytes()).get(BOOT_INFO)
    if address != BACKUP_SRAM:
        problems.append(f"boot_info at {address:#x} (expected {BACKUP_SRAM:#x}): add the BACKUP_SRAM region and "
                        ".backup_sram section to the linker script" if address is not None else "boot_info not found")
    return problems + image_problems(Path(bin_path).read_bytes())


def image_problems(image):
    """The bootloader's / launcher's acceptance checks for a BOOT_SRAM image."""
    problems = []
    stack, entry = struct.unpack_from("<II", image, 0) if len(image) >= 8 else (0, 0)
    if len(image) > MAX_IMAGE: problems.append(f"image {len(image)} bytes exceeds {MAX_IMAGE}")
    if not (0x20000000 <= stack <= 0x20020000 or 0x24000000 <= stack <= 0x24080000):
        problems.append(f"initial stack pointer {stack:#x} outside DTCM/D1 SRAM")
    if not (APP_START <= entry < APP_START + len(image) and entry & 1):
        problems.append(f"entry point {entry:#x} not a Thumb address inside the image")
    return problems


if __name__ == "__main__":
    elf, binary = sys.argv[1:3]
    problems = check(elf, binary)
    for p in problems: print(f"FIRMWARE LAYOUT: {p}", file=sys.stderr)
    if not problems: print(f"layout OK: {binary} (boot_info in backup SRAM, {Path(binary).stat().st_size} bytes)")
    sys.exit(1 if problems else 0)
