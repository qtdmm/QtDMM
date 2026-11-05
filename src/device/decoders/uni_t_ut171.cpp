// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut171.h"

#include "core/sampletypes.h"

#include <QCoreApplication>
#include <cmath>
#include <cstring>

static const bool registered = []() {
  // the UT-D09 cable at 9600 8N1, the UT171B/C also the UT-D07B adapter
  // (baud 0: the Bluetooth entry); none tried. The main value and the
  // frequency beside V AC
  DmmDecoder::addConfig({"Uni-Trend", "UT171A *", "", 9600, FrameFormat::UniTUT171, 8, 1, 1, 0, 40000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT171B *", "", 9600, FrameFormat::UniTUT171, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT171C *", "", 9600, FrameFormat::UniTUT171, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT171B/C (UT-D07B Bluetooth) *", "", 0, FrameFormat::UniTUT171, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  return true;
}();

namespace
{
constexpr quint8 kMeasurement = 0x02;
constexpr int kPayload = 15;   // type, flags, frame type, mode, range, float, 2 status, float

// the modes 0x01-0x1D (spec §6); 0x24 NCV apart
struct Mode
{
  const char *special;
  const char *prefix;
  const char *unit;
  const char *name;   ///< the value's own name, translated; null: by what it measures
};
const Mode kModes[0x1D] = {
  { "AC", "", "V", QT_TRANSLATE_NOOP("UT171", "LoZ") },
  { "DC", "", "V", nullptr },  { "AC", "", "V", nullptr },  { "ACDC", "", "V", nullptr },
  { "DC", "m", "V", nullptr }, { "AC", "m", "V", nullptr }, { "ACDC", "m", "V", nullptr },
  { "BUZ", "", "Ohm", nullptr }, { "CA", "", "F", nullptr }, { "OH", "", "Ohm", nullptr }, { "DI", "", "V", nullptr },
  { "TE", "", "C", nullptr },  { "TE", "", "dF", nullptr }, { "", "n", "S", nullptr },
  { "HZ", "", "Hz", nullptr }, { "DU", "", "%", nullptr },
  { "DC", "µ", "A", nullptr }, { "AC", "µ", "A", nullptr }, { "ACDC", "µ", "A", nullptr },
  { "DC", "m", "A", nullptr }, { "AC", "m", "A", nullptr }, { "ACDC", "m", "A", nullptr },
  { "DC", "", "A", nullptr },  { "AC", "", "A", nullptr },  { "ACDC", "", "A", nullptr },
  { "AC", "", "V", QT_TRANSLATE_NOOP("UT171", "VFC") },
  { "", "", "%", QT_TRANSLATE_NOOP("UT171", "4-20 mA loop") },
  { "DC", "", "A", nullptr },  { "AC", "", "A", nullptr },   // the 600 A clamp (UT171C)
};
constexpr quint8 kNcv = 0x24;

float floatAt(const QByteArray &d, int i)
{
  const quint32 bits = quint32(quint8(d[i])) | (quint32(quint8(d[i + 1])) << 8) | (quint32(quint8(d[i + 2])) << 16)
                       | (quint32(quint8(d[i + 3])) << 24);
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

// the float as the meter would show it: no decimals are sent, so the
// shortest form that gives the float back; no exponent, a letter in the text
// says overload
QString shown(float f)
{
  const QString text = QString::number(double(f), 'g', 7);
  if (!text.contains('e'))
    return text;
  const int decimals = qMax(0, 6 - int(std::floor(std::log10(std::fabs(double(f))))));
  QString plain = QString::number(double(f), 'f', decimals);
  if (plain.contains('.'))
  {
    while (plain.endsWith('0'))
      plain.chop(1);
    if (plain.endsWith('.'))
      plain.chop(1);
  }
  return plain;
}
}

std::optional<DmmDecoder::DmmResponse> DecoderUT171::decode(const QByteArray &data, int id)
{
  const QByteArray d = payloadOf(data);
  // only the measurements; the short replies of the same type are skipped
  if (d.size() < kPayload || quint8(d[0]) != kMeasurement)
    return std::nullopt;
  const quint8 flags = quint8(d[1]), mode = quint8(d[3]), range = quint8(d[4]);
  if (mode != kNcv && (mode == 0 || mode > 0x1D))
    return std::nullopt;
  heard();

  m_result = {};
  m_result.hold = flags & 0x80;
  m_result.range = (flags & 0x40) ? "MANU" : "AUTO";
  m_result.lowBat = flags & 0x04;
  DmmChannel &main = m_result.main();
  main.id = id;
  const float value = floatAt(d, 5);
  if (mode == kNcv)
  {
    // the field strength as a level, no unit
    main.special = "NCV";
    main.val = std::isfinite(value) ? QString::number(int(value)) : QStringLiteral("----");
    main.noValue = true;
    return m_result;
  }

  const Mode &m = kModes[mode - 1];
  main.special = m.special;
  QString prefix = QString::fromUtf8(m.prefix);
  if (mode == 0x0A && range >= 2)
    prefix = range >= 5 ? QStringLiteral("M") : QStringLiteral("k");   // range-relative (spec §5.1)
  main.unit = prefix + QString::fromUtf8(m.unit);
  if (m.name)
    main.name = QCoreApplication::translate("UT171", m.name);
  if (!std::isfinite(value))
  {
    main.val = value < 0 ? QStringLiteral("-OL") : QStringLiteral("OL");
    main.dval = 0;
  }
  else
  {
    main.val = shown(value);
    main.dval = main.val.toDouble() * prefixFactor(prefix);
  }

  // the frequency beside V AC and mV AC, in kHz (spec §5.1); empty while
  // the meter sends none
  if (mode == 0x03 || mode == 0x06)
  {
    DmmChannel &c = m_result.addChannel(id + 1);
    c.special = "HZ";
    c.unit = QStringLiteral("kHz");
    const float aux = floatAt(d, 11);
    if (std::isfinite(aux) && aux != 0.0f)
    {
      c.val = shown(aux);
      c.dval = c.val.toDouble() * 1e3;
    }
  }
  return m_result;
}

QByteArray DecoderUT171::pollRequest() const
{
  // connect: the stream on while none comes (spec §4.3)
  return silent() ? command(0x0A, QByteArray(1, '\x01')) : QByteArray();
}
