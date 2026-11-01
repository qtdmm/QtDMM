#!/usr/bin/env python3
"""Generate decoder test fixtures from the protocol specs in docs/protocols/spec/.

Each spec YAML holds curated test vectors: the raw frame bytes together with a
structured reading of what the meter displayed for that frame. This script
derives the expectations test_decoder checks (dval, unit, range, hold) from that
reading and writes tests/data/decoder/<Protocol>.json.

The expectations are derived from the spec, never from the decoder, so a
failing test means the decoder and the documented protocol disagree.

Usage:
  generate_fixtures.py            write the fixtures
  generate_fixtures.py --check    regenerate in memory and compare with the
                                  committed files; exit 1 on any difference
"""

import argparse
import json
import re
import sys
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
SPEC_DIR = REPO / "docs" / "protocols" / "spec"
OUT_DIR = REPO / "tests" / "data" / "decoder"

PREFIX_FACTOR = {
    "p": 1e-12, "n": 1e-9, "µ": 1e-6, "u": 1e-6, "m": 1e-3,
    "k": 1e3, "M": 1e6, "G": 1e9, "T": 1e12,
}
BASE_UNITS = {"V", "A", "Ohm", "F", "Hz", "%", "°C", "°F", "C", "dF", "RPM", "D", "S", "s", "W", "VA", "lx", "fc", "H"}
# A displayed value is digits, sign, point and spaces; any letter marks an
# overload/underload display (0L. .0L 0.L UL . L0. OL UL ...) with no number.
OVERLOAD = re.compile(r"[A-Za-z]")


def split_unit(unit):
    """'mV' -> ('m', 'V'); 'Ohm' -> ('', 'Ohm'). Only splits when the remainder
    is a known base unit, so 'mF' is milli-farad but 'Hz' stays whole."""
    if len(unit) > 1 and unit[0] in PREFIX_FACTOR and unit[1:] in BASE_UNITS:
        return unit[0], unit[1:]
    return "", unit


def derive_dval(reading):
    value = reading["value"]
    # no number on the display ("-----", blank) or a fault word ("Er")
    if reading.get("shown") in ("none", "fault"):
        return None
    if OVERLOAD.search(value):
        return None
    prefix, _ = split_unit(reading["unit"])
    number = float(value.replace(" ", ""))
    return number * PREFIX_FACTOR.get(prefix, 1.0)


def apply_unit_map(unit, unit_map):
    for src, dst in unit_map.items():
        unit = unit.replace(src, dst)
    return unit


# The port a reading belongs to (kern_spezifikation §2.3), derived from what
# the meter displayed: the function where the log names it, else the unit;
# the coupling only where the quantity has one. Written independently of
# ReadingAdapter, so a test failure means the two disagree.
PORT_BY_UNIT = {
    "V": "voltage", "A": "current", "Ohm": "resistance", "Ω": "resistance", "F": "capacitance",
    "Hz": "frequency", "RPM": "frequency", "%": "duty_cycle", "S": "conductance", "W": "power",
    "VA": "apparent_power", "dBm": "power", "dBV": "voltage", "dB": "gain", "s": "time",
    "C": "temperature", "°C": "temperature", "dF": "temperature", "°F": "temperature",
    "Ah": "electric_charge", "Wh": "energy", "lx": "illuminance", "fc": "illuminance",
    "dBA": "sound_pressure_level", "dBC": "sound_pressure_level",
}
PORT_BY_FUNCTION = {
    "resistance": "resistance", "continuity": "continuity", "diode": "voltage",
    "temperature": "temperature", "frequency": "frequency", "duty cycle": "duty_cycle",
    "conductance": "conductance", "capacitance": "capacitance", "pulse width": "pulse_width",
    "decibel": "power",
}
COUPLED = {"voltage", "current", "power", "apparent_power"}


# The decoders' mode codes in the hand-curated expectations, where the log
# line names the coupling but the reading has no coupling field.
COUPLING_BY_SPECIAL = {"AC": "AC", "DC": "DC", "ACDC": "AC+DC"}


