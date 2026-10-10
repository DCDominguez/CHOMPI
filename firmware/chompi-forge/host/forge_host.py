#!/usr/bin/env python3
"""Forge patch authoring, persistence and acknowledged MIDI control (Python 3.10+)."""
import argparse
import json
import math
from pathlib import Path
import secrets
import sys
import time
import urllib.parse
import urllib.request

PREFIX = [0x7D, 0x46, 0x47, 1]
# The firmware these tools match; its minor is core/protocol.h kFirmwareMinor (tests/test_consistency.py).
FIRMWARE_VERSION = "0.16.0"
ERRORS = {1: "invalid length", 2: "unsupported version", 3: "checksum mismatch",
          4: "invalid patch or preset address", 5: "unknown operation", 6: "device queue busy",
          7: "that slot is empty (preset, sample or recording)", 8: "SD card missing or storage failed",
          9: "storage busy",
          10: "CHOMPI's battery is low and its USB supply is weak or missing, so installing now could leave it dark "
              "until it is switched off and on. Charge it until the battery light (hold SW6 for 2 s) is green or white, "
              "or connect a USB-C to USB-C charger (2 A or more), then install again"}
PRESET_BANKS, PRESET_SLOTS = 8, 15
LIMITS = {"mix": (0, 1), "time_ms": (10, 1000), "feedback": (0, 0.85), "level": (0, 1)}
SCHEMA = {
    "$schema": "https://json-schema.org/draft/2020-12/schema", "type": "object",
    "additionalProperties": False, "required": ["version", "name", "engine", "parameters"],
    "properties": {
        "version": {"const": 1}, "name": {"type": "string", "minLength": 1, "maxLength": 80},
        "engine": {"const": "stereo_delay"},
        "parameters": {"type": "object", "additionalProperties": False,
            "required": [*LIMITS, "bypass"], "properties": {
                **{key: {"type": "number", "minimum": low, "maximum": high}
                   for key, (low, high) in LIMITS.items()}, "bypass": {"type": "boolean"}}}}}


WAVEFORMS = ("sine", "triangle", "saw", "square")
SYNTH_LIMITS = {"attack_ms": (1, 2000), "decay_ms": (1, 2000), "sustain": (0, 1),
                "release_ms": (5, 5000), "cutoff_hz": (40, 16000)}
ROUTES = ("aux>delay>output", "synth>delay>output")
def object_schema(properties):
    return {"type": "object", "additionalProperties": False,
            "required": list(properties), "properties": properties}
SCHEMA2 = object_schema({
    "version": {"type": "integer", "enum": [2]},
    "name": {"type": "string", "minLength": 1, "maxLength": 80},
    "engine": {"type": "string", "enum": ["instrument"]},
    "routing": {"type": "string", "enum": list(ROUTES)},
    "modules": object_schema({
        "synth": object_schema({"waveform": {"type": "string", "enum": list(WAVEFORMS)},
            **{key: {"type": "number", "minimum": low, "maximum": high}
               for key, (low, high) in SYNTH_LIMITS.items()}}),
        "delay": object_schema({key: value for key, value in SCHEMA["properties"]["parameters"]["properties"].items() if key != "level"}),
        "output": object_schema({"level": {"type": "number", "minimum": 0, "maximum": 1}})})})


# ---- Version 3 instrument -------------------------------------------------
LFO_WAVEFORMS = ("sine", "triangle", "square", "sample_hold")
ROUTES3 = ("aux>delay>reverb>output", "synth>delay>reverb>output")
AMP_LIMITS = {key: SYNTH_LIMITS[key] for key in ("attack_ms", "decay_ms", "sustain", "release_ms")}
# Wire order after the shared v2 fields (request indexes 29..67), mirroring
# V3Fields in core/protocol.h. Codec: ("lin"|"log", low, high), ("enum", names),
# ("int", low, high, byte_offset) or ("bool",).
V3_FIELDS = (
    ("synth", "osc2_waveform", ("enum", WAVEFORMS)),
    ("synth", "osc2_level", ("lin", 0, 1)),
    ("synth", "osc2_semitones", ("int", -24, 24, 24)),
    ("synth", "osc2_detune_cents", ("lin", -50, 50)),
    ("synth", "noise", ("lin", 0, 1)),
    ("filter", "resonance", ("lin", 0, 1)),
    ("filter", "env_octaves", ("lin", -6, 6)),
    ("filter", "attack_ms", ("lin", 1, 2000)),
    ("filter", "decay_ms", ("lin", 1, 2000)),
    ("filter", "sustain", ("lin", 0, 1)),
    ("filter", "release_ms", ("lin", 5, 5000)),
    ("lfo", "waveform", ("enum", LFO_WAVEFORMS)),
    ("lfo", "rate_hz", ("log", 0.05, 20)),
    ("lfo", "pitch_cents", ("lin", 0, 200)),
    ("lfo", "filter_octaves", ("lin", 0, 4)),
    ("lfo", "amp_depth", ("lin", 0, 1)),
    ("lfo", "mod_wheel", ("bool",)),
    ("synth", "voices", ("int", 1, 4, 0)),
    ("synth", "glide_ms", ("lin", 0, 2000)),
    ("reverb", "mix", ("lin", 0, 1)),
    ("reverb", "size", ("lin", 0, 1)),
    ("reverb", "damping", ("lin", 0, 1)),
)


def field_schema(codec):
    if codec[0] in ("lin", "log"): return {"type": "number", "minimum": codec[1], "maximum": codec[2]}
    if codec[0] == "enum": return {"type": "string", "enum": list(codec[1])}
    if codec[0] == "int": return {"type": "integer", "minimum": codec[1], "maximum": codec[2]}
    return {"type": "boolean"}


def v3_module_fields():
    """Ordered {module: {key: codec}} for every v3 module, including shared v2 fields."""
    number = lambda low, high: ("lin", low, high)
    modules = {
        "synth": {"waveform": ("enum", WAVEFORMS), **{k: number(*b) for k, b in AMP_LIMITS.items()}},
        "filter": {"cutoff_hz": ("log", 40, 16000)}, "lfo": {},
        "delay": {"mix": number(0, 1), "time_ms": number(10, 1000), "feedback": number(0, 0.85), "bypass": ("bool",)},
        "reverb": {}, "output": {"level": number(0, 1)}}
    for module, key, codec in V3_FIELDS:
        modules[module][key] = codec
    return modules


V3_MODULES = v3_module_fields()
SCHEMA3 = object_schema({
    "version": {"type": "integer", "enum": [3]},
    "name": {"type": "string", "minLength": 1, "maxLength": 80},
    "engine": {"type": "string", "enum": ["instrument"]},
    "routing": {"type": "string", "enum": list(ROUTES3)},
    "modules": object_schema({module: object_schema({key: field_schema(codec) for key, codec in fields.items()})
                              for module, fields in V3_MODULES.items()})})


# ---- Version 4: sampler (TAPE-compatible sample slots) ---------------------
SAMPLE_MODES = ("chromatic", "kit")
SAMPLE_BANKS = ("a", "b", "c", "d", "e")
SAMPLE_SLOTS = 14          # files per bank; slot 15 is the recording (RAM)
ROUTES4 = (*ROUTES3, "sampler>delay>reverb>output")
# Wire order after the v3 fields (request indexes 69..82), mirroring the v4
# entries of V3Fields in core/protocol.h. Index 68 (source) comes from routing.
V4_SAMPLER = (
    ("mode", ("enum", SAMPLE_MODES)),
    ("bank", ("enum", SAMPLE_BANKS)),
    ("slot", ("int", 1, 15, -1)),            # 15 = the recording
    ("pitch_semitones", ("lin", -24, 24)),
    ("start", ("lin", 0, 1)),
    ("end", ("lin", 0, 1)),
    ("loop", ("bool",)),
    ("hold", ("bool",)),                     # true: plays while held (TAPE sustain); false: one-shot trigger
    ("reverse", ("bool",)),
    ("crossfade_ms", ("lin", 0, 250)),
)
V4_MODULES = {module: dict(fields) for module, fields in V3_MODULES.items()}
V4_MODULES["synth"]["voices"] = ("int", 1, 7, 0)
V4_MODULES = {**{k: V4_MODULES[k] for k in ("synth", "filter", "lfo")}, "sampler": dict(V4_SAMPLER),
              **{k: V4_MODULES[k] for k in ("delay", "reverb", "output")}}
