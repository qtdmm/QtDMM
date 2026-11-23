// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/decoders/dtm0660.h"

/*
 * https://www.eevblog.com/forum/testgear/hacking-the-victor-vc-921/
 * https://github.com/pingumacpenguin/QtDMM-DTM0660-Version
 *
 *
 * Very little information is available about this chip, even though it was put on the market in 2013.
 * Most of the information found on the web is in Chinese. Searches suggest that some PeakTech, UNI-T,
 * RadioShack, and Velleman DMMs use that chip.
 *
 */

static const bool registered = []() {
  DmmDecoder::addConfig({"Generic", "DTM0660 4000 count", "", 2400, FrameFormat::DTM0660, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Generic", "DTM0660 6000 count", "", 2400, FrameFormat::DTM0660, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Generic", "DTM0660 8000 count", "", 2400, FrameFormat::DTM0660, 8, 1, 1, 0, 8000, 0, 0, 1});
  // DTM0660 per libsigrok; "*" = not confirmed on hardware yet
  DmmDecoder::addConfig({"PeakTech", "3415 *", "", 2400, FrameFormat::DTM0660, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Velleman", "DVM4100 *", "", 2400, FrameFormat::DTM0660, 8, 1, 1, 0, 6000, 0, 0, 1});
  return true;
}();

size_t DecoderDTM0660::getPacketLength()
{
  return  (m_type == FrameFormat::DTM0660 ? 15 : 0);
}

bool DecoderDTM0660::checkFormat(const char* data, size_t idx)
{
  return (m_type == FrameFormat::DTM0660 && ((data[idx] & 0xf0) == 0xf0));
}

std::optional<DmmDecoder::DmmResponse> DecoderDTM0660::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id     = id;
  m_result.hold   = false;
  m_result.range  = "";
  m_result.showBar = true;

  // check for overload else find sign and fill in digits
  if (((data[3] & 0x0e) == 0x0e) &&
      ((data[4] & 0x0f) == 0x0b) &&
      ((data[5] & 0x0e) == 0x06) &&
      ((data[6] & 0x0f) == 0x01))
  {
    m_result.main().val = " 0L ";
  }
  else
  {
    m_result.main().val = (data[1] & 0x01) ? " -" : "  ";

    // create string;
    for (int i=0; i<4; ++i)
    {
      m_result.main().val += digit( ((data[1+2*i] & 0x0f) <<4) | (data[2+2*i] & 0x0f));
    }
  }

  // find decimal point position
  if (data[3] & 0x01)      m_result.main().val = insertComma( m_result.main().val, 1 );
  else if (data[5] & 0x01) m_result.main().val = insertComma( m_result.main().val, 2 );
  else if(data[7] & 0x01)  m_result.main().val = insertComma( m_result.main().val, 3 );

  m_result.main().dval = m_result.main().val.toDouble();

  // try to find some special modes
  if (data[9] & 0x08) m_result.main().special = "DI";
  if (data[0] & 0x01) m_result.main().special = "AC";
  if (data[0] & 0x02) m_result.main().special = "DC";

  // try to find mode
  if (data[11] & 0x01)
  {
    m_result.main().unit    = "F";
    m_result.main().special = "CA";
  }
  else if (data[11] & 0x02)
  {
    m_result.main().unit    = "Ohm";
    m_result.main().special = "OH";
  }
  else if (data[12] & 0x08)
  {
    m_result.main().unit = "A";
  }
  else if (data[12] & 0x04)
  {
    m_result.main().unit    = "Hz";
    m_result.main().special = "HZ";
  }
  else if (data[12] & 0x02)
  {
    m_result.main().unit = "V";
  }
  else if (data[10] & 0x02)
  {
    m_result.main().unit    = "%";
    m_result.main().special = "PC";
  }

  else if (data[13] & 0x02)
  {
    m_result.main().unit    = "C";
    m_result.main().special = "TE";
  }
  else if (data[13] & 0x01)
  {
    m_result.main().unit    = "F";
    m_result.main().special = "TE";
  }
  else
  {
    qWarning() << "Unknown DTM0660 unit!";
  }

  // try to find prefix
  //
  // Scales inline rather than via prefixFactor(), but the factors agree with it,
  // so dval ends up in SI base units as DmmChannel requires.
  if (data[9] & 0x02)
  {
    m_result.main().dval /= 1e9;
    m_result.main().unit.prepend( "n" );
  }
  else if (data[9] & 0x01)
  {
    m_result.main().dval /= 1e6;
    m_result.main().unit.prepend( "u" );
  }
  else if (data[10] & 0x01)
  {
    m_result.main().dval /= 1e3;
    m_result.main().unit.prepend( "m" );
  }
  else if (data[9] & 0x04)
  {
    m_result.main().dval *= 1e3;
    m_result.main().unit.prepend( "k" );
  }
  else if (data[10] & 0x04)
  {
    m_result.main().dval *= 1e6;
    m_result.main().unit.prepend( "M" );
  }

  return m_result;
}

const char *DecoderDTM0660::digit( int byte )
{
  int           digit[10] = { 0xeb, 0x0a, 0xad, 0x8f, 0x4e, 0xc7, 0xe7, 0x8a, 0xef, 0xcf };
  const char *c_digit[10] = { "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9" };

  byte &= 0xef;

  for (int n=0; n<10; n++)
  {
    if (byte == digit[n]) return c_digit[n];
  }
  qWarning() << "Unknown DTM0660 byte " << byte;
  return 0;
}
