// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut181a.h"

#include "core/sampletypes.h"

#include <QCoreApplication>
#include <cstring>

static const bool registered = []() {
  // the UT-D09 cable at 9600 8N1, or the UT-D07B adapter (baud 0: the
  // Bluetooth entry); not tried here. Main value, two aux values and in
  // MIN/MAX a fourth (spec §5.3)
  DmmDecoder::addConfig({"Uni-Trend", "UT181A *", "", 9600, FrameFormat::UniTUT181A, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 4});
  DmmDecoder::addConfig({"Uni-Trend", "UT181A (UT-D07B Bluetooth) *", "", 0, FrameFormat::UniTUT181A, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 4});
  return true;
}();

namespace
{
constexpr quint8 kMeasurement = 0x02;
constexpr int kFull = 13;    // float32, precision, 8-byte unit
constexpr int kShort = 5;    // float32, precision
constexpr int kUnit = 8;
constexpr int kValues = 6;   // where the values start: type, misc, misc2, mode (2), range

// a frame ending at @p end (exclusive) that starts at @p p: header, length,
// sum (spec §3)
bool frameAt(const unsigned char *b, int p, int end)
{
  if (end - p < 7 || b[p] != 0xAB || b[p + 1] != 0xCD)
    return false;
  const int length = b[p + 2] | (b[p + 3] << 8);
  if (length < 3 || p + 4 + length != end)
    return false;
  quint16 sum = 0;
  for (int i = p + 2; i < end - 2; ++i)
    sum = quint16(sum + b[i]);
  return sum == quint16(b[end - 2] | (b[end - 1] << 8));
}

float floatAt(const QByteArray &d, int i)
{
  const quint32 bits = quint32(quint8(d[i])) | (quint32(quint8(d[i + 1])) << 8) | (quint32(quint8(d[i + 2])) << 16)
                       | (quint32(quint8(d[i + 3])) << 24);
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

QString unitAt(const QByteArray &d, int i)
{
  const QByteArray raw = d.mid(i, kUnit);
  const int end = raw.indexOf('\0');
  return QString::fromLatin1(end < 0 ? raw : raw.left(end));   // 0xB0: the degree sign
}

// the unit string (spec §8) as QtDMM spells it, the prefix apart, and the
// coupling or function; @p family is the mode word's high byte (§6)
struct Unit
{
  QString prefix, base, special;
};
Unit unitOf(QString raw, int family)
{
  Unit u;
  if (raw.endsWith(QLatin1String("ac+dc")))
  {
    u.special = "ACDC";
    raw.chop(5);
  }
  else if (raw.size() > 2 && raw.endsWith(QLatin1String("DC")))
  {
    u.special = "DC";
    raw.chop(2);
  }
  else if (raw.size() > 2 && raw.endsWith(QLatin1String("AC")))
  {
    u.special = "AC";
    raw.chop(2);
  }
  if (raw == QLatin1String("dBV") || raw == QLatin1String("dBm") || raw == QLatin1String("%"))
    u.base = raw;
  else if (raw == QStringLiteral("°C"))
    u.base = "C";
  else if (raw == QStringLiteral("°F"))
    u.base = "dF";
  else
  {
    static const QLatin1String bases[] = { QLatin1String("~"), QLatin1String("Hz"), QLatin1String("V"), QLatin1String("A"),
                                           QLatin1String("S"), QLatin1String("F"), QLatin1String("s") };
    for (const QLatin1String b : bases)
      if (raw.endsWith(b) && raw.size() - b.size() <= 1)
      {
        u.base = b == QLatin1String("~") ? QStringLiteral("Ohm") : QString(b);
        u.prefix = raw.left(raw.size() - b.size());
        if (u.prefix == QLatin1String("u"))
          u.prefix = QStringLiteral("µ");
        break;
      }
    if (u.base.isEmpty())
      u.base = raw;
  }
  if (!u.special.isEmpty())
    return u;
  // the function by the unit, the dial where the unit does not say it
  if (u.base == QLatin1String("Hz"))
    u.special = "HZ";
  else if (u.base == QLatin1String("%"))
    u.special = "DU";
  else if (u.base == QLatin1String("C") || u.base == QLatin1String("dF"))
    u.special = "TE";
  else if (u.base == QLatin1String("F"))
    u.special = "CA";
  else if (u.base == QLatin1String("Ohm"))
    u.special = family == 0x52 ? "BUZ" : "OH";
  else if (u.base == QLatin1String("V") && family == 0x61)
    u.special = "DI";
  else if (u.base == QLatin1String("s") && family == 0x73)
    u.special = "HZ";   // the pulse width, on the frequency dial
  return u;
}

// one value: float32 and precision at @p at, in @p unit (spec §5.2)
void fill(DmmDecoder::DmmChannel &c, const QByteArray &d, int at, const QString &unit, int family,
          double (*factor)(const QString &))
{
  const float v = floatAt(d, at);
  const quint8 precision = quint8(d[at + 4]);
  const Unit u = unitOf(unit, family);
  c.special = u.special;
  c.unit = u.prefix + u.base;
  const bool ol = precision & 0x01, negOl = precision & 0x02;
  if (ol && negOl && v == 0.0f)
  {
    // both bits over 0.0: no reading yet (after a range or mode change)
    c.val = QStringLiteral("----");
    c.noValue = true;
  }
  else if (ol || negOl)
  {
    c.val = negOl && !ol ? QStringLiteral("-OL") : QStringLiteral("OL");
    c.dval = 0;
  }
  else
  {
    // the value as the meter shows it, to its decimals; the float holds more
    c.val = QString::number(double(v), 'f', precision >> 4);
    c.dval = c.val.toDouble() * factor(u.prefix);
  }
}
}

QByteArray DecoderUT181A::command(quint8 opcode, const QByteArray &data)
{
  // AB CD, the length (opcode, data and sum), the opcode and data, the sum
  // of everything after the header (spec §3)
  QByteArray frame("\xAB\xCD", 2);
  const int length = int(data.size()) + 3;
  frame.append(char(length & 0xFF)).append(char(length >> 8)).append(char(opcode)).append(data);
  quint16 sum = 0;
  for (int i = 2; i < frame.size(); ++i)
    sum = quint16(sum + quint8(frame[i]));
  frame.append(char(sum & 0xFF)).append(char(sum >> 8));
  return frame;
}

bool DecoderUT181A::checkFormat(const char *data, size_t idx)
{
  // a variable-length protocol: the bytes since the last frame start at 0;
  // a frame ends here when one starts somewhere before and its length and
  // sum fit (bytes before it are dropped with it)
  const auto *b = reinterpret_cast<const unsigned char *>(data);
  const int end = int(idx) + 1;
  for (int p = 0; p + 7 <= end; ++p)
    if (frameAt(b, p, end))
      return true;
  return false;
}

std::optional<DmmDecoder::DmmResponse> DecoderUT181A::decode(const QByteArray &data, int id)
{
  const auto *b = reinterpret_cast<const unsigned char *>(data.constData());
  int p = 0;
  while (p + 7 <= data.size() && !frameAt(b, p, int(data.size())))
    ++p;
  if (p + 7 > data.size())
    return std::nullopt;
  const QByteArray d = data.mid(p + 4, data.size() - p - 6);   // the payload
  // only the measurements; the replies to commands ("OK", "ER") are skipped
  if (d.size() < kValues + kShort || quint8(d[0]) != kMeasurement)
    return std::nullopt;
  const quint8 misc = quint8(d[1]), misc2 = quint8(d[2]);
  const quint16 mode = quint16(quint8(d[3]) | (quint8(d[4]) << 8));
  const int family = mode >> 8;
  const int format = misc & 0x70;
  {
    QMutexLocker locker(&m_lock);
    m_lastMeasurement.start();
    m_mode = mode;
    m_minMax = format == 0x20;
  }

  m_result = {};
  m_result.channels.resize(4);
  for (int i = 0; i < 4; ++i)
    m_result.channels[i].id = id + i;
  m_result.hold = misc & 0x80;
  m_result.range = (misc2 & 0x01) ? "AUTO" : "MANU";
  auto value = [&](int ch, int at, const QString &unit) { fill(m_result.channels[ch], d, at, unit, family, &prefixFactor); };
  auto name = [&](int ch, const char *text) { m_result.channels[ch].name = QCoreApplication::translate("UT181A", text); };

  switch (format)
  {
    case 0x10:   // REL: relative, reference, absolute
      if (d.size() < kValues + 3 * kFull)
        return std::nullopt;
      for (int i = 0; i < 3; ++i)
        value(i, kValues + i * kFull, unitAt(d, kValues + i * kFull + 5));
      m_result.main().states |= SampleFlag::Relative;
      name(1, QT_TRANSLATE_NOOP("UT181A", "Reference"));
      name(2, QT_TRANSLATE_NOOP("UT181A", "Absolute"));
      break;
    case 0x20:   // MIN/MAX: current, then max, average, min with their seconds; one unit
    {
      const int unitAtOffset = kValues + kShort + 3 * (kShort + 4);
      if (d.size() < unitAtOffset + kUnit)
        return std::nullopt;
      const QString unit = unitAt(d, unitAtOffset);
      value(0, kValues, unit);
      const int slot[3] = { 1, 3, 2 };   // max, average, min -> channels max, min, average
      for (int i = 0; i < 3; ++i)
        value(slot[i], kValues + kShort + i * (kShort + 4), unit);
      m_result.channels[1].states |= SampleFlag::Max;
      m_result.channels[2].states |= SampleFlag::Min;
      m_result.channels[3].states |= SampleFlag::Avg;
      name(1, QT_TRANSLATE_NOOP("UT181A", "Max"));
      name(2, QT_TRANSLATE_NOOP("UT181A", "Min"));
      name(3, QT_TRANSLATE_NOOP("UT181A", "Average"));
      break;
    }
    case 0x40:   // PEAK: max and min
      if (d.size() < kValues + 2 * kFull)
        return std::nullopt;
      value(0, kValues, unitAt(d, kValues + 5));
      value(1, kValues + kFull, unitAt(d, kValues + kFull + 5));
      m_result.channels[0].states |= SampleFlag::Peak | SampleFlag::Max;
      m_result.channels[1].states |= SampleFlag::Peak | SampleFlag::Min;
      name(1, QT_TRANSLATE_NOOP("UT181A", "Peak min"));
      break;
    default:     // normal: main, aux1 and aux2 where present
    {
      if (d.size() < kValues + kFull)
        return std::nullopt;
      value(0, kValues, unitAt(d, kValues + 5));
      int at = kValues + kFull;
      for (int i = 1; i <= 2; ++i)
        if (misc & (1 << i))
        {
          if (d.size() < at + kFull)
            return std::nullopt;
          value(i, at, unitAt(d, at + 5));
          at += kFull;
        }
      break;
    }
  }
  return m_result;
}

QByteArray DecoderUT181A::pollRequest() const
{
  // the stream on (SET_MONITOR 1) while none comes
  QMutexLocker locker(&m_lock);
  if (m_lastMeasurement.isValid() && m_lastMeasurement.elapsed() < 2000)
    return QByteArray();
  return command(0x05, QByteArray(1, '\x01'));
}

QByteArray DecoderUT181A::keyRequest(const QString &key) const
{
  QMutexLocker locker(&m_lock);
  if (key == QLatin1String("hold"))
    return command(0x12, QByteArray(1, '\x5A'));
  // MAX/MIN is on or off only (SET_MIN_MAX): a press switches, held leaves
  if (key == QLatin1String("minmax"))
    return command(0x04, QByteArray(1, m_minMax ? '\x00' : '\x01'));
  if (key == QLatin1String("minmax_off"))
    return command(0x04, QByteArray(1, '\x00'));
  if (key == QLatin1String("auto"))
    return command(0x02, QByteArray(1, '\x00'));
  // REL is the mode word's last nibble: 1 plain, 2 relative (spec §6.1);
  // on the beeper and diode the 2 is Open and Alarm instead
  if (key == QLatin1String("rel") && m_mode != 0 && (m_mode >> 8) != 0x52 && (m_mode >> 8) != 0x61)
  {
    const quint16 mode = (m_mode & 0xFFF0) | ((m_mode & 0x0F) == 2 ? 1 : 2);
    return command(0x01, QByteArray(1, char(mode & 0xFF)).append(char(mode >> 8)));
  }
  return QByteArray();
}

bool DecoderUT181A::hasKey(const QString &key, const QString &model) const
{
  Q_UNUSED(model)
  return key == QLatin1String("hold") || key == QLatin1String("minmax") || key == QLatin1String("minmax_off")
         || key == QLatin1String("auto") || key == QLatin1String("rel");
}