SCHEMA4 = object_schema({
    "version": {"type": "integer", "enum": [4]},
    "name": {"type": "string", "minLength": 1, "maxLength": 80},
    "engine": {"type": "string", "enum": ["instrument"]},
    "routing": {"type": "string", "enum": list(ROUTES4)},
    "modules": object_schema({module: object_schema({key: field_schema(codec) for key, codec in fields.items()})
                              for module, fields in V4_MODULES.items()})})
# ---- Version 5: knob assignments ---------------------------------------------
# v5 = v4 + "knobs": what panel knobs 1-4 (SW4, SW1, SW2, SW3; MIDI CC 20-23)
# control on their first page. "default" keeps the v4 behaviour (delay mix,
# time, feedback, level; with the sampler: pitch, start, end, delay mix).
# Wire byte = firmware Parameter id + 1 (core/parameters.h), 0 = default.
KNOB_TARGETS = {
    "delay.mix": 0, "delay.time_ms": 1, "delay.feedback": 2, "output.level": 3,
    "filter.cutoff_hz": 5, "filter.resonance": 6, "reverb.mix": 7,
    "sampler.pitch_semitones": 8, "sampler.start": 9, "sampler.end": 10,
    "filter.env_octaves": 15, "synth.attack_ms": 16, "synth.decay_ms": 17, "synth.sustain": 18,
    "synth.release_ms": 19, "lfo.rate_hz": 20, "lfo.pitch_cents": 21, "lfo.filter_octaves": 22,
    "lfo.amp_depth": 23, "synth.osc2_level": 24, "synth.osc2_detune_cents": 25, "synth.noise": 26,
    "synth.glide_ms": 27, "reverb.size": 28, "reverb.damping": 29, "sampler.crossfade_ms": 30}
KNOB_CHOICES = ("default", *KNOB_TARGETS)
KNOB_BYTES = {0: "default", **{value + 1: name for name, value in KNOB_TARGETS.items()}}
SCHEMA5 = object_schema({**SCHEMA4["properties"], "version": {"type": "integer", "enum": [5]},
                         "knobs": {"type": "array", "minItems": 4, "maxItems": 4,
                                   "items": {"type": "string", "enum": list(KNOB_CHOICES)}}})
# v6 (firmware 0.13) = v5 + "harmony": harmony mode (core/harmony.h; docs/forge/HARMONY_BRIEF.md).
# On the wire: one word (bits 0-3 tonic, 4-7 mode, 8-10 extension, 11-12 inversion, 13 open,
# 14 voice leading off, 15 Real layout, 16 enabled) in three 7-bit bytes after the knobs (request 87-89).
HARMONY_TONICS = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
HARMONY_MODES = ("major", "natural_minor", "harmonic_minor", "melodic_minor", "dorian", "phrygian", "lydian",
                 "mixolydian", "locrian")
HARMONY_EXTENSIONS = ("triad", "7th", "9th", "11th", "13th", "fifth")   # firmware enum order
HARMONY_DEFAULTS = {"enabled": False, "tonic": "C", "mode": "major", "extension": "triad", "inversion": 0,
                    "open": False, "voice_leading": True, "layout": "static"}
HARMONY_SCHEMA = object_schema({
    "enabled": {"type": "boolean"}, "tonic": {"type": "string", "enum": list(HARMONY_TONICS)},
    "mode": {"type": "string", "enum": list(HARMONY_MODES)}, "extension": {"type": "string", "enum": list(HARMONY_EXTENSIONS)},
    "inversion": {"type": "integer", "minimum": 0, "maximum": 3}, "open": {"type": "boolean"},
    "voice_leading": {"type": "boolean"}, "layout": {"type": "string", "enum": ["static", "real"]}})
SCHEMA6 = object_schema({**SCHEMA5["properties"], "version": {"type": "integer", "enum": [6]}, "harmony": HARMONY_SCHEMA})
# v7 (firmware 0.14) = v6 + "parts": the clock, arpeggiator and bass (core/parts.h; MANUAL 8b).
# On the wire: two words after the harmony word (request 90-92, 93-95): parts::PackArp
# (bits 0-2 pattern, 3-5 rate, 6-7 octaves-1, 8-12 gate/5-1, 13 latch, 14-16 bass, 17-18 bass
# rate, 19-20 bass octave) and parts::PackClock (bits 0-8 bpm, 9-19 seed, 20 send clock).
PARTS_PATTERNS = ("off", "up", "down", "updown", "order", "random")
PARTS_RATES = ("1/4", "1/8", "1/8t", "1/16", "1/16t", "1/32")
BASS_MODES = ("off", "root", "fifth", "alternate", "octave")
BASS_RATES = ("chord", "1/2", "1/4", "1/8")
PARTS_DEFAULTS = {"arp": {"pattern": "off", "rate": "1/8", "octaves": 1, "gate": 50, "latch": True},
                  "bass": {"mode": "off", "rate": "1/4", "octave": 2},
                  "clock": {"bpm": 120, "seed": 0, "send_clock": True}}
PARTS_SCHEMA = object_schema({
    "arp": object_schema({"pattern": {"type": "string", "enum": list(PARTS_PATTERNS)},
                          "rate": {"type": "string", "enum": list(PARTS_RATES)},
                          "octaves": {"type": "integer", "minimum": 1, "maximum": 4},
                          "gate": {"type": "integer", "minimum": 5, "maximum": 100},   # a multiple of 5 (checked)
                          "latch": {"type": "boolean"}}),
    "bass": object_schema({"mode": {"type": "string", "enum": list(BASS_MODES)},
                           "rate": {"type": "string", "enum": list(BASS_RATES)},
                           "octave": {"type": "integer", "minimum": 1, "maximum": 3}}),
    "clock": object_schema({"bpm": {"type": "integer", "minimum": 40, "maximum": 300},
                            "seed": {"type": "integer", "minimum": 0, "maximum": 2047},
                            "send_clock": {"type": "boolean"}})})
SCHEMA7 = object_schema({**SCHEMA6["properties"], "version": {"type": "integer", "enum": [7]}, "parts": PARTS_SCHEMA})


