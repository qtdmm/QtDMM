# Development

Documentation for people working on QtDMM itself. Planned pages:

- **Building and testing** — `compile.sh`, the CMake targets, the CTest suites
  (`test_decoder`, `test_graph`) and how the decoder fixtures in
  `tests/data/decoder/` are generated from the protocol specifications.
- **Architecture** — the path from `Transport` and the `QIODevice` backends
  through `FrameReader` and the `DmmDecoder` subclasses to `MeterConnection`, `InstanceWidget`,
  `LcdWidget` and `GraphWidget`; the `DmmChannel` value contract; the
  `DeviceSession` with the meters of one process.
- **Adding a protocol** — the `DmmDecoder` subclass, a value in the
  `FrameFormat::DataFormat` enum (before `EndOfList`), one row in the table in
  `src/device/protocols.cpp` (name, combo text, chip, factory - the settings combo,
  `FrameFormat::toString()`, `DmmDecoder::getInstance()` and the device table
  all read it; ctest `protocol_table` and `docs_generated` fail when enum and
  table disagree), `addConfig()` for each supported device, and the protocol
  specification and test vectors under `docs/protocols/spec/`. Frames are
  fixed-length (`getPacketLength()`); a line protocol with variable length
  returns 0 there and gets everything since the previous frame, terminator
  included (`fluke_qm.cpp` is the example, it skips the ACK line itself).
- **Adding a USB-HID cable chip** — the chip table and report layouts live
  twice, in `src/device/transports/hidserial.cpp` (QtDMM) and
  `tools/qtdmm-bridge/qtdmm_bridge.py` (the bridge). Both are tested against
  the same vectors, `tests/data/hid_cables.json`, and
  `tests/generate_docs.py --check` (ctest `docs_generated`) fails when either
  table drifts from that file - so a new chip goes into the JSON first, then
  into both implementations.

None of these pages exist yet. The architecture is documented in the code
itself, see below.

## Device pictures

The pictures of the kinds of instrument (`DeviceSymbol`: page 4 of Add
device, the sidebar) are made when CMake runs, by
`cmake/device_symbols.cmake` - CMake alone, no script. Its inputs:

- `assets/devices/<type>.svg`, one template per `DeviceType` id, with the
  placeholders `#FF00FF` (primary), `#00FFFF` (secondary) and `#FFFF00` (the
  outline: dark grey, light in the `_dark` twin for a dark design);
- `assets/devices/manufacturer-colors.json`: per manufacturer its colours,
  the vendors of `addConfig()` it stands for (`match`) and the kinds its
  models are (`types`), optionally other colours for a kind
  (`device_classes`);
- `assets/devices/override/<key>_<type>.svg`, a picture of its own for one
  manufacturer and kind.

Out come `:/devices/<type>.svg` in the neutral colours of `default` and
`:/devices/<key>/<type>.svg` for each manufacturer and kind, each with
`_dark`, and `:/devices/vendors.json` (vendor -> key). A model of a vendor
without colours of its own gets the neutral picture. A new model of a new
kind, or a new vendor in `match`, needs the JSON adjusted: ctest
`device_symbols` says what to add.

## Coding style

[Coding style](coding-style.md): layout, names, Qt idioms and structure
rules for the sources, and how existing code is brought in line.

## Versions and releases

[Versions and releases](releasing.md): the version scheme YY.N (26.1,
26.1-rc1, 26.1.1), where the version of a build comes from, and the steps
for a release.

## SCPI command reference

The network instrument QtDMM presents (Settings → SCPI server) is
specified in [SCPI command reference](scpi-commands.md): message format,
every accepted header, the answers and the error codes.

[MCP server for test sessions](qtdmm-mcp.md): `tools/qtdmm-mcp.py` lets an
AI assistant read a running test instance - values, status and a
screenshot - through the SCPI server.

## API documentation

The headers under `src/` carry Doxygen comments; the class descriptions of
`MeterConnection`, `Transport`, `FrameReader`, `DmmDecoder`, `InstanceWidget`, `GraphWidget`
and `DeviceSession` together describe the architecture. Generate the
HTML with

```
./compile.sh doxygen
```

(or `cmake --build build --target doxygen`, or `mkdir -p build/doxygen &&
doxygen` in the repository root - the `Doxyfile` is checked in) and open
`build/doxygen/html/index.html`. With graphviz installed the pages carry
class, collaboration, include and directory diagrams as interactive SVG
(`dot` is looked up on the PATH; without it doxygen warns once and leaves
the diagrams out). Doxygen itself is optional: without it the target simply
does not exist. The published copy at <https://qtdmm.github.io/QtDMM/api/> is
rebuilt from `master` after every merge and every published release
(`.github/workflows/pages.yml`, which builds the whole site: the landing
page from `assets/pages` and the API pages under `api/`).

## Translations

