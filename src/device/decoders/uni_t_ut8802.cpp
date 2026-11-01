// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut8802.h"

#include "core/sampletypes.h"

#include <QCoreApplication>

static const bool registered = []() {
  // the built-in CP2110 at 9600 8N1; none tried. 4 1/2 digits
  DmmDecoder::addConfig({"Uni-Trend", "UT8802 *", "", 9600, FrameFormat::UniTUT8802, 8, 1, 1, 0, 20000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT8802N *", "", 9600, FrameFormat::UniTUT8802, 8, 1, 1, 0, 20000, 0, 0, 0});
  return true;
}();

namespace
{
// the dial positions (spec §3.3): function and range in one code
struct Position
{
  quint8 code;
  const char *special;
  const char *prefix;   ///< the range's: the digits are in it
  const char *unit;
  const char *name;     ///< the value's own name, translated; null: by what it measures
};
const Position kPositions[] = {
  { 0x01, "DC", "m", "V", nullptr },   { 0x03, "DC", "", "V", nullptr },    { 0x04, "DC", "", "V", nullptr },
  { 0x05, "DC", "", "V", nullptr },    { 0x06, "DC", "", "V", nullptr },    { 0x09, "AC", "", "V", nullptr },
  { 0x0A, "AC", "", "V", nullptr },    { 0x0B, "AC", "", "V", nullptr },    { 0x0C, "AC", "", "V", nullptr },
  { 0x0D, "DC", "µ", "A", nullptr },   { 0x0E, "DC", "m", "A", nullptr },   { 0x10, "AC", "m", "A", nullptr },
  { 0x11, "DC", "m", "A", nullptr },   { 0x12, "DC", "m", "A", nullptr },   { 0x13, "AC", "m", "A", nullptr },
  { 0x14, "AC", "m", "A", nullptr },   { 0x16, "DC", "", "A", nullptr },    { 0x18, "AC", "", "A", nullptr },
  { 0x19, "OH", "", "Ohm", nullptr },  { 0x1A, "OH", "k", "Ohm", nullptr }, { 0x1B, "OH", "k", "Ohm", nullptr },
  { 0x1C, "OH", "k", "Ohm", nullptr }, { 0x1D, "OH", "M", "Ohm", nullptr }, { 0x1F, "OH", "M", "Ohm", nullptr },
  { 0x22, "DU", "", "%", nullptr },    { 0x23, "DI", "", "V", nullptr },    { 0x24, "BUZ", "", "Ohm", nullptr },
  { 0x25, "HFE", "", "", nullptr },    { 0x27, "CA", "n", "F", nullptr },   { 0x28, "CA", "µ", "F", nullptr },
  { 0x29, "CA", "m", "F", nullptr },   { 0x2A, "", "", "V", QT_TRANSLATE_NOOP("UT8802", "SCR") },
  { 0x2B, "HZ", "", "Hz", nullptr },   { 0x2C, "HZ", "k", "Hz", nullptr },  { 0x2D, "HZ", "M", "Hz", nullptr },
};

const Position *positionOf(quint8 code)
{
  for (const Position &p : kPositions)
    if (p.code == code)
      return &p;
  return nullptr;
}

// the five digits, the first one first (spec §3.2)
void digitsOf(const unsigned char *f, int digits[5])
{
  digits[0] = f[4] & 0x0F;
  digits[1] = f[3] >> 4;
  digits[2] = f[3] & 0x0F;
  digits[3] = f[2] >> 4;
  digits[4] = f[2] & 0x0F;
}
}

bool DecoderUT8802::checkFormat(const char *data, size_t idx)
{
  // no sum: the header, a known position, the point and digits that fit
  if (idx < 7)
    return false;
  unsigned char f[8];
  lastBytes(data, idx, 8, f);
  if (f[0] != 0xAC || !positionOf(f[1]) || (f[5] & 0x0F) > 4)
    return false;
  int digits[5];
  digitsOf(f, digits);
  for (int d : digits)
    if (d > 0x0A && d != 0x0C)
      return false;
  return true;
}

std::optional<DmmDecoder::DmmResponse> DecoderUT8802::decode(const QByteArray &data, int id)
{
  if (data.size() < 8)
    return std::nullopt;
  const auto *f = reinterpret_cast<const unsigned char *>(data.constData()) + data.size() - 8;
  const Position *p = positionOf(f[1]);
  if (f[0] != 0xAC || !p)
    return std::nullopt;
  const quint8 status = f[7];
  const int point = f[5] & 0x0F;

  m_result = {};
  m_result.hold = status & 0x10;
  m_result.range = (status & 0x04) ? "MANU" : "AUTO";
  DmmChannel &main = m_result.main();
  main.id = id;
  main.special = p->special;
  const QString prefix = QString::fromUtf8(p->prefix);
  main.unit = prefix + QString::fromUtf8(p->unit);
  if (p->name)
    main.name = QCoreApplication::translate("UT8802", p->name);
  if (status & 0x08)
    main.states |= SampleFlag::Relative;
  if (status & 0x01)
    main.states |= SampleFlag::Min;
  if (status & 0x02)
    main.states |= SampleFlag::Max;

  int digits[5];
  digitsOf(f, digits);
  bool overload = status & 0x40;
  QString shown;
  for (int d : digits)
  {
    overload = overload || d == 0x0C;
    shown += QChar('0' + (d == 0x0A ? 0 : d));
  }
  if (overload)
  {
    main.val = (status & 0x80) ? QStringLiteral("-OL") : QStringLiteral("OL");
    main.dval = 0;
    return m_result;
  }
  if (point > 0)
    shown.insert(shown.size() - point, '.');
  // leading zeros off, up to the one before the point
  while (shown.size() > 1 && shown[0] == '0' && shown[1] != '.')
    shown.remove(0, 1);
  if (status & 0x80)
    shown.prepend('-');
  main.val = shown;
  main.dval = shown.toDouble() * prefixFactor(prefix);
  return m_result;
}