def parts_words(p):
    a, b, c = p["arp"], p["bass"], p["clock"]
    arp = (PARTS_PATTERNS.index(a["pattern"]) | PARTS_RATES.index(a["rate"]) << 3 | (a["octaves"] - 1) << 6
           | (a["gate"] // 5 - 1) << 8 | int(a["latch"]) << 13 | BASS_MODES.index(b["mode"]) << 14
           | BASS_RATES.index(b["rate"]) << 17 | (b["octave"] - 1) << 19)
    return arp, c["bpm"] | c["seed"] << 9 | int(c["send_clock"]) << 20


def parts_from_words(arp, clock):
    bpm = clock & 511
    if arp >> 21 or clock >> 21 or (arp & 7) > 5 or (arp >> 3 & 7) > 5 or (arp >> 8 & 31) > 19 or (arp >> 14 & 7) > 4 \
            or (arp >> 19 & 3) > 2 or not 40 <= bpm <= 300:
        raise ValueError("Invalid parts in status")
    return {"arp": {"pattern": PARTS_PATTERNS[arp & 7], "rate": PARTS_RATES[arp >> 3 & 7], "octaves": (arp >> 6 & 3) + 1,
                    "gate": 5 * ((arp >> 8 & 31) + 1), "latch": bool(arp >> 13 & 1)},
            "bass": {"mode": BASS_MODES[arp >> 14 & 7], "rate": BASS_RATES[arp >> 17 & 3], "octave": (arp >> 19 & 3) + 1},
            "clock": {"bpm": bpm, "seed": clock >> 9 & 2047, "send_clock": bool(clock >> 20 & 1)}}


def check_parts(p):
    def fail(): raise ValueError("parts: arp {pattern, rate, octaves 1-4, gate 5-100 in 5s, latch}, bass {mode, rate, octave 1-3}, "
                                 "clock {bpm 40-300, seed 0-2047, send_clock} (see the v7 schema)")
    if not isinstance(p, dict) or set(p) != set(PARTS_DEFAULTS) \
            or any(not isinstance(p[k], dict) or set(p[k]) != set(PARTS_DEFAULTS[k]) for k in PARTS_DEFAULTS): fail()
    a, b, c = p["arp"], p["bass"], p["clock"]
    ints = (a["octaves"], a["gate"], b["octave"], c["bpm"], c["seed"])
    if any(type(v) is not int for v in ints) or type(a["latch"]) is not bool or type(c["send_clock"]) is not bool \
            or a["pattern"] not in PARTS_PATTERNS or a["rate"] not in PARTS_RATES or b["mode"] not in BASS_MODES \
            or b["rate"] not in BASS_RATES or not 1 <= a["octaves"] <= 4 or not 5 <= a["gate"] <= 100 or a["gate"] % 5 \
            or not 1 <= b["octave"] <= 3 or not 40 <= c["bpm"] <= 300 or not 0 <= c["seed"] <= 2047: fail()


def harmony_word(h):
    return (HARMONY_TONICS.index(h["tonic"]) | HARMONY_MODES.index(h["mode"]) << 4 | HARMONY_EXTENSIONS.index(h["extension"]) << 8
            | h["inversion"] << 11 | int(h["open"]) << 13 | int(not h["voice_leading"]) << 14 | int(h["layout"] == "real") << 15
            | int(h["enabled"]) << 16)


def harmony_from_word(w):
    if w >> 17 or (w & 15) > 11 or (w >> 4 & 15) > 8 or (w >> 8 & 7) > 5: raise ValueError("Invalid harmony in status")
    return {"enabled": bool(w >> 16 & 1), "tonic": HARMONY_TONICS[w & 15], "mode": HARMONY_MODES[w >> 4 & 15],
            "extension": HARMONY_EXTENSIONS[w >> 8 & 7], "inversion": w >> 11 & 3, "open": bool(w >> 13 & 1),
            "voice_leading": not w >> 14 & 1, "layout": "real" if w >> 15 & 1 else "static"}


# Knob pages (core/knob_layout.h): TAPE's pages first, Forge's extra controls after,
# then the patch's own knob (v5 assignment) as one last page. Knobs 1-4 are SW4, SW1,
# SW2, SW3 on the panel. "tape.*" controls are device performance state (not in patches).
KNOB_SWITCHES = ("SW4", "SW1", "SW2", "SW3")
DEFAULT_KNOBS = {"synth": ("delay.mix", "delay.time_ms", "delay.feedback", "output.level"),
                 "sampler": ("sampler.pitch_semitones", "sampler.start", "sampler.end", "delay.mix")}
KNOB_LAYOUT = {
    "synth": (("tape.speed", "tape.voice_gain", "filter.resonance", "filter.env_octaves"),
              ("synth.attack_ms", "synth.decay_ms", "lfo.rate_hz"),
              ("synth.release_ms", "synth.sustain", "lfo.filter_octaves", "synth.osc2_detune_cents"),
              ("tape.space", "tape.saturation", "tape.dj_filter")),
    "sampler": (("tape.speed", "tape.voice_gain", "filter.resonance", "filter.env_octaves"),
                ("sampler.start", "synth.attack_ms", "lfo.rate_hz"),
                ("sampler.end", "synth.release_ms", "lfo.filter_octaves", "sampler.crossfade_ms"),
                ("tape.space", "tape.saturation", "tape.dj_filter")),
    "effects": (("delay.mix", "output.level"), ("delay.time_ms",), ("delay.feedback",),
                ("tape.space", "tape.saturation", "tape.dj_filter"))}


def _is_sampler(patch):
    return patch.get("version", 1) >= 4 and patch.get("routing", "").startswith("sampler")


def _layout(patch):
    """Which knob layout applies: effects-only (line in, no voices), sampler or synth."""
    if patch.get("version", 1) == 1 or patch.get("routing", "").startswith("aux"): return KNOB_LAYOUT["effects"]
    return KNOB_LAYOUT["sampler" if _is_sampler(patch) else "synth"]


def knob_pages(patch, knob):
    """How many pages knob 1-4 has for this patch (the patch page only when assigned)."""
    assigned = patch.get("knobs", ["default"] * 4)[knob - 1]
    return len(_layout(patch)[knob - 1]) + (assigned != "default")


def knob_control(patch, knob, page=None):
    """The control ("module.key") knob 1-4 turns on `page` (1-based); without a page,
    the patch's assignment for the knob (CC 20-23), else its source's default."""
    sampler = _is_sampler(patch)
    assigned = patch.get("knobs", ["default"] * 4)[knob - 1]
    if page is None:
        return DEFAULT_KNOBS["sampler" if sampler else "synth"][knob - 1] if assigned == "default" else assigned
    layout = _layout(patch)[knob - 1]
    if page <= len(layout): return layout[page - 1]
    return assigned if assigned != "default" else layout[0]


def control_value(patch, control):
    """Current value of a "module.key" control, or None if this patch version lacks it
    (or it is device performance state, "tape.*")."""
    module, key = control.split(".")
    if module == "tape": return None
    if patch["version"] == 1:
        return patch["parameters"].get(key) if module in ("delay", "output") else None
    modules = patch["modules"]
    if patch["version"] == 2 and module == "filter": module = "synth"
    return modules.get(module, {}).get(key)


SAMPLER_DEFAULTS = {"mode": "chromatic", "bank": "a", "slot": 1, "pitch_semitones": 0, "start": 0, "end": 1,
                    "loop": False, "hold": True, "reverse": False, "crossfade_ms": 10}


def check_value(path, value, codec):
    kind = codec[0]
    if kind in ("lin", "log"):
        if type(value) not in (float, int) or not math.isfinite(value) or not codec[1] <= value <= codec[2]:
            raise ValueError(f"{path} must be a number in [{codec[1]}, {codec[2]}]")
    elif kind == "enum":
        if value not in codec[1]: raise ValueError(f"{path} must be one of {', '.join(codec[1])}")
    elif kind == "int":
        if type(value) is not int or not codec[1] <= value <= codec[2]:
            raise ValueError(f"{path} must be an integer in [{codec[1]}, {codec[2]}]")
    elif type(value) is not bool:
        raise ValueError(f"{path} must be true or false")


def validate_instrument3(patch):
    v5 = patch.get("version") in (5, 6, 7)
    v6 = patch.get("version") in (6, 7)
    v7 = patch.get("version") == 7
    if set(patch) != {"version", "name", "engine", "routing", "modules", *(("knobs",) if v5 else ()), *(("harmony",) if v6 else ()),
                      *(("parts",) if v7 else ())} \
            or type(patch["version"]) is not int:
        raise ValueError("Invalid instrument fields")
    if v6:
        h = patch["harmony"]
        if not isinstance(h, dict) or set(h) != set(HARMONY_DEFAULTS) \
                or any(type(h[k]) is not bool for k in ("enabled", "open", "voice_leading")) \
                or h["tonic"] not in HARMONY_TONICS or h["mode"] not in HARMONY_MODES or h["extension"] not in HARMONY_EXTENSIONS \
                or type(h["inversion"]) is not int or not 0 <= h["inversion"] <= 3 or h["layout"] not in ("static", "real"):
            raise ValueError("harmony: " + ", ".join(HARMONY_DEFAULTS) + " (see the v6 schema)")
    if v7: check_parts(patch["parts"])
    v4 = patch["version"] >= 4
    if v5 and (not isinstance(patch["knobs"], list) or len(patch["knobs"]) != 4
               or any(not isinstance(k, str) or k not in KNOB_CHOICES for k in patch["knobs"])):
        raise ValueError("knobs must list 4 of: " + ", ".join(KNOB_CHOICES))
    spec = V4_MODULES if v4 else V3_MODULES
    if patch["engine"] != "instrument" or patch["routing"] not in (ROUTES4 if v4 else ROUTES3):
        raise ValueError("Unsupported engine or routing")
    if not isinstance(patch["name"], str) or not 1 <= len(patch["name"].strip()) <= 80 or len(patch["name"]) > 80:
        raise ValueError("Patch name must contain 1–80 characters")
    modules = patch["modules"]
    if not isinstance(modules, dict) or set(modules) != set(spec):
        raise ValueError("Expected modules: " + ", ".join(spec))
    for module, fields in spec.items():
        if not isinstance(modules[module], dict) or set(modules[module]) != set(fields):
            raise ValueError(f"{module} module needs exactly: {', '.join(fields)}")
        for key, codec in fields.items():
            check_value(f"{module}.{key}", modules[module][key], codec)
    # Compared as the device sees them (14-bit words): two values that round to the same
    # word would pass here and be rejected by the firmware (0.15.1).
    if v4 and not (unit_word(modules["sampler"]["start"], spec["sampler"]["start"])
                   < unit_word(modules["sampler"]["end"], spec["sampler"]["end"])):
        raise ValueError("sampler.start must be less than sampler.end")
    return patch


def to_unit(value, codec):
    if codec[0] == "log": return math.log(value / codec[1]) / math.log(codec[2] / codec[1])
    return (value - codec[1]) / (codec[2] - codec[1])


def from_unit(unit, codec):
    if codec[0] == "log": return codec[1] * (codec[2] / codec[1]) ** unit
    return codec[1] + (codec[2] - codec[1]) * unit


def unit_word(value, codec):
    # Clamp guards float rounding at range ends; 16384 would wrap to 0 in 14 bits.
    return int(min(max(to_unit(value, codec), 0.0), 1.0) * 16383 + 0.5)


def encode_word(value, codec):
    return word14(unit_word(value, codec))


# v2 -> v3 cutoff by waveform (DC approved 2026-10-11): a sine has no harmonics to lose.
V2_CUTOFF_FACTOR = {"sine": 1.0, "triangle": 1.6, "saw": 2.5, "square": 2.5}


def v2_cutoff(synth):
    low, high = SYNTH_LIMITS["cutoff_hz"]
    return round(min(high, max(low, synth["cutoff_hz"] * V2_CUTOFF_FACTOR[synth["waveform"]])), 1)


def upgrade_patch(patch, to=3):
    """Return a v3-v7 patch with the same delay/output (and v2 synth)
    settings; new modules start neutral (osc2/noise/LFO depths/reverb mix 0,
    filter envelope 0; v4 adds the sampler module, unused until routing selects
    it; v5 adds knob assignments, all "default"; v6 harmony off; v7 arp/bass off). The filter envelope copies the amp
    envelope's shape at zero depth. The v3 filter is a steeper resonant low-pass
    (12 dB/oct instead of v2's 6 dB/oct), so a v2 cutoff is raised by the waveform's
    harmonic content to keep the brightness (V2_CUTOFF_FACTOR, measured on CHOMPI
    2026-10-11: Saw Bass was half as bright upgraded 1:1)."""
    patch = validate_patch(patch)
    if to not in (3, 4, 5, 6, 7) or patch["version"] > to: raise ValueError("Upgrade target must be 3-7 and not older")
    if patch["version"] == to: return patch
    if to == 7:
        v6 = upgrade_patch(patch, 6)
        return validate_patch({**v6, "version": 7, "parts": json.loads(json.dumps(PARTS_DEFAULTS))})
    if to == 6:
        v5 = upgrade_patch(patch, 5)
        return validate_patch({**v5, "version": 6, "harmony": dict(HARMONY_DEFAULTS)})
    if to == 5:
        v4 = upgrade_patch(patch, 4)
        return validate_patch({**v4, "version": 5, "knobs": ["default"] * 4})
    if patch["version"] == 3:
        modules = {**json.loads(json.dumps(patch["modules"])), "sampler": dict(SAMPLER_DEFAULTS)}   # deep copy
        return validate_patch({**patch, "version": 4,
                               "modules": {module: modules[module] for module in V4_MODULES}})
    source = effect_patch(patch)["parameters"]
    v2 = patch["version"] == 2
    old_synth = patch["modules"]["synth"] if v2 else {
        "waveform": "sine", "attack_ms": 5, "decay_ms": 300, "sustain": 0.6, "release_ms": 300, "cutoff_hz": 8000}
    synth = {"waveform": old_synth["waveform"], **{key: old_synth[key] for key in AMP_LIMITS},
             "osc2_waveform": old_synth["waveform"], "osc2_level": 0, "osc2_semitones": 0,
             "osc2_detune_cents": 0, "noise": 0, "voices": 4, "glide_ms": 0}
    routing = ("synth>delay>reverb>output" if v2 and patch["routing"] == "synth>delay>output"
               else "aux>delay>reverb>output")
    upgraded = {"version": 3, "name": patch["name"], "engine": "instrument", "routing": routing, "modules": {
        "synth": synth,
        # The filter envelope starts neutral (0 octaves) but follows the amp envelope's shape,
        # so turning its amount on the panel is heard over the whole note.
        "filter": {"cutoff_hz": v2_cutoff(old_synth) if v2 else old_synth["cutoff_hz"], "resonance": 0, "env_octaves": 0,
                   **{key: old_synth[key] for key in ("attack_ms", "decay_ms", "sustain", "release_ms")}},
        "lfo": {"waveform": "sine", "rate_hz": 5, "pitch_cents": 0, "filter_octaves": 0, "amp_depth": 0, "mod_wheel": False},
        "delay": {key: source[key] for key in ("mix", "time_ms", "feedback", "bypass")},
        "reverb": {"mix": 0, "size": 0.5, "damping": 0.5},
        "output": {"level": source["level"]}}}
    upgraded["modules"] = {module: {key: upgraded["modules"][module][key] for key in fields}
                           for module, fields in V3_MODULES.items()}
    return upgrade_patch(validate_patch(upgraded), to) if to == 4 else validate_patch(upgraded)


def effect_patch(patch):
    if patch["version"] == 1: return patch
    return {"version": 1, "name": patch["name"], "engine": "stereo_delay",
            "parameters": {**patch["modules"]["delay"], **patch["modules"]["output"]}}


def validate_instrument(patch):
    if set(patch) != {"version", "name", "engine", "routing", "modules"} or type(patch["version"]) is not int:
        raise ValueError("Invalid instrument fields")
    if patch["engine"] != "instrument" or patch["routing"] not in ROUTES:
        raise ValueError("Unsupported engine or routing")
    modules = patch["modules"]
    if not isinstance(modules, dict) or set(modules) != {"synth", "delay", "output"}:
        raise ValueError("Expected synth, delay and output modules")
    for key in modules:
        if not isinstance(modules[key], dict): raise ValueError("Module must be an object")
    synth = modules["synth"]
    if set(synth) != {"waveform", *SYNTH_LIMITS} or synth["waveform"] not in WAVEFORMS:
        raise ValueError("Invalid synth module")
    if set(modules["delay"]) != {"mix", "time_ms", "feedback", "bypass"} or set(modules["output"]) != {"level"}:
        raise ValueError("Invalid delay/output module")
    for key, (low, high) in SYNTH_LIMITS.items():
        if type(synth[key]) not in (float, int) or not low <= synth[key] <= high or not math.isfinite(synth[key]):
            raise ValueError(f"{key} must be in [{low}, {high}]")
    validate_patch(effect_patch(patch))
    return patch


def validate_patch(patch):
    if isinstance(patch, dict) and patch.get("version") == 2:
        return validate_instrument(patch)
    if isinstance(patch, dict) and patch.get("version") in (3, 4, 5, 6, 7):
        validate_instrument3(patch)
        validate_patch(effect_patch(patch))
        return patch
    if not isinstance(patch, dict) or set(patch) != {"version", "name", "engine", "parameters"}:
        raise ValueError("Patch requires exactly version, name, engine and parameters")
    if type(patch["version"]) is not int or patch["version"] != 1:
        raise ValueError("Only patch version 1 is supported")
    if patch["engine"] != "stereo_delay":
        raise ValueError("Only stereo_delay is available in this firmware")
    if not isinstance(patch["name"], str) or not 1 <= len(patch["name"].strip()) <= 80:
        raise ValueError("Patch name must contain 1–80 characters")
    if len(patch["name"]) > 80:
        raise ValueError("Patch name exceeds 80 characters")
    p = patch["parameters"]
    if not isinstance(p, dict) or set(p) != {*LIMITS, "bypass"}:
        raise ValueError("Parameter keys must match the schema exactly")
    for key, (low, high) in LIMITS.items():
        value = p[key]
        if type(value) not in (float, int) or not low <= value <= high or not math.isfinite(value):
            raise ValueError(f"{key} must be a finite number in [{low}, {high}]")
    if type(p["bypass"]) is not bool:
        raise ValueError("bypass must be a boolean")
    return patch


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_json(text):
    if len(text.encode("utf-8")) > 65536:
        raise ValueError("JSON exceeds 64 KiB")
    return json.loads(text, object_pairs_hook=unique_keys,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(f"Invalid JSON: {value}")))


