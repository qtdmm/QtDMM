// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut325f.h"

#include <cstring>
#include <utility>

#include "core/sampletypes.h"

// No meter has been tried: the "*".
static const bool registered = []() {
  DmmDecoder::addConfig({"Uni-Trend", "UT325F *", "", 0, FrameFormat::UniTUT325F, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 4});
  return true;
}();

namespace
{
constexpr quint8 kLive = 0x01;
constexpr quint8 kPanel = 0x06;
constexpr quint8 kGetPanel = 0x35;
constexpr quint8 kSetPanel = 0x36;
constexpr int kLiveLength = 36;
constexpr int kPanelLength = 23;
constexpr int kPanelFields = 13;   // live output ... mains filter
// fields of the panel block
constexpr int kLiveOut = 0;
constexpr int kHold = 1;
constexpr int kBle = 2;
constexpr int kMode = 9;
constexpr int kUnit = 10;

// A frame from @p p to @p end: AA 55, a length (BE16), length + 4 bytes
bool frameAt(const unsigned char *b, int p, int end)
{
  if (end - p < 7 || b[p] != 0xAA || b[p + 1] != 0x55)
    return false;
  const int length = (b[p + 2] << 8) | b[p + 3];
  return length > 0 && p + 4 + length == end;
}

float floatLittleEndian(const unsigned char *b)
{
  const quint32 bits = (quint32(b[3]) << 24) | (quint32(b[2]) << 16) | (quint32(b[1]) << 8) | quint32(b[0]);
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

// one probe: its °C in the meter's unit, or the status' word
void fill(DmmDecoder::DmmChannel &c, int probe, const unsigned char *celsius, quint8 status, int unit)
{
  static const char *const units[] = { "C", "dF", "K" };
  static const char types[] = "KJTERSNB";
  const int type = status & 0x0F;
  c.name = type < 8 ? QStringLiteral("T%1 (%2)").arg(probe).arg(QLatin1Char(types[type])) : QStringLiteral("T%1").arg(probe);
  c.special = QStringLiteral("TE");
  c.unit = QString::fromLatin1(units[unit]);
  switch (status >> 4)
  {
    case 0: break;
    case 1: c.val = QStringLiteral("-OL"); return;
    case 2: c.val = QStringLiteral("OL"); return;
    default: c.val = QStringLiteral("----"); c.noValue = true; return;
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

QByteArray DecoderUT325F::frame(quint8 type, const QByteArray &payload)
{
  const int length = int(payload.size()) + 3;
  QByteArray f = QByteArray::fromHex("aa55");
  f.append(char(length >> 8)).append(char(length & 0xFF)).append(char(type)).append(payload);
  appendSum16(f);
  return f;
}

QByteArray DecoderUT325F::getPanel()
{
  return frame(kGetPanel);
}

QByteArray DecoderUT325F::setPanel(int field, quint8 value) const
{
  if (m_panel.size() != kPanelFields)
    return QByteArray();
  QByteArray p = m_panel;
  p[kLiveOut] = 1;
  p[kBle] = 1;
  if (field >= 0)
    p[field] = char(value);
  return frame(kSetPanel, p);
}

QByteArray DecoderUT325F::takeCommand()
{
  return std::exchange(m_pending, QByteArray());
}

bool DecoderUT325F::checkFormat(const char *data, size_t idx)
{
  // the bytes since the last frame start at 0; a frame ends here when one
  // starts somewhere before and its length fits
  const auto *b = reinterpret_cast<const unsigned char *>(data);
  const int end = int(idx) + 1;
  for (int p = 0; p + 7 <= end; ++p)
    if (frameAt(b, p, end))
      return true;
  return false;
}

std::optional<DmmDecoder::DmmResponse> DecoderUT325F::decode(const QByteArray &data, int id)
{
  const auto *b = reinterpret_cast<const unsigned char *>(data.constData());
  const int end = int(data.size());
  int p = 0;
  while (p + 7 <= end && !frameAt(b, p, end))
    ++p;
  if (p + 7 > end)
    return std::nullopt;
  const unsigned char *f = b + p;
  const int length = end - p;

  if (f[4] == kPanel && length >= kPanelLength)
  {
    m_panel = QByteArray(reinterpret_cast<const char *>(f + 5), kPanelFields);
    m_battery = batteryOfLevel(f[18], 5);   // after the fields: BATT_Status 0..5, 5 full
    // the live stream off (as after switching the meter on): on, once
    if (m_panel[kLiveOut] != 1 || m_panel[kBle] != 1)
      m_pending = setPanel();
    return std::nullopt;
  }
  if (f[4] != kLive || length < kLiveLength)
    return std::nullopt;   // device info, stored readings, channel settings, ...

  const int unit = m_panel.size() == kPanelFields && quint8(m_panel[kUnit]) <= 2 ? quint8(m_panel[kUnit]) : 0;
  quint32 states = 0;
  switch (f[33])
  {
    case 1: states = SampleFlag::Max; break;
    case 2: states = SampleFlag::Min; break;
    case 3: states = SampleFlag::Avg; break;
    default: break;
  }
  m_result = {};
  m_result.channels.resize(4);
  for (int i = 0; i < 4; ++i)
  {
    DmmChannel &c = m_result.channels[i];
    c.id = id + i;
    c.states = states;
    fill(c, i + 1, f + 5 + 4 * i, f[21 + i], unit);
  }
  m_result.hold = m_panel.size() == kPanelFields && m_panel[kHold] == 1;
  m_result.battery = m_battery;
  return m_result;
}

bool DecoderUT325F::hasKey(const QString &key, const QString &model) const
{
  Q_UNUSED(model);
  // the keys need the panel, which comes only after connecting
  return key == QLatin1String("hold") || key == QLatin1String("minmax") || key == QLatin1String("unit");
}

QByteArray DecoderUT325F::keyRequest(const QString &key) const
{
  if (m_panel.size() != kPanelFields)
    return QByteArray();
  if (key == QLatin1String("hold"))
    return setPanel(kHold, m_panel[kHold] == 1 ? 0 : 1);
  if (key == QLatin1String("minmax"))
    return setPanel(kMode, quint8((quint8(m_panel[kMode]) + 1) % 4));
  if (key == QLatin1String("unit"))
    return setPanel(kUnit, quint8((quint8(m_panel[kUnit]) + 1) % 3));
  return QByteArray();
}
