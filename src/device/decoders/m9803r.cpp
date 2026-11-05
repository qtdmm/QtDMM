#include "device/decoders/m9803r.h"
#include "core/siprefix.h"

/*
 Reverse engeneered by Matthias Toussaint

    Port settings seem to be 9600 7E1 or 9600 7O1 (looks like a bug)
    Protocoll is 11 bytes binary fixed length
    END OF PACKET is 0x0d 0x0a
    Multimeter continuously sends data

Byte 0: Sign ored together
0x00 -> positive
0x08 -> negative
0x01 -> overflow

Byte 1: Digit 4 binary
Byte 2: Digit 3 binary
Byte 3: Digit 2 binary
Byte 4: Digit 1 binary

Byte 5: Mode
0x00 DCV
0x01 ACV
0x02 DCA
0x03 ACA
0x04 Ohms
0x05 Ohms + Beep
0x06 Diode V
0x07 ADP
0x08 DCA (10A)
0x09 ACA (10A)
0x0A Freq
0x0C Capacity

Byte 6:Decimal point position
Display	Unit	Value
Frequency
0.000	kHz	0x00
00.00	kHz	0x01
00.00	Hz	0x05
000.0	Hz	0x06
Voltage
000.0	mV	0x00
0.000	V	0x01
00.00	V	0x02
000.0	V	0x03
0000.	V	0x04
Current
0.000	mA	0x00
00.00	mA	0x01
000.0	mA	0x02
Capacity
0.000	nF	0x00
00.00	nF	0x01
000.0	nF	0x02
0.000	uF	0x03
00.00	uF	0x04
Resistance
000.0	Ohm	0x00
0.000	kOhm	0x01
00.00	kOhm	0x02
000.0	kOhm	0x03
0000.	kOhm	0x04
00.00	MOhm	0x05

Byte 7: Hold/min/Max/Rel ored together
0x01 Hold
0x02 Rel
0x04 Min
0x08 Max

Byte 8: Auto/Manu
0x01 APOF (AutoPowerOff)
0x02 Manu
0x04 Auto
0x08 MEM
*/

static const bool registered = []() {
  DmmDecoder::addConfig({"ELV", "M9803R", "", 9600, FrameFormat::M9803RContinuous, 7, 1, 1, 1, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"MASTECH", "M9803R", "", 9600, FrameFormat::M9803RContinuous, 7, 1, 1, 1, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"McVoice", "M-980T", "", 9600, FrameFormat::M9803RContinuous, 7, 1, 1, 0, 4000, 0, 0, 1});
  return true;
}();

size_t DecoderM9803R::getPacketLength()
{
  return  (m_type == FrameFormat::M9803RContinuous ? 11 : 0);
}


bool DecoderM9803R::checkFormat(const char* data, size_t idx)
{
  return (m_type==FrameFormat::M9803RContinuous && idx >= 10 && ringByte(data, idx, 1) == 0x0d && data[idx] == 0x0a);
}