The build compiles `assets/translations/qtdmm_*.ts` with lrelease only. After
adding or changing `tr()` strings, refresh the sources explicitly:

```
cmake --build build --target update_translations
```

then fill in the new entries (Qt Linguist or a text editor) and commit the
`.ts` files. lupdate is kept out of the normal build on purpose: it rewrites
files in the source tree, and two targets running it in parallel corrupted
them.

Alt letters (the `&` in a text) must not repeat within one menu or among the
widgets shown together, or Alt+letter only moves the focus between them. The
tests `mnemonics_<language>` start QtDMM offscreen with
`--check-mnemonics` in every language, go through all menus, all settings
pages and every kind of port on the meter page, and list each doubled letter
together with the letters already taken there:

```
ctest --test-dir build -R mnemonics --output-on-failure
```

The language comes from `LANG`, so on Windows and macOS the tests check the
system language only.

The compiled `.qm` files are embedded in the binary. Distribution packages
that want them as separate files (Fedora's `%find_lang --with-qt`) configure
with `-DQTDMM_EMBED_TRANSLATIONS=OFF`: the `.qm` files are then installed to
`share/qtdmm/translations`, where `src/main.cpp` finds them relative to the
binary (`<prefix>/bin/../share/qtdmm/translations`). On Windows and macOS the
option is always on, so the portable ZIP and the app bundle stay
self-contained.

## Platforms

Linux, macOS and FreeBSD are built and tested by the GitHub workflows with
the distribution's Qt and hidapi (FreeBSD runs inside a VM on an Ubuntu runner
via `vmactions/freebsd-vm`, since GitHub has no FreeBSD runner). Windows is built on `windows-latest` with Qt from
`install-qt-action` and hidapi compiled from source via CMake `FetchContent`
(see `CMakeLists.txt`); the workflow uploads a portable ZIP and an Inno Setup
installer, both produced by CPack from the same install tree
(`cmake/deploy.cmake`). Vendored third-party code lives under `src/3rdparty/`: tiny-AES-c (public
domain, the Victron decryption) and miniz (MIT, deflate and CRC for the
XLSX/ODS export - the ZIP container itself is written by
`SpreadsheetWriter`, since LibreOffice rejects miniz's data descriptors on
the ODS mimetype entry) and QCustomPlot 2.1.1 in Wireshark's fork
(GPL-2.0-or-later, trimmed of old Qt code paths; the XY diagram, histogram
and heatmap draw with it, on top of `QcpGraphCommon`, OpenGL only with
`Graph/openGl`, off by default: classic QCustomPlot's OpenGL leaves artefacts
and costs more with several plots). It is meant as a step towards NeoQCP.

The symbols come in three icon themes under `assets/icons/sets/`, compiled
into the resources (`:/icons`); the code asks for symbols by name with
`QIcon::fromTheme()`:

- `qtdmm-oxygen` - KDE's **Oxygen**, coloured PNGs in 16 to 48 px, the default
  (*Appearance → Symbols → Coloured*).
- `qtdmm-breeze`, `qtdmm-breeze-dark` - KDE's **Breeze**, monochrome SVG
  (*Plain*); `Designs` picks light or dark by the window colour.

Both are LGPL-3.0-or-later (`assets/icons/sets/LICENSE.breeze`, `LICENSE.oxygen`).
*System* (Linux/BSD, when the desktop has an icon theme) puts the desktop's
theme first, with the plain Breeze set as the fallback. `tools/update_icons.py`
copies the names in `NAMES` from installed Breeze and Oxygen themes (Oxygen
lacks five of them under that name: `OXYGEN_ALIAS`) and adds QtDMM's own
symbols from `assets/icons/own/<style>/`: `own/breeze/` as SVG drawn with
`currentColor`, `own/oxygen/` as SVG or 32 px PNG (DMM, display, analog
meter, integration, instances, arrange, Poincaré plot); a symbol `own/oxygen/` lacks is taken
from `own/breeze/`. To use a new symbol, add its name to `NAMES` (or files to
`own/`), run the script and commit the result. The program icon lives in
`assets/icons/app/`.

The Bluetooth LE port (`src/device/transports/ble.cpp`) needs Qt6 Bluetooth and
is optional: CMake option `QTDMM_WITH_BLE` (on when the module is found;
the Windows workflow installs `qtconnectivity`, Ubuntu `qt6-connectivity-dev`,
FreeBSD `qt6-connectivity`). Sound and video need Qt6 Multimedia, optional the
same way: `QTDMM_WITH_MULTIMEDIA` (`qtmultimedia`, Ubuntu `qt6-multimedia-dev`,
FreeBSD `qt6-multimedia`). Platform-specific code is limited to the permission
hint in `src/device/meterconnection.cpp`, the serial port naming in
`src/device/transports/serial.cpp` and the console attach in `src/main.cpp`.

Longer-term idea from the original README: split measuring and recording into
a separate background daemon.
