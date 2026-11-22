# Protocols

Every supported meter speaks one of the wire protocols below, each handled by one
decoder class in `src/device/decoders/`. This section collects what is known about
those protocols, where that knowledge comes from, and how it is turned into
regression tests.

## Layout

- `sources/` — the original protocol notes, captures and datasheets, kept
  unchanged. The `*.log` files pair raw frame bytes with the reading the meter
  displayed at that moment, which makes them ground truth for tests.
- `spec/` — one YAML file per protocol with curated test vectors derived from
  those sources. `tests/generate_fixtures.py` turns them into the fixtures under
  `tests/data/decoder/` that `test_decoder` runs.

## Decoders, devices and sources

| Decoder | Protocol (`FrameFormat`) | Devices | Captures in `sources/` | Test vectors |
|---|---|---|---|---|
| `ascii.cpp` | `Metex14`, `PeakTech10`, `Voltcraft14Continuous`, `Voltcraft15Continuous`, `Sigrok` | 48 devices (`Sigrok`: the SCPI bench meters of `sigrok_devices.cpp` - Keysight 34465A, Agilent 3440xA/34410A, HP 34401A, Siglent SDM3055 - read through sigrok-cli's `scpi-dmm` driver): Metex M-3xxx/M-46xx/ME-xx, Voltcraft M-36xx/M-46xx/ME-xx/VC 6xx, PeakTech 4xxx/451, Mastech MAS-34x, Radioshack 22-805/RS22-168A, Digitech QM1350, McVoice M-345pro, Sinometer MAS-343 | `metex-22t.log` (Voltcraft ME-22T), `sigrok-ut61e.log` (Uni-T UT61E through sigrok-cli) | Metex14, Sigrok: yes; PeakTech10: synthetic |
| `brymen_bm52x.cpp` | `BrymenBM52x`, `BrymenBM82x` | Brymen BM525s, BM829s over the BU-86X adapter (unconfirmed) | — (layout from libsigrok's `bm52x.c`, live readings only) | synthetic |
| `brymen_bm86x.cpp` | `BrymenBM86x` | Brymen BM867s, BM869s over the BU-86X adapter (unconfirmed) | — (layout from libsigrok's `bm86x.c`) | synthetic |
| `brymen_bm25x.cpp` | `BrymenBM25x` | Brymen BM250/251/252/257 (unconfirmed) | — (layout from libsigrok's `bm25x.c`) | synthetic |
| `appa_dmm.cpp` | `AppaDmm` | APPA 501/502/506/506B/507/208/208B, BENNING MM 12, Sefram 7351/7352B, KPS DMM9000BT over the optical cable, 9600 8N1 (unconfirmed) | — (frame layout from libsigrok's `appa-dmm` by Martin Eitzenberger, GPL, branch `appadmm-stage` of github.com/Cymaphore/libsigrok) | synthetic |
| `appa_dmm.cpp` | `Appa500` | APPA 503/505, Voltcraft VC-930/VC-950, ISO-TECH IDM503/505, CMT 3503 (the older 500 protocol; unconfirmed) | — (same source, its legacy 500 part) | synthetic |
| `cyrustek_es51922.cpp` | `CyrustekES51922` | Uni-Trend UT61E, Wintex TD2200 | `UT61E.log` | yes |
| `cyrustek_es51962.cpp` | `CyrustekES51962` | PeakTech 3315, Uni-Trend UT70B | `UT70B.log` | yes |
| `cyrustek_es51986.cpp` | `CyrustekES51986` | Iso-Tech IDM 73, Tenma 72-1016, Uni-Trend UT803 | `UT803.log` | yes |
| `do3122.cpp` | `DO3122Continuous` | Duratool DO3122 | — (`duratool.pdf` is the meter's datasheet, no frame description; layout from the decoder) | synthetic |
| `fluke_qm.cpp` | `FlukeQM` | Fluke 87-IV/89-IV/187/189 (9600 Bd) and 287/289 (115200 Bd) over the IR serial adapter (unconfirmed) | `fluke_18x_remote.pdf`, `fluke_28x_remote.pdf` (Fluke's Remote Interface Specifications; no capture) | yes, from the specifications' examples |
| `fluke45.cpp` | `Fluke45` | Fluke 45 bench meter, RS-232 (unconfirmed) | `fluke_45_users_manual.pdf` (chapter 5; no capture) | yes, built from the manual's tables |
| `keysight_u12xx.cpp` | `KeysightU12xx` | Agilent/Keysight U1231A–U1282A over the IR-USB cable (U1173A/B, 9600 8N1; unconfirmed) | — (answer formats from libsigrok's `agilent-dmm`, GPL) | synthetic |
| `gdm703.cpp` | `GDM703Continuous` | Voltcraft GDM 703/704/705 (unconfirmed; chip WENS98A per the [sigrok IC list](https://sigrok.org/wiki/Multimeter_ICs)) | — (layout from Toussaint's CDMM, `ablage/CDMM`) | synthetic |
| `dtm0660.cpp` | `DTM0660` | Generic DTM0660 (4000/6000/8000 count) | — (`dtm0660-brochure.pdf` has no serial frame; layout from the decoder, after libsigrok) | synthetic |
| `benetech_gm1020.cpp` | `BenetechGM1020` | Benetech GM1020 luxmeter, 19200 8N1 (unconfirmed) | — (frame layout from `gm1020.py` of [parametrek/gm1020](https://github.com/parametrek/gm1020), GPL) | synthetic |
| `mastech_ms6514.cpp` | `MastechMS6514` | MASTECH MS6514 two-channel thermometer, 9600 8N1 (unconfirmed) | — (frame layout from libsigrok's `mastech-ms6514`, GPL) | synthetic |
| `m9803r.cpp` | `M9803RContinuous` | ELV M9803R, MASTECH M9803R, McVoice M-980T | — (byte table in the decoder's header, reverse engineered by Toussaint) | synthetic from the byte table, plus frames of a real meter published on the web (one, 4.700 kOhm, corrected for an apparent typo) |
| `qm1537.cpp` | `QM1537Continuous` | Digitek DT4000ZC, Digitech QM1537, PeakTech 3430, TekPower TP4000ZC, Uni-Trend UT61B/C/D, Vichy VC99 | `UT61BCD.log` | yes |
| `rs22812.cpp` | `RS22812Continuous` | Radioshack 22-812 | — (layout from [rs22812](https://github.com/syn-net/rs22812) and the sigrok wiki) | synthetic |
| `uni_t_ut382.cpp` | `UniTUT382` | Uni-Trend UT382 luxmeter over its USB cable, 19200 8N1 (unconfirmed) | — (frame layout from `ut382.py` of [parametrek/gm1020](https://github.com/parametrek/gm1020), GPL) | synthetic |
| `uni_t_ut32x.cpp` | `UniTUT32x` | Uni-Trend UT325 (UT322, Tenma 72-7712/72-7715 unconfirmed) thermometers over the HE2325U cable, 2400 8N1 | `UT325.log` | yes |
| `vc820.cpp` | `VC820Continuous` | Digitek DT-9062/INO2513, Digitech QM1462/QM1538, HoldPeak HP-90EPC, PeakTech 3330, Tenma 72-7745, Uni-Trend UT60A/UT60E, Voltcraft VC 820/840 | `UT60AE.log` | yes |
| `vc820.cpp` | `VC820BLE` | HoldPeak HP-570C-APP, AOPUTTRIVER AP-570C-APP clamps over Bluetooth LE (confirmed by a user on the HP-570C-APP, the same meter as the AP-570C-APP): the FS9721 frame in two notifications of 7 bytes, service 0xFFB0, notify 0xFFB2 | — (GATT layout and segment map from AiCare's app, decompiled, `ablage/holdpeak/`; FS9721-LP3 per [lygte-info](https://lygte-info.dk/review/DMMBTMeter%20BT-570C-APP.html)) | synthetic |
| `vc870.cpp` | `VC870Continuous` | Voltcraft VC 870 | `vc870_protocol.pdf` (frame description); one frame validated on a real VC870 | yes, one real frame (DCV), the rest synthetic from the document's tables; resistance above 400 Ohm and the AC ranges left out, see below |
| `victron_ble.cpp` | `VictronBLE` | Victron SmartShunt, BMV-712 Smart, SmartSolar / BlueSolar MPPT, Phoenix Inverter Smart over Bluetooth LE (SmartShunt 500A/50mV, SmartSolar MPPT 100/20 48V and Phoenix Inverter 12V 500VA verified live) | — (bit layouts and test vectors from [victron-ble](https://github.com/keshavdv/victron-ble); AES-CTR decryption tested in `test_victronble`) | yes |
| `unit_idmm.cpp` | `UniTiDMM` | UNI-T UT60BT over Bluetooth LE GATT (verified live, dial walk against the LCD) | `tmp/ut60bt/walk_all.log` (UT60BT captures); frames and tables from [ble-multimeter](https://github.com/deligent4/ut60bt_multimeter) and [ut61xpy](https://github.com/olegv142/ut61xpy), both in `ablage/` | yes |
| `unit_idmm.cpp` | `UniTUT61Plus` | UNI-T UT61B+/D+/E+ over the UT-D09 USB-HID cable or the UT-D07B Bluetooth adapter, UT161B/D/E over Bluetooth (unconfirmed) | — (synthetic vectors from UNI-T's app range tables via [unit_ut61eplus](https://github.com/ljakob/unit_ut61eplus), matching [ut61xpy](https://github.com/olegv142/ut61xpy)) | yes, synthetic |
| `vc940.cpp` | `VC940Continuous` | Tenma 72-7732, Uni-Trend UT71B/UT71CDE/UT804, Voltcraft VC 920/940/960 | `UT71BCDE.log`, `UT804.log` | yes |
| `raw_capture.h` | `RawCapture` | none: a meter QtDMM does not know yet, frames cut at a pause of 50 ms for the capture window | — | no, it reads nothing |

`FlukeQM` and `Fluke45` are polled protocols with variable-length frames:
QtDMM sends the query once a second (`QM\r`, or `FUNC1?;AUTO?;MOD?;VAL1?\r`
for the 45), the meter answers with status lines (CMD_ACK `0\r`, the 45's
`=>` prompt or a command echo) and one reading line. The reader hands
everything since the previous frame to the decoder (`getPacketLength()` 0),
which takes the last line.

`KeysightU12xx` is polled as well, with two questions in turn: `CONF?` (what
the meter measures) and `FETC?` (the value), terminated `\n\r\n` as libsigrok
does. Only the value line ends a frame; the `CONF?` answer before it, and the
`*<n>` the meter sends when its rotary switch turns, reach the decoder with
it. After a switch no value is shown until `CONF?` has been answered again;
`CONF?` is also asked every tenth time.
`AppaDmm` and `Appa500` are polled with a fixed frame (`55 55 01 00 AB`, read
display; `55 55 00 00 AA`, read all data of the older 500 series) and answer
with a fixed-length frame `55 55 <command> <length> <data> <sum>`; the
checksum is the sum of all bytes before it, so `checkFormat()` checks it. A
display count from `0x700000` on is a word on the display (libsigrok's
`appadmm_wordcode_e`). The overload words `OL`, `OLA`, `OLV` and `OLVA` are an
overload reading (`OL`; the unit from the unit code, else the one the word
names); every other word (`----`, blank, `Er`, `FUSE`, `APO`, ...) gives no
reading. The 500 protocol marks a word with bit 6 (its number is the count,
0 is `Er`) and a blank display with bit 7. APPA's S series and the other Bluetooth-only
models are not read (no serial port).
`UniTUT382` sends only after "USB.1" is set in the meter's menu (long press
MENU); after each power-on it is back at USB.0. A frame is 30 bytes `0x3?`
whose low nibbles give 15 data bytes, CR LF and one more byte (perhaps a
checksum, not checked). `BenetechGM1020` streams after its "live start"
command `3C 01 00 00 00 00 00 3D`, which is sent as the poll request; the live
value is always in lx, the meter's fc setting changes only its display.

`VictronBLE` is not a serial protocol at all: `BleAdvertisementDevice`
decrypts each Bluetooth advertisement (`src/device/victronble.cpp`, AES-128-CTR
with the key from VictronConnect) and hands the decoder one text line - the
readout type and the plaintext as hex - so the decoder stays a pure
bit-field parser that the fixtures can exercise.

Two protocol families transmit every telegram twice in a row (UT803, UT70B, and
the VC940 per the original implementation); their decoders deliberately accept
only the second copy so one measurement yields one reading. The specs record
this as `frames_per_reading: 2`.

## Datasheets and protocol documents in `sources/`

- `ES51962.PDF`, `ES51981.pdf`, `ES51986.pdf` — Cyrustek chip datasheets for
  the corresponding decoders.
- `UT803+command+instructions.pdf` — Uni-Trend's protocol notes for the UT803.
- `vc870_protocol.pdf` — the VC870 protocol description.
- `dtm0660-brochure.pdf`, `duratool.pdf` — material for the DTM0660 and DO3122
  decoders.
- `fluke_18x_remote.pdf`, `fluke_28x_remote.pdf` — Fluke's Remote Interface
  Specifications for the 189/187/89-IV/87-IV and the 289/287; the `QM`
  examples in them are the test vectors of `spec/fluke_qm.yaml`.
- `fluke_45_users_manual.pdf` — the Fluke 45 Users Manual; chapter 5
  (computer interface) is the source of `spec/fluke45.yaml`.
- `ES232.pdf`, `FS9922-DMM4-DS-15_EN.pdf`, `BM250-BM250s-6000-count-digital-multimeters-r1.pdf`
  — chip and meter documents whose mapping to a decoder has not been confirmed
  here; do not assume one without checking.

## Captures for meters without a decoder

`UT109.log`, `UT233.log`, `UT321.log`, `UT325.log`, `UT60FG.log`, `UT70D.log`
and `UT81B.log` document Uni-Trend meters that QtDMM does not support yet.
They are not usable as tests today, but are the starting material for adding
those devices.

`voltcraft14.json` and `voltcraft15.json` run the real Metex captures of
`metex-22t.log` through `Voltcraft14Continuous` and `Voltcraft15Continuous`
(the same frame layout, the second with a line feed after the carriage
return). `peaktech10.json` is synthetic, built from the PeakTech 451 layout
(`#`, six characters of value, four of unit); no capture exists yet.

`sigrok.json` runs `sigrok-ut61e.log` through `Sigrok`: the lines of
sigrok-cli for a Uni-T UT61E, padded the way `SigrokDevice` hands them to the
decoder. sigrok-cli prints `<channel>: <value> <unit> <flags>` with the same
unit spelling for every libsigrok driver, so one meter covers the format. The
capture includes an overload (`inf`) and the continuity mode, which has no unit.

### Left out of the synthetic fixtures

Points where the decoder and the sources do not obviously agree, so no
vector asserts either side until a real meter (or a capture) settles them:

- `vc870.cpp`, resistance ranges 40 kOhm to 40 MOhm: the decoder's factors
  are ten times those of the frequency and capacitance ranges of the same
  table (40 kOhm: `1e1`, dot position 4), and the range table of
  `vc870_protocol.pdf` reads like 40.000 kOhm there. The AC voltage and
  current ranges are scaled one decade differently from the DC ones; that
  fits 4000-count AC ranges, which the document does not say.
- `m9803r.cpp`: the 000.0 Ohm range (mode 4, decimal point 0) puts the point
  after the first digit, not the third as the byte table says. An overload
  shows as `.0L`.

## Provenance

The decoders largely originate with QtDMM's original author, Matthias
Toussaint (<http://www.mtoussaint.de/qtdmm.html>), and were extended from
several external sources. The Uni-Trend protocol logs (`UT*.log`) are the work
of heha, author of the Windows read-out tool DMM.EXE, published at
<https://heha.fwh.is/hs/UNI-T/>; the source of that tool is an independent
implementation of the same protocols and was used to cross-check several of
the specs here. `metex-22t.log` was contributed by Simon Hartmann, as noted in
the file itself. Further protocol knowledge was taken from the sigrok DMM
protocol parsers (<https://sigrok.org/>, `libsigrok/src/dmm/`), which served as
a third reference wherever the logs and heha's tool left room for doubt. The
datasheets are the respective manufacturers' documents. When a decoder looks wrong, consult these sources
first — several apparent bugs (such as the frame deduplication above) turned
out to be deliberate once the notes were read.
