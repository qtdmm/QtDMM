// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/decoders/do3122.h"

static const bool registered = []() {
  DmmDecoder::addConfig({"Duratool", "DO3122", "", 9600, FrameFormat::DO3122Continuous, 8, 1, 1, 0, 4000, 0, 0, 0});
  return true;
}();

size_t DecoderDO3122::getPacketLength()
{
  return  (m_type == FrameFormat::DO3122Continuous ? 22 : 0);
}

bool DecoderDO3122::checkFormat(const char* data, size_t idx)
{
  // only once a whole frame is in: before that the look-back lands on stale
  // bytes of the ring
  if (m_type == FrameFormat::DO3122Continuous && idx >= 21)
  {
    if ((ringByte(data, idx, 21) != 0xAAu)
        || (ringByte(data, idx, 20) != 0x55u)
        || (ringByte(data, idx, 19) != 0x52u)
        || (ringByte(data, idx, 18) != 0x24u)
       )
      return false;

    if (ringByte(data, idx, 17) != 0x01u)
    {
      (void)fprintf(stderr, "bad mode %#x\n", ringByte(data, idx, 17));
      return false;
    }

    return true;
  }
  return false;
}

std::optional<DmmDecoder::DmmResponse> DecoderDO3122::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id     = id;
  m_result.hold   = false;
  m_result.range  = "";
  m_result.showBar = true;

  QString val = "";
  QString special = "";
  QString unit;
  double d_val;
  int idx;
  bool convOk = true;

  // Check for overload
  if ((static_cast<uint8_t>(data[6u]) == 0x80u)
      && (static_cast<uint8_t>(data[7u]) == 0x58u)
      && (static_cast<uint8_t>(data[8u]) == 0x5fu)
      && (static_cast<uint8_t>(data[9u]) == 0x00u)
     )
  {
    val = "OL";
    d_val = 0.0;
  }
  else
  {
    if (0u != (static_cast<uint8_t>(data[10u]) & 0x08u))
      val = "-";

    for (idx = 9; idx > 5; idx--)
    {
      // Check for blank
      if (data[idx] == 0u)
        continue;

      // Check for .dp
      if (data[idx] & 0x80u)
        val += '.';

      val += digit(data[idx], &convOk);

      if (false == convOk)
      {
        m_result.error = tr("Digit parse fail");
        return m_result;
      }
    }

    d_val = val.toDouble(&convOk);
  }

  if (convOk)
  {
    switch (data[10u] & 0x07u)
    {
      case 0x00u: special = ""; break;
      case 0x01u: special = "Diode"; break;
      case 0x02u: special = "AC"; break;
      case 0x04u: special = "DC"; break;
      default: convOk = false; break;
    }

    if ((data[20u] != 0u) && (data[21u] == 0u))
    {
      switch (static_cast<uint8_t>(data[20u] & 0xffu))
      {
        case 0x01u: unit = "C"; break;
        case 0x02u: unit = "F"; break;
        case 0x90u: unit = "mF"; d_val *= 0.001; break;
        case 0xA0u: unit = "uF"; d_val *= 0.000001; break;
        case 0xC0u: unit = "nF"; d_val *= 0.000000001; break;
        default: convOk = false; break;
      }
    }
    else if (data[21u] != 0u)
    {
      switch (static_cast<uint8_t>(data[21u] & 0x33u))
      {
        case 0x00u: unit = ""; break;
        case 0x01u: d_val *= 0.000001; unit = 'u'; break;
        case 0x02u: d_val *= 0.001; unit = 'm'; break;
        case 0x10u: d_val *= 1000000; unit = 'M'; break;
        case 0x20u: d_val *= 1000; unit = 'k'; break;
        default: convOk = false; break;
      }

      switch (static_cast<uint8_t>(data[21u] & 0xCCu))
      {
        case 0x04u: unit += "A"; break;
        case 0x08u: unit += "V"; break;
        case 0x40u: unit += "Ohm"; break;
        case 0x80u: unit += "Hz"; break;
        default: convOk = false; break;
      }
    }
    else
      convOk = false;

    if (true == convOk)
    {
      m_result.main().dval   = d_val;
      m_result.main().special= special;
      m_result.main().val    = val;
      m_result.main().unit  = unit;
    }
    else
      m_result.error = tr("Parser errors");
  }
  else
    m_result.error = tr("Parser errors");

  return m_result;
}

const char *DecoderDO3122::digit(int byte, bool *convOk)
{
  int           digit[10] = { 0x5f, 0x06, 0x6b, 0x2f, 0x36, 0x3d, 0x7d, 0x07, 0x7f, 0x3f };
  const char *c_digit[10] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

  byte &= 0x07f;

  *convOk = true;

  for (int n = 0; n < 10; n++)
  {
    if (byte == digit[n])
      return c_digit[n];
  }

  *convOk = false;
  return 0;
}