def load_patch(path):
    with Path(path).open("rb") as file:
        raw = file.read(65537)
    if len(raw) > 65536:
        raise ValueError("Patch file exceeds 64 KiB")
    return validate_patch(parse_json(raw.decode("utf-8")))


def save_patch(patch, path, overwrite=False):
    validate_patch(patch)
    # Exclusive creation by default: AI output cannot silently replace a preset.
    with Path(path).open("w" if overwrite else "x", encoding="utf-8") as file:
        file.write(json.dumps(patch, indent=2, allow_nan=False) + "\n")


def word14(value):
    return [value & 127, (value >> 7) & 127]


def read14(data, index):
    return data[index] | data[index + 1] << 7


def checksum(data):
    return (-sum(data)) & 127


def message(opcode, sequence, data=()):
    if type(sequence) is not int or not 0 <= sequence <= 16383:
        raise ValueError("Sequence must be 0–16383")
    payload = PREFIX + [opcode] + word14(sequence) + list(data)
    return payload + [checksum(payload)]


def encode_patch(patch, sequence):
    validate_patch(patch)
    p = effect_patch(patch)["parameters"]
    values = [p["mix"], (p["time_ms"] - 10) / 990, p["feedback"] / 0.85, p["level"]]
    data = [1]
    for value in values:
        # Clamp guards float rounding at range ends; 16384 would wrap to 0 in 14 bits.
        data.extend(word14(int(min(max(value, 0.0), 1.0) * 16383 + 0.5)))
    data.append(int(p["bypass"]))
    if patch["version"] == 2:
        data[0] = 2
        synth = patch["modules"]["synth"]
        data.extend([ROUTES.index(patch["routing"]), WAVEFORMS.index(synth["waveform"])])
        for key, (low, high) in SYNTH_LIMITS.items():
            value = (math.log(synth[key] / low) / math.log(high / low) if key == "cutoff_hz"
                     else (synth[key] - low) / (high - low))
            data.extend(word14(int(min(max(value, 0.0), 1.0) * 16383 + 0.5)))
    elif patch["version"] in (3, 4, 5, 6, 7):
        data[0] = patch["version"]
        modules = patch["modules"]
        route = (ROUTES4 if patch["version"] >= 4 else ROUTES3).index(patch["routing"])
        data.extend([min(route, 1), WAVEFORMS.index(modules["synth"]["waveform"])])
        for key, codec in V3_MODULES["synth"].items():
            if key in AMP_LIMITS: data.extend(encode_word(modules["synth"][key], codec))
        data.extend(encode_word(modules["filter"]["cutoff_hz"], V3_MODULES["filter"]["cutoff_hz"]))
        for module, key, codec in V3_FIELDS:
            value = modules[module][key]
            if codec[0] == "enum": data.append(codec[1].index(value))
            elif codec[0] == "int": data.append(value + codec[3])
            elif codec[0] == "bool": data.append(int(value))
            else: data.extend(encode_word(value, codec))
        if patch["version"] >= 4:
            data.append(int(route == 2))                 # source: sampler
            for key, codec in V4_SAMPLER:
                data.extend(encode_value(modules["sampler"][key], codec))
        if patch["version"] >= 5:
            data.extend(0 if knob == "default" else KNOB_TARGETS[knob] + 1 for knob in patch["knobs"])
        if patch["version"] >= 6:
            w = harmony_word(patch["harmony"]); data.extend([w & 127, w >> 7 & 127, w >> 14 & 127])
        if patch["version"] == 7:
            for w in parts_words(patch["parts"]): data.extend([w & 127, w >> 7 & 127, w >> 14 & 127])
    return message(1, sequence, data)


