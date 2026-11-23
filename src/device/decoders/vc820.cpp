// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/decoders/vc820.h"

#include "core/sampletypes.h"

/* FortuneFS9721
 *
 * Packet structure:
 * Byte  bits 7-4 	Bit 3 	Bit 2 	Bit 1 	Bit 0
 * 0 	0x1  AC 	DC 	Auto 	RS232
 * 1 	0x2  Negative 	1A 	1B 	1C
 * 2 	0x3  1D 	1E 	1F 	1G
 * 3 	0x4  DP1 	2A 	2B 	2C
 * 4 	0x5  2D 	2E 	2F 	2G
 * 5 	0x6  DP2 	3A 	3B 	3C
 * 6 	0x7  3D 	3E 	3F 	3G
 * 7 	0x8  DP3 	4A 	4B 	4C
 * 8 	0x9  4D 	4E 	4F 	4G
 * 9 	0xa  u 	n 	k 	Diode
 * 10 0xb  m 	% 	M 	Beep
 * 11 0xc  Farads 	Ohms 	Rel 	Hold
 * 12 0xd  A 	V 	Hz 	Low battery
 * 13 0xe User3 User2 User1 User0
 *
 * Segment lettering:
 *   	C
 *
 * B 		G
 *
 *   	F
 *
 * A 		E
 *
 *   	D
 *  *
 *
 */

