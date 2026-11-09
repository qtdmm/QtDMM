// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/zotek.h"

#include "core/sampletypes.h"

static const bool registered = []() {
  // Bluetooth LE, no baud rate. Not tried with QtDMM; dmm-tools has run a
  // ZOYI ZT-5B (type 2). Counts from ZOTEK's manuals, the twins from the
  // product pages (dmm-tools spec §1)
  DmmDecoder::addConfig({"ZOTEK", "ZT-5B *", "", 0, FrameFormat::ZotekBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"ZOTEK", "ZT-5BQ *", "", 0, FrameFormat::ZotekBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"ZOTEK", "ZT-300AB *", "", 0, FrameFormat::ZotekBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"ZOTEK", "ZT-5566SE *", "", 0, FrameFormat::ZotekBLE, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"ANENG", "V05B *", "", 0, FrameFormat::ZotekBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"ANENG", "ST207 *", "", 0, FrameFormat::ZotekBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"ANENG", "AN9002 *", "", 0, FrameFormat::ZotekBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"ANENG", "AN999S *", "", 0, FrameFormat::ZotekBLE, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 2});
  return true;
}();

namespace
{
// what every notification and command is XORed with (spec §4)
const unsigned char kKey[20] = { 0x41, 0x21, 0x73, 0x55, 0xA2, 0xC1, 0x32, 0x71, 0x66, 0xAA,
                                 0x3B, 0xD0, 0xE2, 0xA8, 0x33, 0x14, 0x20, 0x1A, 0xAA, 0xBB };
// the bytes the apps read of each type (spec §5); a ZT-5B sends exactly 10
constexpr int kLength[5] = { 0, 10, 10, 11, 19 };

// the layout each model sends (spec §1; type 2 seen on a ZT-5B, the others
// inferred from the annunciators each layout carries)
struct ModelType
{
  const char *name;   ///< DMMInfo::name without " *"
  int type;
};
const ModelType kModels[] = {
  { "ZOTEK ZT-5BQ", 1 }, { "ANENG ST207", 1 },
  { "ZOTEK ZT-5B", 2 }, { "ANENG V05B", 2 },
  { "ZOTEK ZT-300AB", 3 }, { "ANENG AN9002", 3 },
  { "ZOTEK ZT-5566SE", 4 }, { "ANENG AN999S", 4 },
};

int typeOfModel(const QString &model)
{
  for (const ModelType &m : kModels)
  {
    const QString name = QString::fromLatin1(m.name);
    if (DmmDecoder::sameModel(model, name) || DmmDecoder::sameModel(model, name.section(' ', 1)))
      return m.type;
  }
  return 0;
}

// a seven-segment glyph, bit 4 (point or sign) cleared, as the character it
// shows; 0 for an unknown one (spec §6.1, segments a f e DP b g c d)
char glyph(unsigned char g)
{
  switch (g & 0xEF)
  {
    case 0xEB: return '0';
    case 0x0A: return '1';
    case 0xAD: return '2';
    case 0x8F: return '3';
    case 0x4E: return '4';
    case 0xC7: return '5';
    case 0xE7: return '6';
    case 0x8A: return '7';
    case 0xEF: return '8';
    case 0xCF: return '9';
    case 0xEE: return 'A';
    case 0xE5: return 'E';
    case 0xE4: return 'F';
    case 0x61: return 'L';
    case 0x27: return 'o';
    case 0x23: return 'u';
    case 0x65: return 't';
    case 0x67: return 'b';
    case 0xE1: return 'C';
    case 0x2F: return 'd';
    case 0x04: return '-';
    case 0x00: return ' ';
    default:   return 0;
  }
}

// One display: its glyphs from the left, each with whether the point is lit
// before it. The text as the LCD shows it ("-12.34", " .0L ", "Auto"),
// nullopt for an unknown glyph (a corrupt packet)
std::optional<QString> displayText(const unsigned char *glyphs, const bool *points, int n, bool minus)
{
  QString text = minus ? QStringLiteral("-") : QString();
  for (int i = 0; i < n; ++i)
  {
    const char c = glyph(glyphs[i]);
    if (!c)
      return std::nullopt;
    if (points[i])
      text += '.';
    text += QLatin1Char(c);
  }
  return text;
}

// The annunciators of all four layouts, read by type (spec §7)
struct Annunciators
{
  bool ac = false, dc = false, volt = false, amp = false, ohm = false, farad = false, hz = false, percent = false;
  bool degC = false, degF = false, diode = false, beep = false;
  bool hold = false, rel = false, max = false, min = false, peak = false, autoRange = false, manual = false, lowBat = false;
  bool n = false, u = false, m = false, k = false, M = false;   ///< the prefixes lit
};

Annunciators annunciators(const unsigned char *p, int type)
{
  auto at = [p](int byte, int bit) { return bool(p[byte] & (1 << bit)); };
  Annunciators a;
  switch (type)
  {
    case 1:   // ZT-5BQ
      a.beep = at(3, 3); a.hold = at(3, 1); a.lowBat = at(3, 0);
      a.ac = at(7, 7); a.dc = at(7, 6); a.volt = at(7, 5); a.n = at(7, 4);
      a.M = at(8, 7); a.m = at(8, 6); a.k = at(8, 5); a.ohm = at(8, 4); a.u = at(8, 3); a.amp = at(8, 2);
      a.diode = at(8, 1); a.farad = at(8, 0);
      a.peak = at(9, 6); a.percent = at(9, 5); a.degC = at(9, 3); a.degF = at(9, 2); a.hz = at(9, 1); a.rel = at(9, 0);
      break;
    case 2:   // ZT-5B; byte 3 bit 2 is the over-voltage warning, not shown
      a.beep = at(3, 3); a.hold = at(3, 1); a.lowBat = at(3, 0);
      a.u = at(8, 7); a.amp = at(8, 6); a.diode = at(8, 5); a.farad = at(8, 4); a.ac = at(8, 3); a.dc = at(8, 2);
      a.volt = at(8, 1); a.n = at(8, 0);
      a.degC = at(9, 7); a.degF = at(9, 6); a.hz = at(9, 5); a.percent = at(9, 4); a.M = at(9, 3); a.m = at(9, 2);
      a.k = at(9, 1); a.ohm = at(9, 0);
      break;
    case 3:   // ZT-300AB: a prefix of its own for each unit, as on its LCD
      a.beep = at(3, 3); a.rel = at(3, 1); a.lowBat = at(3, 0);
      a.diode = at(7, 7); a.degC = at(7, 6); a.degF = at(7, 5); a.hold = at(7, 4);
      a.farad = at(8, 4); a.ac = at(8, 3); a.percent = at(8, 2); a.min = at(8, 1); a.max = at(8, 0);
      a.amp = at(9, 7); a.dc = at(9, 6); a.volt = at(9, 4); a.M = at(9, 3); a.k = at(9, 2); a.ohm = at(9, 1);
      a.hz = at(9, 0);
      a.manual = at(10, 1); a.autoRange = at(10, 0);
      if (a.farad)
      {
        a.n = at(8, 7); a.m = at(8, 6); a.u = at(8, 5);
      }
      else if (a.amp)
      {
        a.m = at(10, 3); a.u = at(10, 2);
      }
      else if (a.volt)
        a.m = at(9, 5);
      break;
    case 4:   // ZT-5566SE, the main display
      a.diode = at(3, 6); a.beep = at(3, 5); a.rel = at(3, 4); a.autoRange = at(3, 2); a.manual = at(3, 1);
      a.volt = at(4, 4); a.hold = at(4, 3); a.peak = at(4, 2); a.max = at(4, 1); a.min = at(4, 0);
      a.ac = at(13, 6); a.dc = at(13, 1);
      a.hz = at(16, 7); a.ohm = at(16, 6); a.k = at(16, 5); a.M = at(16, 4);
      a.n = at(17, 3); a.m = at(17, 2); a.u = at(17, 1); a.farad = at(17, 0);
      a.amp = at(18, 4);
      break;
  }
  return a;
}
}

