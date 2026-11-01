// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/eevblog_121gw.h"

#include "core/sampletypes.h"

#include <QCoreApplication>

static const bool registered = []() {
  // Bluetooth LE built in (baud 0); not tried. 4 4/5 digits, 55000 counts
  // (manual p.15); the secondary display and in the VA modes both operands
  DmmDecoder::addConfig({"EEVblog", "121GW *", "", 0, FrameFormat::Eevblog121GW, 8, 1, 1, 0, 55000, 0, 0, 0, "", "", 3});
  return true;
}();

namespace
{
constexpr int kLength = 18;   // bytes 1-18 of the packet

// a range: digits after the point, the prefix (spec §6.2)
struct GwRange
{
  int decimals;
  const char *prefix;
};
struct Mode
{
  const char *special;
  const char *unit;
  QList<GwRange> ranges;
  const char *name;   ///< the value's own name, translated; null: by what it measures
};
const QList<GwRange> kVolts = { { 4, "" }, { 3, "" }, { 2, "" }, { 1, "" } };
const QList<GwRange> kMilliVolts = { { 3, "m" }, { 2, "m" } };
const QList<GwRange> kMicroVa = { { 2, "µ" }, { 1, "µ" }, { 1, "µ" }, { 0, "µ" } };
const QList<GwRange> kMilliVa = { { 3, "m" }, { 2, "m" }, { 2, "m" }, { 1, "m" } };
const QList<GwRange> kVa = { { 1, "m" }, { 0, "m" }, { 3, "" }, { 2, "" } };
const QList<GwRange> kMicroAmps = { { 3, "µ" }, { 2, "µ" } };
const QList<GwRange> kMilliAmps = { { 4, "m" }, { 3, "m" } };
const QList<GwRange> kAmps = { { 2, "m" }, { 4, "" }, { 3, "" } };
const Mode kModes[25] = {
  { "AC", "V", { { 1, "" } }, QT_TRANSLATE_NOOP("121GW", "LoZ") },
  { "DC", "V", kVolts, nullptr },
  { "AC", "V", kVolts, nullptr },
  { "DC", "V", kMilliVolts, nullptr },
  { "AC", "V", kMilliVolts, nullptr },
  { "TE", "C", { { 1, "" } }, nullptr },
  { "HZ", "Hz", { { 3, "" }, { 2, "" }, { 4, "k" }, { 3, "k" }, { 2, "k" } }, nullptr },
  { "HZ", "s", { { 4, "m" }, { 3, "m" }, { 2, "m" } }, nullptr },   // the pulse width in ms
  { "DU", "%", { { 1, "" } }, nullptr },
  { "OH", "Ohm", { { 3, "" }, { 2, "" }, { 4, "k" }, { 3, "k" }, { 2, "k" }, { 4, "M" }, { 3, "M" } }, nullptr },
  { "BUZ", "Ohm", { { 2, "" } }, nullptr },
  { "DI", "V", { { 4, "" }, { 3, "" } }, nullptr },
  { "CA", "F", { { 2, "n" }, { 1, "n" }, { 3, "µ" }, { 2, "µ" }, { 1, "µ" }, { 0, "µ" } }, nullptr },
  { "AC", "VA", kMicroVa, nullptr },
  { "AC", "VA", kMilliVa, nullptr },
  { "AC", "VA", kVa, nullptr },
  { "AC", "A", kMicroAmps, nullptr },
  { "DC", "A", kMicroAmps, nullptr },
  { "AC", "A", kMilliAmps, nullptr },
  { "DC", "A", kMilliAmps, nullptr },
  { "AC", "A", kAmps, nullptr },
  { "DC", "A", kAmps, nullptr },
  { "DC", "VA", kMicroVa, nullptr },
  { "DC", "VA", kMilliVa, nullptr },
  { "DC", "VA", kVa, nullptr },
};

bool isVa(int mode)
{
  return (mode >= 13 && mode <= 15) || (mode >= 22 && mode <= 24);
}

// the count with @p decimals digits after the point, as the meter shows it
QString shown(quint32 count, int decimals, bool negative)
{
  QString s = QString::number(count);
  if (decimals > 0)
  {
    s = s.rightJustified(decimals + 1, '0');
    s.insert(s.size() - decimals, '.');
  }
  return negative ? "-" + s : s;
}

// what the secondary display shows (spec §7.1): its name, unit and prefix,
// or nothing for a setting, a blank display and codes not known
struct Sub
{
  const char *special;
  const char *prefix;
  const char *unit;
  const char *name;
};
std::optional<Sub> subOf(int code, bool kilo)
{
  switch (code)
  {
    case 1:
    case 2:
      return Sub{ code == 1 ? "DC" : "AC", "", "V", nullptr };   // a VA mode's voltage
    case 6:
      return Sub{ "HZ", kilo ? "k" : "", "Hz", nullptr };
    case 16:
    case 17:
      return Sub{ code == 16 ? "AC" : "DC", "µ", "A", nullptr };
    case 18:
    case 19:
    case 20:
    case 21:
      return Sub{ code % 2 ? "DC" : "AC", "m", "A", nullptr };   // in range 0's unit (UEi's app)
    case 100:
    case 101:
      return Sub{ "TE", "", "C", QT_TRANSLATE_NOOP("121GW", "Internal temperature") };
    case 105:
    case 106:
      return Sub{ "TE", "", "dF", QT_TRANSLATE_NOOP("121GW", "Internal temperature") };
    case 110:
      return Sub{ "DC", "", "V", QT_TRANSLATE_NOOP("121GW", "Battery") };
    case 150:
      return Sub{ "DC", "m", "V", QT_TRANSLATE_NOOP("121GW", "Burden voltage") };
    case 180:
      return Sub{ "", "", "dBm", nullptr };
    default:
      return std::nullopt;
  }
}
}

