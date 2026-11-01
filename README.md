<!-- GENERATED from docs/ by tests/generate_docs.py - edit the pages in docs/, not this file -->

[![Release](https://img.shields.io/github/v/release/qtdmm/QtDMM?sort=date)](https://github.com/qtdmm/QtDMM/releases)
[![License: GPL-3.0-or-later](https://img.shields.io/github/license/qtdmm/QtDMM)](https://github.com/qtdmm/QtDMM/blob/master/LICENSE)

[![Quality Gate](https://sonarcloud.io/api/project_badges/measure?project=qtdmm_QtDMM&metric=alert_status)](https://sonarcloud.io/dashboard?id=qtdmm_QtDMM)
[![Security](https://sonarcloud.io/api/project_badges/measure?project=qtdmm_QtDMM&metric=security_rating)](https://sonarcloud.io/dashboard?id=qtdmm_QtDMM)
[![Reliability](https://sonarcloud.io/api/project_badges/measure?project=qtdmm_QtDMM&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=qtdmm_QtDMM)
[![Maintainability](https://sonarcloud.io/api/project_badges/measure?project=qtdmm_QtDMM&metric=sqale_rating)](https://sonarcloud.io/dashboard?id=qtdmm_QtDMM)

| Build               | Status |
| ------------------- | ------ |
| Windows             | [![Windows](https://github.com/qtdmm/QtDMM/actions/workflows/cmake-windows-platform.yml/badge.svg?branch=master)](https://github.com/qtdmm/QtDMM/actions/workflows/cmake-windows-platform.yml) |
| macOS               | [![macOS](https://github.com/qtdmm/QtDMM/actions/workflows/cmake-mac-platform.yml/badge.svg?branch=master)](https://github.com/qtdmm/QtDMM/actions/workflows/cmake-mac-platform.yml) |
| FreeBSD             | [![FreeBSD](https://github.com/qtdmm/QtDMM/actions/workflows/cmake-freebsd-platform.yml/badge.svg?branch=master)](https://github.com/qtdmm/QtDMM/actions/workflows/cmake-freebsd-platform.yml) |
| Flatpak             | [![Flatpak](https://github.com/qtdmm/QtDMM/actions/workflows/flatpak.yml/badge.svg?branch=master)](https://github.com/qtdmm/QtDMM/actions/workflows/flatpak.yml) |
| Nix                 | [![Nix](https://github.com/qtdmm/QtDMM/actions/workflows/nix.yml/badge.svg?branch=master)](https://github.com/qtdmm/QtDMM/actions/workflows/nix.yml) |
| Fedora Copr master  | [![Copr master](https://img.shields.io/badge/Fedora-Copr%20master-51A2DA?logo=fedora&logoColor=white)](https://copr.fedorainfracloud.org/coprs/frankb/QtDMM/package/qtdmm-git-master/) |
| Fedora Copr release | [![Copr release](https://copr.fedorainfracloud.org/coprs/frankb/QtDMM/package/qtdmm/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/frankb/QtDMM/package/qtdmm/) |
| openSUSE (OBS)      | [![OBS build result](https://build.opensuse.org/projects/electronics/packages/QtDMM/badge.svg?type=default)](https://build.opensuse.org/package/show/electronics/QtDMM) |

# QtDMM

**QtDMM** is a cross-platform digital multimeter (DMM) readout application
with a built-in, configurable transient recorder. It's especially useful for users
of older multimeters whose original software no longer runs on modern operating
systems.

QtDMM provides a reliable, open-source alternative to outdated or proprietary DMM
software and runs on current **Linux**, **Windows**, **macOS** and **FreeBSD**
systems.

Website with downloads and the online handbook: **<https://qtdmm.de>**,
contact: <hello@qtdmm.de>.
Source code and bug reports: <https://github.com/qtdmm/QtDMM>.

## Features

- Transient Recorder
  - Manual start
  - Scheduled start at a specific time
  - Automatic start triggered when defined thresholds are reached
- Analog meter display with auto-ranging scale; display, meter, graph and readings table arranged automatically or freely
- Alarms: banner, beep, popup, program or recorder start/stop when the reading leaves its range, stays there for a while, overloads or stops coming (see [Alarms](docs/user/alarms.md))
- Export as CSV, Excel (.xlsx) or OpenDocument (.ods) - recorder and readings table
- Export the graph as SVG, PDF, PNG or JPEG - vector output for documents and the web
- Readings table: every value the meter sent, with time, mode and range; copy to a spreadsheet or export (see [Readings table](docs/user/readings-table.md))
- Several meters at once, one window each, recording in sync
- Calculated values across meters (power from voltage and current, ...)
- SCPI server: the meter answers `*IDN?` and `READ?` on a TCP port for lxi-tools, Python or LabVIEW, announced by mDNS (see [SCPI server](docs/user/scpi-server.md))
- Simulated meter (sine, square, noise, discharge curve, ...) for demos and testing
- various connection backends
  - RS232 Serial (USB-Serial and native UART)
  - USB HID-serial support (HOITEK HE2325U & compatible)
  - RFC2217 remote serial, with **qtdmm-bridge** to serve serial and HID meters from a Raspberry Pi, found by mDNS (see [Meters over the network](docs/user/remote-bridge.md))
  - Sigrok support via sigrok-cli application - Keysight, Agilent, HP and Siglent SCPI bench meters are in the model list (see [Bench meters](docs/user/bench-meters.md))
  - Bluetooth LE: the UNI-T UT60BT (and the UT61B+/D+/E+ with the UT-D07B adapter) over a Bluetooth connection, and Victron SmartShunt / BMV-712, SmartSolar / BlueSolar MPPT and Phoenix Inverter Smart over their encrypted "Instant Readout" broadcasts (see [Bluetooth LE](docs/user/bluetooth.md))
- supports lots of DMMs, see [Supported devices](docs/user/supported-devices.md)
  - custom serial settings dialog for yet unknown DMMs

## Key Advantages

- Supports many RS232-based/USB-based multimeters
- Recording via network remote with RFC2217 remote serial support
- Runs several meters side by side and combines them (power from U and I)
- No proprietary software required
- Fully **open source**, licensed under the GPL version 3 or later
- Easy to use and extend

## Documentation

- **[User Guide](docs/user/index.md)** — connecting a meter, the recorder, CSV
  export, troubleshooting, keyboard shortcuts. The same pages are built into
  QtDMM as its handbook (F1).
- **[Protocols](docs/protocols/index.md)** — the wire protocols of the supported
  meters: which decoder handles which device, the original protocol notes and
  captures, and the test vectors derived from them.
- **[Development](docs/dev/index.md)** — building, testing, and extending QtDMM,
  including how to add support for a new meter; the Doxygen API
  documentation is at <https://qtdmm.github.io/QtDMM/api/>.
- **[Contributing](https://github.com/qtdmm/QtDMM/blob/master/CONTRIBUTING.md)** —
  reporting a bug or a meter, capturing a meter's frames, translations and
  pull requests.

This site is published at <https://qtdmm.de/docs/>. Build it locally with
`pip install mkdocs` and `mkdocs serve` from the repository root.

## Supported hardware

Every meter QtDMM can decode, taken from the decoder registrations in
`src/device/decoders/` (this page is generated from them by
`tests/generate_docs.py`). Choosing one of these models on the Multimeter
settings page fills in the serial parameters below; meters not listed can
often be used with *Manual settings* if they speak one of the listed
protocols - the *Chip* column helps: a meter built around the same chip
(named in its manual or on the sigrok wiki) usually speaks the same
protocol. See [Connecting a meter](docs/user/connecting.md).

*Type* is the kind of instrument (handheld or bench multimeter, thermometer,
battery monitor, ...). *Serial* is baud rate, data bits, parity (N/E/O) and stop bits; meters that
are not read over a serial line say how they are reached instead
(*USB-HID (BU-86X)* for the Brymen cable, *Bluetooth LE*, *sigrok-cli*).
*Lines* are the control lines the cable needs driven. *Counts* is the display
resolution. Not every entry has been confirmed on hardware recently; models
marked ¹ were added from chip datasheets and protocol documentation and have not
been tried with QtDMM at all. Models marked ² are sold without a serial
interface and need a hardware modification first (see the notes below the
table). If you can confirm one, or get an unlisted
meter working, please report it on the
[project page](https://github.com/qtdmm/QtDMM/issues).

| Vendor | Model | Type | Chip | Protocol | Serial | Lines | Counts |
|---|---|---|---|---|---|---|---|
| Agilent | 34405A (sigrok) ¹ | Bench multimeter | - | `Sigrok` | sigrok-cli | - | 100000 |
| Agilent | 34410A (sigrok) ¹ | Bench multimeter | - | `Sigrok` | sigrok-cli | - | 1000000 |
| Agilent | 34460A (sigrok) ¹ | Bench multimeter | - | `Sigrok` | sigrok-cli | - | 1000000 |
| Agilent | U1231A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 6000 |
| Agilent | U1232A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 6000 |
| Agilent | U1233A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 6000 |
| Agilent | U1241A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 10000 |
| Agilent | U1241B ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 10000 |
| Agilent | U1242A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 10000 |
| Agilent | U1242B ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 10000 |
| Agilent | U1251A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 50000 |
| Agilent | U1251B ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 50000 |
| Agilent | U1252A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 50000 |
| Agilent | U1252B ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 50000 |
| Agilent | U1253A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 50000 |
| Agilent | U1253B ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 50000 |
| Agilent | U1271A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 30000 |
| Agilent | U1272A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 30000 |
| Agilent | U1273A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 30000 |
| ANENG | AN9002 ¹ | Handheld multimeter | - | `ZotekBLE` | Bluetooth LE | - | 6000 |
| ANENG | AN999S ¹ | Bench multimeter | - | `ZotekBLE` | Bluetooth LE | - | 20000 |
| ANENG | ST207 ¹ | Clamp meter | - | `ZotekBLE` | Bluetooth LE | - | 6000 |
| ANENG | V05B ¹ | Handheld multimeter | - | `ZotekBLE` | Bluetooth LE | - | 6000 |
| AOPUTTRIVER | AP-570C-APP | Clamp meter | FS9721 LP3 | `VC820BLE` | Bluetooth LE | - | 4000 |
| APPA | 155B ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| APPA | 156B ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| APPA | 157B ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| APPA | 158B ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| APPA | 172B ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | 173B ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | 175B ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | 177B ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | 179B ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | 208 ¹ | Bench multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| APPA | 208B (Bluetooth) ¹ | Bench multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| APPA | 208B ¹ | Bench multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| APPA | 501 ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 40000 |
| APPA | 502 ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 40000 |
| APPA | 503 ¹ | Handheld multimeter | - | `Appa500` | 9600 8N1 | - | 50000 |
| APPA | 505 ¹ | Handheld multimeter | - | `Appa500` | 9600 8N1 | - | 50000 |
| APPA | 506 ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| APPA | 506B (Bluetooth) ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| APPA | 506B ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| APPA | 507 ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| APPA | 71 ¹ | Handheld multimeter | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| APPA | 73 ¹ | Handheld multimeter | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| APPA | A17N ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | S0 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | S1 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | S2 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | S3 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | sFlex-10A ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| APPA | sFlex-18A ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Benetech | GM1020 ¹ | Luxmeter | - | `BenetechGM1020` | 19200 8N1 | - | 4000 |
| BENNING | CM 10-1 ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| BENNING | CM 10-PV ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| BENNING | CM 12 ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| BENNING | CM 9-2 ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| BENNING | MM 10-1 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| BENNING | MM 10-PV ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| BENNING | MM 12 (Bluetooth) ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| BENNING | MM 12 ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| Brymen | BM250 ¹ | Handheld multimeter | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM251 ¹ | Handheld multimeter | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM252 ¹ | Handheld multimeter | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM255 ¹ | Handheld multimeter | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM257 ¹ | Handheld multimeter | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM521s ¹ | Handheld multimeter | Brymen BM52x (BU-86X) | `BrymenBM52x` | USB-HID (BU-86X) | - | 6000 |
| Brymen | BM525s ¹ | Handheld multimeter | Brymen BM52x (BU-86X) | `BrymenBM52x` | USB-HID (BU-86X) | - | 6000 |
| Brymen | BM821s ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Brymen | BM822s ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Brymen | BM827s ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Brymen | BM829s ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Brymen | BM867s ¹ | Handheld multimeter | Brymen BM86x (BU-86X) | `BrymenBM86x` | USB-HID (BU-86X) | - | 50000 |
| Brymen | BM869s | Handheld multimeter | Brymen BM86x (BU-86X) | `BrymenBM86x` | USB-HID (BU-86X) | - | 50000 |
| CMT | 3503 ¹ | Handheld multimeter | - | `Appa500` | 9600 8N1 | - | 50000 |
| CO2Meter | CM-INK-1 ¹ | Air quality meter | Holtek | `TfaAirControl` | USB-HID (Holtek) | - | 5000 |
| Digitech | QM1350 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Digitech | QM1462 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Digitech | QM1537 | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Digitech | QM1538 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Digitek | DT-9062 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Digitek | DT-9602R+ ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Digitek | DT4000ZC | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Digitek | INO2513 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Duratool | DO3122 | Handheld multimeter | - | `DO3122Continuous` | 9600 8N1 | - | 4000 |
| Elma | BM525s ¹ | Handheld multimeter | Brymen BM52x (BU-86X) | `BrymenBM52x` | USB-HID (BU-86X) | - | 6000 |
| Elma | BM821s ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Elma | BM829s ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Elma | BM869s ¹ | Handheld multimeter | Brymen BM86x (BU-86X) | `BrymenBM86x` | USB-HID (BU-86X) | - | 50000 |
| ELV | M9803R | Handheld multimeter | - | `M9803RContinuous` | 9600 7E1 | DTR | 4000 |
| EZA | EZ-735 ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Fluke | 187 ¹ | Handheld multimeter | - | `FlukeQM` | 9600 8N1 | - | 50000 |
| Fluke | 189 ¹ | Handheld multimeter | - | `FlukeQM` | 9600 8N1 | - | 50000 |
| Fluke | 287 ¹ | Handheld multimeter | - | `FlukeQM` | 115200 8N1 | - | 50000 |
| Fluke | 289 ¹ | Handheld multimeter | - | `FlukeQM` | 115200 8N1 | - | 50000 |
| Fluke | 45 ¹ | Bench multimeter | - | `Fluke45` | 9600 8N1 | - | 100000 |
| Fluke | 87-IV ¹ | Handheld multimeter | - | `FlukeQM` | 9600 8N1 | - | 20000 |
| Fluke | 89-IV ¹ | Handheld multimeter | - | `FlukeQM` | 9600 8N1 | - | 50000 |
| Generic | DTM0660 4000 count ² | Generic | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 4000 |
| Generic | DTM0660 6000 count ² | Generic | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 6000 |
| Generic | DTM0660 8000 count ² | Generic | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 8000 |
| Greenlee | DM-210A ¹ | Handheld multimeter | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Greenlee | DM-810A ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Greenlee | DM-820A ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Greenlee | DM-830A ¹ | Handheld multimeter | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Greenlee | DM-860A ¹ | Handheld multimeter | Brymen BM86x (BU-86X) | `BrymenBM86x` | USB-HID (BU-86X) | - | 50000 |
| GW Instek | GDM-397 ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| HoldPeak | HP-570C-APP | Clamp meter | FS9721 LP3 | `VC820BLE` | Bluetooth LE | - | 4000 |
| HoldPeak | HP-90EPC | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| HP | 34401A (sigrok) ¹ | Bench multimeter | - | `Sigrok` | sigrok-cli | - | 1000000 |
| Iso-Tech | IDM 503 ¹ | Handheld multimeter | - | `Appa500` | 9600 8N1 | - | 50000 |
| Iso-Tech | IDM 505 ¹ | Handheld multimeter | - | `Appa500` | 9600 8N1 | - | 50000 |
| Iso-Tech | IDM 73 | Handheld multimeter | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| Keysight | 34465A (sigrok) ¹ | Bench multimeter | - | `Sigrok` | sigrok-cli | - | 1000000 |
| Keysight | U1241C ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 10000 |
| Keysight | U1242C ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 10000 |
| Keysight | U1273AX ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 30000 |
| Keysight | U1281A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 60000 |
| Keysight | U1282A ¹ | Handheld multimeter | - | `KeysightU12xx` | 9600 8N1 | - | 60000 |
| KPS | DCM7000BT ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| KPS | DCM8000BT ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| KPS | DMM3500BT ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| KPS | DMM9000BT (Bluetooth) ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| KPS | DMM9000BT ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| MASTECH | M9803R | Handheld multimeter | - | `M9803RContinuous` | 9600 7E1 | DTR | 4000 |
| MASTECH | MAS-343 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| MASTECH | MAS-345 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| MASTECH | MS6514 ¹ | Thermometer | - | `MastechMS6514` | 9600 8N1 | - | 9999 |
| MASTECH | MS8250B ¹ | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| McVoice | M-345pro | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| McVoice | M-980T | Handheld multimeter | - | `M9803RContinuous` | 9600 7N1 | DTR | 4000 |
| Megger | DCM1500S ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Megger | DPM1000 ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Metex | M-3360D ¹ | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3640D ¹ | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3660D | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3830D | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3840D | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3850D | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3850M | Handheld multimeter | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 4000 |
| Metex | M-3870D | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N1 | DTR | 4000 |
| Metex | M-4650C | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 20000 |
| Metex | ME-11 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Metex | ME-21 ¹ | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Metex | ME-22 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 2400 7N2 | DTR | 4000 |
| Metex | ME-32 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Metex | ME-42 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Metex | Universal System 9140 ¹ | Bench multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | universal system 9160 | Bench multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metravi | PRO Solar-1 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Metrel | MD9015 ¹ | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| OWON | B33 ¹ | Handheld multimeter | - | `OwonBLE` | Bluetooth LE | - | 4000 |
| OWON | B33T+ ¹ | Handheld multimeter | - | `OwonBLE` | Bluetooth LE | - | 4000 |
| OWON | B35T+ ¹ | Handheld multimeter | - | `OwonBLE` | Bluetooth LE | - | 6000 |
| OWON | B41T+ ¹ | Handheld multimeter | - | `OwonBLE` | Bluetooth LE | - | 22000 |
| OWON | CM2100B | Clamp meter | - | `OwonBLE` | Bluetooth LE | - | 20000 |
| OWON | CMS061 ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 20000 |
| OWON | CMS101 ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 20000 |
| OWON | OW16B ¹ | Handheld multimeter | - | `OwonBLE` | Bluetooth LE | - | 6000 |
| OWON | OW18B ¹ | Handheld multimeter | - | `OwonBLE` | Bluetooth LE | - | 6000 |
| OWON | OW18E ¹ | Handheld multimeter | - | `OwonBLE` | Bluetooth LE | - | 20000 |
| OWON | OW65B ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 6000 |
| OWON | OW67B ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 60000 |
| OWON | OW69B ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 60000 |
| PCE | PCE-DM32 ¹ | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| PeakTech | 2025 ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| PeakTech | 3315 | Handheld multimeter | ES51962 | `CyrustekES51962` | 2400 7N1 | DTR | 4000 |
| PeakTech | 3330 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| PeakTech | 3415 ¹ | Handheld multimeter | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 6000 |
| PeakTech | 3430 | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 19200 7N2 | DTR | 4000 |
| PeakTech | 4010 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 4000 |
| PeakTech | 4015A | Bench multimeter | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 100000 |
| PeakTech | 4360 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| PeakTech | 4390 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 4000 |
| PeakTech | 451 | Handheld multimeter | - | `PeakTech10` | 600 7N2 | DTR | 4000 |
| Pro'sKit | MT-1820 ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Radioshack | 22-805 DMM | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Radioshack | 22-812 | Handheld multimeter | - | `RS22812Continuous` | 4800 8N1 | DTR | 4000 |
| Radioshack | RS22-168A | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| RS PRO | 155B ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| RS PRO | 156B ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| RS PRO | 157B ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| RS PRO | 158B ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| RS PRO | S1 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| RS PRO | S2 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| RS PRO | S3 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Sanwa | PC7000 ¹ | Handheld multimeter | Brymen BM86x (BU-86X) | `BrymenBM86x` | USB-HID (BU-86X) | - | 50000 |
| Sefram | 7220 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Sefram | 7221 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Sefram | 7223 ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Sefram | 7351 ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| Sefram | 7352B (Bluetooth) ¹ | Handheld multimeter | - | `AppaDmm` | Bluetooth LE | - | 50000 |
| Sefram | 7352B ¹ | Handheld multimeter | - | `AppaDmm` | 9600 8N1 | - | 50000 |
| Sefram | MW3516BF ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Sefram | MW3526BF ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Sefram | MW3536BF ¹ | Clamp meter | - | `AppaDmm` | Bluetooth LE | - | 6000 |
| Siglent | SDM3055 (sigrok) ¹ | Bench multimeter | - | `Sigrok` | sigrok-cli | - | 200000 |
| sigrok | SCPI DMM (any scpi-dmm model) ¹ | Generic | - | `Sigrok` | sigrok-cli | - | 100000 |
| Sinometer | MAS-343 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| SparkFun | 70C ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Tecpel | DMM-8061 ¹ | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| TekPower | TP4000ZC | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Tenma | 72-1016 | Bench multimeter | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| Tenma | 72-6870 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Tenma | 72-7712 ¹ | Thermometer | - | `UniTUT32x` | 2400 8N1 | - | 9999 |
| Tenma | 72-7715 ¹ | Thermometer | - | `UniTUT32x` | 2400 8N1 | - | 9999 |
| Tenma | 72-7730 ¹ | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 20000 |
| Tenma | 72-7732 | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Tenma | 72-7745 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Tenma | 72-9380A ¹ | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| TFA Dostmann | AIRCO2NTROL Coach | Air quality meter | Holtek | `TfaAirControl` | USB-HID (Holtek) | - | 5000 |
| TFA Dostmann | AIRCO2NTROL Mini ¹ | Air quality meter | Holtek | `TfaAirControl` | USB-HID (Holtek) | - | 5000 |
| Uni-Trend | UT161B ¹ | Handheld multimeter | - | `UniTUT61Plus` | Bluetooth LE | - | 6000 |
| Uni-Trend | UT161D ¹ | Handheld multimeter | - | `UniTUT61Plus` | Bluetooth LE | - | 6000 |
| Uni-Trend | UT161E ¹ | Handheld multimeter | - | `UniTUT61Plus` | Bluetooth LE | - | 22000 |
| Uni-Trend | UT181A (UT-D07B Bluetooth) ¹ | Handheld multimeter | - | `UniTUT181A` | Bluetooth LE | - | 60000 |
| Uni-Trend | UT181A ¹ | Handheld multimeter | - | `UniTUT181A` | 9600 8N1 | - | 60000 |
| Uni-Trend | UT202BT | Clamp meter | - | `UniTUT202BT` | Bluetooth LE | - | 10000 |
| Uni-Trend | UT217A ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 20000 |
| Uni-Trend | UT217B ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 20000 |
| Uni-Trend | UT30A ¹ | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT30E ¹ | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT321 ¹ | Thermometer | - | `UniTUT32x` | 2400 8N1 | - | 9999 |
| Uni-Trend | UT322 ¹ | Thermometer | - | `UniTUT32x` | 2400 8N1 | - | 9999 |
| Uni-Trend | UT325 ¹ | Thermometer | - | `UniTUT32x` | 2400 8N1 | - | 9999 |
| Uni-Trend | UT333BT | Air quality meter | - | `UniTiENV` | Bluetooth LE | - | 1000 |
| Uni-Trend | UT353BT | Sound level meter | - | `UniTiENV` | Bluetooth LE | - | 10000 |
| Uni-Trend | UT363BT ¹ | Air quality meter | - | `UniTiENV` | Bluetooth LE | - | 10000 |
| Uni-Trend | UT382 ¹ | Luxmeter | - | `UniTUT382` | 19200 8N1 | - | 2000 |
| Uni-Trend | UT383BT ¹ | Luxmeter | - | `UniTiENV` | Bluetooth LE | - | 100000 |
| Uni-Trend | UT60A | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT60BT | Handheld multimeter | - | `UniTiDMM` | Bluetooth LE | - | 10000 |
| Uni-Trend | UT60D ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Uni-Trend | UT60E | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT61A ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Uni-Trend | UT61B | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT61B+ (UT-D07B Bluetooth) ¹ | Handheld multimeter | - | `UniTUT61Plus` | Bluetooth LE | - | 6000 |
| Uni-Trend | UT61B+ ¹ | Handheld multimeter | - | `UniTUT61Plus` | 9600 8N1 | - | 6000 |
| Uni-Trend | UT61C | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Uni-Trend | UT61D | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Uni-Trend | UT61D+ (UT-D07B Bluetooth) ¹ | Handheld multimeter | - | `UniTUT61Plus` | Bluetooth LE | - | 6000 |
| Uni-Trend | UT61D+ ¹ | Handheld multimeter | - | `UniTUT61Plus` | 9600 8N1 | - | 6000 |
| Uni-Trend | UT61E | Handheld multimeter | ES51922 | `CyrustekES51922` | 19200 7O1 | DTR | 22000 |
| Uni-Trend | UT61E+ (UT-D07B Bluetooth) ¹ | Handheld multimeter | - | `UniTUT61Plus` | Bluetooth LE | - | 22000 |
| Uni-Trend | UT61E+ ¹ | Handheld multimeter | - | `UniTUT61Plus` | 9600 8N1 | - | 22000 |
| Uni-Trend | UT70B | Handheld multimeter | ES51962 | `CyrustekES51962` | 2400 7N1 | DTR | 4000 |
| Uni-Trend | UT71A ¹ | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 20000 |
| Uni-Trend | UT71B | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 200000 |
| Uni-Trend | UT71CDE | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Uni-Trend | UT803 | Bench multimeter | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| Uni-Trend | UT804 | Bench multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| V&A | VA18B ¹ | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| V&A | VA40B ¹ | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Velleman | DVM4100 ¹ | Handheld multimeter | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 6000 |
| Vichy | VC99 ² | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Victor | 70C ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Victor | 86C ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Victron | BlueSolar MPPT ¹ | Solar charge controller | - | `VictronBLE` | Bluetooth LE | - | 1000 |
| Victron | BMV-712 Smart ¹ | Battery monitor | - | `VictronBLE` | Bluetooth LE | - | 6000 |
| Victron | Phoenix Inverter Smart | Inverter | - | `VictronBLE` | Bluetooth LE | - | 6000 |
| Victron | SmartShunt | Battery monitor | - | `VictronBLE` | Bluetooth LE | - | 6000 |
| Victron | SmartSolar MPPT | Solar charge controller | - | `VictronBLE` | Bluetooth LE | - | 1000 |
| Voltcraft | GDM 703 ¹ | Bench multimeter | WENS98A | `GDM703Continuous` | 9600 8N1 | DTR | 4000 |
| Voltcraft | GDM 704 ¹ | Bench multimeter | WENS98A | `GDM703Continuous` | 9600 8N1 | DTR | 4000 |
| Voltcraft | GDM 705 ¹ | Bench multimeter | WENS98A | `GDM703Continuous` | 9600 8N1 | DTR | 4000 |
| Voltcraft | M-3610D | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Voltcraft | M-3650D | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Voltcraft | M-3860 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 20000 |
| Voltcraft | M-4650CR | Handheld multimeter | - | `Voltcraft14Continuous` | 1200 7N2 | DTR | 20000 |
| Voltcraft | M-4660 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 50000 |
| Voltcraft | M-4660A | Handheld multimeter | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 50000 |
| Voltcraft | M-4660M | Handheld multimeter | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 50000 |
| Voltcraft | ME-11 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Voltcraft | ME-22T | Handheld multimeter | Metex KS57C2016 | `Metex14` | 2400 7N2 | DTR | 4000 |
| Voltcraft | ME-32 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Voltcraft | ME-42 | Handheld multimeter | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Voltcraft | MS-9140 ¹ | Bench multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Voltcraft | MS-9160 ¹ | Bench multimeter | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Voltcraft | MXD-4660A | Handheld multimeter | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 50000 |
| Voltcraft | VC 630 | Bench multimeter | - | `Voltcraft14Continuous` | 4800 7N1 | DTR | 50000 |
| Voltcraft | VC 635 | Bench multimeter | - | `Voltcraft15Continuous` | 2400 7N1 | DTR | 50000 |
| Voltcraft | VC 650 | Bench multimeter | - | `Voltcraft14Continuous` | 4800 7N1 | DTR | 50000 |
| Voltcraft | VC 655 | Bench multimeter | - | `Voltcraft15Continuous` | 2400 7N1 | DTR | 50000 |
| Voltcraft | VC 670 | Bench multimeter | - | `Voltcraft14Continuous` | 4800 7N1 | DTR | 50000 |
| Voltcraft | VC 820 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Voltcraft | VC 830 ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Voltcraft | VC 840 | Handheld multimeter | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Voltcraft | VC 850 ¹ | Handheld multimeter | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Voltcraft | VC 870 | Handheld multimeter | - | `VC870Continuous` | 9600 8N1 | DTR | 40000 |
| Voltcraft | VC 920 | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Voltcraft | VC 930 ¹ | Handheld multimeter | - | `Appa500` | 9600 8N1 | - | 50000 |
| Voltcraft | VC 940 | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Voltcraft | VC 950 ¹ | Handheld multimeter | - | `Appa500` | 9600 8N1 | - | 50000 |
| Voltcraft | VC 960 | Handheld multimeter | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Voltcraft | VC-880 ¹ | Handheld multimeter | ES51966A | `VoltcraftVC880` | 9600 8N1 | - | 40000 |
| Voltcraft | VC-890 ¹ | Handheld multimeter | ES51997P | `VoltcraftVC890` | 9600 8N1 | - | 60000 |
| Voltcraft | VC650BT ¹ | Handheld multimeter | ES51966A | `VoltcraftVC880` | 9600 8N1 | - | 40000 |
| Voltcraft | VC871 ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 60000 |
| Voltcraft | VC891 ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 60000 |
| Voltcraft | VC915 ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 20000 |
| Voltcraft | VC925 PV ¹ | Handheld multimeter | - | `OwonBLE15` | Bluetooth LE | - | 20000 |
| Wintex | TD2200 | Handheld multimeter | ES51922 | `CyrustekES51922` | 19200 7N1 | DTR | 22000 |
| ZOTEK | ZT-300AB ¹ | Handheld multimeter | - | `ZotekBLE` | Bluetooth LE | - | 6000 |
| ZOTEK | ZT-5566SE ¹ | Bench multimeter | - | `ZotekBLE` | Bluetooth LE | - | 20000 |
| ZOTEK | ZT-5B ¹ | Handheld multimeter | - | `ZotekBLE` | Bluetooth LE | - | 6000 |
| ZOTEK | ZT-5BQ ¹ | Clamp meter | - | `ZotekBLE` | Bluetooth LE | - | 6000 |

¹ settings taken from the chip, not yet confirmed on hardware with QtDMM.

² needs a hardware modification - the meter has no interface as sold:

- **Generic DTM0660 4000 count, DTM0660 6000 count, DTM0660 8000 count**: the DTM0660 chip has a UART, but meters built on it (Victor VC921 and similar) ship without an interface: an IR LED with a 100 Ω resistor goes on the chip's TX pin and the RS232 bit in the 24C02 EEPROM has to be set - [eevblog: hacking the Victor VC-921](https://www.eevblog.com/forum/testgear/hacking-the-victor-vc-921/). PeakTech 3415 and Velleman DVM4100 use the same chip but come with a cable and need no change.
- **Vichy VC99**: the FS9922-DMM4 chip sends the display at 2400 baud on an unused pin; a wire or IR LED has to be soldered to it and *REL* held to start the output - [Hackaday](https://hackaday.com/2010/11/30/unlocking-rs232-serial-comm-on-a-multimeter/), [David Pilling](https://www.davidpilling.com/wiki/index.php/VC99).

307 devices across 55 vendors.

By type: Handheld multimeter 230, Bench multimeter 28, Clamp meter 25, Thermometer 6, Air quality meter 5, Generic 4, Luxmeter 3, Battery monitor 2, Solar charge controller 2, Inverter 1, Sound level meter 1.

## Command line

```
qtdmm [options]
```

| Option | Meaning |
|---|---|
| `--config-id <id>` | Use the named configuration instead of `default`. Each id has its own settings file: its window, its views and the devices it connects. |
| `--connect <id>` | Connect the entry `<id>` of *My devices* at the start, after those connected last time; repeat it for more. Its readings reach the SCPI server (`INSTrument:SELect`), and it records with the others; see [SCPI server](docs/user/scpi-server.md#qtdmms-own-commands). The ids are listed by `QTDMM:DEVice:CATalog?`. `--also <id>` (before 26.4) does the same. |
| `--config-dir <dir>` | Directory for the configuration files (default: the platform's user config location). |
| `--vxi11-rpc auto\|system\|own` | For this run, where VXI-11 gets its portmapper from: the system's rpcbind, an own one on port 111, or automatically (see [SCPI server](docs/user/scpi-server.md#vxi-11)). Replaces the setting without changing it. |
| `--vxi11-portmapper-port <port>` | For tests and development: VXI-11 with an own portmapper on this port instead of 111 (0: any free one), for this run. VISA libraries and lxi-tools ask port 111 and do not find it there; a client of your own (or a test) asks this port. |
| `--debug` | Print every frame received from the meter as hex to the console. Useful when a meter is not decoded correctly — include this output in a bug report. |
| `-h`, `--help` | Show the options. |
| `-v`, `--version` | Show the version. |

### Several meters at once

One QtDMM reads several meters, all alike. *Connect* in the context menu
of an entry of [My devices](docs/user/my-devices.md) (or dragging it onto
**Connected devices**, or *Connect* at the end of the assistant *Add
device*) connects the device in the same QtDMM, with a front panel of its
own; `--connect <id>` does the same at the start, and over SCPI
`QTDMM:DEVice:OPEN`. The node **Connected devices** of the sidebar (F9)
lists them with their current reading, the active device in bold; *Close*
in the context menu closes one (see [My devices](docs/user/my-devices.md#several-meters-front-panels-and-the-active-device)). They record together, on one
time base, and a [calculated value](docs/user/calculated-values.md) reads them by
their names.

`--config-id` keeps a set of settings of its own (a file of its own).
A second QtDMM started with another id runs on its own, without a link to
the first; started with the same id it says that this one runs already.
The settings files of the instances of QtDMM 26.3 and earlier stay where
they are.

### Debug output

`--debug` writes each received frame as a line of hex bytes, e.g.

```
30 30 30 30 30 31 3B 30 30 30 3A 30 0D 0A
```

These lines are exactly what the decoder test fixtures are made of, so a short
capture together with what the meter displayed at the time is the most useful
thing to attach when reporting a decoding problem.

## Licensing
- **0.9.5 until today tuxmaster and various contributors, see AUTHORS file**
- **0.9.3 and before (c) 2001-2016 M.Toussaint <qtdmm@mtoussaint.de>**

QtDMM 0.9.0 and beyond is distributed under the GNU General Public License,
version 3 or (at your option) any later version. Versions before 0.9.0 are
licensed under the GNU GPL version 2 or later.