def encode_value(value, codec):
    if codec[0] == "enum": return [codec[1].index(value)]
    if codec[0] == "int": return [value + codec[3]]
    if codec[0] == "bool": return [int(value)]
    return encode_word(value, codec)


# Status reply length per patch version without the build identity (firmware before 0.15.2).
STATUS_LENGTHS = {1: 30, 2: 42, 3: 81, 4: 96, 5: 100, 6: 103, 7: 109}
IDENTITY_SIZE = 6
POWER_SIZE = 3                                              # battery, power flags, charge state (0.15.2)
STATUS_EXTRA = IDENTITY_SIZE + POWER_SIZE
BATTERY_LEVELS = ("full", "high", "medium", "low", "unknown")   # core/power.h Battery
BATTERY_COLOURS = ("white", "green", "yellow", "red", None)      # SW6 held 2 s shows the same colour
BUILD_DEVELOPMENT, BUILD_DIRTY, BUILD_SAFE_MODE = 1, 2, 4


def decode_identity(minor, identity):
    """The firmware version and build (core/protocol.h EncodeIdentity); older firmware has no identity."""
    if identity is None:
        return {"firmware": f"0.{minor}", "build": None, "development": None, "dirty": None, "safe_mode": None}
    patch, flags = identity[0], identity[1]
    commit = sum(b << (7 * i) for i, b in enumerate(identity[2:6]))
    return {"firmware": f"0.{minor}.{patch}", "build": f"{commit:07x}" if commit else None,
            "development": bool(flags & BUILD_DEVELOPMENT), "dirty": bool(flags & BUILD_DIRTY),
            "safe_mode": bool(flags & BUILD_SAFE_MODE)}


def decode_power(raw):
    """Battery and charger from a 0.15.2 status reply (same fields as Inspector page 2)."""
    level, flags, charge = raw
    if level > 4 or charge > 7:
        raise ValueError("Invalid power fields")
    return {"battery": BATTERY_LEVELS[level], "colour": BATTERY_COLOURS[level], "usb_power": bool(flags & 1),
            "charger_fault": bool(flags & 2), "weak_supply": bool(flags & 8), "battery_low_reading": bool(flags & 16),
            "install_allowed": not flags & 32, "charge_state": charge, "charging": charge in (1, 2, 3, 4), "charge_done": charge == 5}


def describe_power(power):
    """One line for people, e.g. "Battery high (green, as SW6 shows) · USB power · charging · install allowed"."""
    if power is None:
        return "Battery: not reported (firmware before 0.15.2; hold SW6 for 2 s on CHOMPI)"
    level = f"Battery {power['battery']}" + (f" ({power['colour']}, as SW6 shows)" if power["colour"] else "")
    parts = [level, "USB power" if power["usb_power"] else "on battery"]
    if power["charge_done"]: parts.append("charged")
    elif power["charging"]: parts.append("charging")
    if power["weak_supply"]: parts.append("weak USB supply (use a USB-C charger, 2 A or more)")
    if power["battery_low_reading"]: parts.append("a reading below 3.0 V")
    if power["charger_fault"]: parts.append("CHARGER FAULT")
    parts.append("install allowed" if power["install_allowed"] else "install refused: charge first")
    return " · ".join(parts)