bool Decoder121GW::checkFormat(const char *data, size_t idx)
{
  // bytes 1-18 whose XOR with F2 is 0, byte 13 bits 7-5 clear (the ASCII
  // packets of old firmware set them), a mode and range of the tables
  if (idx < size_t(kLength - 1))
    return false;
  unsigned char b[kLength];
  lastBytes(data, idx, kLength, b);
  quint8 x = 0;
  for (unsigned char c : b)
    x ^= c;
  const int mode = b[4] & 0x1F;
  return x == 0xF2 && (b[12] & 0xE0) == 0 && mode < 25 && (b[5] & 0x0F) < kModes[mode].ranges.size();
}

std::optional<DmmDecoder::DmmResponse> Decoder121GW::decode(const QByteArray &data, int id)
{
  if (data.size() < kLength)
    return std::nullopt;
  // p(i): packet byte i, the F2 being byte 0
  const char *body = data.constData() + data.size() - kLength;
  const auto p = [body](int i) { return quint8(body[i - 1]); };
  const int mode = p(5) & 0x1F, range = p(6) & 0x0F;
  if (mode >= 25 || range >= kModes[mode].ranges.size())
    return std::nullopt;
  const Mode &m = kModes[mode];
  const GwRange &r = m.ranges[range];
  const int coupling = (p(15) >> 3) & 0x03;   // 1 DC, 2 AC, 3 DC+AC

  m_result = {};
  m_result.range = (p(15) & 0x04) ? "AUTO" : "MANU";
  m_result.lowBat = p(15) & 0x01;
  const int hold = (p(17) >> 2) & 0x03;   // 1 A-HOLD, 2 HOLD
  m_result.hold = hold == 1 || hold == 2;
  DmmChannel &main = m_result.main();
  main.id = id;
  main.special = (mode == 1 || mode == 2) && coupling == 3 ? "ACDC" : m.special;
  QString unit = QString::fromUtf8(m.unit);
  if (mode == 5 && (p(6) & 0x30) == 0x10)
    unit = "dF";   // the c/F setting: byte 6 bit 4 F, bit 5 C (spec §6.4)
  const QString prefix = QString::fromUtf8(r.prefix);
  main.unit = prefix + unit;
  if (m.name)
    main.name = QCoreApplication::translate("121GW", m.name);
  else if ((p(15) & 0x40) && main.special == QLatin1String("AC"))
    main.name = QCoreApplication::translate("121GW", "Low-pass");   // the 1 kHz filter
  if (p(16) & 0x10)
    main.states |= SampleFlag::Relative;
  const int minMax = p(16) & 0x07;
  if (p(15) & 0x20)
    main.states |= SampleFlag::Peak | (minMax == 2 ? SampleFlag::Min : SampleFlag::Max);   // 1 ms peak
  else if (minMax == 1)
    main.states |= SampleFlag::Max;
  else if (minMax == 2)
    main.states |= SampleFlag::Min;
  else if (minMax == 3 || minMax == 4)
    main.states |= SampleFlag::Avg;

  if (p(6) & 0x80)
  {
    main.val = (p(6) & 0x40) ? QStringLiteral("-OL") : QStringLiteral("OL");
    main.dval = 0;
  }
  else
  {
    const quint32 count = (quint32(p(5) >> 6) << 16) | (quint32(p(7)) << 8) | p(8);
    main.val = shown(count, r.decimals, p(6) & 0x40);
    main.dval = main.val.toDouble() * prefixFactor(prefix);
  }

  // the secondary display; in the VA modes the voltage and the current each
  // its own value, the one not shown empty (spec §7.1, dmm-tools)
  DmmChannel &sub = m_result.addChannel(id + 1);
  DmmChannel *current = nullptr;
  if (isVa(mode))
  {
    sub.special = mode < 16 ? "AC" : "DC";
    sub.unit = QStringLiteral("V");
    current = &m_result.addChannel(id + 2);
    current->special = sub.special;
    current->unit = (mode == 13 || mode == 22) ? QStringLiteral("µA") : QStringLiteral("mA");
  }
  const bool blank = p(9) == 0 && p(10) == 0 && p(11) == 0 && p(12) == 0;
  const std::optional<Sub> s = blank ? std::nullopt : subOf(p(9), p(10) & 0x20);
  if (!s)
    return m_result;
  const bool isCurrent = QLatin1String(s->unit) == QLatin1String("A");
  DmmChannel &c = current && isCurrent ? *current : sub;
  c.special = s->special;
  const QString subPrefix = QString::fromUtf8(s->prefix);
  c.unit = subPrefix + QString::fromUtf8(s->unit);
  if (s->name)
    c.name = QCoreApplication::translate("121GW", s->name);
  if (p(10) & 0x80)
  {
    c.val = (p(10) & 0x40) ? QStringLiteral("-OL") : QStringLiteral("OL");
    c.dval = 0;
  }
  else
  {
    c.val = shown((quint32(p(11)) << 8) | p(12), p(10) & 0x07, p(10) & 0x40);
    c.dval = c.val.toDouble() * prefixFactor(subPrefix);
  }
  return m_result;
}

QByteArray Decoder121GW::keyRequest(const QString &key) const
{
  // never 84, the long 1ms PEAK: it switches Bluetooth off (spec §11.1)
  static const QHash<QString, quint8> codes = {
    { "range", 0x01 }, { "hold", 0x02 }, { "rel", 0x03 }, { "peak", 0x04 }, { "peak_off", 0x04 },
    { "select1", 0x05 }, { "minmax", 0x06 }, { "minmax_off", 0x86 }, { "lamp", 0x85 },
  };
  const auto it = codes.constFind(key);
  if (it == codes.constEnd())
    return QByteArray();
  const QByteArray hex = QByteArray(1, char(*it)).toHex().toUpper();
  return QByteArray(1, '\xF4') + hex + hex;
}
