#!/usr/bin/env python3
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bootstrap a protocol spec from one of the capture logs in docs/protocols/sources/.

The logs pair a label with the raw frame the meter sent for it, one per line:

    DC  0.0000 V AUTO BG<TAB>30 30 30 30 30 30 3B 30 30 30 3A 30 0D 0A<TAB>remark

This script reads such a log (latin-1 or UTF-8), parses the label into
a structured reading and prints a spec YAML skeleton to stdout. The output is a
starting point for the human-curated file in docs/protocols/spec/ - review every
vector, fill in 'expect' where the decoder's special string is known, and move
anything doubtful to 'skipped'. Lines the parser cannot interpret are already
listed under 'skipped' with a reason; vectors with a 'note' deserve a second
look.

Usage:
  import_protocol_log.py docs/protocols/sources/UT61E.log \\
      --protocol CyrustekES51922 --decoder src/device/decoders/cyrustek_es51922.cpp \\
      --devices "Uni-Trend UT61E" "Wintex TD2200" --packet-length 14 \\
      > docs/protocols/spec/cyrustek_es51922.yaml
"""

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

COUPLING = {"DC": "DC", "AC": "AC", "A+D": "AC+DC", "AC+DC": "AC+DC"}
RANGE_MODE = {"AUTO": "auto", "A.": "auto", "AU": "auto", "MAN": "manual", "MANU": "manual",
              "M.": "manual", "FIX": "manual"}
FLAGS = {"BG", "Rel", "MAX", "MIN", "HOLD", "LowBat", "Diode", "Pieps", "TRMS"}
# Spellings that differ between logs but mean the same display flag.
FLAG_ALIASES = {"H": "HOLD", "Delta": "Rel", "REL": "Rel", "Ton": "Pieps", "TR": "TRMS"}
UNIT = re.compile(r"^[pnµumkMGT]?(?:V|A|Ohm|\u03a9|\u2126|F|Hz|%|°C|°F|RPM)$")
RANGE_LABEL = re.compile(r"^\d+[A-Za-zµ%°]*$")
CAPTURE = re.compile(r"^(?P<label>[^\t]*?)\t+(?P<bytes>(?:[0-9A-Fa-f]{2}\s+)*[0-9A-Fa-f]{2})\s*(?:\t\s*(?P<remark>.*?))?\s*$")


def parse_label(label):
    """Return (reading, notes) or raise ValueError with the reason."""
    tokens = [FLAG_ALIASES.get(t, t) for t in label.split()]
    notes = []
    reading = {}

    if tokens and tokens[0] in COUPLING:
        reading["coupling"] = COUPLING[tokens.pop(0)]

    flags = [t for t in tokens if t in FLAGS]
    tokens = [t for t in tokens if t not in FLAGS]

    unit_idx = next((i for i, t in enumerate(tokens) if UNIT.match(t)), None)
    if unit_idx is None:
        raise ValueError("no unit token found")
    if unit_idx == 0:
        raise ValueError("no value before the unit")

    reading["value"] = " ".join(tokens[:unit_idx])
    reading["unit"] = tokens[unit_idx].replace("\u03a9", "Ohm").replace("\u2126", "Ohm")
    rest = tokens[unit_idx + 1:]

    if not rest or rest[0] not in RANGE_MODE:
        raise ValueError("no range mode (AUTO/MANU/...) after the unit")
    reading["range_mode"] = RANGE_MODE[rest.pop(0)]

    range_label = [t for t in rest if RANGE_LABEL.match(t)]
    unknown = [t for t in rest if not RANGE_LABEL.match(t)]
    if range_label:
        reading["range_label"] = " ".join(range_label)
    if flags:
        reading["flags"] = flags
    if unknown:
        notes.append("unrecognised tokens: " + " ".join(unknown))
    return reading, notes


METEX_MODE = {"DC": ("DC", None), "AC": ("AC", None), "OH": (None, "resistance"),
              "FR": (None, "frequency"), "DI": (None, "diode"), "TE": (None, "temperature"),
              "HF": (None, "hFE"), "LO": (None, "logic")}
METEX_LINE = re.compile(r"^'(?P<frame>.{13})\\r'\s*$")


def parse_metex(line):
    """A metex-22t.log line is the 14-byte ASCII frame itself, quoted, e.g.
    'DC  0017    V\\r'. Mode is chars 0-1, value 2-8, unit 9-12, then CR."""
    m = METEX_LINE.match(line)
    if not m:
        return None
    frame = m.group("frame")
    label = frame.strip()
    mode, value, unit = frame[0:2], frame[2:9].strip(), frame[9:13].strip()
    bytes_ = " ".join(f"{ord(c):02X}" for c in frame) + " 0D"
    if mode not in METEX_MODE:
        return label, bytes_, None, [], f"unknown mode {mode!r}"
    if not unit:
        return label, bytes_, None, [], f"no unit in the frame (mode {mode})"
    coupling, function = METEX_MODE[mode]
    # the meter writes kilo as a capital K
    unit = "k" + unit[1:] if unit.startswith("K") else unit
    reading = {"value": value, "unit": unit}   # Metex14 carries no range mode
    if coupling:
        reading["coupling"] = coupling
    if function:
        reading["function"] = function
    return label, bytes_, reading, [], ""


def yaml_str(s):
    # JSON string literals are valid YAML double-quoted scalars.
    return json.dumps(s, ensure_ascii=False)


def render_reading(reading):
    parts = [f"value: {yaml_str(reading['value'])}", f"unit: {yaml_str(reading['unit'])}"]
    if "coupling" in reading:
        parts.append(f"coupling: {reading['coupling']}")
    if "function" in reading:
        parts.append(f"function: {reading['function']}")
    if "range_mode" in reading:
        parts.append(f"range_mode: {reading['range_mode']}")
    if "flags" in reading:
        parts.append("flags: [" + ", ".join(reading["flags"]) + "]")
    if "range_label" in reading:
        parts.append(f"range_label: {yaml_str(reading['range_label'])}")
    return "{" + ", ".join(parts) + "}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("log", type=Path)
    ap.add_argument("--protocol", required=True, help="FrameFormat::DataFormat name")
    ap.add_argument("--decoder", required=True, help="path of the decoder source")
    ap.add_argument("--devices", nargs="*", default=[])
    ap.add_argument("--frames-per-reading", type=int, default=1)
    ap.add_argument("--packet-length", type=int,
                    help="expected frame length; lines with other lengths are skipped")
    ap.add_argument("--format", choices=["hex", "metex"], default="hex",
                    help="hex: label TAB bytes (default); metex: quoted 14-byte ASCII frames")
    args = ap.parse_args()

    # Most logs are latin-1, two are UTF-8; a UTF-8 decode of latin-1 text fails
    # on the first umlaut, so try UTF-8 first and fall back.
    raw = args.log.read_bytes()
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        text = raw.decode("latin-1")
    source = args.log.resolve().relative_to(REPO).as_posix()

    vectors, skipped = [], []
    for line in text.splitlines():
        if args.format == "metex":
            parsed = parse_metex(line)
            if parsed is None:
                continue
            label, frame, reading, notes, problem = parsed
            if reading is None:
                skipped.append((label, problem))
            else:
                vectors.append((label, frame, reading, notes, ""))
            continue
        m = CAPTURE.match(line)
        if not m:
            continue
        label = m.group("label").strip()
        frame = " ".join(b.upper() for b in m.group("bytes").split())
        remark = (m.group("remark") or "").strip()
        nbytes = len(frame.split())

        if args.packet_length and nbytes != args.packet_length:
            skipped.append((label, f"{nbytes} bytes, expected {args.packet_length}"))
            continue
        try:
            reading, notes = parse_label(label)
        except ValueError as e:
            skipped.append((label, str(e)))
            continue
        vectors.append((label, frame, reading, notes, remark))

    out = []
    out.append(f"protocol: {args.protocol}")
    out.append(f"decoder: {args.decoder}")
    out.append("devices: [" + ", ".join(args.devices) + "]")
    out.append(f"frames_per_reading: {args.frames_per_reading}")
    out.append("unit_map:                      # log spelling -> what the decoder emits")
    out.append('  "µ": "u"')
    out.append("sources:")
    out.append(f"  - {source}")
    out.append("")
    out.append("# 'reading' is what the log says the meter displayed; dval, unit, range and")
    out.append("# hold are derived from it. Put decoder-specific assertions (special, val)")
    out.append("# under 'expect' by hand. 'range_label' and 'remark' are informational.")
    out.append("vectors:")
    for label, frame, reading, notes, remark in vectors:
        out.append(f"  - source_line: {yaml_str(label)}")
        out.append(f"    bytes: {frame}")
        out.append(f"    reading: {render_reading(reading)}")
        if remark:
            out.append(f"    remark: {yaml_str(remark)}")
        for n in notes:
            out.append(f"    note: {yaml_str(n)}")
    out.append("")
    out.append("skipped:")
    if not skipped:
        out.append("  []")
    for label, reason in skipped:
        out.append(f"  - source_line: {yaml_str(label)}")
        out.append(f"    reason: {yaml_str(reason)}")

    print("\n".join(out))
    print(f"# {len(vectors)} vectors, {len(skipped)} skipped, "
          f"{sum(1 for v in vectors if v[3])} with notes", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
