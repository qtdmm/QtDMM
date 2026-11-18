// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut381a.h"

#include <cmath>
#include <cstring>

#include "core/sampletypes.h"

// No meter has been tried: the "*".
static const bool registered = []() {
  DmmDecoder::addConfig({"Uni-Trend", "UT381A *", "", 0, FrameFormat::UniTUT381A, 8, 1, 1, 0, 200000, 0, 0, 0});
  return true;
}();

namespace
{
constexpr quint8 kTypeLive = 1;
constexpr quint8 kTypeSetting = 2;
constexpr quint8 kTypeHold = 3;
constexpr quint8 kSendHold = 0x31;
constexpr int kLiveLength = 15;
constexpr double kLuxToFc = 0.093;   // UNI-T's app (UnitUtil_Light)

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
}

bool DecoderUT381A::checkFormat(const char *data, size_t idx)
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

std::optional<DmmDecoder::DmmResponse> DecoderUT381A::decode(const QByteArray &data, int id)
{
  Q_UNUSED(id);
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
    case kTypeSetting:
      if (length >= 10)
        m_fc = f[4] & 0x80;
      return std::nullopt;
    case kTypeHold:
      if (length >= 7)
        m_hold = f[4] == 1;
      return std::nullopt;
    case kTypeLive:
      if (length >= kLiveLength)
        break;
      return std::nullopt;
    default:
      return std::nullopt;   // stored data, version, zero calibration, ...
  }

  m_result = {};
  DmmChannel &c = m_result.main();
  c.id = 0;
  c.unit = m_fc ? QStringLiteral("fc") : QStringLiteral("lx");
  m_result.hold = m_hold;
  m_result.showBar = true;
  m_result.battery = batteryOfLevel(f[5] & 0x07, 5);   // BATT_Status 0..5, 5 full
  switch (f[4])
  {
    case 1: c.states |= SampleFlag::Max; break;
    case 2: c.states |= SampleFlag::Min; break;
    case 3: c.states |= SampleFlag::Avg; break;
    default: break;
  }
  if (f[5] & 0x80)
  {
    c.val = QStringLiteral("OL");
    return m_result;
  }
  // the app shows the amount (abs()), in fc converted from lux
  const double lux = std::abs(double(floatBigEndian(f + 6)));
  const double shown = m_fc ? lux * kLuxToFc : lux;
  c.val = QString::number(shown, 'f', shown < 100 ? 2 : 1);
  c.dval = c.val.toDouble();
  return m_result;
}

QByteArray DecoderUT381A::commandFrame(quint8 type, const QByteArray &payload)
{
  QByteArray f = QByteArray::fromHex("aabb");
  f.append(char(payload.size() + 3)).append(char(type)).append(payload);
  appendSum16(f);
  return f;
}

QByteArray DecoderUT381A::keyRequest(const QString &key) const
{
  if (key == QLatin1String("hold"))
    return commandFrame(kSendHold, QByteArray(1, m_hold ? '\0' : '\1'));
  return QByteArray();
}
