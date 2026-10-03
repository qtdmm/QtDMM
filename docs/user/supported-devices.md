# Supported devices

Every meter QtDMM can decode, taken from the decoder registrations in
`src/device/decoders/` (this page is generated from them by
`tests/generate_docs.py`). Choosing one of these models on the Multimeter
settings page fills in the serial parameters below; meters not listed can
often be used with *Manual settings* if they speak one of the listed
protocols - the *Chip* column helps: a meter built around the same chip
(named in its manual or on the sigrok wiki) usually speaks the same
protocol. See [Connecting a meter](connecting.md).

*Serial* is baud rate, data bits, parity (N/E/O) and stop bits; meters that
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

| Vendor | Model | Chip | Protocol | Serial | Lines | Counts |
|---|---|---|---|---|---|---|
| Agilent | 34405A (sigrok) ¹ | - | `Sigrok` | sigrok-cli | - | 100000 |
| Agilent | 34410A (sigrok) ¹ | - | `Sigrok` | sigrok-cli | - | 1000000 |
| Agilent | 34460A (sigrok) ¹ | - | `Sigrok` | sigrok-cli | - | 1000000 |
| APPA | 71 ¹ | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| APPA | 73 ¹ | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| Brymen | BM250 ¹ | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM251 ¹ | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM252 ¹ | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM257 ¹ | Brymen BM25x | `BrymenBM25x` | 9600 8N1 | DTR RTS | 6000 |
| Brymen | BM525s ¹ | Brymen BM52x (BU-86X) | `BrymenBM52x` | USB-HID (BU-86X) | - | 6000 |
| Brymen | BM829s ¹ | Brymen BM82x (BU-86X) | `BrymenBM82x` | USB-HID (BU-86X) | - | 6000 |
| Brymen | BM867s ¹ | Brymen BM86x (BU-86X) | `BrymenBM86x` | USB-HID (BU-86X) | - | 50000 |
| Brymen | BM869s | Brymen BM86x (BU-86X) | `BrymenBM86x` | USB-HID (BU-86X) | - | 50000 |
| Digitech | QM1350 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Digitech | QM1462 | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Digitech | QM1537 | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Digitech | QM1538 | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Digitek | DT-9062 | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Digitek | DT-9602R+ ¹ | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Digitek | DT4000ZC | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Digitek | INO2513 | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Duratool | DO3122 | - | `DO3122Continuous` | 9600 8N1 | - | 4000 |
| ELV | M9803R | - | `M9803RContinuous` | 9600 7E1 | DTR | 4000 |
| Fluke | 187 ¹ | - | `FlukeQM` | 9600 8N1 | - | 50000 |
| Fluke | 189 ¹ | - | `FlukeQM` | 9600 8N1 | - | 50000 |
| Fluke | 287 ¹ | - | `FlukeQM` | 115200 8N1 | - | 50000 |
| Fluke | 289 ¹ | - | `FlukeQM` | 115200 8N1 | - | 50000 |
| Fluke | 45 ¹ | - | `Fluke45` | 9600 8N1 | - | 100000 |
| Fluke | 87-IV ¹ | - | `FlukeQM` | 9600 8N1 | - | 20000 |
| Fluke | 89-IV ¹ | - | `FlukeQM` | 9600 8N1 | - | 50000 |
| Generic | DTM0660 4000 count ² | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 4000 |
| Generic | DTM0660 6000 count ² | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 6000 |
| Generic | DTM0660 8000 count ² | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 8000 |
| GW Instek | GDM-397 ¹ | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| HoldPeak | HP-90EPC | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| HP | 34401A (sigrok) ¹ | - | `Sigrok` | sigrok-cli | - | 1000000 |
| Iso-Tech | IDM 73 | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| Keysight | 34465A (sigrok) ¹ | - | `Sigrok` | sigrok-cli | - | 1000000 |
| MASTECH | M9803R | - | `M9803RContinuous` | 9600 7E1 | DTR | 4000 |
| MASTECH | MAS-343 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| MASTECH | MAS-345 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| MASTECH | MS8250B ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| McVoice | M-345pro | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| McVoice | M-980T | - | `M9803RContinuous` | 9600 7N1 | DTR | 4000 |
| Metex | M-3360D ¹ | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3640D ¹ | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3660D | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3830D | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3840D | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3850D | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | M-3850M | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 4000 |
| Metex | M-3870D | Metex KS57C2016 | `Metex14` | 1200 7N1 | DTR | 4000 |
| Metex | M-4650C | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 20000 |
| Metex | ME-11 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Metex | ME-21 ¹ | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Metex | ME-22 | Metex KS57C2016 | `Metex14` | 2400 7N2 | DTR | 4000 |
| Metex | ME-32 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Metex | ME-42 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Metex | Universal System 9140 ¹ | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metex | universal system 9160 | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Metrel | MD9015 ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| PCE | PCE-DM32 ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| PeakTech | 2025 ¹ | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| PeakTech | 3315 | ES51962 | `CyrustekES51962` | 2400 7N1 | DTR | 4000 |
| PeakTech | 3330 | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| PeakTech | 3415 ¹ | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 6000 |
| PeakTech | 3430 | FS9922-DMM4 | `QM1537Continuous` | 19200 7N2 | DTR | 4000 |
| PeakTech | 4010 | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 4000 |
| PeakTech | 4015A | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 100000 |
| PeakTech | 4360 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| PeakTech | 4390 | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 4000 |
| PeakTech | 451 | - | `PeakTech10` | 600 7N2 | DTR | 4000 |
| Pro'sKit | MT-1820 ¹ | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Protek | 504 ¹ | FS9721 LP3 | `VC820Continuous` | 1200 7N2 | DTR RTS | 4000 |
| Protek | 505 ¹ | FS9721 LP3 | `VC820Continuous` | 1200 7N2 | DTR RTS | 4000 |
| Protek | 506 ¹ | FS9721 LP3 | `VC820Continuous` | 1200 7N2 | DTR RTS | 4000 |
| Radioshack | 22-805 DMM | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Radioshack | 22-812 | - | `RS22812Continuous` | 4800 8N1 | DTR | 4000 |
| Radioshack | RS22-168A | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Siglent | SDM3055 (sigrok) ¹ | - | `Sigrok` | sigrok-cli | - | 200000 |
| sigrok | SCPI DMM (any scpi-dmm model) ¹ | - | `Sigrok` | sigrok-cli | - | 100000 |
| Sinometer | MAS-343 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| SparkFun | 70C ¹ | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Tecpel | DMM-8061 ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| TekPower | TP4000ZC | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Tenma | 72-1016 | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| Tenma | 72-6870 ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Tenma | 72-7730 ¹ | - | `VC940Continuous` | 2400 7O1 | DTR | 20000 |
| Tenma | 72-7732 | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Tenma | 72-7745 | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Tenma | 72-9380A ¹ | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| TFA Dostmann | AIRCO2NTROL Mini ¹ | Holtek | `TfaAirControl` | USB-HID (Holtek) | - | 5000 |
| Uni-Trend | UT161B ¹ | - | `UniTUT61Plus` | Bluetooth LE | - | 6000 |
| Uni-Trend | UT161D ¹ | - | `UniTUT61Plus` | Bluetooth LE | - | 6000 |
| Uni-Trend | UT161E ¹ | - | `UniTUT61Plus` | Bluetooth LE | - | 22000 |
| Uni-Trend | UT30A ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT30E ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT60A | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT60BT | - | `UniTiDMM` | Bluetooth LE | - | 10000 |
| Uni-Trend | UT60D ¹ | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Uni-Trend | UT60E | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT61A ¹ | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Uni-Trend | UT61B | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 4000 |
| Uni-Trend | UT61B+ (UT-D07B Bluetooth) ¹ | - | `UniTUT61Plus` | Bluetooth LE | - | 6000 |
| Uni-Trend | UT61B+ ¹ | - | `UniTUT61Plus` | 9600 8N1 | - | 6000 |
| Uni-Trend | UT61C | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Uni-Trend | UT61D | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Uni-Trend | UT61D+ (UT-D07B Bluetooth) ¹ | - | `UniTUT61Plus` | Bluetooth LE | - | 6000 |
| Uni-Trend | UT61D+ ¹ | - | `UniTUT61Plus` | 9600 8N1 | - | 6000 |
| Uni-Trend | UT61E | ES51922 | `CyrustekES51922` | 19200 7O1 | DTR | 22000 |
| Uni-Trend | UT61E+ (UT-D07B Bluetooth) ¹ | - | `UniTUT61Plus` | Bluetooth LE | - | 22000 |
| Uni-Trend | UT61E+ ¹ | - | `UniTUT61Plus` | 9600 8N1 | - | 22000 |
| Uni-Trend | UT70B | ES51962 | `CyrustekES51962` | 2400 7N1 | DTR | 4000 |
| Uni-Trend | UT71A ¹ | - | `VC940Continuous` | 2400 7O1 | DTR | 20000 |
| Uni-Trend | UT71B | - | `VC940Continuous` | 2400 7O1 | DTR | 200000 |
| Uni-Trend | UT71CDE | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Uni-Trend | UT803 | ES51986 | `CyrustekES51986` | 19200 7O1 | DTR | 6000 |
| Uni-Trend | UT804 | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| V&A | VA18B ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| V&A | VA40B ¹ | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Velleman | DVM4100 ¹ | DTM0660 | `DTM0660` | 2400 8N1 | DTR | 6000 |
| Vichy | VC99 ² | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Victron | BlueSolar MPPT ¹ | - | `VictronBLE` | Bluetooth LE | - | 1000 |
| Victron | BMV-712 Smart ¹ | - | `VictronBLE` | Bluetooth LE | - | 6000 |
| Victron | Phoenix Inverter Smart | - | `VictronBLE` | Bluetooth LE | - | 6000 |
| Victron | SmartShunt | - | `VictronBLE` | Bluetooth LE | - | 6000 |
| Victron | SmartSolar MPPT | - | `VictronBLE` | Bluetooth LE | - | 1000 |
| Voltcraft | GDM 703 ¹ | WENS98A | `GDM703Continuous` | 9600 8N1 | DTR | 4000 |
| Voltcraft | GDM 704 ¹ | WENS98A | `GDM703Continuous` | 9600 8N1 | DTR | 4000 |
| Voltcraft | GDM 705 ¹ | WENS98A | `GDM703Continuous` | 9600 8N1 | DTR | 4000 |
| Voltcraft | M-3610D | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Voltcraft | M-3650D | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Voltcraft | M-3860 | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 20000 |
| Voltcraft | M-4650CR | - | `Voltcraft14Continuous` | 1200 7N2 | DTR | 20000 |
| Voltcraft | M-4660 | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 50000 |
| Voltcraft | M-4660A | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 50000 |
| Voltcraft | M-4660M | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 50000 |
| Voltcraft | ME-11 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Voltcraft | ME-22T | Metex KS57C2016 | `Metex14` | 2400 7N2 | DTR | 4000 |
| Voltcraft | ME-32 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Voltcraft | ME-42 | Metex KS57C2016 | `Metex14` | 600 7N2 | DTR | 4000 |
| Voltcraft | MS-9140 ¹ | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Voltcraft | MS-9160 ¹ | Metex KS57C2016 | `Metex14` | 1200 7N2 | DTR | 4000 |
| Voltcraft | MXD-4660A | Metex KS57C2016 | `Metex14` | 9600 7N2 | DTR | 50000 |
| Voltcraft | VC 630 | - | `Voltcraft14Continuous` | 4800 7N1 | DTR | 50000 |
| Voltcraft | VC 635 | - | `Voltcraft15Continuous` | 2400 7N1 | DTR | 50000 |
| Voltcraft | VC 650 | - | `Voltcraft14Continuous` | 4800 7N1 | DTR | 50000 |
| Voltcraft | VC 655 | - | `Voltcraft15Continuous` | 2400 7N1 | DTR | 50000 |
| Voltcraft | VC 670 | - | `Voltcraft14Continuous` | 4800 7N1 | DTR | 50000 |
| Voltcraft | VC 820 | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Voltcraft | VC 830 ¹ | FS9922-DMM4 | `QM1537Continuous` | 2400 8N1 | DTR | 6000 |
| Voltcraft | VC 840 | FS9721 LP3 | `VC820Continuous` | 2400 8N1 | DTR | 4000 |
| Voltcraft | VC 870 | - | `VC870Continuous` | 9600 8N1 | DTR | 40000 |
| Voltcraft | VC 920 | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Voltcraft | VC 940 | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Voltcraft | VC 960 | - | `VC940Continuous` | 2400 7O1 | DTR | 40000 |
| Wintex | TD2200 | ES51922 | `CyrustekES51922` | 19200 7N1 | DTR | 22000 |

¹ settings taken from the chip, not yet confirmed on hardware with QtDMM.

² needs a hardware modification - the meter has no interface as sold:

- **Generic DTM0660 4000 count, DTM0660 6000 count, DTM0660 8000 count**: the DTM0660 chip has a UART, but meters built on it (Victor VC921 and similar) ship without an interface: an IR LED with a 100 Ω resistor goes on the chip's TX pin and the RS232 bit in the 24C02 EEPROM has to be set - [eevblog: hacking the Victor VC-921](https://www.eevblog.com/forum/testgear/hacking-the-victor-vc-921/). PeakTech 3415 and Velleman DVM4100 use the same chip but come with a cable and need no change.
- **Vichy VC99**: the FS9922-DMM4 chip sends the display at 2400 baud on an unused pin; a wire or IR LED has to be soldered to it and *REL* held to start the output - [Hackaday](https://hackaday.com/2010/11/30/unlocking-rs232-serial-comm-on-a-multimeter/), [David Pilling](https://www.davidpilling.com/wiki/index.php/VC99).

157 devices across 38 vendors.
