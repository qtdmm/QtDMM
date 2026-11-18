// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut382a.h"

#include <cmath>
#include <cstring>

#include "core/sampletypes.h"

// No meter has been tried: the "*".
static const bool registered = []() {
  DmmDecoder::addConfig({"Uni-Trend", "UT382A *", "", 0, FrameFormat::UniTUT382A, 8, 1, 1, 0, 400000, 0, 0, 0, "", "", 2});
  return true;
}();

namespace
{
constexpr quint8 kLive = 0x01;
constexpr quint8 kSettings = 0x02;
constexpr quint8 kHold = 0x03;
constexpr quint8 kStart = 0x04;
constexpr quint8 kStatus = 0x0C;
constexpr int kLiveLength = 19;
constexpr int kStatusLength = 11;
constexpr int kStartLength = 13;   // up to T3, without the check
constexpr double kLuxToFc = 0.093;   // UNI-T's app

enum Mode
{
  Normal = 0, Max = 1, Min = 2, Avg = 3, ZeroCal = 4, Countdown = 5, Difference = 6, Integral = 7, Intensity = 8
};

// A frame from @p p to @p end: AA BB, its length byte, length + 3 bytes
bool frameAt(const unsigned char *b, int p, int end)
{
  return end - p >= 6 && b[p] == 0xAA && b[p + 1] == 0xBB && p + 3 + b[p + 2] == end;
}

float floatBigEndian(const unsigned char *b)
{
  const quint32 bits = (quint32(b[0]) << 24) | (quint32(b[1]) << 16) | (quint32(b[2]) << 8) | quint32(b[3]);
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

// the decimals of UNI-T's app: two below 100, one below 1000, else none
void setValue(DmmDecoder::DmmChannel &c, double v, const QString &unit)
{
  const double a = std::abs(v);
  c.unit = unit;
  c.val = QString::number(v, 'f', a < 100 ? 2 : a < 1000 ? 1 : 0);
  c.dval = c.val.toDouble();
}
}

QByteArray DecoderUT382A::checkStatus()
{
  return QByteArray::fromHex("aabb04400001a9");
}

QByteArray DecoderUT382A::pollRequest() const
{
  return QByteArray::fromHex("aabb04390001a2");   // "start data": one live frame
}

bool DecoderUT382A::checkFormat(const char *data, size_t idx)
{
  // the bytes since the last frame start at 0; a frame ends here when one
  // starts somewhere before and its length fits
  const auto *b = reinterpret_cast<const unsigned char *>(data);
  const int end = int(idx) + 1;
  for (int p = 0; p + 6 <= end; ++p)
    if (frameAt(b, p, end))
      return true;
  return false;
}

std::optional<DmmDecoder::DmmResponse> DecoderUT382A::decode(const QByteArray &data, int id)
{
  const auto *b = reinterpret_cast<const unsigned char *>(data.constData());
  const int end = int(data.size());
  int p = 0;
  while (p + 6 <= end && !frameAt(b, p, end))
    ++p;
  if (p + 6 > end)
    return std::nullopt;
  const unsigned char *f = b + p;
  const int length = end - p;

  switch (f[3])
  {
    case kSettings:
      if (length >= 8)
        m_fc = f[4] & 0x80;
      return std::nullopt;
    case kStatus:
      if (length >= kStatusLength)
      {
        m_hold = f[4] == 1;
        m_fc = f[5] & 0x80;
      }
      return std::nullopt;
    case kHold:
      if (length >= 7)
        m_hold = f[4] == 1;
      return std::nullopt;
    case kStart:
      if (length >= kStartLength)
      {
        m_hold = false;   // as the app does
        m_distanceCm = int(qint32((quint32(f[6]) << 24) | (quint32(f[7]) << 16) | (quint32(f[8]) << 8) | quint32(f[9])));
      }
      return std::nullopt;
    case kLive:
      if (length >= kLiveLength)
        break;
      return std::nullopt;
    default:
      return std::nullopt;   // serial number, stored readings, version, ...
  }

  const int mode = f[4];
  if (mode == ZeroCal || mode > Intensity)
    return std::nullopt;   // the zero offset, or a mode the notes do not have
  if (mode != Intensity)
    m_distanceCm = 0;      // the start frame's number was something else

  m_result = {};
  m_result.channels.resize(1);
  DmmChannel &c = m_result.main();
  c.id = id;
  m_result.hold = m_hold;
  m_result.showBar = true;
  const QString lux = m_fc ? QStringLiteral("fc") : QStringLiteral("lx");
  switch (mode)
  {
    case Max: c.states |= SampleFlag::Max; break;
    case Min: c.states |= SampleFlag::Min; break;
    case Avg: c.states |= SampleFlag::Avg; break;
    default: break;
  }
  const quint8 flags = f[5];
  if (flags & 0x80)
  {
    c.unit = lux;
    c.val = QStringLiteral("OL");
    return m_result;
  }
  if (flags & 0x40)
  {
    c.unit = lux;
    c.val = QStringLiteral("----");
    c.noValue = true;
    return m_result;
  }

  const double v1 = std::abs(double(floatBigEndian(f + 6)));
  const double shown = m_fc ? v1 * kLuxToFc : v1;
  if (mode == Difference)
  {
    // value 1 is the amount, bit 5 its sign
    setValue(c, (flags & 0x20) ? -shown : shown, lux);
    return m_result;
  }
  if (mode == Integral)
  {
    setValue(c, shown, lux);
    m_result.channels.resize(2);
    DmmChannel &e = m_result.channels[1];
    e.id = id + 1;
    setValue(e, std::abs(double(floatBigEndian(f + 10))), QStringLiteral("lx·h"));
    return m_result;
  }
  if (mode == Intensity && m_distanceCm > 0)
  {
    // cd = lx x m²; the lux as the second value
    const double metres = m_distanceCm / 100.0;
    setValue(c, v1 * metres * metres, QStringLiteral("cd"));
    m_result.channels.resize(2);
    DmmChannel &l = m_result.channels[1];
    l.id = id + 1;
    setValue(l, shown, lux);
    return m_result;
  }
  setValue(c, shown, lux);
  return m_result;
}

QByteArray DecoderUT382A::keyRequest(const QString &key) const
{
  if (key == QLatin1String("hold"))
    return QByteArray::fromHex(m_hold ? "aabb043100019a" : "aabb043101019b");
  return QByteArray();
}