std::optional<DmmDecoder::DmmResponse> DecoderM9803R::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id     = id;
  m_result.hold   = false;
  m_result.range  = bit(data,8,2) ? "AUTO" : "MANU";
  m_result.showBar = true;

  QString val;
  QString special;
  QString unit;

  if (data[0] & 0x01)
    val = "  0L";
  else
  {
    for (int i = 1; i <= 4; ++i)
      if (quint8(data[i]) > 9)
        return std::nullopt;
    val = data[0] == 0x08 ? "-" : "";
    val += QChar('0' + data[4]);
    val += QChar('0' + data[3]);
    val += QChar('0' + data[2]);
    val += QChar('0' + data[1]);
  }

  switch (data[5])
  {
    case 0x00:   // DC V
    case 0x01:   // AC V
      switch (data[6])
      {
        case 0x00:
          m_result.main().unit = "mV";
          val = insertCommaIT(val, 3);
          break;
        case 0x01:
          m_result.main().unit = "V";
          val = insertCommaIT(val, 1);
          break;
        case 0x02:
          m_result.main().unit = "V";
          val = insertCommaIT(val, 2);
          break;
        case 0x03:
          m_result.main().unit = "V";
          val = insertCommaIT(val, 3);
          break;
        case 0x04:
          m_result.main().unit = "V";
          break;
      }
      m_result.main().special = data[5] == 0x00 ? "DC" : "AC";
      break;
    case 0x02:   // DC mA
    case 0x03:   // AC mA
      switch (data[6])
      {
        case 0x00:
          m_result.main().unit = "mA";
          val = insertCommaIT(val, 1);
          break;
        case 0x01:
          m_result.main().unit = "mA";
          val = insertCommaIT(val, 2);
          break;
        case 0x02:
          m_result.main().unit = "mA";
          val = insertCommaIT(val, 3);
          break;
      }
      m_result.main().special = data[5] == 0x02 ? "DC" : "AC";
      break;
    case 0x04:
      switch (data[6])
      {
        case 0x00:
          m_result.main().unit = "Ohm";
          val = insertCommaIT(val, 1);
          break;
        case 0x01:
          m_result.main().unit = "kOhm";
          val = insertCommaIT(val, 1);
          break;
        case 0x02:
          m_result.main().unit = "kOhm";
          val = insertCommaIT(val, 2);
          break;
        case 0x03:
          m_result.main().unit = "kOhm";
          val = insertCommaIT(val, 3);
          break;
        case 0x04:
          m_result.main().unit = "kOhm";
          break;
        case 0x05:
          m_result.main().unit = "MOhm";
          val = insertCommaIT(val, 2);
          break;
      }
      m_result.main().special = "OH";
      break;
    case 0x05:
      switch (data[6])
      {
        case 0x00:
          m_result.main().unit = "Ohm";
          val = insertCommaIT(val, 3);
          break;
      }
      m_result.main().special = "OH";
      break;
    case 0x06:
      m_result.main().unit = "V";
      val = insertCommaIT(val, 1);
      m_result.main().special = "DI";
      break;
    case 0x08:
      m_result.main().unit = "A";
      val = insertCommaIT(val, 2);
      m_result.main().special = "DC";
      break;
    case 0x09:
      m_result.main().unit = "A";
      val = insertCommaIT(val, 2);
      m_result.main().special = "AC";
      break;
    case 0x0A:
      m_result.showBar = false;
      switch (data[6])
      {
        case 0x00:
          m_result.main().unit = "kHz";
          val = insertCommaIT(val, 1);
          break;
        case 0x01:
          m_result.main().unit = "kHz";
          val = insertCommaIT(val, 2);
          break;
        case 0x05:
          m_result.main().unit = "Hz";
          val = insertCommaIT(val, 2);
          break;
        case 0x06:
          m_result.main().unit = "Hz";
          val = insertCommaIT(val, 3);
          break;
      }
      m_result.main().special = "HZ";
      break;
    case 0x0C:
      switch (data[6])
      {
        case 0x00:
          m_result.main().unit = "nF";
          val = insertCommaIT(val, 1);
          break;
        case 0x01:
          m_result.main().unit = "nF";
          val = insertCommaIT(val, 2);
          break;
        case 0x02:
          m_result.main().unit = "nF";
          val = insertCommaIT(val, 3);
          break;
        case 0x03:
          m_result.main().unit = "uF";
          val = insertCommaIT(val, 1);
          break;
        case 0x04:
          m_result.main().unit = "uF";
          val = insertCommaIT(val, 2);
          break;
      }
      m_result.main().special = "CA";
      break;
  }

  m_result.main().val    = val.trimmed();
  // the unit strings above carry their SI prefix ("mV", "kOhm"); dval is in
  // base units like every decoder's, so 123.4 mV is 0.1234
  m_result.main().dval   = m_result.main().val.toDouble() * prefixFactor(SiPrefix::split(m_result.main().unit).prefix);

  return m_result;
}