QByteArray DecoderZotek::scramble(QByteArray data)
{
  for (int i = 0; i < data.size(); ++i)
    data[i] = char(static_cast<unsigned char>(data[i]) ^ kKey[i % 20]);
  return data;
}

int DecoderZotek::packetType(const QByteArray &frame)
{
  const auto *f = reinterpret_cast<const unsigned char *>(frame.constData());
  for (int type = 1; type <= 4; ++type)
  {
    const int start = int(frame.size()) - kLength[type];
    if (start >= 0 && (f[start] ^ kKey[0]) == 0x5A && (f[start + 1] ^ kKey[1]) == 0xA5 && (f[start + 2] ^ kKey[2]) == type)
      return type;
  }
  return 0;
}

// A packet ends here when its header and type sit its length back. Only the
// bytes since the previous frame count (FrameReader starts each at index
// 0): what lies before them in the ring is stale. A longer notification
// leaves its rest at the start of the next frame, decode() takes the end
bool DecoderZotek::checkFormat(const char *data, size_t idx)
{
  for (int type = 1; type <= 4; ++type)
  {
    const int n = kLength[type];
    if (int(idx) + 1 >= n && (ringByte(data, idx, n - 1) ^ kKey[0]) == 0x5A
        && (ringByte(data, idx, n - 2) ^ kKey[1]) == 0xA5 && (ringByte(data, idx, n - 3) ^ kKey[2]) == type)
      return true;
  }
  return false;
}

