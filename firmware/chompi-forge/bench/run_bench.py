#!/usr/bin/env python3
"""Instruction-count CPU benchmark: Forge vs the stock TAPE / TEMPO / WAVE DSP.

Each bench ELF is built with the firmware's compiler and flags (Cortex-M7,
FPv5-D16 hard float, -O3) and executed in the Unicorn emulator. The same
24-sample audio block the firmware runs is counted instruction by instruction.

What this measures: executed ARM instructions per audio sample for the DSP
code only (no UI, controls, SD streaming or interrupts in any app).
What it does not: cycles. The M7 dual-issues, has caches and SDRAM latency;
real CPI may be well below or above 1. Treat results as *relative* cost
against stock firmware that is known to run on this chip, not as CPU %.

Requires: arm-none-eabi-gcc on PATH (make bench builds the ELFs),
pip install unicorn pyelftools.
"""
import bisect
import json
from pathlib import Path
import struct
import sys

from elftools.elf.elffile import ELFFile
from unicorn import UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_THUMB, Uc
from unicorn.arm_const import (UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC, UC_ARM_REG_LR, UC_ARM_REG_PC,
                               UC_ARM_REG_R0, UC_ARM_REG_SP, UC_CPU_ARM_MAX)

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "bench"
sys.path.insert(0, str(ROOT / "host"))
import forge_host  # noqa: E402

RATE, BLOCK, CLOCK_HZ = 48000, 24, 480_000_000
BUDGET = CLOCK_HZ / RATE            # cycles available per sample at 480 MHz
RETURN = 0x10000000                 # unmapped-code sentinel the bench returns to


class Bench:
    def __init__(self, elf_path):
        self.elf = ELFFile(open(elf_path, "rb"))
        # A-profile "max" core runs the same Thumb-2 + VFPv5 instructions; Unicorn's
        # M-profile model does not expose the FPU enable.
        self.mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
        self.mu.ctl_set_cpu_model(UC_CPU_ARM_MAX)
        # D1 SRAM (code/data), DTCM, SDRAM, ITCM; the peripheral range is plain
        # scratch memory so init-time HAL register writes (TEMPO's clock timer)
        # land harmlessly. Nothing in the measured DSP touches peripherals.
        for base, size in ((0x24000000, 4 << 20), (0x20000000, 128 << 10), (0xC0000000, 64 << 20),
                           (0x00000000, 64 << 10), (0x40000000, 0x20000000), (RETURN, 4096)):
            self.mu.mem_map(base, size)
        for segment in self.elf.iter_segments():
            if segment["p_type"] == "PT_LOAD" and segment["p_filesz"]:
                self.mu.mem_write(segment["p_paddr"], segment.data())
        self.mu.reg_write(UC_ARM_REG_C1_C0_2, self.mu.reg_read(UC_ARM_REG_C1_C0_2) | (0xF << 20))
        self.mu.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        symbols = [s for s in self.elf.get_section_by_name(".symtab").iter_symbols()
                   if s["st_info"]["type"] in ("STT_FUNC", "STT_OBJECT") and s["st_value"]]
        self.symbols = {s.name: s["st_value"] & ~1 for s in symbols}
        functions = sorted((s["st_value"] & ~1, s.name) for s in symbols if s["st_info"]["type"] == "STT_FUNC")
        self.starts = [a for a, _ in functions]
        self.names = [n for _, n in functions]
        self.counts = None

    def write(self, name, data):
        self.mu.mem_write(self.symbols[name], bytes(data))

    def read_floats(self, name, count):
        return struct.unpack(f"<{count}f", self.mu.mem_read(self.symbols[name], 4 * count))

    def call(self, name, arg=0):
        self.mu.reg_write(UC_ARM_REG_SP, 0x2001C000)
        self.mu.reg_write(UC_ARM_REG_LR, RETURN | 1)
        self.mu.reg_write(UC_ARM_REG_R0, arg & 0xFFFFFFFF)
        self.mu.emu_start(self.symbols[name] | 1, RETURN, count=200_000_000)
        if self.mu.reg_read(UC_ARM_REG_PC) & ~1 != RETURN:
            raise RuntimeError(f"{name} did not return (instruction cap reached)")
        return self.mu.reg_read(UC_ARM_REG_R0)

    def measure(self, blocks, arg=0):
        counts = {}
        starts, names = self.starts, self.names

        def hook(uc, address, size, user):
            i = bisect.bisect_right(starts, address) - 1
            key = names[i] if i >= 0 else "?"
            counts[key] = counts.get(key, 0) + 1
        handle = self.mu.hook_add(UC_HOOK_CODE, hook)
        self.mu.ctl_flush_tb()      # blocks translated during warm-up must be re-instrumented
        for _ in range(blocks):
            self.call("bench_block", arg)
        self.mu.hook_del(handle)
        total = sum(counts.values())
        return total / (blocks * BLOCK), sorted(counts.items(), key=lambda kv: -kv[1])[:6], total