def find_chompi_ports(midi=None):
    """CHOMPI's MIDI input and output by name, as Connect CHOMPI finds them."""
    midi = midi or midi_module()
    inputs = [n for n in midi.get_input_names() if "chompi" in n.lower()]
    outputs = [n for n in midi.get_output_names() if "chompi" in n.lower()]
    if not inputs or not outputs:
        raise RuntimeError("CHOMPI is not connected over USB (no CHOMPI MIDI ports); use a data cable")
    return inputs[0], outputs[0]


def describe_build(status):
    """One line for people, e.g. "Forge 0.15.2 (build 0f5bb18, development)"."""
    notes = [f"build {status['build']}" if status.get("build") else "build unknown" if status.get("development") is not None else None,
             "development" if status.get("development") else None, "uncommitted changes" if status.get("dirty") else None,
             "SAFE MODE" if status.get("safe_mode") else None]
    notes = [n for n in notes if n]
    return f"Forge {status['firmware']}" + (f" ({', '.join(notes)})" if notes else "")


def decode_response(data, sequence):
    data = list(data)
    if len(data) < 8 or data[:4] != PREFIX or any(type(x) is not int or not 0 <= x < 128 for x in data):
        raise ValueError("Invalid Forge reply header")
    if read14(data, 5) != sequence:
        raise ValueError("Unrelated reply sequence")
    if checksum(data) != 0:
        raise ValueError("Invalid reply checksum")
    if data[4] == 0x41 and len(data) == 9:
        raise RuntimeError("Device rejected request: " + ERRORS.get(data[7], "unknown error"))
    if data[4] == 0x42:
        if len(data) != 12 or data[7] != 0 or data[8] not in (1, 2) or data[9] >= PRESET_BANKS or data[10] >= PRESET_SLOTS:
            raise ValueError("Invalid preset acknowledgement")
        return {"sequence": sequence, "action": "stored" if data[8] == 1 else "erased",
                "bank": data[9] + 1, "slot": data[10] + 1}
    if data[4] == 0x43:
        if len(data) != 33 or data[7] != 0 or any(data[10 + 3 * b] > 1 for b in range(PRESET_BANKS)):
            raise ValueError("Invalid preset list")
        banks = {}
        for b in range(PRESET_BANKS):
            bits = data[8 + 3 * b] | data[9 + 3 * b] << 7 | data[10 + 3 * b] << 14
            banks[b + 1] = [s + 1 for s in range(PRESET_SLOTS) if bits >> s & 1]
        return {"sequence": sequence, "occupied": banks}
    if data[4] == 0x44:
        if len(data) != 36 or data[7] != 0 or data[28] > 7:
            raise ValueError("Invalid sample list")
        samples = {mode: {bank: [s + 1 for s in range(SAMPLE_SLOTS) if read14(data, 8 + 2 * (m * 5 + b)) >> s & 1]
                          for b, bank in enumerate(SAMPLE_BANKS)} for m, mode in enumerate(SAMPLE_MODES)}
        return {"sequence": sequence, "samples": samples, "card": bool(data[28] & 1), "recording": bool(data[28] & 2),
                "busy": bool(data[28] & 4),
                "recording_seconds": (read14(data, 29) | data[31] << 14) / 1000,
                "capacity_seconds": (read14(data, 32) | data[34] << 14) / 1000}
    if data[4] == 0x45:
        if len(data) != 13 or data[7] != 0 or data[8] > 2 or data[9] > 1 or data[10] > 4 or data[11] >= SAMPLE_SLOTS:
            raise ValueError("Invalid sample acknowledgement")
        return {"sequence": sequence, "action": ("saved", "erased", "copied")[data[8]],
                "mode": SAMPLE_MODES[data[9]], "bank": SAMPLE_BANKS[data[10]], "slot": data[11] + 1}
    if (data[4] != 0x40 or len(data) < 18 or data[7] != 0 or data[8] not in STATUS_LENGTHS or data[17] > 1
            or len(data) - STATUS_LENGTHS[data[8]] not in (0, STATUS_EXTRA)):
        raise ValueError("Invalid Forge status payload")
    identity = len(data) != STATUS_LENGTHS[data[8]]          # 0.15.2: patch, flags and build after the minor
    patch = {"version": 1, "name": "Captured from Forge", "engine": "stereo_delay", "parameters": {
        "mix": read14(data, 9) / 16383,
        "time_ms": 10 + 990 * read14(data, 11) / 16383,
        "feedback": 0.85 * read14(data, 13) / 16383,
        "level": read14(data, 15) / 16383, "bypass": bool(data[17])}}
    offset = 18
    if data[8] in (3, 4, 5, 6, 7):
        patch, offset = decode_v3(data, patch["name"]), {3: 69, 4: 84, 5: 88, 6: 91, 7: 97}[data[8]]
    elif data[8] == 2:
        if data[18] > 1 or data[19] > 3:
            raise ValueError("Invalid instrument status")
        synth = {"waveform": WAVEFORMS[data[19]]}
        for index, (key, (low, high)) in enumerate(SYNTH_LIMITS.items()):
            value = read14(data, 20 + 2 * index) / 16383
            synth[key] = low * (high / low) ** value if key == "cutoff_hz" else low + (high - low) * value
        params = patch["parameters"]
        patch = {"version": 2, "name": patch["name"], "engine": "instrument", "routing": ROUTES[data[18]],
                 "modules": {"synth": synth, "delay": {k: v for k, v in params.items() if k != "level"},
                             "output": {"level": params["level"]}}}
        offset = 30
    return {"sequence": sequence, **decode_identity(data[offset + 10], data[offset + 11:offset + 17] if identity else None),
            "power": decode_power(data[offset + 17:offset + 20]) if identity else None,
            "patch": validate_patch(patch),
            "cpu_average_percent": read14(data, offset) / 10,
            "cpu_max_percent": read14(data, offset + 2) / 10,
            "dropped": read14(data, offset + 4) | data[offset + 6] << 14,
            "rejected": read14(data, offset + 7) | data[offset + 9] << 14}


def decode_v3(data, name):
    """Inverse of the v3/v4/v5 part of encode_patch for an 81/96/100-byte status reply."""
    v4 = data[8] >= 4
    spec = V4_MODULES if v4 else V3_MODULES
    if data[18] > 1 or data[19] > 3:
        raise ValueError("Invalid v3/v4/v5 instrument status")
    modules = {module: {} for module in spec}
    modules["delay"] = {"mix": read14(data, 9) / 16383, "time_ms": 10 + 990 * read14(data, 11) / 16383,
                        "feedback": 0.85 * read14(data, 13) / 16383, "bypass": bool(data[17])}
    modules["output"] = {"level": read14(data, 15) / 16383}
    modules["synth"]["waveform"] = WAVEFORMS[data[19]]
    index = 20
    for key, codec in AMP_LIMITS.items():
        modules["synth"][key] = from_unit(read14(data, index) / 16383, ("lin", *codec)); index += 2
    modules["filter"]["cutoff_hz"] = from_unit(read14(data, index) / 16383, ("log", 40, 16000)); index += 2
    for module, key, codec in V3_FIELDS:
        if codec[0] in ("enum", "int", "bool"):
            raw = data[index]; index += 1
            if codec[0] == "enum":
                if raw >= len(codec[1]): raise ValueError("Invalid v3 enum in status")
                value = codec[1][raw]
            elif codec[0] == "int":
                value = raw - codec[3]
                high = 7 if v4 and key == "voices" else codec[2]
                if not codec[1] <= value <= high: raise ValueError("Invalid v3 integer in status")
            else:
                if raw > 1: raise ValueError("Invalid v3 flag in status")
                value = bool(raw)
        else:
            value = from_unit(read14(data, index) / 16383, codec); index += 2
        modules[module][key] = value
    route = data[18]
    if v4:
        if index != 69 or data[69] > 1: raise ValueError("Invalid v4 source in status")
        route = 2 if data[69] and data[18] else data[18]   # reply index = request index + 1
        index = 70
        for key, codec in V4_SAMPLER:
            if codec[0] in ("enum", "int", "bool"):
                raw = data[index]; index += 1
                if codec[0] == "enum":
                    if raw >= len(codec[1]): raise ValueError("Invalid v4 enum in status")
                    value = codec[1][raw]
                elif codec[0] == "int":
                    value = raw - codec[3]
                    if not codec[1] <= value <= codec[2]: raise ValueError("Invalid v4 slot in status")
                else:
                    if raw > 1: raise ValueError("Invalid v4 flag in status")
                    value = bool(raw)
            else:
                value = from_unit(read14(data, index) / 16383, codec); index += 2
            modules["sampler"][key] = value
    ordered = {module: {key: modules[module][key] for key in fields} for module, fields in spec.items()}
    patch = {"version": data[8], "name": name, "engine": "instrument",
             "routing": (ROUTES4 if v4 else ROUTES3)[route], "modules": ordered}
    if data[8] >= 5:
        if any(b not in KNOB_BYTES for b in data[84:88]): raise ValueError("Invalid v5 knob assignment in status")
        patch["knobs"] = [KNOB_BYTES[b] for b in data[84:88]]   # reply index = request index 83-86 + 1
    if data[8] >= 6:
        patch["harmony"] = harmony_from_word(data[88] | data[89] << 7 | data[90] << 14)   # request 87-89 + 1
    if data[8] == 7:
        patch["parts"] = parts_from_words(data[91] | data[92] << 7 | data[93] << 14, data[94] | data[95] << 7 | data[96] << 14)
    return patch