std::optional<DmmDecoder::DmmResponse> DecoderZotek::decode(const QByteArray &data, int id)
{
  const int type = packetType(data);
  if (!type)
    return std::nullopt;
  const QByteArray plain = scramble(data.right(kLength[type]));
  const auto *p = reinterpret_cast<const unsigned char *>(plain.constData());

  // the digits from the left with the point before each, and the sign
  unsigned char glyphs[5];
  bool points[5] = {};
  int digits = 4;
  bool minus;
  if (type == 4)
  {
    // one byte per digit, rightmost first; a leading "1" (bits 3 and 2 of
    // byte 13) makes the 19999 counts
    const bool one = (p[13] & 0x0C) == 0x0C;
    digits = 0;
    if (one)
      glyphs[digits++] = 0x0A;
    for (int byte = 12; byte >= 9; --byte, ++digits)
    {
      glyphs[digits] = p[byte];
      points[digits] = p[byte] & 0x10;
    }
    minus = p[13] & 0x80;
  }
  else
  {
    // digit k: high nibble from byte 3+k, low nibble from byte 4+k; bit 4
    // of the high-nibble byte is the minus for the first, the point before
    // it for the others
    for (int k = 0; k < 4; ++k)
    {
      glyphs[k] = (p[3 + k] & 0xF0) | (p[4 + k] & 0x0F);
      points[k] = k > 0 && (p[3 + k] & 0x10);
    }
    minus = p[3] & 0x10;
  }
  const std::optional<QString> text = displayText(glyphs, points, digits, minus);
  if (!text)
    return std::nullopt;

  const Annunciators a = annunciators(p, type);
  m_result = {};
  DmmChannel &main = m_result.main();
  main.id = id;
  m_result.hold = a.hold;
  m_result.lowBat = a.lowBat;
  m_result.range = a.autoRange ? "AUTO" : a.manual ? "MANU" : "";
  if (a.rel)
    main.states |= SampleFlag::Relative;
  if (a.max)
    main.states |= SampleFlag::Max;
  if (a.min)
    main.states |= SampleFlag::Min;
  if (a.peak)
    main.states |= SampleFlag::Peak;

  // the function from the annunciators, diode before continuity (a ZT-5B
  // lights both in diode)
  QString unit;
  bool small = false;   // n, µ and m apply (V, A, F); k and M to Ohm and Hz
  if (a.diode)
  {
    main.special = "DI";
    unit = "V";
  }
  else if (a.beep)
  {
    main.special = "BUZ";
    unit = "Ohm";
  }
  else if (a.degC || a.degF)
  {
    main.special = "TE";
    unit = a.degC ? "C" : "dF";
  }
  else if (a.farad)
  {
    main.special = "CA";
    unit = "F";
    small = true;
  }
  else if (a.percent)
  {
    main.special = "DU";
    unit = "%";
  }
  else if (a.hz)
  {
    main.special = "HZ";
    unit = "Hz";
  }
  else if (a.ohm)
  {
    main.special = "OH";
    unit = "Ohm";
  }
  else if (a.volt || a.amp)
  {
    main.special = a.ac && a.dc ? "ACDC" : a.ac ? "AC" : "DC";
    unit = a.volt ? "V" : "A";
    small = true;
  }
  QString prefix;
  if (small)
    prefix = a.n ? "n" : a.u ? "µ" : a.m ? "m" : "";
  else if (unit == QLatin1String("Ohm") || unit == QLatin1String("Hz"))
    prefix = a.k ? "k" : a.M ? "M" : "";
  main.unit = prefix + unit;

  // words: OL, Auto (auto mode before a reading), EF and dashes (NCV);
  // nothing lit at all (a function change) is no reading. OL only for the
  // meter's 0L: any other L was one vendor app's fallback, nothing the
  // meter is known to show (dmm-tools 4087be5); such text shows as it is
  QString bare = *text;
  bare.remove('.').remove(' ');
  if (bare.isEmpty())
    return std::nullopt;
  if (bare == QLatin1String("0L"))
  {
    main.val = minus ? "-OL" : "OL";
    main.dval = 0;
  }
  else if (bare == QLatin1String("EF") || (!minus && bare.count('-') == bare.size()))
  {
    // the field strength in dashes: words, no number
    main.special = "NCV";
    main.unit.clear();
    main.val = bare;
    main.dval = 0;
    main.noValue = true;
  }
  else
  {
    QString number = *text;
    number.remove(' ');
    bool ok = false;
    const double shown = number.toDouble(&ok);
    main.val = number;
    main.dval = ok ? shown * prefixFactor(prefix) : 0;
    main.noValue = !ok;   // text no table knows, or Auto: no number
    if (bare == QLatin1String("Auto"))
    {
      main.special.clear();
      main.unit.clear();
    }
  }

  if (type == 4)
  {
    // the second display, rightmost digit in byte 5; points on bytes 5-7,
    // its minus on byte 8; units %, Hz and k of its own
    unsigned char glyphs2[4];
    bool points2[4] = {};
    for (int i = 0; i < 4; ++i)
    {
      glyphs2[i] = p[8 - i];
      points2[i] = i > 0 && (p[8 - i] & 0x10);
    }
    const std::optional<QString> text2 = displayText(glyphs2, points2, 4, p[8] & 0x10);
    if (!text2)
      return std::nullopt;
    DmmChannel &second = m_result.addChannel(1);
    QString shown2 = *text2;
    shown2.remove(' ');
    if (QString(shown2).remove('.').remove('-').isEmpty())
      shown2.clear();   // off
    const bool percent2 = p[4] & 0x80, hz2 = p[4] & 0x40, kilo2 = p[4] & 0x20;
    second.special = percent2 ? "DU" : hz2 ? "HZ" : "";
    second.unit = percent2 ? "%" : hz2 ? (kilo2 ? "kHz" : "Hz") : "";
    second.val = shown2;
    bool ok = false;
    const double v = shown2.toDouble(&ok);
    second.dval = ok ? v * (hz2 && kilo2 ? 1e3 : 1.0) : 0;
  }

  m_unit = main.unit;
  return m_result;
}

