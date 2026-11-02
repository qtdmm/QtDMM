#!/usr/bin/env python3
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
"""Takes the specification data of the meters dmm-tools has transcribed from
their manuals (resolution and accuracy per range, input impedance, overload
protection) into assets/specs/<id>.json, for the Data tab of a device's
info.

    tools/update_device_specs.py ../ablage/dmm-tools

runs dmm-tools' dump_specs example (cargo, offline: the crates must be in
cargo's cache) for each model below and writes its JSON with the QtDMM
models it belongs to and the dmm-tools commit; the other files there
(hp570c-app.json, cmt3503.json, cm2100b.json) are written by hand and left alone. dmm-tools is GPL-3.0-or-later
(https://github.com/antoinecellerier/dmm-tools).
"""
import json
import pathlib
import subprocess
import sys

# dmm-tools device id -> the QtDMM models ("<vendor> <model>" of addConfig,
# without " *" and a "(...)" suffix such as the UT-D07B adapter)
MODELS = {
    "ut803": ["Uni-Trend UT803"],
    "ut804": ["Uni-Trend UT804"],
    "ut61eplus": ["Uni-Trend UT61E+"],
    "ut61b+": ["Uni-Trend UT61B+"],
    "ut61d+": ["Uni-Trend UT61D+"],
    "ut161b": ["Uni-Trend UT161B"],
    "ut161d": ["Uni-Trend UT161D"],
    "ut161e": ["Uni-Trend UT161E"],
    "zt5b": ["ZOTEK ZT-5B", "ANENG V05B"],
}

REPO = pathlib.Path(__file__).resolve().parent.parent
OUT = REPO / "assets" / "specs"


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    tools = pathlib.Path(sys.argv[1]).resolve()
    commit = subprocess.run(["git", "-C", str(tools), "rev-parse", "--short", "HEAD"], check=True,
                            capture_output=True, text=True).stdout.strip()
    OUT.mkdir(exist_ok=True)
    for device, models in MODELS.items():
        dump = subprocess.run(["cargo", "run", "--offline", "-q", "-p", "dmm-lib", "--example", "dump_specs", "--",
                               "--format", "json", device], cwd=tools, check=True, capture_output=True, text=True)
        spec = json.loads(dump.stdout)
        out = {"source": f"dmm-tools {commit}", "device": device, "models": models}
        out.update(spec)
        (OUT / f"{device}.json").write_text(json.dumps(out, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"wrote    assets/specs/{device}.json  ({spec['model']}, {len(spec['tables'])} tables)")


if __name__ == "__main__":
    main()