def demangle(name):
    try:
        import subprocess
        return subprocess.run(["arm-none-eabi-c++filt", name], capture_output=True, text=True).stdout.strip() or name
    except OSError:
        return name


def run(label, elf, init_arg=0, setup=None, warm=200, blocks=40, outputs=("out_l",)):
    bench = Bench(elf)
    if setup: setup(bench)
    status = bench.call("bench_init", init_arg)
    if status != 0:
        raise RuntimeError(f"{label}: bench_init returned {status}")
    for _ in range(warm):
        bench.call("bench_block")
    per_sample, top, total = bench.measure(blocks)
    peak = max(abs(v) for name in outputs for v in bench.read_floats(name, BLOCK))
    if not all(abs(v) < 1e30 for name in outputs for v in bench.read_floats(name, BLOCK)):
        raise RuntimeError(f"{label}: non-finite output")
    return {"label": label, "instructions_per_sample": round(per_sample, 1),
            "percent_of_480MHz_at_CPI_1": round(100 * per_sample / BUDGET, 1),
            "output_peak": round(peak, 4),
            "top_functions": [(demangle(n), round(c / (blocks * BLOCK), 1)) for n, c in top]}


def forge_runs():
    elf = BUILD / "forge_bench.elf"
    results = []
    cases = (("Forge v1 delay only (aux)", "01-dry.json", 0), ("Forge v1 long echo (aux)", "03-long-echo.json", 0),
             ("Forge v2 Soft Pad, 4 voices", "05-soft-pad.json", 4),
             ("Forge v3 Warm Pad, 4 voices", "07-warm-pad.json", 4),
             ("Forge v3 Acid Bass, mono", "08-acid-bass.json", 4),
             ("Forge v3 Bell Keys, 4 voices", "09-bell-keys.json", 4),
             ("Forge v3 CPU Stress, 4 voices (worst case)", "10-cpu-stress.json", 4))
    for label, preset, notes in cases:
        patch = forge_host.load_patch(ROOT / "presets" / preset)
        payload = forge_host.encode_patch(patch, 1)

        def setup(bench, payload=payload):
            bench.write("request", payload)
            bench.write("request_size", struct.pack("<I", len(payload)))
        results.append(run(label, elf, notes, setup, outputs=("out_l", "out_r")))
    return results


def upstream_runs():
    results = []
    for name, label, arg in (
            ("tape_bench.elf", "TAPE 2.0 FX + output stage (7 voices NOT included)", 0),
            ("tempo_bench.elf", "TEMPO 1.0 FX + output stage (sample engines NOT included)", 0),
            ("wave_bench.elf", "WAVE 1.0 synth engine, 8 voices, delay side", 0),
            ("wave_bench.elf", "WAVE 1.0 synth engine, 8 voices, reverb side", 1)):
        elf = BUILD / name
        if elf.exists():
            results.append(run(label, elf, arg, warm=600, outputs=("out0", "out1")))
        else:
            print(f"skipped {name}: not built", file=sys.stderr)
    return results


def check(results):
    """Gate: every Forge scenario must cost no more than WAVE's shipping synth
    engine (known to run on this chip). Returns a list of failures."""
    wave = [r["instructions_per_sample"] for r in results if r["label"].startswith("WAVE")]
    if not wave:
        return ["WAVE baseline not built; cannot check"]
    ceiling = min(wave)
    return [f"{r['label']}: {r['instructions_per_sample']} > WAVE {ceiling}"
            for r in results if r["label"].startswith("Forge") and r["instructions_per_sample"] > ceiling]


def main():
    results = forge_runs() + upstream_runs()
    width = max(len(r["label"]) for r in results)
    print(f"{'Workload':{width}}  instr/sample  %480MHz@CPI1  peak")
    for r in results:
        print(f"{r['label']:{width}}  {r['instructions_per_sample']:12.1f}  {r['percent_of_480MHz_at_CPI_1']:12.1f}  {r['output_peak']:.3f}")
    out = BUILD / "results.json"
    out.write_text(json.dumps(results, indent=2) + "\n")
    print(f"\nPer-function breakdown: {out}")
    if "--check" in sys.argv:
        failures = check(results)
        print("CHECK:", "FAIL\n  " + "\n  ".join(failures) if failures else "PASS (every Forge scenario <= WAVE engine)")
        sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
