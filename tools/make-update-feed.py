#!/usr/bin/env python3
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes latest.json, the file QtDMM's update check reads from the Pages
site (https://qtdmm.github.io/QtDMM/latest.json), from GitHub's list of
releases:

    gh api repos/qtdmm/QtDMM/releases | tools/make-update-feed.py [<version> <commit> <date>] > site/latest.json

"stable" is the newest release, "beta" the newest pre-release when it is
newer than that (else null: a beta user is offered the final release). Each
names its version, date, release page and the download of every package
kind with its SHA256 (GitHub's digest of the file):

    {"format": 1,
     "stable": {"version": "26.4", "date": "2026-11-22", "notes": "https://...",
                "files": {"appimage": {"url": ..., "sha256": ..., "size": ...}, ...}},
     "beta": null,
     "master": {"version": "27.1-dev.530+gdef5678", "commit": "def5678...", "date": "2026-11-23"}}

"master" is the newest commit on master, for those who build QtDMM from
its sources: the arguments <version> <commit> <date> (version from
cmake -P cmake/git_version.cmake), else null.

Drafts and tags that are not versions (YY.N[.P][-alphaK|-betaK|-rcK]) are
left out.
"""
import json
import re
import sys

VERSION = re.compile(r"^(\d+)\.(\d+)(?:\.(\d+))?(?:-(alpha|beta|rc)(\d+))?$")
SUFFIX_ORDER = {"alpha": 0, "beta": 1, "rc": 2}

# file name (after "QtDMM-<tag>") -> package kind
KINDS = {
    "-x86_64.AppImage": "appimage",
    "-x86_64.flatpak": "flatpak",
    "-windows-x64.exe": "windows-installer",
    "-windows-x64.zip": "windows-zip",
    "-macos.dmg": "macos",
    ".tar.gz": "source",
}


def version_key(tag):
    """Sort key of a tag, None for one that is not a version. A final
    release sorts after its candidates."""
    m = VERSION.match(tag)
    if not m:
        return None
    year, n, patch, suffix, k = m.groups()
    pre = (SUFFIX_ORDER[suffix], int(k)) if suffix else (3, 0)
    return (int(year), int(n), int(patch or 0), pre)


def entry(release):
    tag = release["tag_name"]
    files = {}
    for asset in release.get("assets", []):
        name = asset["name"]
        for ending, kind in KINDS.items():
            if name == f"QtDMM-{tag}{ending}":
                digest = asset.get("digest") or ""
                files[kind] = {
                    "url": asset["browser_download_url"],
                    "sha256": digest.removeprefix("sha256:") if digest.startswith("sha256:") else "",
                    "size": asset["size"],
                }
    return {
        "version": tag,
        "date": (release.get("published_at") or "")[:10],
        "notes": release["html_url"],
        "files": files,
    }


def master_entry(args):
    if len(args) != 3:
        return None
    version, commit, date = args
    return {"version": version, "commit": commit, "date": date}


def feed(releases, master=None):
    published = [r for r in releases if not r.get("draft") and version_key(r["tag_name"])]
    published.sort(key=lambda r: version_key(r["tag_name"]), reverse=True)
    stable = next((r for r in published if not r.get("prerelease")), None)
    beta = next((r for r in published if r.get("prerelease")), None)
    if beta and stable and version_key(beta["tag_name"]) < version_key(stable["tag_name"]):
        beta = None
    return {
        "format": 1,
        "stable": entry(stable) if stable else None,
        "beta": entry(beta) if beta else None,
        "master": master,
    }


def main():
    # stdin to stdout: no file names to open (the shell does the redirection)
    json.dump(feed(json.load(sys.stdin), master_entry(sys.argv[1:])), sys.stdout, indent=1)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
