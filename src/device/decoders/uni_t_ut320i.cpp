// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut320i.h"

#include <cstring>

// No meter has been tried: the "*".
static const bool registered = []() {
  DmmDecoder::addConfig({"Uni-Trend", "UT320i *", "", 0, FrameFormat::UniTUT320i, 8, 1, 1, 0, 2000, 0, 0, 0, "", "", 2});
  return true;
}();

namespace
{
constexpr quint8 kLive = 0x01;
constexpr quint8 kSettings = 0x06;
constexpr quint8 kDefaultLive = 0x11;
constexpr int kLivePayload = 17;
constexpr int kDefaultPayload = 15;
constexpr int kSettingsPayload = 7;

// A frame from @p p to @p end: AA BB, its length byte, length + 3 bytes,
// the 16-bit sum right
bool frameAt(const unsigned char *b, int p, int end)
{
  return end - p >= 6 && b[p] == 0xAA && b[p + 1] == 0xBB && p + 3 + b[p + 2] == end
         && DmmDecoder::sum16Valid(b + p, end - p);
}

float floatLittleEndian(const unsigned char *b)
{
  const quint32 bits = (quint32(b[3]) << 24) | (quint32(b[2]) << 16) | (quint32(b[1]) << 8) | quint32(b[0]);
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

// one probe: its °C in the meter's unit, or the flag's word
void fill(DmmDecoder::DmmChannel &c, const unsigned char *celsius, int flag, int unit)
{
  static const char *const units[] = { "C", "dF", "K" };
  c.special = QStringLiteral("TE");
  c.unit = QString::fromLatin1(units[unit]);
  switch (flag)
  {
    case 1: c.val = QStringLiteral("OL"); return;
    case 2: c.val = QStringLiteral("-OL"); return;
    case 3: c.val = QStringLiteral("----"); c.noValue = true; return;
    default: break;
  }
  double t = floatLittleEndian(celsius);
  if (unit == 1)
    t = t * 1.8 + 32;
  else if (unit == 2)
    t += 273.15;
  c.val = QString::number(t, 'f', 1);
  c.dval = c.val.toDouble();
}
}

QByteArray DecoderUT320i::settingsRequest()
{
  QByteArray f = QByteArray::fromHex("aabb043106");
  appendSum16(f);
  return f;
}

bool DecoderUT320i::checkFormat(const char *data, size_t idx)
{
  // the bytes since the last frame start at 0; a frame ends here when one
  // starts somewhere before, its length fits and its sum is right
  const auto *b = reinterpret_cast<const unsigned char *>(data);
  const int end = int(idx) + 1;
  for (int p = 0; p + 6 <= end; ++p)
    if (frameAt(b, p, end))
      return true;
  return false;
}

std::optional<DmmDecoder::DmmResponse> DecoderUT320i::decode(const QByteArray &data, int id)
{
  const auto *b = reinterpret_cast<const unsigned char *>(data.constData());
  const int end = int(data.size());
  int p = 0;
  while (p + 6 <= end && !frameAt(b, p, end))
    ++p;
  if (p + 6 > end)
    return std::nullopt;
  const unsigned char *f = b + p;
  const unsigned char *payload = f + 4;
  const int payloadLength = end - p - 6;

  switch (f[3])
  {
    case kSettings:
      if (payloadLength >= kSettingsPayload && payload[1] <= 2)
        m_unit = payload[1];
      return std::nullopt;
    case kLive:
      if (payloadLength < kLivePayload)
        return std::nullopt;
      break;
    case kDefaultLive:
      if (payloadLength < kDefaultPayload)
        return std::nullopt;
      break;
    default:
      return std::nullopt;   // stored readings, clock, version, ...
  }

  m_result = {};
  m_result.channels.resize(2);
  m_result.channels[0].id = id;
  m_result.channels[0].name = QStringLiteral("T1");
  m_result.channels[1].id = id + 1;
  m_result.channels[1].name = QStringLiteral("T2");
  if (f[3] == kLive)
  {
    fill(m_result.channels[0], payload + 8, (payload[16] >> 2) & 3, m_unit);
    fill(m_result.channels[1], payload + 12, payload[16] & 3, m_unit);
  }
  else
  {
    // no probe chosen: one temperature, the second value cleared
    fill(m_result.channels[0], payload + 10, payload[14] & 3, m_unit);
    m_result.channels[1].name.clear();
  }
  return m_result;
}
