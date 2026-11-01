// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/brymen_bm78xbt.h"

#include "core/sampletypes.h"

#include <QCoreApplication>

static const bool registered = []() {
  // Bluetooth LE built in (baud 0); none tried. 60000 counts
  DmmDecoder::addConfig({"Brymen", "BM788BT *", "", 0, FrameFormat::BrymenBM78xBT, 8, 1, 1, 0, 60000, 0, 0, 0});
  DmmDecoder::addConfig({"Brymen", "BM787BT *", "", 0, FrameFormat::BrymenBM78xBT, 8, 1, 1, 0, 60000, 0, 0, 0});
  return true;
}();

namespace
{
constexpr int kLength = 32;

// AC, DC, AC+DC and Hz as the sub functions of the volt and current mains
// (spec §6.5); the others by their main
QString specialOf(int main, int sub)
{
  switch (main)
  {
    case 0x02:   // AutoCheck: LoZ AC, LoZ DC
      return sub == 0 ? "AC" : sub == 1 ? "DC" : QString();
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    {
      static const char *const subs[] = { "AC", "DC", "ACDC", "HZ" };
      return sub < 4 ? subs[sub] : main != 0x06 || sub != 8 ? "AC" : "";   // LPF, inrush; mA 08 4-20 mA
    }
    case 0x17:   // VFD: its Hz, its V AC
      return sub == 0 ? "HZ" : "AC";
    case 0x0C:
      return "TE";
    case 0x0D:
      return "OH";
    case 0x0E:
      return "CA";
    case 0x0F:
      return "BUZ";
    case 0x10:
      return "DI";
    case 0x12:
      return "DU";
    case 0x13:
    case 0x23:
      return "HZ";
    case 0x22:
      return "NCV";
    default:
      return QString();
  }
}

// the unit code [26] (spec §6.4)
QString unitOf(int code)
{
  switch (code)
  {
    case 0x02: return "V";
    case 0x03: return "A";
    case 0x04: return "Ohm";
    case 0x05: return "S";
    case 0x06: return "F";
    case 0x08: return "Hz";
    case 0x0A:
    case 0x4F: return "%";
    case 0x14: return "C";
    case 0x15: return "dF";
    default: return QString();
  }
}

// the prefix [25], a signed power of ten
QString prefixOf(qint8 power)
{
  switch (power)
  {
    case -9: return "n";
    case -6: return "µ";
    case -3: return "m";
    case 3: return "k";
    case 6: return "M";
    case 9: return "G";
    default: return QString();
  }
}

// the texts the count selects with the ASCII flag (spec §6.7)
QString textOf(quint32 code)
{
  static const char *const texts[] = { "OL", "Auto", "InEr", "-", "--", "---", "----", "-----",
                                       "diSC", "CALi", "EF-H", "EF-L", "rS-3", "SoC", "SoH", "bAd" };
  return code < 16 ? QString::fromLatin1(texts[code]) : QStringLiteral("----");
}
}

quint16 DecoderBM78xBT::crc(const unsigned char *b, int n)
{
  quint16 c = 0xFFFF;
  for (int i = 0; i < n; ++i)
  {
    c ^= b[i];
    for (int k = 0; k < 8; ++k)
      c = (c & 1) ? quint16((c >> 1) ^ 0xA001) : quint16(c >> 1);
  }
  return c;
}

QByteArray DecoderBM78xBT::login(const QString &address)
{
  // FF 01 20 01, version 01, the address, the code low byte first, 01,
  // the password 0000 as four binary digits, the CRC, FF 03 (spec §5, §7)
  unsigned char p[kLength] = { 0xFF, 0x01, 0x20, 0x01, 0x01 };
  const QStringList octets = address.split(':');   // a UUID (macOS): zeros
  if (octets.size() == 6)
    for (int i = 0; i < 6; ++i)
      p[5 + i] = quint8(octets[5 - i].toUInt(nullptr, 16));
  p[11] = 0x51;
  p[12] = 0x01;
  p[13] = 0x01;
  const quint16 c = crc(p + 2, 26);
  p[28] = quint8(c & 0xFF);
  p[29] = quint8(c >> 8);
  p[30] = 0xFF;
  p[31] = 0x03;
  return QByteArray(reinterpret_cast<const char *>(p), kLength);
}

bool DecoderBM78xBT::checkFormat(const char *data, size_t idx)
{
  if (idx < size_t(kLength - 1))
    return false;
  unsigned char f[kLength];
  lastBytes(data, idx, kLength, f);
  return f[0] == 0xFF && f[1] == 0x02 && f[2] == 0x20 && f[3] == 0x05 && f[30] == 0xFF && f[31] == 0x03
         && crc(f + 2, 26) == quint16(f[28] | (f[29] << 8));
}

std::optional<DmmDecoder::DmmResponse> DecoderBM78xBT::decode(const QByteArray &data, int id)
{
  if (data.size() < kLength)
    return std::nullopt;
  const auto *f = reinterpret_cast<const unsigned char *>(data.constData()) + data.size() - kLength;
  const quint8 flag0 = f[14], flag1 = f[15];
  const int main = f[18], sub = f[20];

  m_result = {};
  m_result.hold = flag0 & 0x28;   // HOLD, AUTO-HOLD
  m_result.range = (flag0 & 0x10) ? "AUTO" : "MANU";
  DmmChannel &c = m_result.main();
  c.id = id;
  c.special = specialOf(main, sub);
  const QString prefix = prefixOf(qint8(f[25]));
  c.unit = prefix + unitOf(f[26]);
  if (main == 0x02)
    c.name = QCoreApplication::translate("Brymen", "LoZ");
  else if (main == 0x06 && sub == 0x08)
    c.name = QCoreApplication::translate("Brymen", "4-20 mA loop");
  else if (main == 0x0C && sub == 0x02)
    c.name = QCoreApplication::translate("Brymen", "T1 - T2");
  if (flag0 & 0x40)
    c.states |= SampleFlag::Relative;
  if (flag0 & 0x80)
    c.states |= SampleFlag::Peak;   // CREST, MAX or MIN with it
  if (flag1 & 0x08)
    c.states |= SampleFlag::Max;
  if (flag1 & 0x04)
    c.states |= SampleFlag::Min;
  if (flag1 & 0x02)
    c.states |= SampleFlag::Avg;

  // the count, 24-bit signed; negative by the sign flag or the count
  // (the document has both, spec §6.3)
  qint32 count = qint32(f[21] | (f[22] << 8) | (f[23] << 16));
  if (count & 0x800000)
    count -= 0x1000000;
  const bool negative = (flag1 & 0x40) || count < 0;
  if (flag1 & 0x20)
  {
    c.val = negative ? QStringLiteral("-OL") : QStringLiteral("OL");
    c.dval = 0;
  }
  else if (flag0 & 0x04)
  {
    c.val = textOf(quint32(count));
    c.noValue = true;
  }
  else
  {
    // [24] the digits before the point, 0 none; [27] all digits
    const int decimals = f[24] == 0 ? 0 : qMax(0, int(f[27]) - int(f[24]));
    QString s = QString::number(qAbs(count));
    if (decimals > 0)
    {
      s = s.rightJustified(decimals + 1, '0');
      s.insert(s.size() - decimals, '.');
    }
    c.val = negative ? "-" + s : s;
    c.dval = c.val.toDouble() * prefixFactor(prefix);
  }
  return m_result;
}
