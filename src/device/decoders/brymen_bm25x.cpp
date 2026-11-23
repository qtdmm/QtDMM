// Copyright (c) 2026 The QtDMM developers
// Copyright (c) 2014 Janne Huttunen <jahuttun@gmail.com> (libsigrok bm25x.c)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/decoders/brymen_bm25x.h"

// Brymen BM25x family (libsigrok "brymen-bm25x": 9600/8n1/rts=1/dtr=1).
// "*": ported from the libsigrok parser, not confirmed on hardware.
static const bool registered = []() {
  DmmDecoder::addConfig({"Brymen", "BM250 *", "", 9600, FrameFormat::BrymenBM25x, 8, 1, 1, 0, 6000, 0, 1, 1});
  DmmDecoder::addConfig({"Brymen", "BM251 *", "", 9600, FrameFormat::BrymenBM25x, 8, 1, 1, 0, 6000, 0, 1, 1});
  DmmDecoder::addConfig({"Brymen", "BM252 *", "", 9600, FrameFormat::BrymenBM25x, 8, 1, 1, 0, 6000, 0, 1, 1});
  DmmDecoder::addConfig({"Brymen", "BM257 *", "", 9600, FrameFormat::BrymenBM25x, 8, 1, 1, 0, 6000, 0, 1, 1});
  // HKJ's TestController device list, "BrymenBM250"
  DmmDecoder::addConfig({"Brymen", "BM255 *", "", 9600, FrameFormat::BrymenBM25x, 8, 1, 1, 0, 6000, 0, 1, 1});
  DmmDecoder::addConfig({"Greenlee", "DM-210A *", "", 9600, FrameFormat::BrymenBM25x, 8, 1, 1, 0, 6000, 0, 1, 1});
  return true;
}();

size_t DecoderBrymenBM25x::getPacketLength()
{
  return (m_type == FrameFormat::BrymenBM25x ? 15 : 0);
}

bool DecoderBrymenBM25x::checkFormat(const char *data, size_t idx)
{
  // the last byte is tagged 0xE.; the 0x02 fourteen bytes earlier confirms
  // the alignment (the tag sequence 1..14 is verified in decode())
  return (ringByte(data, idx, 0) & 0xf0) == 0xe0 && ringByte(data, idx, 14) == 0x02;
}

QChar DecoderBrymenBM25x::digit(const unsigned char *buf, int num)
{
  // segments a-g of digit num are spread over two bytes
  const int val = (buf[3 + 2 * num] & 0x0e) | ((buf[4 + 2 * num] << 4) & 0xf0);
  switch (val)
  {
    case 0xbe: return '0';
    case 0xa0: return '1';
    case 0xda: return '2';
    case 0xf8: return '3';
    case 0xe4: return '4';
    case 0x7c: return '5';
    case 0x7e: return '6';
    case 0xa8: return '7';
    case 0xfe: return '8';
    case 0xfc: return '9';
    case 0x00: return ' ';
    case 0x40: return '-';
    case 0x16: return 'L';
    case 0x1e: return 'C';
    case 0x4e: return 'F';
    case 0x5e: return 'E';
    case 0x62: return 'n';
    case 0x42: return 'r';
    default:   return '?';
  }
}

std::optional<DmmDecoder::DmmResponse> DecoderBrymenBM25x::decode(const QByteArray &data, int id)
{
  if (data.size() != 15)
    return std::nullopt;
  const unsigned char *buf = reinterpret_cast<const unsigned char *>(data.constData());
  if (buf[0] != 0x02)
    return std::nullopt;
  for (int i = 1; i < 15; i++)
    if ((buf[i] >> 4) != i)
      return std::nullopt;

  m_result = {};
  m_result.main().id = id;
  m_result.showBar = true;
  m_result.hold = (buf[11] & 0x08) != 0;
  m_result.range = (buf[1] & 0x08) ? "AUTO" : "MANU";

  // digits; the fourth may be the C/F of a temperature reading
  QString digits;
  bool temperature = false;
  QString tempUnit;
  for (int i = 0; i < 4; i++)
  {
    const QChar c = digit(buf, i);
    if (i == 3 && (c == 'C' || c == 'F'))
    {
      temperature = true;
      tempUnit = c;
      break;
    }
    digits += c;
  }

  // decimal point: bit 0 of bytes 9, 7, 5 = the point left of display digit
  // 4, 3, 2. Its position is fixed on the display, so with only three
  // numeric digits (the fourth is C/F) the same bit means one decimal less -
  // libsigrok bm25x.c decode_scale(): pos = point + digits - 4.
  int point = 0;
  for (int i = 1; i < 4; i++)
    if (buf[11 - 2 * i] & 0x01)
      point = i;   // i = 1 -> "123.4", 3 -> "1.234" (four digits)
  const int numDigits = digits.size();
  QString val = digits;
  const int decimals = point > 0 ? point + numDigits - 4 : 0;
  if (decimals > 0 && decimals <= numDigits)
    val.insert(numDigits - decimals, '.');
  if (buf[3] & 0x01)
    val.prepend('-');

  // "0L" (digit 1 = 0, digit 2 = L) is the overload display
  const bool overload = digit(buf, 1) == '0' && digit(buf, 2) == 'L';
  const bool numeric = !overload && !digits.contains(QRegularExpression("[^0-9 .-]"));

  QString prefix;
  if (buf[11] & 0x02)      prefix = "M";
  else if (buf[11] & 0x01) prefix = "k";
  else if (buf[13] & 0x01) prefix = "m";
  else if (buf[13] & 0x02) prefix = "u";
  else if (buf[12] & 0x01) prefix = "n";

  const bool dc = (buf[1] & 0x04) != 0;
  const bool ac = (buf[1] & 0x02) != 0;
  QString unit, special = dc ? "DC" : (ac ? "AC" : "");
  if (temperature)      { unit = tempUnit; special = "TE"; prefix.clear(); }
  else if (buf[12] & 0x04) { unit = "Ohm"; special = "OH"; }
  else if (buf[13] & 0x04) { unit = "F";   special = "CA"; }
  else if (buf[12] & 0x02) { unit = "Hz";  special = "HZ"; }
  else if (buf[14] & 0x04) { unit = "V";   if (!dc && !ac) special = "DI"; }
  else if (buf[14] & 0x02) { unit = "A"; }
  else if (!dc && !ac)     { special = ""; }

  m_result.main().val = overload ? QStringLiteral(" OL ") : val.trimmed();
  m_result.main().special = special;
  if (numeric)
  {
    // formatResultValue() expects the raw digits and inserts the point
    // itself; the string already carries it, so scale directly
    m_result.main().dval = val.toDouble() * prefixFactor(prefix);
    m_result.main().unit = prefix + unit;
  }
  else
  {
    m_result.main().dval = 0.0;
    m_result.main().unit = prefix + unit;
  }
  return m_result;
}
