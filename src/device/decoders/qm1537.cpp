// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/decoders/qm1537.h"

// FS9922-DMM4

static const bool registered = []() {
  DmmDecoder::addConfig({"Digitech", "QM1537", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"PeakTech", "3430", "", 19200, FrameFormat::QM1537Continuous, 7, 2, 1, 0, 4000, 0, 0, 1});
  // models marked "*" come from chip data (libsigrok, datasheets) and are
  // not confirmed on hardware yet
  DmmDecoder::addConfig({"Digitek", "DT-9602R+ *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Pro'sKit", "MT-1820 *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"SparkFun", "70C *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT60D *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT61A *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 830 *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"GW Instek", "GDM-397 *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});   // libsigrok-master fs9922
  DmmDecoder::addConfig({"PeakTech", "2025 *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});      // libsigrok-master fs9922
  DmmDecoder::addConfig({"Uni-Trend", "UT61B", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT61C", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT61D", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Vichy", "VC99", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 6000, 0, 0, 1});
  // FS9922 at 2400 8N1 in HKJ's TestController device list ("SimpleDMM");
  // display counts not known, 4000 as the FS9922-DMM4
  DmmDecoder::addConfig({"Victor", "70C *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Victor", "86C *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 850 *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"EZA", "EZ-735 *", "", 2400, FrameFormat::QM1537Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  return true;
}();

size_t DecoderQM1537::getPacketLength()
{
  return  (m_type == FrameFormat::QM1537Continuous ? 14 : 0);
}

bool DecoderQM1537::checkFormat(const char* data, size_t idx)
{
  // The frame ends with CR LF (UT61BCD.log: 14 bytes, bytes 12/13 = 0D 0A).
  // Firing on the CR alone, as this used to, handed the reader the previous
  // frame's LF plus 13 bytes - decode() then saw the LF where the sign
  // belongs and reported "Data error!" for every reading.
  return (m_type == FrameFormat::QM1537Continuous && idx && data[idx - 1] == 0x0d && data[idx] == 0x0a);
}

std::optional<DmmDecoder::DmmResponse> DecoderQM1537::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id = id;
  const auto dataError = [this]
  {
    printf("Data error!\n");
    m_result.error = "Data error!";
    return m_result;
  };
  m_result.hold      = bit(data,7,1);
  m_result.range     = bit(data,7,5) ? "AUTO" : "MANU";
  m_result.showBar= bit(data,7,0);
  m_result.main().special = (data[7] & 0x08) ? "AC" : "DC";

  if (data[0] == '-')
  {
    // A single character: insertCommaIT() below allows for exactly one leading
    // sign/space. The former " -" pushed the decimal point one place left and
    // made every negative reading ten times too small.
    m_result.main().val = "-";
  }
  else if(data[0] == '+')
  {
    m_result.main().val = "";
  }
  else
    return dataError();

  for(int i=1;i<4;i++)
  {
    if(data[i]<'0' || data[i]>'9')
      return dataError();
  }

  if ((data[1] == ';') &&
      (data[2] == '0') &&
      (data[3] == ':') &&
      (data[4] == ';'))
  {
     m_result.main().val += "  0L";
  }
  else
    m_result.main().val += makeValue(data,1,4);

  bool doUnits = true;

  switch (data[6])
  {
    case 0x31: m_result.main().val = insertCommaIT (m_result.main().val,1); break;
    case 0x32: m_result.main().val = insertCommaIT (m_result.main().val,2); break;
    case 0x34: m_result.main().val = insertCommaIT (m_result.main().val,3); break;
    // default case is no comma/decimal point at all.
  }

  /* OK, now let's figure out what we're looking at. */
  if (data[10] & 0x80)
  {
    /* Voltage, including diode test */
    m_result.main().unit = "V";
    if (data[9] & 0x04)
    {
      /* Diode test */
      m_result.main().special = "DI";
      m_result.main().unit = "V";
    }
  }
  else if (data[10] & 0x40)
  {
    /* Current */
    m_result.main().unit = "A";
  }
  else if (data[10] & 0x20)
  {
    /* Resistance, including continuity test */
    m_result.main().unit = "Ohm";
    m_result.main().special = "OH";
  }
  else if (data[10] & 0x08)
  {
    /* Frequency */
    m_result.main().unit = "Hz";
    m_result.main().special = "HZ";
  }
  else if (data[10 ] & 0x04)
  {
    /* Capacitance */
    m_result.main().unit = "F";
    m_result.main().special = "CA";
  }
  else if (data[9] & 0x02)
  {
    /* Duty cycle */
    m_result.main().unit = "%";
    m_result.main().special = "PC";
    doUnits = false;
  }
  else if (data[10] & 0x02)
  {
    /* Temperature, byte 10 bit 1 per docs/protocols/sources/UT61BCD.log
       (UT61B/C only, not the UT61D). "C" and "dF" are the strings LcdWidget
       maps to the degree glyphs. */
    m_result.main().unit = "C";
    m_result.main().special = "TE";
    doUnits = false;
  }
  else if (data[10] & 0x01)
  {
    m_result.main().unit = "dF";
    m_result.main().special = "TE";
    doUnits = false;
  }

  QString prefix;

  if (doUnits)
  {
    if (data[8] & 0x02)      prefix = "n";
    else if (data[9] & 0x80) prefix = "u";
    else if (data[9] & 0x40) prefix = "m";
    else if (data[9] & 0x20) prefix = "k";
    else if (data[9] & 0x10) prefix = "M";

    m_result.main().unit.prepend(prefix);
  }

  // Computed only here, because the prefix is not known before the unit switch
  // above (duty cycle takes no prefix at all). dval is contractually in SI base
  // units - see DmmChannel.
  m_result.main().dval = m_result.main().val.toDouble() * prefixFactor(prefix);

 return m_result;
}
