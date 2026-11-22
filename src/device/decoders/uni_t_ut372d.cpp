// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut372d.h"

#include <QCoreApplication>
#include <cmath>

#include "core/sampletypes.h"

// No meter has been tried: the "*".
static const bool registered = []() {
  DmmDecoder::addConfig({"Uni-Trend", "UT372D *", "", 0, FrameFormat::UniTUT372D, 8, 1, 1, 0, 100000, 0, 0, 0, "", "", 4});
  return true;
}();

namespace
{
constexpr int kLiveLength = 35;
constexpr int kDisplays = 7;      // the first of the four displays
constexpr int kDisplaySize = 6;
constexpr quint8 kSetUnit = 0x31;
constexpr quint8 kSetStatus = 0x32;

// A frame from @p p to @p end: AA BB, its length byte, length + 3 bytes
bool frameAt(const unsigned char *b, int p, int end)
{
  return end - p >= 6 && b[p] == 0xAA && b[p + 1] == 0xBB && p + 3 + b[p + 2] == end;
}

// one display: overflow mark, decimals, int32 BE = value x 100
void fill(DmmDecoder::DmmChannel &c, const unsigned char *d, const QString &unit)
{
  c.unit = unit;
  if (d[0] != 0)
  {
    c.val = QStringLiteral("OL");
    return;
  }
  const qint32 raw = qint32((quint32(d[2]) << 24) | (quint32(d[3]) << 16) | (quint32(d[4]) << 8) | quint32(d[5]));
  const int decimals = qBound(0, int(d[1]), 6);
  c.val = QString::number(raw / 100.0, 'f', decimals);
  c.dval = c.val.toDouble();
}
}

QString DecoderUT372D::unitName(int mode, int unit)
{
  static const char *const rotation[] = { "RPM", "Hz", "REV" };
  static const char *const wheel[] = { "m/min", "in/min", "ft/min", "m", "in", "ft" };
  if (mode <= 1)
    return unit >= 0 && unit < 3 ? QString::fromLatin1(rotation[unit]) : QString();
  if (mode <= 3)
    return unit >= 0 && unit < 6 ? QString::fromLatin1(wheel[unit]) : QString();
  return QString();
}

bool DecoderUT372D::checkFormat(const char *data, size_t idx)
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

std::optional<DmmDecoder::DmmResponse> DecoderUT372D::decode(const QByteArray &data, int id)
{
  const auto *b = reinterpret_cast<const unsigned char *>(data.constData());
  const int end = int(data.size());
  int p = 0;
  while (p + 6 <= end && !frameAt(b, p, end))
    ++p;
  if (p + 6 > end || end - p < kLiveLength)
    return std::nullopt;
  const unsigned char *f = b + p;
  if (f[3] != 1 && f[3] != 5)
    return std::nullopt;
  const QString unit = unitName(f[4], f[5]);
  if (unit.isEmpty())
    return std::nullopt;   // a probe or unit the notes do not have
  m_mode = f[4];
  m_unit = f[5];
  m_measuring = f[31] == 1;

  m_result = {};
  m_result.channels.resize(4);
  for (int i = 0; i < 4; ++i)
  {
    m_result.channels[i].id = id + i;
    fill(m_result.channels[i], f + kDisplays + i * kDisplaySize, unit);
  }
  // the words of the UT181A, translated there
  m_result.channels[1].states |= SampleFlag::Max;
  m_result.channels[1].name = QCoreApplication::translate("UT181A", "Max");
  m_result.channels[2].states |= SampleFlag::Min;
  m_result.channels[2].name = QCoreApplication::translate("UT181A", "Min");
  m_result.channels[3].states |= SampleFlag::Avg;
  m_result.channels[3].name = QCoreApplication::translate("UT181A", "Average");
  m_result.hold = !m_measuring;
  m_result.lowBat = f[32] == 1;
  m_result.showBar = true;
  return m_result;
}

QByteArray DecoderUT372D::setFrame(quint8 type, quint8 mode, quint8 unit, quint8 measure)
{
  QByteArray f = QByteArray::fromHex("aabb06");
  f.append(char(type)).append(char(mode)).append(char(unit)).append(char(measure));
  appendSum16(f);
  return f;
}

QByteArray DecoderUT372D::keyRequest(const QString &key) const
{
  if (key == QLatin1String("hold"))
    return setFrame(kSetStatus, quint8(m_mode), quint8(m_unit), m_measuring ? 0 : 1);
  if (key == QLatin1String("unit"))
    return setFrame(kSetUnit, quint8(m_mode), quint8((m_unit + 1) % (m_mode <= 1 ? 3 : 6)), 0);
  return QByteArray();
}
