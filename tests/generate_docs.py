#!/usr/bin/env python3
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the documents that must not drift from their source.

1. docs/user/supported-devices.md - the device table, read off the
   DmmDecoder::addConfig({...}) registrations in src/device/decoders/*.cpp. Those calls
   are the only thing that decides what the settings dialog offers, so the
   documentation is derived from them rather than maintained by hand.

2. README.md - the GitHub landing page, assembled from docs/ so the website,
   the in-app handbook and the README share one source. Relative links are
   rewritten to paths that work from the repository root.

Usage:
  generate_docs.py            write both files
  generate_docs.py --check    regenerate in memory and compare; exit 1 on drift
"""

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DECODER_DIR = REPO / "src" / "device" / "decoders"
DOCS = REPO / "docs"
DEVICES_MD = DOCS / "user" / "supported-devices.md"
README = REPO / "README.md"

# {"Vendor", "Model", "", baud, FrameFormat::Proto, bits, stopBits, framesPerReading,
#  parity, display, externalSetup, rts, dtr[, sigrokExe, sigrokDriver, channels]}
ADD_CONFIG = re.compile(
    r'addConfig\(\s*\{\s*"(?P<vendor>[^"]*)"\s*,\s*"(?P<model>[^"]*)"\s*,\s*"[^"]*"\s*,'
    r'\s*(?P<baud>\d+)\s*,\s*FrameFormat::(?P<protocol>\w+)\s*,\s*(?P<bits>\d)\s*,'
    r'\s*(?P<stop>\d)\s*,\s*(?P<values>\d+)\s*,\s*(?P<parity>\d)\s*,\s*(?P<counts>\d+)\s*,'
    r'\s*(?P<ext>\d)\s*,\s*(?P<rts>\d)\s*,\s*(?P<dtr>\d)')
PARITY = {"0": "N", "1": "E", "2": "O"}

# The chip behind a protocol, where it is known: lets a user match an
# unlisted meter to a protocol. Read from the protocol table in
# src/device/protocols.cpp - one row per line:
#   { FrameFormat::<Id>, "<Name>", QT_TRANSLATE_NOOP("Protocols", "<text>"), "<chip>", "<transport>", make<...> },
PROTOCOL_ROW = re.compile(
    r'\{\s*FrameFormat::(?P<id>\w+)\s*,\s*"(?P<name>\w+)"\s*,\s*QT_TRANSLATE_NOOP\("Protocols",\s*"(?P<text>[^"]*)"\)'
    r'\s*,\s*"(?P<chip>[^"]*)"\s*,\s*"(?P<transport>[^"]*)"\s*,\s*make<(?P<decoder>\w+)>')


def protocol_table():
    """{name: {"id", "text", "chip", "decoder"}} from src/device/protocols.cpp."""
    text = (REPO / "src" / "device" / "protocols.cpp").read_text(encoding="utf-8")
    rows = {}
    for m in PROTOCOL_ROW.finditer(text):
        rows[m.group("name")] = {"id": m.group("id"), "text": m.group("text"),
                                 "chip": m.group("chip"), "transport": m.group("transport"),
                                 "decoder": m.group("decoder")}
    if not rows:
        sys.exit("no protocol rows found in src/device/protocols.cpp - format changed?")
    return rows


CHIP = {name: row["chip"] for name, row in protocol_table().items() if row["chip"]}


def device_types():
    """src/device/devicetype.cpp: each protocol's kind, the models listed one
    by one, and the kinds' names - (by_protocol, by_model, names)."""
    src = (REPO / "src" / "device" / "devicetype.cpp").read_text(encoding="utf-8")
    names = dict(re.findall(r'case (\w+):\s*return QCoreApplication::translate\("DeviceType", "([^"]+)"\)', src))
    body = src[src.index("Type ofProtocol"):src.index("namespace\n{")]
    by_protocol, pending = {}, []
    for line in body.splitlines():
        m = re.search(r"case FrameFormat::(\w+):", line)
        if m:
            pending.append(m.group(1))
        m = re.search(r"return (\w+);", line)
        if m and pending:
            for p in pending:
                by_protocol[p] = m.group(1)
            pending = []
    by_model = {(v, mo): t for v, mo, t in re.findall(r'\{ "([^"]+)", "([^"]+)", (\w+) \}', src)}
    return by_protocol, by_model, names


TYPE_BY_PROTOCOL, TYPE_BY_MODEL, TYPE_NAMES = device_types()
# How a meter is reached when it has no baud rate - "Bluetooth LE",
# "USB-HID (BU-86X)", "sigrok-cli" - straight from src/device/protocols.cpp, so a
# new backend is declared next to its protocol rather than guessed here.
TRANSPORT = {name: row["transport"] for name, row in protocol_table().items() if row["transport"]}

# Meters that are sold WITHOUT a serial interface: the chip has the output, but
# it takes soldering (an IR LED or a wire on a chip pin, sometimes an EEPROM
# change) before the meter sends anything. Keyed by (vendor, model prefix) as
# registered; the text ends up in the footnote section of the device table.
HARDWARE_MOD = {
    ("Generic", "DTM0660"): (
        "the DTM0660 chip has a UART, but meters built on it (Victor VC921 and "
        "similar) ship without an interface: an IR LED with a 100 Ω resistor "
        "goes on the chip's TX pin and the RS232 bit in the 24C02 EEPROM has to "
        "be set - "
        "[eevblog: hacking the Victor VC-921](https://www.eevblog.com/forum/testgear/hacking-the-victor-vc-921/). "
        "PeakTech 3415 and Velleman DVM4100 use the same chip but come with a "
        "cable and need no change."),
    ("Vichy", "VC99"): (
        "the FS9922-DMM4 chip sends the display at 2400 baud on an unused pin; "
        "a wire or IR LED has to be soldered to it and *REL* held to start the "
        "output - [Hackaday](https://hackaday.com/2010/11/30/unlocking-rs232-serial-comm-on-a-multimeter/), "
        "[David Pilling](https://www.davidpilling.com/wiki/index.php/VC99)."),
}


def hardware_mod(vendor, model):
    """The HARDWARE_MOD note for a registered meter, or None."""
    for (v, prefix), note in HARDWARE_MOD.items():
        if vendor == v and model.startswith(prefix):
            return note
    return None


def devices():
    rows = []
    for src in sorted(DECODER_DIR.glob("*.cpp")):
        for m in ADD_CONFIG.finditer(src.read_text(encoding="utf-8")):
            d = m.groupdict()
            lines = "DTR" if d["dtr"] == "1" else ""
            if d["rts"] == "1":
                lines = (lines + " RTS").strip()
            model = d["model"]
            unconfirmed = model.endswith("*")
            marks = (" ¹" if unconfirmed else "") + (" ²" if hardware_mod(d["vendor"], model) else "")
            rows.append({
                "vendor": d["vendor"], "model": model.rstrip("* ").strip() + marks,
                "hardware_mod": hardware_mod(d["vendor"], model),
                "protocol": d["protocol"], "chip": CHIP.get(d["protocol"], "-"),
                "type": TYPE_NAMES[TYPE_BY_MODEL.get((d["vendor"], model.rstrip("* ").strip()),
                                                     TYPE_BY_PROTOCOL[d["protocol"]])],
                # baud 0: not a serial line, the protocol's transport says what it is
                "serial": f'{d["baud"]} {d["bits"]}{PARITY[d["parity"]]}{d["stop"]}' if d["baud"] != "0"
                          else TRANSPORT.get(d["protocol"], "?"),
                "counts": d["counts"], "lines": lines or "-",
                "decoder": src.name,
            })
    rows.sort(key=lambda r: (r["vendor"].lower(), r["model"].lower()))
    return rows


def render_devices(rows):
    out = [
        "# Supported devices",
        "",
        "Every meter QtDMM can decode, taken from the decoder registrations in",
        "`src/device/decoders/` (this page is generated from them by",
        "`tests/generate_docs.py`). Choosing one of these models on the Multimeter",
        "settings page fills in the serial parameters below; meters not listed can",
        "often be used with *Manual settings* if they speak one of the listed",
        "protocols - the *Chip* column helps: a meter built around the same chip",
        "(named in its manual or on the sigrok wiki) usually speaks the same",
        "protocol. See [Connecting a meter](connecting.md).",
        "",
        "*Type* is the kind of instrument (handheld or bench multimeter, thermometer,",
        "battery monitor, ...). *Serial* is baud rate, data bits, parity (N/E/O) and stop bits; meters that",
        "are not read over a serial line say how they are reached instead",
        "(*USB-HID (BU-86X)* for the Brymen cable, *Bluetooth LE*, *sigrok-cli*).",
        "*Lines* are the control lines the cable needs driven. *Counts* is the display",
        "resolution. Not every entry has been confirmed on hardware recently; models",
        "marked ¹ were added from chip datasheets and protocol documentation and have not",
        "been tried with QtDMM at all. Models marked ² are sold without a serial",
        "interface and need a hardware modification first (see the notes below the",
        "table). If you can confirm one, or get an unlisted",
        "meter working, please report it on the",
        "[project page](https://github.com/qtdmm/QtDMM/issues).",
        "",
        "| Vendor | Model | Type | Chip | Protocol | Serial | Lines | Counts |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for r in rows:
        out.append(f'| {r["vendor"]} | {r["model"]} | {r["type"]} | {r["chip"]} | `{r["protocol"]}` | {r["serial"]} '
                   f'| {r["lines"]} | {r["counts"]} |')
    out.append("")
    out.append("¹ settings taken from the chip, not yet confirmed on hardware with QtDMM.")
    out.append("")
    out.append("² needs a hardware modification - the meter has no interface as sold:")
    out.append("")
    seen = []
    for r in rows:
        if r["hardware_mod"] and r["hardware_mod"] not in seen:
            seen.append(r["hardware_mod"])
            group = [x for x in rows if x["hardware_mod"] == r["hardware_mod"]]
            models = [x["model"].replace(" ²", "").replace(" ¹", "") for x in group]
            names = f'{r["vendor"]} {", ".join(models)}' if len({x["vendor"] for x in group}) == 1 \
                else ", ".join(f'{x["vendor"]} {m}' for x, m in zip(group, models))
            out.append(f"- **{names}**: {r['hardware_mod']}")
    out.append("")
    out.append(f"{len(rows)} devices across {len({r['vendor'] for r in rows})} vendors.")
    kinds = {}
    for r in rows:
        kinds[r["type"]] = kinds.get(r["type"], 0) + 1
    out.append("")
    out.append("By type: " + ", ".join(f"{t} {n}" for t, n in
                                       sorted(kinds.items(), key=lambda k: (-k[1], k[0]))) + ".")
    return "\n".join(out) + "\n"


LINK = re.compile(r"\]\((?!https?://|mailto:|#|/)([^)]+)\)")


def rebase_links(text, page_dir):
    """Make links in a page from docs/<page_dir> resolve from the repo root."""
    prefix = "docs/" + (page_dir + "/" if page_dir else "")
    return LINK.sub(lambda m: f"]({prefix}{m.group(1)})", text)


def demote(text):
    """Shift every heading one level down so a page nests under a README H2."""
    return re.sub(r"^(#+) ", lambda m: "#" + m.group(1) + " ", text, flags=re.M)


def render_readme(devices_md):
    index = (DOCS / "index.md").read_text(encoding="utf-8")
    cmdline = (DOCS / "user" / "command-line.md").read_text(encoding="utf-8")

    head, licensing = index.split("\n## Licensing\n", 1)
    devices_body = devices_md.split("\n", 1)[1]        # drop the H1
    cmdline_body = cmdline.split("\n", 1)[1]

    parts = [
        "<!-- GENERATED from docs/ by tests/generate_docs.py - edit the pages in docs/, not this file -->",
        "",
        rebase_links(head.rstrip("\n"), ""),
        "",
        "## Supported hardware",
        rebase_links(devices_body.rstrip("\n"), "user"),
        "",
        "## Command line",
        demote(rebase_links(cmdline_body.rstrip("\n"), "user")),
        "",
        "## Licensing",
        licensing.strip("\n"),
        "",
    ]
    return "\n".join(parts)


def protocol_drift():
    """Every FrameFormat::DataFormat value before EndOfList must have exactly
    one row in src/device/protocols.cpp, with the enum id as its name."""
    header = (REPO / "src" / "device" / "frameformat.h").read_text(encoding="utf-8")
    body = header[header.index("enum DataFormat"):header.index("EndOfList")]
    enum_ids = [m.group(1) for m in re.finditer(r"^\s*(\w+)\s*(?:=\s*-?\d+)?\s*,", body, re.M) if m.group(1) != "Invalid"]
    rows = protocol_table()
    problems = []
    for e in enum_ids:
        if e not in rows:
            problems.append(f"FrameFormat::{e} has no row in src/device/protocols.cpp")
    for name, row in rows.items():
        if row["id"] != name:
            problems.append(f"protocols.cpp row {name}: enum id {row['id']} differs from the name")
        if row["id"] not in enum_ids:
            problems.append(f"protocols.cpp row {name}: FrameFormat::{row['id']} is not in the enum")
    return problems


def transport_drift():
    """A device without a baud rate must have a transport on its protocol
    row."""
    problems = []
    for r in devices():
        if r["serial"] == "?":
            problems.append(f'{r["vendor"]} {r["model"]} ({r["protocol"]}) has no baud rate and its '
                            f'protocol row in src/device/protocols.cpp has no transport')
        # a protocol may be reached both ways - the UT61E+ over the UT-D09 USB
        # cable (a baud rate) or the UT-D07B Bluetooth adapter (baud 0) - so a
        # baud rate next to a transport is not an error
    return problems


def hid_cable_drift():
    """The HID cable table exists three times: tests/data/hid_cables.json (the
    truth both test suites read), kCables in src/device/transports/hidserial.cpp and
    HID_CABLES in tools/qtdmm-bridge/qtdmm_bridge.py. Returns the problems."""
    import json

    truth = {(c["vid"].lower(), c["pid"].lower()): c["chip"]
             for c in json.loads((REPO / "tests" / "data" / "hid_cables.json").read_text(encoding="utf-8"))["cables"]}
    cpp = {}
    for m in re.finditer(r"\{\s*0x([0-9a-fA-F]{4}),\s*0x([0-9a-fA-F]{4}),\s*HIDSerialDevice::Chip::(\w+)\s*\}",
                         (REPO / "src" / "device" / "transports" / "hidserial.cpp").read_text(encoding="utf-8")):
        cpp[(m.group(1).lower(), m.group(2).lower())] = m.group(3)
    py = {}
    for m in re.finditer(r"\(0x([0-9a-fA-F]{4}),\s*0x([0-9a-fA-F]{4})\):\s*\(\"(\w+)\"",
                         (REPO / "tools" / "qtdmm-bridge" / "qtdmm_bridge.py").read_text(encoding="utf-8")):
        py[(m.group(1).lower(), m.group(2).lower())] = m.group(3)
    problems = []
    for name, table in (("hidserial.cpp kCables", cpp), ("qtdmm_bridge.py HID_CABLES", py)):
        if table != truth:
            problems.append(f"{name} differs from tests/data/hid_cables.json: "
                            f"{sorted(set(table.items()) ^ set(truth.items()))}")
    return problems


def option_drift():
    """The command-line options exist in src/main.cpp and are described twice,
    in docs/qtdmm.1.in (the man page) and docs/user/command-line.md. Returns
    the options one of them is missing."""
    main_cpp = (REPO / "src" / "main.cpp").read_text(encoding="utf-8")
    options = set(re.findall(r'addOption\(\{\s*"([\w-]+)"', main_cpp))
    if "addHelpOption()" in main_cpp:
        options.add("help")
    if "addVersionOption()" in main_cpp:
        options.add("version")
    man = "\n".join(l for l in (REPO / "docs" / "qtdmm.1.in").read_text(encoding="utf-8").splitlines()
                    if not l.startswith('.\\"')).replace("\\-", "-")   # without roff comments
    md = (REPO / "docs" / "user" / "command-line.md").read_text(encoding="utf-8")
    problems = []
    for name, text in (("docs/qtdmm.1.in", man), ("docs/user/command-line.md", md)):
        documented = set(re.findall(r"--([a-z][\w-]*)", text))
        for o in sorted(options - documented):
            problems.append(f"option --{o} (src/main.cpp) is not described in {name}")
        for o in sorted(documented - options):
            problems.append(f"{name} describes --{o}, which src/main.cpp does not have")
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    devices_md = render_devices(devices())
    outputs = {DEVICES_MD: devices_md}
    outputs[README] = render_readme(devices_md)

    stale = 0
    for path, text in outputs.items():
        rel = path.relative_to(REPO)
        if args.check:
            current = path.read_text(encoding="utf-8") if path.exists() else None
            ok = current == text
            stale += not ok
            print(f'{"ok" if ok else "STALE":8s} {rel}')
        else:
            path.write_text(text, encoding="utf-8")
            print(f"wrote    {rel}")

    for problem in protocol_drift() + transport_drift() + hid_cable_drift() + option_drift():
        print(f"DRIFT    {problem}", file=sys.stderr)
        stale += 1

    if stale:
        print(f"\n{stale} file(s) differ from their sources - run tests/generate_docs.py",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
