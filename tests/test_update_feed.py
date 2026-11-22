#!/usr/bin/env python3
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/make-update-feed.py: tests/data/update/releases.json (26.3, 26.4-rc1,
26.4, a draft 27.1, the pre-release 27.1-rc1, a tag 27.1-dev) gives
tests/data/update/latest.json, which test_update reads too."""
import importlib.util
import json
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("feed", root / "tools" / "make-update-feed.py")
feed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(feed)

data = root / "tests" / "data" / "update"
releases = json.loads((data / "releases.json").read_text(encoding="utf-8"))
expected = json.loads((data / "latest.json").read_text(encoding="utf-8"))
failed = 0


def check(ok, what):
    global failed
    if not ok:
        failed += 1
        print("FAIL:", what)


result = feed.feed(releases)
check(result == expected, "releases.json -> latest.json (regenerate with tools/make-update-feed.py)")
check(result["stable"]["version"] == "26.4", "stable: the newest release, not the draft")
check(result["beta"]["version"] == "27.1-rc1", "beta: the newest pre-release")
check(set(result["stable"]["files"]) == set(feed.KINDS.values()), "every kind of file, no other")
check(feed.feed([r for r in releases if r["tag_name"] != "27.1-rc1"])["beta"] is None,
      "an older pre-release is no beta")
check(feed.version_key("26.4") > feed.version_key("26.4-rc1") > feed.version_key("26.4-beta2"), "rc before final")
check(feed.version_key("26.10") > feed.version_key("26.9"), "numbers, not text")
check(feed.version_key("27.1-dev") is None, "a dev tag is no version")
if failed:
    sys.exit(1)
print("All update feed tests passed.")