def preset_message(opcode, sequence, bank=None, slot=None):
    """Device-preset request. bank 1-8 and slot 1-15 as printed on the panel."""
    if opcode == 7:
        return message(7, sequence)
    if type(bank) is not int or type(slot) is not int or not 1 <= bank <= PRESET_BANKS or not 1 <= slot <= PRESET_SLOTS:
        raise ValueError(f"Bank must be 1-{PRESET_BANKS} and slot 1-{PRESET_SLOTS}")
    return message(opcode, sequence, [bank - 1, slot - 1])


def sample_message(opcode, sequence, action=None, mode=None, bank=None, slot=None,
                   to_mode=None, to_bank=None, to_slot=None):
    """Sampler requests: opcode 8 lists samples; 9 saves the recording into, erases
    or copies a TAPE sample slot (mode chromatic/kit, bank a-e, slot 1-14)."""
    if opcode == 8:
        return message(8, sequence)
    actions = ("save", "erase", "copy")
    if action not in actions: raise ValueError("Action must be save, erase or copy")
    def address(m, b, s):
        if m not in SAMPLE_MODES or b not in SAMPLE_BANKS or type(s) is not int or not 1 <= s <= SAMPLE_SLOTS:
            raise ValueError(f"Mode must be chromatic/kit, bank a-e and slot 1-{SAMPLE_SLOTS}")
        return [SAMPLE_MODES.index(m), SAMPLE_BANKS.index(b), s - 1]
    source = address(mode, bank, slot)
    target = address(to_mode, to_bank, to_slot) if action == "copy" else [0, 0, 0]
    return message(9, sequence, [actions.index(action), *source, *target])


def shared_words(a, b):
    """Words two device names share, ignoring numbers and short words: pairs MIDI ports
    ("CHOMPI 0" / "CHOMPI 1") and an audio interface's inputs and outputs."""
    words = lambda s: {w for w in s.lower().replace("(", " ").replace(")", " ").split() if len(w) > 2 and not w.isdigit()}
    return len(words(a) & words(b))


def midi_module():
    try:
        import mido
    except ImportError as error:
        raise RuntimeError("Install MIDI dependencies: python -m pip install -r host/requirements.txt") from error
    return mido


def exchange(payload, input_name, output_name, timeout=2.0, midi=None, decoder=None):
    if not math.isfinite(timeout) or timeout <= 0 or timeout > 30:
        raise ValueError("Timeout must be greater than 0 and at most 30 seconds")
    midi = midi or midi_module()
    sequence = read14(payload, 5)
    # Explicit names: never send a Forge packet to an automatically selected synth.
    if input_name not in midi.get_input_names() or output_name not in midi.get_output_names():
        raise ValueError("Port not found; run ports and copy the exact input/output names")
    with midi.open_input(input_name) as source, midi.open_output(output_name) as destination:
        destination.send(midi.Message("sysex", data=payload))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for _ in range(64):
                reply = source.receive(block=False)
                if reply is None:
                    break
                if reply.type != "sysex":
                    continue
                data = list(reply.data)
                if len(data) < 7 or data[:3] != PREFIX[:3] or read14(data, 5) != sequence:
                    continue
                result = (decoder or decode_response)(data, sequence)
                if payload[4] == 1 and data[8:len(payload)] != list(payload[7:-1]):
                    raise RuntimeError("Acknowledgement does not match the requested patch; query status")
                return result
            time.sleep(0.002)
    raise TimeoutError("No matching acknowledgement. The device may have applied the patch; query status before retrying.")


def play(output_name, cc=None, notes=(), velocity=100, hold=1.0, zero_velocity_off=False,
         bend=None, sustain=False, midi=None, sleep=time.sleep):
    """Send channel-1 test traffic (no acknowledgement exists for CC/notes).
    Note-offs are always sent, even if interrupted, so tests cannot leave stuck notes.
    bend (-8192..8191, mido convention) applies after note-on and is recentred at the end.
    sustain holds the pedal (CC64), releases the keys at once, waits, then lifts the pedal."""
    midi = midi or midi_module()
    if bend is not None and (type(bend) is not int or not -8192 <= bend <= 8191):
        raise ValueError("Bend must be an integer -8192..8191 (0 = centre)")
    if (bend is not None or sustain) and not notes:
        raise ValueError("Bend and sustain need at least one note")
    values = [*(cc or ()), *notes, velocity]
    if any(type(v) is not int or not 0 <= v <= 127 for v in values):
        raise ValueError("CC numbers/values, notes and velocity must be integers 0–127")
    if notes and velocity == 0:
        raise ValueError("Use velocity 1–127; note-off is sent automatically")
    if not math.isfinite(hold) or not 0 <= hold <= 30:
        raise ValueError("Hold must be 0–30 seconds")
    if output_name not in midi.get_output_names():
        raise ValueError("Port not found; run ports and copy the exact output name")
    with midi.open_output(output_name) as destination:
        if cc:
            destination.send(midi.Message("control_change", channel=0, control=cc[0], value=cc[1]))
        def release():
            for note in notes:
                destination.send(midi.Message("note_on", channel=0, note=note, velocity=0) if zero_velocity_off
                                 else midi.Message("note_off", channel=0, note=note, velocity=0))
        released = False
        try:
            if sustain:
                destination.send(midi.Message("control_change", channel=0, control=64, value=127))
            for note in notes:
                destination.send(midi.Message("note_on", channel=0, note=note, velocity=velocity))
            if bend is not None:
                destination.send(midi.Message("pitchwheel", channel=0, pitch=bend))
            if sustain:
                release(); released = True
            if notes:
                sleep(hold)
        finally:
            if not released:
                release()
            if bend is not None:
                destination.send(midi.Message("pitchwheel", channel=0, pitch=0))
            if sustain:
                destination.send(midi.Message("control_change", channel=0, control=64, value=0))