static const bool registered = []() {
  DmmDecoder::addConfig({"Digitek", "DT-9062"  , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  // FS9721 per libsigrok (digitek-dt4000zc); was listed
  // under the FS9922 protocol before
  DmmDecoder::addConfig({"Digitek", "DT4000ZC" , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"TekPower", "TP4000ZC", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  // models marked "*" come from chip data (libsigrok fs9721 list) and are
  // not confirmed on hardware yet
  DmmDecoder::addConfig({"MASTECH", "MS8250B *", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"PCE", "PCE-DM32 *", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Tecpel", "DMM-8061 *", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"V&A", "VA18B *", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"V&A", "VA40B *", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  // from Matthias Toussaint's CDMM device table (ablage/CDMM). Its Protek
  // 504/505/506 are left out: CDMM calls their protocol ASCII but decodes an
  // FS9721 frame, and HKJ's TestController reads them as an ASCII line - no
  // capture to settle it
  DmmDecoder::addConfig({"Metrel", "MD9015 *", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT30A *", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT30E *", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Digitek", "INO2513"  , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Digitech", "QM1462"  , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Digitech", "QM1538"  , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"HoldPeak", "HP-90EPC", "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"PeakTech", "3330"    , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Tenma", "72-7745"    , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Tenma", "72-6870"    , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});   // sigrok PR #282; confirmed on hardware 2026-10
  DmmDecoder::addConfig({"Uni-Trend", "UT60A"  , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT60E"  , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 820" , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 840" , "", 2400, FrameFormat::VC820Continuous, 8, 1, 1, 0, 4000, 0, 0, 1});
  // the same frame over Bluetooth LE, as two notifications of 7 bytes
  // (ablage/holdpeak: AiCare's "INTELLIGENT CLAMP METER" app, decompiled; the
  // FS9721-LP3 per lygte-info's review of the BTMeter BT-570C-APP twin); the
  // HoldPeak tried by a user, the AOPUTTRIVER is the same meter
  DmmDecoder::addConfig({"HoldPeak", "HP-570C-APP", "", 0, FrameFormat::VC820BLE, 8, 1, 1, 0, 4000, 0, 0, 0});
  DmmDecoder::addConfig({"AOPUTTRIVER", "AP-570C-APP", "", 0, FrameFormat::VC820BLE, 8, 1, 1, 0, 4000, 0, 0, 0});
  return true;
}();

// The last 14 bytes, their sequence numbers 1 to 14 in the high nibbles: over
// Bluetooth a frame comes in two halves, and after a lost first half the
// second one of the frame before must not be read with it.
bool DecoderVC820::checkFormat(const char* data, size_t idx)
{
  if ((data[idx] & 0xf0) != 0xe0)
    return false;
  unsigned char f[kFrameLength];
  lastBytes(data, idx, kFrameLength, f);
  for (int i = 0; i < kFrameLength; ++i)
    if ((f[i] >> 4) != i + 1)
      return false;
  return true;
}

size_t DecoderVC820::getPacketLength()
{
  return kFrameLength;
}

std::optional<DmmDecoder::DmmResponse> DecoderVC820::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id = id;
  m_result.hold      = (data[11] & 0x01);
  m_result.lowBat    = (data[12] & 0x01);
  if (data[11] & 0x02)
    m_result.main().states |= SampleFlag::Relative;
  m_result.showBar= true;
  m_result.range = (bit(data, 0, 1)? "AUTO" : "MANU");

  QString val;
  QString special;
  QString unit;

  const char *in = data.data();

  // check for overload else find sign and fill in digits
  if (((in[3] & 0x07) == 0x07) &&
      ((in[4] & 0x0f) == 0x0d) &&
      ((in[5] & 0x07) == 0x06) &&
      ((in[6] & 0x0f) == 0x08))
    val = "  0L";
  else
  {
    val = (in[1] & 0x08) ? " -" : "  ";

    // create string;
    for (int i = 0; i < 4; ++i)
      val += vc820Digit(((quint8(in[1 + 2 * i]) << 4) & 0xf0) | (in[2 + 2 * i] & 0x0f));   // unsigned: a shifted negative char is UB
  }



  // find comma position
  if (in[3] & 0x08) val = insertComma(val, 1);
  else if (in[5] & 0x08) val = insertComma(val, 2);
  else if (in[7] & 0x08) val = insertComma(val, 3);

  // The prefix has to be known here already: dval is contractually in SI base
  // units (see DmmChannel), so it must be scaled by the prefix factor.
  QString prefix;
  if (in[9] & 0x04)       prefix = "n";
  else if (in[ 9] & 0x08) prefix = "u";
  else if (in[10] & 0x08) prefix = "m";
  else if (in[ 9] & 0x02) prefix = "k";
  else if (in[10] & 0x02) prefix = "M";

  m_result.main().dval  = val.toDouble() * prefixFactor(prefix);

  // try to find some special modes
  if (in[9] & 0x01)
    special = "DI";
  else
    special = (in[0] & 0x08) ? "AC" : "DC";

  // try to find mode
  if (in[11] & 0x08)
  {
    unit    = "F";
    special = "CA";
  }
  else if (in[11] & 0x04)
  {
    unit    = "Ohm";
    special = "OH";
  }
  else if (in[12] & 0x08)
    unit = "A";
  else if (in[12] & 0x02)
  {
    unit    = "Hz";
    special = "HZ";
  }
  else if (in[12] & 0x04)
    unit = "V";
  else if (in[10] & 0x04)
  {
    unit    = "%";
    special = "PC";
  }
  // byte 13 carries the four "user defined" flags c2c1 of the FS9721; their
  // meaning is per meter. Per libsigrok: c2c1_00 (bit 0) is degrees C on the
  // UT60E, Tenma 72-7745, VA18B; c2c1_10 (bit 2) on the DT4000ZC, Tenma
  // 72-6870, PCE-DM32; c2c1_01 (bit 1) is C on the VA40B but F on the
  // PCE-DM32 and PeakTech 3330, so it stays unhandled here.
  else if (in[13] & 0x01 || in[13] & 0x04)
  {
    unit    = "C";
    special = "TE";
  }
  else
    qWarning() << "Unknown unit!";

  unit.prepend(prefix);

  m_result.main().special= special;
  m_result.main().val    = val;
  m_result.main().unit  = unit;

  return m_result;
}

const char *DecoderVC820::vc820Digit(int byte)
{
  int           digit[10] = { 0x7d, 0x05, 0x5b, 0x1f, 0x27, 0x3e, 0x7e, 0x15, 0x7f, 0x3f };
  const char *c_digit[10] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

  byte &= 0x7f;

  for (int n = 0; n < 10; n++)
  {
    if (byte == digit[n])
      return c_digit[n];
  }
  return 0;
}