// The keys ZOTEK's apps send (spec §8.2), cmd 03: a ZT-5B answered all of
// B0-B8 and did what they say; the other models are untried. They pick a
// function (B8 AUTO is the auto function, not auto ranging) rather than
// step to the next; B5 ZERO only in capacitance, B6/B7 set °C or °F, the
// other one than shown. The V, mV, Ω and A keys are left out: a ZT-5B
// ignored V and A, its jacks choose them, and the ZT-300AB has a dial.
QByteArray DecoderZotek::keyRequest(const QString &key) const
{
  static const QHash<QString, quint8> codes = {
    { "fn_cap", 0xB0 }, { "fn_diode", 0xB1 }, { "fn_ncv", 0xB2 }, { "fn_hz", 0xB3 }, { "hold", 0xB4 },
    { "fn_auto", 0xB8 }, { "minmax", 0xD1 },
  };
  quint8 code = 0;
  const auto it = codes.constFind(key);
  if (it != codes.constEnd())
    code = it.value();
  else if (key == QLatin1String("zero") && m_unit.endsWith('F') && m_unit != QLatin1String("dF"))
    code = 0xB5;
  else if (key == QLatin1String("unit2"))
    code = m_unit == QLatin1String("C") ? 0xB7 : 0xB6;
  if (!code)
    return QByteArray();
  QByteArray f = QByteArray::fromHex("abcd03");
  f.append(char(code)).append(4, '\0');
  appendSum16(f);
  return scramble(f);
}

// Which keys to offer: the ZT-5B's (type 2) and the ZT-5BQ's, the clamp of
// the same make, all of them; the dial meters only MAX/MIN, the ZT-5566SE
// HOLD as well - ZOTEK's older app greys out the others on the ZT-300AB
bool DecoderZotek::hasKey(const QString &key, const QString &model) const
{
  static const QStringList pocket = { "fn_auto", "fn_cap", "fn_diode", "fn_ncv", "fn_hz", "hold", "zero", "unit2" };
  switch (typeOfModel(model))
  {
    case 1:
    case 2: return pocket.contains(key);
    case 3: return key == QLatin1String("minmax");
    case 4: return key == QLatin1String("minmax") || key == QLatin1String("hold");
    default: return false;
  }
}

QStringList DecoderZotek::modelsFromFrame(const QByteArray &frame) const
{
  const int type = packetType(frame);
  QStringList out;
  for (const DMMInfo &info : getDeviceConfigurations())
    if (info.protocol == m_type && typeOfModel(info.name) == type)
      out << info.name;
  return out;
}