def generate_patch(prompt, model, endpoint="http://127.0.0.1:11434/api/chat", opener=None):
    if not prompt.strip() or len(prompt) > 4000 or not model.strip():
        raise ValueError("Provide a model and a prompt of 1–4000 characters")
    url = urllib.parse.urlparse(endpoint)
    if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password:
        raise ValueError("Use an HTTP(S) Ollama chat endpoint without embedded credentials")
    if url.scheme == "http" and url.hostname not in ("localhost", "127.0.0.1", "::1"):
        raise ValueError("Plain HTTP is limited to a local Ollama server; use HTTPS for a remote endpoint")
    system = ("Author a JSON Forge v1 stereo_delay patch matching the supplied schema. "
              "Only mix, time_ms, feedback, level and bypass exist. Do not invent algorithms, filters, "
              "reverb or code. Translate the request only within these delay controls. "
              "Default level to 0.25 unless explicitly requested. Return only the JSON object.")
    body = {"model": model, "stream": False, "format": SCHEMA,
            "messages": [{"role": "system", "content": system}, {"role": "user", "content": prompt}],
            "options": {"temperature": 0}}
    request = urllib.request.Request(endpoint, json.dumps(body).encode(),
                                     {"Content-Type": "application/json"}, method="POST")
    with (opener or urllib.request.urlopen)(request, timeout=90) as response:
        raw = response.read(65537)
    if len(raw) > 65536:
        raise ValueError("Model response exceeds 64 KiB")
    envelope = parse_json(raw.decode("utf-8"))
    try:
        content = envelope["message"]["content"]
    except (KeyError, TypeError) as error:
        raise ValueError("Ollama response has no message content") from error
    if not isinstance(content, str):
        raise ValueError("Model content must be JSON text")
    return validate_patch(parse_json(content))


def cli(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    schema = commands.add_parser("schema", help="Print an authoring JSON schema (v1 delay by default)")
    schema.add_argument("--instrument", action="store_true", help="Print the v7 instrument schema (sampler, knob choices, harmony, arp/bass/clock)")
    commands.add_parser("ports", help="List MIDI ports")
    validate = commands.add_parser("validate"); validate.add_argument("patch")
    upgrade = commands.add_parser("upgrade", help="Convert an older patch file to a newer instrument file (v3 by default, --to 3-7)")
    upgrade.add_argument("patch"); upgrade.add_argument("out"); upgrade.add_argument("--to", type=int, default=3, choices=(3, 4, 5, 6, 7))
    encode = commands.add_parser("encode", help="Print SysEx bytes without using MIDI")
    encode.add_argument("patch"); encode.add_argument("--sequence", type=int, default=1)
    preset_help = {"store": "Save the device's current sound to an SD preset slot",
                   "recall": "Load an SD preset slot into the device (same as the panel/program change)",
                   "erase": "Delete an SD preset slot", "slots": "List occupied SD preset slots",
                   "samples": "List TAPE sample slots on the card and the recording",
                   "sample-save": "Save the device's recording as a TAPE sample file",
                   "sample-erase": "Delete a TAPE sample file (and its _double)",
                   "sample-copy": "Copy a TAPE sample file to another slot"}
    for name in ("send", "status", "capture", "panic", "store", "recall", "erase", "slots",
                 "samples", "sample-save", "sample-erase", "sample-copy"):
        command = commands.add_parser(name, help=preset_help.get(name))
        if name == "send": command.add_argument("patch")
        if name == "capture": command.add_argument("file")
        if name in ("store", "recall", "erase"):
            command.add_argument("bank", type=int, help="1-8"); command.add_argument("slot", type=int, help="1-15")
        if name.startswith("sample-"):
            for prefix in ("", "to_") if name == "sample-copy" else ("",):
                command.add_argument(prefix + "mode", choices=SAMPLE_MODES)
                command.add_argument(prefix + "bank", choices=SAMPLE_BANKS)
                command.add_argument(prefix + "slot", type=int, help=f"1-{SAMPLE_SLOTS}")
        command.add_argument("--input", required=True); command.add_argument("--output", required=True)
        command.add_argument("--timeout", type=float, default=2.0)
    cc = commands.add_parser("cc", help="Send one channel-1 control change (e.g. 85 127 = wet bypass on, 123 0 = panic)")
    cc.add_argument("number", type=int); cc.add_argument("value", type=int); cc.add_argument("--output", required=True)
    note = commands.add_parser("note", help="Play channel-1 notes together, hold, then release them")
    note.add_argument("notes", type=int, nargs="+"); note.add_argument("--output", required=True)
    note.add_argument("--velocity", type=int, default=100); note.add_argument("--hold", type=float, default=1.0)
    note.add_argument("--zero-velocity-off", action="store_true", help="Release with note-on velocity 0 instead of note-off")
    note.add_argument("--bend", type=int, help="Pitch bend -8192..8191 while held (8191 = +2 semitones); recentred afterwards")
    note.add_argument("--sustain", action="store_true", help="Pedal down, release keys at once, hold, then pedal up")
    battery = commands.add_parser("battery", help="Battery, USB power and charging over USB (Forge 0.15.2 or newer); "
                                  "finds CHOMPI's ports itself")
    battery.add_argument("--input"); battery.add_argument("--output"); battery.add_argument("--timeout", type=float, default=2.0)
    battery.add_argument("--json", action="store_true", help="Print the decoded fields instead of a sentence")
    ai = commands.add_parser("ai", help="Ask a local Ollama model for a validated patch; does not send MIDI")
    ai.add_argument("prompt"); ai.add_argument("--model", required=True)
    ai.add_argument("--endpoint", default="http://127.0.0.1:11434/api/chat")
    ai.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    if args.command == "schema": print(json.dumps(SCHEMA7 if args.instrument else SCHEMA, indent=2))
    elif args.command == "ports":
        midi = midi_module()
        print(json.dumps({"inputs": midi.get_input_names(), "outputs": midi.get_output_names()}, indent=2))
    elif args.command == "validate": print(json.dumps(load_patch(args.patch), indent=2))
    elif args.command == "upgrade":
        save_patch(upgrade_patch(load_patch(args.patch), args.to), args.out); print(f"wrote {args.out}")
    elif args.command == "encode":
        print(bytes([0xF0, *encode_patch(load_patch(args.patch), args.sequence), 0xF7]).hex(" "))
    elif args.command == "cc":
        play(args.output, cc=(args.number, args.value)); print("sent")
    elif args.command == "note":
        play(args.output, notes=args.notes, velocity=args.velocity, hold=args.hold,
             zero_velocity_off=args.zero_velocity_off, bend=args.bend, sustain=args.sustain)
        print("sent and released")
    elif args.command == "battery":
        names = (args.input, args.output) if args.input and args.output else find_chompi_ports()
        try:
            status = exchange(message(2, secrets.randbelow(16384)), *names, args.timeout)
        except TimeoutError:
            raise RuntimeError("CHOMPI did not answer: it runs stock firmware (TAPE), not Forge. "
                               "Hold SW6 for 2 s on CHOMPI: white = charged, green = good, yellow = low.") from None
        if args.json: print(json.dumps({"firmware": status["firmware"], "build": status["build"], "power": status["power"]}, indent=2))
        else: print(describe_build(status)); print(describe_power(status["power"]))
    elif args.command == "ai":
        patch = generate_patch(args.prompt, args.model, args.endpoint)
        save_patch(patch, args.out)
        print(json.dumps(patch, indent=2))
    else:
        sequence = secrets.randbelow(16384)
        if args.command == "samples" or args.command.startswith("sample-"):
            payload = (sample_message(8, sequence) if args.command == "samples" else
                       sample_message(9, sequence, args.command[7:], args.mode, args.bank, args.slot,
                                      getattr(args, "to_mode", None), getattr(args, "to_bank", None),
                                      getattr(args, "to_slot", None)))
        elif args.command in ("store", "recall", "erase", "slots"):
            opcode = {"store": 4, "recall": 5, "erase": 6, "slots": 7}[args.command]
            payload = preset_message(opcode, sequence, getattr(args, "bank", None), getattr(args, "slot", None))
        else:
            payload = encode_patch(load_patch(args.patch), sequence) if args.command == "send" else message(3 if args.command == "panic" else 2, sequence)
        result = exchange(payload, args.input, args.output, args.timeout)
        if args.command == "capture": save_patch(result["patch"], args.file)
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(cli())
    except (ValueError, RuntimeError, OSError, TimeoutError) as error:
        print(f"Forge: {error}", file=sys.stderr)
        sys.exit(1)
