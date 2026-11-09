// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/unit_idmm.h"

#include <QRegularExpression>

#include "core/sampletypes.h"

static const bool registered = []() {
  // Bluetooth LE: no baud rate; the address is chosen on the Multimeter page
  DmmDecoder::addConfig({"Uni-Trend", "UT60BT", "", 0, FrameFormat::UniTiDMM, 8, 1, 1, 0, 10000, 0, 0, 0});
  // UT161 series: Bluetooth built in like the UT60BT, ranges of the UT61+
  // (vendor tables funOl_UT161B/D/E); not tried with QtDMM
  DmmDecoder::addConfig({"Uni-Trend", "UT161B *", "", 0, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT161D *", "", 0, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT161E *", "", 0, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 22000, 0, 0, 0});
  // and over the UT-D09 USB cable as the UT61+ (dmm-tools); the UT161E's
  // frames there are real (dmm-tools issue #7), now vectors of the spec
  DmmDecoder::addConfig({"Uni-Trend", "UT161E (UT-D09 USB)", "", 9600, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 22000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT161D (UT-D09 USB) *", "", 9600, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT161B (UT-D09 USB) *", "", 9600, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 6000, 0, 0, 0});
  // UT61B+/D+/E+ (per ut61xpy): the USB cable UT-D09 is a USB-HID UART (both
  // revisions, CP2110 and CH9329) at 9600 8N1; the UT-D07B Bluetooth adapter
  // carries the same frames over the same GATT service as the UT60BT. The
  // UT61E+ on the cable: real frames (dmm-tools' captures) are vectors now
  DmmDecoder::addConfig({"Uni-Trend", "UT61E+", "", 9600, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 22000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT61D+ *", "", 9600, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT61B+ *", "", 9600, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT61E+ (UT-D07B Bluetooth) *", "", 0, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 22000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT61D+ (UT-D07B Bluetooth) *", "", 0, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT61B+ (UT-D07B Bluetooth) *", "", 0, FrameFormat::UniTUT61Plus, 8, 1, 1, 0, 6000, 0, 0, 0});
  // UT202BT clamp, Bluetooth built in, 9999 counts; not tried with QtDMM
  DmmDecoder::addConfig({"Uni-Trend", "UT202BT", "", 0, FrameFormat::UniTUT202BT, 8, 1, 1, 0, 10000, 0, 0, 0});
  return true;
}();

namespace
{
// fn (byte 3, bit 7 masked) -> special, base unit and the SI prefix per range
// digit (byte 4 - '0'). One prefix string means the unit never changes.
// Function codes and ranges as UNI-T's own app has them (funOl_UT60BT.json and
// TestDataModel.functionStrings, saved by the unit_ut61eplus project - see
// ablage/unit_ut61eplus/from_vendor): V positions start in a 999.9 mV range,
// two mV ranges, OHM kOhm from range 1 and MOhm from range 4, CAP nF up to
// range 2 (999.9 nF), Hz up to range 2 (999.90 Hz), the mA position's range
// 1 is amperes. Checked on a UT60BT dial by dial (2026-09-24) for V, mV,
// OHM, the lowest CAP and Hz ranges. Codes 22-30 belong to other models.
struct Function
{
  const char *name;       ///< for the spec/debug output
  const char *special;    ///< "" = AC/DC from flags C bit 3
  const char *unit;
  const char *prefixes[8];
};

const Function kFunctions[] = {
  /*  0 */ { "ACV",    "",    "V",   { "m", "", "", "" } },
  /*  1 */ { "ACmV",   "",    "V",   { "m" } },
  /*  2 */ { "DCV",    "",    "V",   { "m", "", "", "" } },
  /*  3 */ { "DCmV",   "",    "V",   { "m" } },
  /*  4 */ { "Hz",     "HZ",  "Hz",  { "", "", "", "k", "k", "k", "M", "M" } },
  /*  5 */ { "%",      "DU",  "%",   { "" } },
  /*  6 */ { "OHM",    "OH",  "Ohm", { "", "k", "k", "k", "M", "M", "M" } },
  /*  7 */ { "CONT",   "BUZ", "Ohm", { "" } },
  /*  8 */ { "DIODE",  "DI",  "V",   { "" } },
  /*  9 */ { "CAP",    "CA",  "F",   { "n", "n", "n", "µ", "µ", "µ", "m", "m" } },
  /* 10 */ { "°C",     "TE",  "C",   { "" } },
  /* 11 */ { "°F",     "TE",  "dF",  { "" } },
  /* 12 */ { "DCuA",   "",    "A",   { "µ" } },
  /* 13 */ { "ACuA",   "",    "A",   { "µ" } },
  /* 14 */ { "DCmA",   "",    "A",   { "m", "" } },
  /* 15 */ { "ACmA",   "",    "A",   { "m", "" } },
  /* 16 */ { "DCA",    "",    "A",   { "" } },
  /* 17 */ { "ACA",    "",    "A",   { "" } },
  /* 18 */ { "HFE",    "HFE", "",    { "" } },
  /* 19 */ { "Live",   "NCV", "",    { "" } },
  /* 20 */ { "NCV",    "NCV", "",    { "" } },
  /* 21 */ { "LozV",   "",    "V",   { "" } },
  /* 22 */ { "ACA",    "",    "A",   { "" } },
  /* 23 */ { "DCA",    "",    "A",   { "" } },
  /* 24 */ { "LPF",    "",    "V",   { "" } },
  /* 25 */ { "AC/DC",  "",    "V",   { "" } },
  /* 26 */ { "LPF",    "",    "V",   { "" } },
  /* 27 */ { "AC+DC",  "",    "A",   { "" } },
  /* 28 */ { "LPFA",   "",    "V",   { "" } },
  /* 29 */ { "AC+DC2", "",    "A",   { "" } },
  /* 30 */ { "INRUSH", "",    "V",   { "" } },
};
constexpr int kFunctionCount = int(sizeof(kFunctions) / sizeof(kFunctions[0]));

// The UT61B+/D+/E+ and the UT161 series speak the same frame with other
// ranges - the vendor tables funOl_UT61E+/D+/B+ and funOl_UT161E/D/B agree
// with each other and with ut61xpy: the V positions start in volts (2.2 V
// resp. 6 V), the mA position stays in mA, CAP is nF up to range 1, Hz is
// Hz up to range 1. The function says AC or DC; only where it does not
// (25 AC/DC, the DC V position that alternates DC and AC readings, and the
// AC+DC current) flags C bit 3 marks the AC ones - real UT61E+ and UT161E
// frames leave it clear in AC V and AC A (dmm-tools' captures).
const Function kFunctions61Plus[] = {
  /*  0 */ { "ACV",    "AC",    "V",   { "" } },
  /*  1 */ { "ACmV",   "AC",    "V",   { "m" } },
  /*  2 */ { "DCV",    "DC",    "V",   { "" } },
  /*  3 */ { "DCmV",   "DC",    "V",   { "m" } },
  /*  4 */ { "Hz",     "HZ",  "Hz",  { "", "", "k", "k", "k", "M", "M", "M" } },
  /*  5 */ { "%",      "DU",  "%",   { "" } },
  /*  6 */ { "OHM",    "OH",  "Ohm", { "", "k", "k", "k", "M", "M", "M" } },
  /*  7 */ { "CONT",   "BUZ", "Ohm", { "" } },
  /*  8 */ { "DIODE",  "DI",  "V",   { "" } },
  /*  9 */ { "CAP",    "CA",  "F",   { "n", "n", "µ", "µ", "µ", "m", "m", "m" } },
  /* 10 */ { "°C",     "TE",  "C",   { "" } },
  /* 11 */ { "°F",     "TE",  "dF",  { "" } },
  /* 12 */ { "DCuA",   "DC",    "A",   { "µ" } },
  /* 13 */ { "ACuA",   "AC",    "A",   { "µ" } },
  /* 14 */ { "DCmA",   "DC",    "A",   { "m" } },
  /* 15 */ { "ACmA",   "AC",    "A",   { "m" } },
  /* 16 */ { "DCA",    "DC",    "A",   { "" } },
  /* 17 */ { "ACA",    "AC",    "A",   { "" } },
  /* 18 */ { "HFE",    "HFE", "",    { "" } },
  /* 19 */ { "Live",   "NCV", "",    { "" } },
  /* 20 */ { "NCV",    "NCV", "",    { "" } },
  /* 21 */ { "LozV",   "AC",    "V",   { "" } },
  /* 22 */ { "ACA",    "AC",    "A",   { "" } },
  /* 23 */ { "DCA",    "DC",    "A",   { "" } },
  /* 24 */ { "LPF",    "AC",    "V",   { "" } },
  /* 25 */ { "AC/DC",  "",    "V",   { "" } },
  /* 26 */ { "LPF",    "AC",    "V",   { "" } },
  /* 27 */ { "AC+DC",  "",    "A",   { "" } },
  /* 28 */ { "LPF",    "AC",    "V",   { "" } },
  /* 29 */ { "AC+DC2", "",    "A",   { "" } },
  /* 30 */ { "INRUSH", "",    "V",   { "" } },
};
constexpr int kFunctionCount61Plus = int(sizeof(kFunctions61Plus) / sizeof(kFunctions61Plus[0]));

// The UT202BT clamp: no dial, colour keys pick the function. Its functions
// and ranges as UNI-T's app has them (asset funOl1_UT202BT.json) with the
// manual's printed ranges, after dmm-tools (GPL-3.0-or-later, tables/
// ut202bt.rs and the UT61 family spec §9; no code taken): V and A 9.999,
// 99.99 and 600.0, Ω from 99.99 Ω in 7 ranges, CAP 99.99 nF to 99.9 mF, Hz
// to 9.999 MHz, LPF only in range 2, °C in range 1, °F in range 0. The
// clamp's codes 22 (AC A), 28 (LPF A) and 30 (inrush) are amperes here,
// where the generic table has volts. UNVERIFIED, no UT202BT has been
// connected: whether AC A comes as 17 or 22 (the app names both "ACA"),
// and the inrush range 0 (the asset's alone). A code the meter does not
// have is no reading (nullptr).
const Function kFunctions202[] = {
  /*  0 */ { "ACV",    "",    "V",   { "" } },
  /*  1 */ { nullptr,  "",    "",    { "" } },
  /*  2 */ { "DCV",    "",    "V",   { "" } },
  /*  3 */ { nullptr,  "",    "",    { "" } },
  /*  4 */ { "Hz",     "HZ",  "Hz",  { "", "", "k", "k", "k", "M" } },
  /*  5 */ { nullptr,  "",    "",    { "" } },
  /*  6 */ { "OHM",    "OH",  "Ohm", { "", "", "k", "k", "k", "M", "M" } },
  /*  7 */ { "CONT",   "BUZ", "Ohm", { "" } },
  /*  8 */ { nullptr,  "",    "",    { "" } },
  /*  9 */ { "CAP",    "CA",  "F",   { "n", "n", "µ", "µ", "µ", "m", "m" } },
  /* 10 */ { "°C",     "TE",  "C",   { "" } },
  /* 11 */ { "°F",     "TE",  "dF",  { "" } },
  /* 12 */ { nullptr,  "",    "",    { "" } },
  /* 13 */ { nullptr,  "",    "",    { "" } },
  /* 14 */ { nullptr,  "",    "",    { "" } },
  /* 15 */ { nullptr,  "",    "",    { "" } },
  /* 16 */ { nullptr,  "",    "",    { "" } },
  /* 17 */ { "ACA",    "",    "A",   { "" } },
  /* 18 */ { nullptr,  "",    "",    { "" } },
  /* 19 */ { nullptr,  "",    "",    { "" } },
  /* 20 */ { "NCV",    "NCV", "",    { "" } },
  /* 21 */ { nullptr,  "",    "",    { "" } },
  /* 22 */ { "ACA",    "",    "A",   { "" } },
  /* 23 */ { nullptr,  "",    "",    { "" } },
  /* 24 */ { "LPF",    "",    "V",   { "" } },
  /* 25 */ { nullptr,  "",    "",    { "" } },
  /* 26 */ { nullptr,  "",    "",    { "" } },
  /* 27 */ { nullptr,  "",    "",    { "" } },
  /* 28 */ { "LPFA",   "",    "A",   { "" } },
  /* 29 */ { nullptr,  "",    "",    { "" } },
  /* 30 */ { "INRUSH", "",    "A",   { "" } },
};
constexpr int kFunctionCount202 = int(sizeof(kFunctions202) / sizeof(kFunctions202[0]));

// The poll: AB CD, length 3, '^' (0x5E), 0x01, 16-bit BE sum of the first four
const QByteArray kPoll = QByteArray::fromHex("abcd035e01d9");
}

QByteArray DecoderUniTiDMM::pollRequest() const
{
  return kPoll;
}

QByteArray DecoderUniTiDMM::commandFrame(quint8 cmd)
{
  QByteArray f = QByteArray::fromHex("abcd03");
  f.append(char(cmd));
  appendSum16(f);
  return f;
}

// The key codes: ble-multimeter (uni-t.md, Controls, tried on a UT60BTk) and
// ut61eplus.py (_COMMANDS) agree. The UT60BT has no Hz/%, MAX/MIN and PEAK
// keys (its Hz is a dial position; tried 2026-10-06: they do nothing), the
// UT61x+ has them.
//
// The UT202BT: the bytes UNI-T's app sends it and no others (dmm-tools, UT61
// family spec §6.5): RANGE and HOLD, and its colour keys, 0x31 A~ (long
// press 0x32, inrush), 0x33 V~, 0x35 Ω, 0x36 NCV/PEAK (long press 0x37,
// peak capture). UNVERIFIED: what each does on the meter; whether the
// family's 0x4D starts PEAK there instead of 0x37, and whether 0x47 AUTO
// works (the app never sends it, so neither does QtDMM)
QByteArray DecoderUniTiDMM::keyRequest(const QString &key) const
{
  if (m_type == FrameFormat::UniTUT202BT)
  {
    static const QHash<QString, quint8> codes202 = {
      { "range", 0x46 }, { "hold", 0x4A }, { "fn_a", 0x31 }, { "fn_a_long", 0x32 }, { "fn_v", 0x33 },
      { "fn_ohm", 0x35 }, { "fn_ncv", 0x36 }, { "fn_ncv_long", 0x37 },
    };
    const auto it = codes202.constFind(key);
    return it != codes202.constEnd() ? commandFrame(it.value()) : QByteArray();
  }
  static const QHash<QString, quint8> codes = {
    { "range", 0x46 }, { "auto", 0x47 }, { "rel", 0x48 }, { "hold", 0x4A }, { "lamp", 0x4B }, { "select1", 0x4C },
  };
  static const QHash<QString, quint8> codes61Plus = {
    { "minmax", 0x41 }, { "minmax_off", 0x42 }, { "select2", 0x49 }, { "peak", 0x4D }, { "peak_off", 0x4E },
  };
  auto it = codes.constFind(key);
  if (it != codes.constEnd())
    return commandFrame(it.value());
  if (m_type == FrameFormat::UniTUT61Plus && (it = codes61Plus.constFind(key)) != codes61Plus.constEnd())
    return commandFrame(it.value());
  return QByteArray();
}

bool DecoderUniTiDMM::hasKey(const QString &key, const QString &model) const
{
  // whether a UT61B+ reacts to 0x4D stays open (dmm-tools, UT61 family #7)
  if ((key == QLatin1String("peak") || key == QLatin1String("peak_off"))
      && (model.startsWith(QLatin1String("UT61B+")) || model.startsWith(QLatin1String("UT161B"))))
    return false;
  return !keyRequest(key).isEmpty();
}

bool DecoderUniTiDMM::frameValid(const unsigned char *f)
{
  if (f[0] != 0xAB || f[1] != 0xCD || f[2] != kFrameLength - 3)
    return false;
  return sum16Valid(f, kFrameLength);
}

// The last kFrameLength bytes form a frame: header, length and checksum all
// have to agree, so a stray name answer (11 bytes) or a split notification
// only delays the next frame, it never produces a false one. The reader's
// FIFO is a ring, so the frame may run across its end.
bool DecoderUniTiDMM::checkFormat(const char *data, size_t idx)
{
  unsigned char f[kFrameLength];
  lastBytes(data, idx, kFrameLength, f);
  return frameValid(f);
}

std::optional<DmmDecoder::DmmResponse> DecoderUniTiDMM::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id = id;
  m_result.showBar = true;
  if (data.size() < kFrameLength)
    return std::nullopt;
  const auto *f = reinterpret_cast<const unsigned char *>(data.constData()) + data.size() - kFrameLength;
  if (!frameValid(f))
    return std::nullopt;

  // bit 7: the meter's second display in a frame of its own (dmm-tools,
  // family spec §2.3, after UNI-T's app): the frequency beside AC V and AC A
  // of a clamp, °F beside °C. Function and range as a main frame; bar and
  // flags are not known there, so the second value takes none of them
  const bool second = f[3] & 0x80;
  const int fn = f[3] & 0x7F;
  const bool plus = m_type == FrameFormat::UniTUT61Plus;
  const bool clamp = m_type == FrameFormat::UniTUT202BT;
  // a function the table lacks: its digits, without a unit, so a meter on
  // it does not look dead (dmm-tools 2738c05). A second display's frame of
  // one stays refused: as a reading it would switch the value every frame
  static const Function unknown = { "unknown", "", "", { "" } };
  const int count = plus ? kFunctionCount61Plus : clamp ? kFunctionCount202 : kFunctionCount;
  const Function *listed = fn < count ? (plus ? &kFunctions61Plus[fn] : clamp ? &kFunctions202[fn] : &kFunctions[fn])
                                      : nullptr;
  const bool known = listed && listed->name;
  if (!known && second)
    return std::nullopt;
  const Function &func = known ? *listed : unknown;
  const int range = f[4] - '0';
  const unsigned char flagsA = f[14], flagsB = f[15], flagsC = f[16];

  // prefix for this range; past the listed ranges the last one listed
  const char *prefix = func.prefixes[0];
  for (int r = qBound(0, range, 7); r >= 0; --r)
    if (func.prefixes[r])
    {
      prefix = func.prefixes[r];
      break;
    }

  m_result.main().special = !known         ? QString()
                            : *func.special ? QString::fromLatin1(func.special)
                                            : QString::fromLatin1(flagsC & 0x08 ? "AC" : "DC");
  // UT61E+ DC V position with AC shown alongside: the meter alternates, the
  // AC frames become the second value (display line 2, "2nd" in the table)
  if ((plus && fn == 25 && (flagsC & 0x08)) || second)
    m_result.main().id = id + 1;
  m_result.main().unit = QString::fromUtf8(prefix) + QString::fromLatin1(func.unit);
  if (second)
  {
    // its flags are not known: HOLD as the main display has it, else the
    // LCD's HOLD flickered with each frequency frame (UT202BT, AC A)
    m_result.showBar = false;
    m_result.hold = m_hold;
    return displayed(f, prefix);
  }
  m_result.hold = m_hold = flagsA & 0x02;
  // the meter's other keys: A bit3 MAX, bit2 MIN, bit0 REL; C bit2/bit1 peak
  // max/min
  if (flagsA & 0x08)
    m_result.main().states |= SampleFlag::Max;
  if (flagsA & 0x04)
    m_result.main().states |= SampleFlag::Min;
  if (flagsA & 0x01)
    m_result.main().states |= SampleFlag::Relative;
  if (flagsC & 0x06)
  {
    m_result.main().states |= SampleFlag::Peak;
    // which of the two the LCD shows (UT202BT: C bit2 PEAK MAX, bit1 PEAK MIN)
    if (flagsC & 0x04)
      m_result.main().states |= SampleFlag::Max;
    if (flagsC & 0x02)
      m_result.main().states |= SampleFlag::Min;
  }
  m_result.range = (flagsB & 0x04) ? "MANU" : "AUTO";
  return displayed(f, prefix);
}

DmmDecoder::DmmResponse DecoderUniTiDMM::displayed(const unsigned char *f, const char *prefix)
{
  // the LCD text: sign and decimal point included; the dot also floats
  // inside "OL" ("OL.", "O.L", ".OL") depending on the range
  QString text = QString::fromLatin1(reinterpret_cast<const char *>(f + 5), 7).trimmed();
  QString bare = text;
  bare.remove('.').remove(' ');
  static const QRegularExpression overload("^-?OL$");
  if (overload.match(bare).hasMatch())
  {
    m_result.main().val = bare.startsWith('-') ? "-OL" : "OL";
    m_result.main().dval = 0;
    return m_result;
  }
  text.remove(' ');
  bool ok = false;
  const double shown = text.toDouble(&ok);
  if (!ok)
  {
    // NCV strength bar ("EF", "----"), no number: show it as it is
    m_result.main().val = text;
    m_result.main().dval = 0;
    m_result.main().noValue = true;
    return m_result;
  }
  m_result.main().val = text;
  m_result.main().dval = shown * prefixFactor(QString::fromUtf8(prefix));
  return m_result;
}
