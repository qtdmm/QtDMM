// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut8803.h"

#include "core/sampletypes.h"

#include <QCoreApplication>

static const bool registered = []() {
  // the built-in CP2110 at 9600 8N1; none tried. 6000 counts
  DmmDecoder::addConfig({"Uni-Trend", "UT8803 *", "", 9600, FrameFormat::UniTUT8803, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT8803E *", "", 9600, FrameFormat::UniTUT8803, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT8803N *", "", 9600, FrameFormat::UniTUT8803, 8, 1, 1, 0, 6000, 0, 0, 0});
  return true;
}();

namespace
{
constexpr int kLength = 21;

// the modes (spec §4.2) and how their range gives the prefix (§4.5, after
// uci.dll as dmm-tools reads it)
enum Ladder { None, MilliVolt, Ohm, Henry, Farad, Hertz };
struct Mode
{
  const char *special;
  const char *prefix;   ///< the mode's own (µA, mA)
  const char *unit;
  Ladder ladder;
  const char *name;     ///< the value's own name, translated; null: by what it measures
};
const Mode kModes[0x17] = {
  { "AC", "", "V", MilliVolt, nullptr },  { "DC", "", "V", MilliVolt, nullptr },
  { "AC", "µ", "A", None, nullptr },      { "AC", "m", "A", None, nullptr },     { "AC", "", "A", None, nullptr },
  { "DC", "µ", "A", None, nullptr },      { "DC", "m", "A", None, nullptr },     { "DC", "", "A", None, nullptr },
  { "OH", "", "Ohm", Ohm, nullptr },      { "BUZ", "", "Ohm", Ohm, nullptr },    { "DI", "", "V", None, nullptr },
  { "", "", "H", Henry, QT_TRANSLATE_NOOP("UT8803", "Inductance") },
  { "", "", "", None, QT_TRANSLATE_NOOP("UT8803", "Quality factor") },
  { "OH", "", "Ohm", Ohm, QT_TRANSLATE_NOOP("UT8803", "ESR") },
  { "CA", "", "F", Farad, nullptr },
  { "", "", "D", None, nullptr },
  { "OH", "", "Ohm", Ohm, QT_TRANSLATE_NOOP("UT8803", "ESR") },
  { "HFE", "", "", None, nullptr },       { "", "", "V", None, QT_TRANSLATE_NOOP("UT8803", "SCR") },
  { "TE", "", "C", None, nullptr },       { "TE", "", "dF", None, nullptr },
  { "HZ", "", "Hz", Hertz, nullptr },     { "DU", "", "%", None, nullptr },
};

QString prefixFor(Ladder ladder, int range)
{
  switch (ladder)
  {
    case MilliVolt:
      return range == 0 ? QStringLiteral("m") : QString();
    case Ohm:
    case Hertz:
      return range >= 1 && range <= 3 ? QStringLiteral("k") : range >= 4 && range <= 5 ? QStringLiteral("M") : QString();
    case Henry:
      // range 0 the manual's 600 µH row, as the 2020 SDK has it; the 2018
      // lookup gave m there, a thousand times off (dmm-tools 78a8503)
      return range == 0 ? QStringLiteral("µ") : range < 4 ? QStringLiteral("m") : QString();
    case Farad:
      return range <= 2 ? QStringLiteral("n") : range <= 5 ? QStringLiteral("µ") : QStringLiteral("m");
    default:
      return QString();
  }
}
}

bool DecoderUT8803::checkFormat(const char *data, size_t idx)
{
  if (idx < size_t(kLength - 1))
    return false;
  unsigned char f[kLength];
  lastBytes(data, idx, kLength, f);
  quint16 sum = 0;
  for (int i = 0; i < kLength - 2; ++i)
    sum = quint16(sum + f[i]);
  return f[0] == 0xAB && f[1] == 0xCD && f[3] == 0x02 && f[4] <= 0x16
         && sum == quint16((f[kLength - 2] << 8) | f[kLength - 1]);
}

std::optional<DmmDecoder::DmmResponse> DecoderUT8803::decode(const QByteArray &data, int id)
{
  if (data.size() < kLength)
    return std::nullopt;
  const auto *f = reinterpret_cast<const unsigned char *>(data.constData()) + data.size() - kLength;
  if (f[4] > 0x16)
    return std::nullopt;
  const Mode &m = kModes[f[4]];
  const int range = f[5] - 0x30;

  m_result = {};
  m_result.hold = f[14] & 0x01;
  m_result.range = (f[15] & 0x02) ? "MANU" : "AUTO";
  DmmChannel &main = m_result.main();
  main.id = id;
  main.special = m.special;
  const QString prefix = QString::fromUtf8(m.prefix) + prefixFor(m.ladder, range);
  main.unit = prefix + QString::fromUtf8(m.unit);
  if (m.name)
    main.name = QCoreApplication::translate("UT8803", m.name);
  if (f[15] & 0x01)
    main.states |= SampleFlag::Relative;
  if (f[16] & 0x01)
    main.states |= SampleFlag::Min;
  if (f[16] & 0x02)
    main.states |= SampleFlag::Max;

  // the display as sent, the sign apart (spec §2.3)
  QString shown = QString::fromLatin1(reinterpret_cast<const char *>(f + 7), 5);
  shown.remove(QChar('\0')).remove(' ');
  const bool minus = f[14] & 0x08;
  bool number = false;
  const double value = shown.toDouble(&number);
  if ((f[14] & 0x04) || shown.contains(QLatin1String("OL")))
  {
    main.val = minus ? QStringLiteral("-OL") : QStringLiteral("OL");
    main.dval = 0;
  }
  else if (!number)
  {
    main.val = shown.isEmpty() ? QStringLiteral("----") : shown;
    main.noValue = true;
  }
  else
  {
    if (minus && !shown.startsWith('-'))
      shown.prepend('-');
    main.val = shown;
    main.dval = (minus && value > 0 ? -value : value) * prefixFactor(prefix);
  }
  return m_result;
}
