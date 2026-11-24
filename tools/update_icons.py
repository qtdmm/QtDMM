#!/usr/bin/env python3
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds QtDMM's icon sets in assets/icons/sets/ from installed icon themes.

QtDMM ships the symbols it uses as icon themes compiled into the resources:

- qtdmm-breeze, qtdmm-breeze-dark: KDE's Breeze, monochrome ("Plain" in the
  settings), light and dark, scalable SVG. From /usr/share/icons/breeze and
  breeze-dark (package breeze-icon-theme or breeze-icons).
- qtdmm-oxygen: KDE's Oxygen, coloured ("Coloured", the default), PNG in
  16, 22, 32 and 48 px. From /usr/share/icons/oxygen (package
  oxygen-icon-theme or oxygen-icons).

QtDMM's own symbols come from assets/icons/own/<style>/: own/breeze/ as SVG
(the dark variant only swaps the text colour), own/oxygen/ as SVG or 32 px PNG. A
symbol own/oxygen/ lacks is taken from own/breeze/ as SVG.

Run it after adding a name to NAMES or a file to own/, and commit the
result; the build does not need the themes installed. Both themes are
LGPL-3.0-or-later (assets/icons/sets/LICENSE.breeze, LICENSE.oxygen).
"""
import pathlib
import shutil
import sys

# the names QtDMM asks for (QIcon::fromTheme / <iconset theme=...>)
NAMES = [
    "application-exit", "application-menu", "configure", "document-open",
    "document-print", "document-save", "document-save-as", "edit-delete", "go-home",
    "go-next", "go-previous", "media-playback-pause", "media-playback-start",
    "help-about", "help-contents", "help-whatsthis", "list-add", "list-remove",
    "measure", "media-playback-stop", "media-record", "network-connect",
    "network-disconnect", "network-server", "network-wired", "notifications",
    "office-chart-line", "preferences-desktop-theme-global", "system-run", "table",
    # the tiles of the assistant "Add device"
    "code-function", "drive-removable-media-usb", "preferences-system-bluetooth", "window-new",
    # the device sidebar
    "view-sidetree",
    # the toolbar (26.2): export/import, clear the graph, reset min/max
    "document-export", "document-import", "draw-eraser", "edit-clear-history",
    # the keyboard settings page
    "input-keyboard",
    # the project list: folders and file types
    "application-pdf", "folder", "image-x-generic", "text-csv", "text-x-generic", "x-office-spreadsheet",
    # the project window: a photo of the setup
    "camera-photo", "smartphone", "camera-web",
    # the web view's settings page and sidebar entry
    "internet-web-browser",
]
# Breeze has these under another name: its browser icon is coloured, the globe
# fits the monochrome set
BREEZE_ALIAS = {
    "internet-web-browser": "globe",
}
# Oxygen has these under another name (chosen by eye, paket_26_2 §1a)
OXYGEN_ALIAS = {
    "edit-clear-history": "edit-clear",   # the broom; Oxygen's own is an hourglass
    "help-whatsthis": "help-contextual",
    "notifications": "preferences-desktop-notification",
    "preferences-desktop-theme-global": "preferences-desktop-theme",
    "table": "view-list-details",
}
OXYGEN_SIZES = [16, 22, 32, 48]
CONTEXTS = ("actions", "devices", "places", "preferences", "status", "apps", "mimetypes", "categories")
LIGHT_TEXT, DARK_TEXT = "#232629", "#eff0f1"

SCALABLE_DIR = """
[{dir}]
Size=22
Type=Scalable
MinSize=8
MaxSize=256
"""
FIXED_DIR = """
[{dir}]
Size={size}
Type=Fixed
"""


def index(name: str, comment: str, dirs: list[str], body: str) -> str:
    return f"[Icon Theme]\nName={name}\nComment={comment}\nDirectories={','.join(dirs)}\n{body}"


def find(theme: pathlib.Path, name: str, size: str, ext: str) -> pathlib.Path | None:
    for context in CONTEXTS:
        p = theme / context / size / (name + ext)
        if p.exists():
            return p.resolve()
        p = theme / size / context / (name + ext)   # Oxygen: base/<size>/<context>/
        if p.exists():
            return p.resolve()
    return None


def fresh(out: pathlib.Path) -> None:
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)


def breeze(system: pathlib.Path, own: pathlib.Path, sets: pathlib.Path, theme: str, src: str, text: str | None) -> None:
    out = sets / theme
    fresh(out)
    (out / "22").mkdir()
    (out / "index.theme").write_text(index(theme, "Breeze symbols used by QtDMM (LGPL-3.0-or-later, KDE)",
                                           ["22"], SCALABLE_DIR.format(dir="22")))
    for name in NAMES:
        p = find(system / src, BREEZE_ALIAS.get(name, name), "22", ".svg")
        if not p:
            sys.exit(f"{name} ({BREEZE_ALIAS.get(name, name)}).svg not found in {system / src}")
        shutil.copyfile(p, out / "22" / (name + ".svg"))
    for svg in sorted((own / "breeze").glob("*.svg")):
        data = svg.read_text()
        if text:
            data = data.replace(LIGHT_TEXT, text)
        (out / "22" / svg.name).write_text(data)


def oxygen(system: pathlib.Path, own: pathlib.Path, sets: pathlib.Path) -> None:
    out = sets / "qtdmm-oxygen"
    fresh(out)
    src = system / "oxygen" / "base"
    dirs = [f"{s}x{s}" for s in OXYGEN_SIZES]
    for d in dirs:
        (out / d).mkdir()
    for name in NAMES:
        found = False
        for d in dirs:
            p = find(src, OXYGEN_ALIAS.get(name, name), d, ".png")
            if p:
                shutil.copyfile(p, out / d / (name + ".png"))
                found = True
        if not found:
            sys.exit(f"{name} ({OXYGEN_ALIAS.get(name, name)}) not found in {src}")
    # own symbols: drawn for Oxygen where there is one, else the Breeze SVG
    (out / "scalable").mkdir()
    drawn = {p.stem for p in (own / "oxygen").glob("*.*")}
    for png in sorted((own / "oxygen").glob("*.png")):
        shutil.copyfile(png, out / "32x32" / png.name)
    for svg in sorted((own / "oxygen").glob("*.svg")):
        shutil.copyfile(svg, out / "scalable" / svg.name)
    for svg in sorted((own / "breeze").glob("*.svg")):
        if svg.stem not in drawn:
            shutil.copyfile(svg, out / "scalable" / svg.name)
    body = "".join(FIXED_DIR.format(dir=d, size=s) for d, s in zip(dirs, OXYGEN_SIZES))
    body += SCALABLE_DIR.format(dir="scalable")
    (out / "index.theme").write_text(index("qtdmm-oxygen", "Oxygen symbols used by QtDMM (LGPL-3.0-or-later, KDE)",
                                           dirs + ["scalable"], body))


def main() -> None:
    root = pathlib.Path(__file__).resolve().parent.parent
    icons = root / "assets" / "icons"
    own, sets = icons / "own", icons / "sets"
    system = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "/usr/share/icons")
    breeze(system, own, sets, "qtdmm-breeze", "breeze", None)
    breeze(system, own, sets, "qtdmm-breeze-dark", "breeze-dark", DARK_TEXT)
    oxygen(system, own, sets)
    print(f"{len(NAMES)} symbols per set; own: {len(list((own / 'breeze').glob('*.svg')))} Breeze, "
          f"{len(list((own / 'oxygen').glob('*.*')))} Oxygen")


if __name__ == "__main__":
    main()
