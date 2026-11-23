// Copyright (c) 2026 The QtDMM developers
// Copyright (c) 2014 Aurelien Jacobs <aurel@gnuage.org> (libsigrok bm86x.c)
// Copyright (c) 2019 Gerhard Sittig <gerhard.sittig@gmx.net> (libsigrok bm86x.c)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/decoders/brymen_bm86x.h"

// Brymen BM86x over the BU-86X adapter (libsigrok "brymen-bm86x", "hid/bu86x").
// "*": ported from the libsigrok parser, not confirmed on hardware. The
// BM869s was confirmed by a user (2026-09).
static const bool registered = []() {
  DmmDecoder::addConfig({"Brymen", "BM867s *", "", 0, FrameFormat::BrymenBM86x, 8, 1, 1, 0, 50000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Brymen", "BM869s", "", 0, FrameFormat::BrymenBM86x, 8, 1, 1, 0, 50000, 0, 0, 0, "", "", 2});
  // the BM869s under other names (HKJ's TestController device list, "BrymenDMM")
  DmmDecoder::addConfig({"Elma", "BM869s *", "", 0, FrameFormat::BrymenBM86x, 8, 1, 1, 0, 50000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Greenlee", "DM-860A *", "", 0, FrameFormat::BrymenBM86x, 8, 1, 1, 0, 50000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Sanwa", "PC7000 *", "", 0, FrameFormat::BrymenBM86x, 8, 1, 1, 0, 50000, 0, 0, 0, "", "", 2});
  return true;
}();

size_t DecoderBrymenBM86x::getPacketLength()
{
  return (m_type == FrameFormat::BrymenBM86x ? 24 : 0);
}

bool DecoderBrymenBM86x::checkFormat(const char *data, size_t idx)
{
  // bytes 16..19 of the 24-byte packet are 0x86: seven to four before the end
  for (int back = 4; back <= 7; back++)
    if (ringByte(data, idx, back) != 0x86)
      return false;
  return true;
}

char DecoderBrymenBM86x::digit(unsigned char b)
{
  switch (b >> 1)
  {
    case 0x20: return '-';
    case 0x5f: return '0';
    case 0x50: return '1';
    case 0x6d: return '2';
    case 0x7c: return '3';
    case 0x72: return '4';
    case 0x3e: return '5';
    case 0x3f: return '6';
    case 0x54: return '7';
    case 0x7f: return '8';
    case 0x7e: return '9';
    case 0x0f: return 'C';
    case 0x27: return 'F';
    case 0x0b: return 'L';
    case 0x79: return 'd';
    case 0x10: return 'i';
    case 0x39: return 'o';
    default:   return '\0';
  }
}

QString DecoderBrymenBM86x::digits(const unsigned char *pkt, int count, unsigned char signFlag, QChar *tempUnit)
{
  QString text;
  if (pkt[0] & signFlag)
    text += '-';
  for (int pos = 0; pos < count; pos++)
  {
    const unsigned char byte = pkt[1 + pos];
    if (pos > 0 && pos < 5 && (byte & 0x01))
      text += '.';
    const char c = digit(byte);
    if (pos == 5 && (c == 'C' || c == 'F'))
    {
      if (tempUnit)
        *tempUnit = QChar(c);
    }
    else if (c)
      text += c;
  }
  return text;
}

std::optional<DmmDecoder::DmmResponse> DecoderBrymenBM86x::decode(const QByteArray &data, int /*id*/)
{
  if (data.size() != 24)
    return std::nullopt;
  const unsigned char *buf = reinterpret_cast<const unsigned char *>(data.constData());
  for (int i = 16; i <= 19; i++)
    if (buf[i] != 0x86)
      return std::nullopt;

  m_result = {};
  // one frame carries both displays: main is id 0, the second id 1 (and
  // channels 2 in the table, so the display shows it)
  m_result.main().id = 0;
  DmmChannel &sub = m_result.addChannel(1);
  m_result.showBar = true;
  m_result.hold = (buf[1] & 0x08) != 0;
  m_result.range = (buf[1] & 0x01) ? "AUTO" : "MANU";

  // --- main display ---
  QChar tempUnit;
  const QString second = digits(&buf[9], 4, 0, nullptr);
  const bool isDiode = second == "diod";
  QString text = digits(&buf[2], 6, 0x80, &tempUnit);
  const bool overload = text.contains("0L") || text.contains("0.L");

  const bool dc = (buf[1] & 0x10) != 0;
  const bool ac = (buf[2] & 0x01) != 0;
  QString unit, special = dc ? "DC" : (ac ? "AC" : "");
  if (buf[8] & 0x01)       { unit = "V"; if (isDiode) special = "DI"; }
  else if (buf[14] & 0x80) { unit = "A"; }
  else if (buf[14] & 0x20) { unit = "F"; special = "CA"; }
  else if (buf[14] & 0x10) { unit = "S"; }                  // conductance
  else if (buf[15] & 0x01) { unit = "Hz"; special = "HZ"; }
  else if (buf[10] & 0x01) { unit = "Ohm"; special = "BUZ"; }
  else if (buf[15] & 0x10) { unit = "Ohm"; special = "OH"; }
  else if (buf[15] & 0x02) { unit = "dBm"; special = "PO"; }
  else if (buf[15] & 0x80) { unit = "%"; special = "DU"; }
  else if ((buf[2] & 0x0a) && !tempUnit.isNull()) { unit = tempUnit; special = "TE"; }

  // the "m" of dBm must not read as milli
  unsigned char ind15 = buf[15];
  if (ind15 & 0x02)
    ind15 &= ~0x04;
  QString prefix;
  if (buf[14] & 0x40) prefix = "n";
  if (buf[15] & 0x08) prefix = "u";
  if (ind15 & 0x04)   prefix = "m";
  if (buf[15] & 0x40) prefix = "k";
  if (buf[15] & 0x20) prefix = "M";
  if (unit == "dBm" || special == "TE")
    prefix.clear();

  bool ok = false;
  const double value = text.toDouble(&ok);
  m_result.main().val = overload ? QStringLiteral(" OL ") : text;
  m_result.main().unit = prefix + unit;
  m_result.main().special = special;
  m_result.main().dval = (ok && !overload) ? value * prefixFactor(prefix) : 0.0;
  if (!ok && !overload)
    return std::nullopt;

  // --- secondary display ---
  const QString text2 = digits(&buf[9], 4, 0x10, nullptr);
  bool ok2 = false;
  const double value2 = text2.toDouble(&ok2);
  if (ok2)
  {
    QString unit2;
    if (buf[14] & 0x08)      unit2 = "V";
    else if (buf[9] & 0x04)  unit2 = "A";
    else if (buf[9] & 0x08)  unit2 = "%";
    else if (buf[14] & 0x04) unit2 = "Hz";
    else if ((buf[9] & 0x40) && !tempUnit.isNull()) unit2 = tempUnit;
    QString prefix2;
    if (buf[9] & 0x01)  prefix2 = "u";
    if (buf[9] & 0x02)  prefix2 = "m";
    if (buf[14] & 0x02) prefix2 = "k";
    if (buf[14] & 0x01) prefix2 = "M";
    if (!unit2.isEmpty())
    {
      sub.dval = value2 * prefixFactor(prefix2);
      sub.val = text2;
      sub.unit = prefix2 + unit2;
    }
  }
  m_result.lowBat = (buf[9] & 0x80) != 0;
  return m_result;
}