def derive_port(reading, expect=None):
    expect = expect or {}
    special = expect.get("special", "")
    flags = reading.get("flags", [])
    unit = reading["unit"]
    base = unit
    if unit not in PORT_BY_UNIT and len(unit) > 1 and unit[0] in PREFIX_FACTOR and unit[1:] in PORT_BY_UNIT:
        base = unit[1:]
    function = reading.get("function")
    if function is None and ("Pieps" in flags or special == "BUZ"):
        function = "continuity"
    if function is None and ("Diode" in flags or special in ("DI", "Diode")):
        function = "diode"
    quantity = PORT_BY_FUNCTION.get(function) or PORT_BY_UNIT.get(base)
    if quantity is None:
        return None
    parts = [quantity]
    coupling = reading.get("coupling") or COUPLING_BY_SPECIAL.get(special)
    if quantity in COUPLED and coupling:
        parts += {"AC": ["ac"], "DC": ["dc"], "AC+DC": ["ac", "dc"], "ACDC": ["ac", "dc"]}[coupling]
    if function == "diode":
        parts.append("diode")
    return ".".join(parts)


def build_case(vector, spec):
    reading = vector["reading"]
    expected = {}

    dval = derive_dval(reading)
    if dval is not None:
        expected["dval"] = dval

    expected["unit"] = apply_unit_map(reading["unit"], spec.get("unit_map", {}))
    # Protocols that do not transmit the range mode (Metex14) leave it out.
    if "range_mode" in reading:
        expected["range"] = "AUTO" if reading["range_mode"] == "auto" else "MANU"
    expected["hold"] = "HOLD" in reading.get("flags", [])
    # the meter shows no number, or a fault word instead (DmmChannel)
    if reading.get("shown") == "none":
        expected["val"] = reading["value"]
        expected["noValue"] = True
    elif reading.get("shown") == "fault":
        expected["val"] = reading["value"]
        expected["fault"] = reading["value"]
    port = derive_port(reading, vector.get("expect"))
    if port is not None:
        expected["port"] = port

    # Hand-curated assertions on top of the derived ones (special, val, ...).
    expected.update(vector.get("expect", {}))

    source = Path(vector.get("source", spec["sources"][0])).name
    return {
        "comment": f"[{source}] '{vector['source_line']}'",
        "hex": vector["bytes"],
        "expected": expected,
    }


def build_fixture(spec, stem):
    fixture = {
        "decoder": spec["protocol"],
        "comment": (
            "GENERATED by tests/generate_fixtures.py from "
            f"docs/protocols/spec/{stem}.yaml - edit the spec, not this file. "
            "Expectations are derived from the documented readings in "
            + ", ".join(Path(s).name if "/" in s else s for s in spec["sources"])
            + ", not from the decoder."
        ),
    }
    if spec.get("frames_per_reading", 1) != 1:
        fixture["framesPerReading"] = spec["frames_per_reading"]
    fixture["tests"] = [build_case(v, spec) for v in spec.get("vectors", [])]
    return fixture


def render(fixture):
    return json.dumps(fixture, indent=2, ensure_ascii=False) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true",
                        help="compare with committed fixtures instead of writing")
    args = parser.parse_args()

    specs = sorted(SPEC_DIR.glob("*.yaml"))
    if not specs:
        print(f"no specs found in {SPEC_DIR}", file=sys.stderr)
        return 1

    stale = []
    for path in specs:
        spec = yaml.safe_load(path.read_text(encoding="utf-8"))
        fixture = build_fixture(spec, path.stem)
        text = render(fixture)
        out = OUT_DIR / f"{path.stem}.json"
        counts = f"{len(fixture['tests'])} vectors, {len(spec.get('skipped', []))} skipped"
        pending = sum(len(g.get("vectors", [])) for g in spec.get("pending", []))
        if pending:
            counts += f", {pending} PENDING decoder review"

        if args.check:
            current = out.read_text(encoding="utf-8") if out.exists() else None
            status = "ok" if current == text else ("MISSING" if current is None else "STALE")
            if status != "ok":
                stale.append(out)
            print(f"{status:8s} {out.relative_to(REPO)}  ({counts})")
        else:
            out.write_text(text, encoding="utf-8")
            print(f"wrote    {out.relative_to(REPO)}  ({counts})")

    if args.check and stale:
        print(f"\n{len(stale)} fixture(s) differ from their spec - run generate_fixtures.py",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
