#include "device/decoders/cyrustek_es51962.h"

// https://www.mikrocontroller.net/topic/98208

static const bool registered = []()
{
  DmmDecoder::addConfig({"PeakTech", "3315", "",  2400,  FrameFormat::CyrustekES51962, 7, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT70B", "",  2400,  FrameFormat::CyrustekES51962, 7, 1, 1, 0, 4000, 0, 0, 1});
  return true;
}();

bool DecoderCyrusTekES51962::checkFormat(const char *data, size_t idx)
{
  // The 12 is deliberate and must stay above the 11-byte packet length: the
  // UT70B sends every datagram twice in a row (see docs/protocols/sources/UT70B.log),
  // so skipping the first copy's terminator at idx == 10 and matching only the
  // second at idx == 21 gives one reading per measurement. Do not "simplify"
  // this to getPacketLength() - unlike vc940, this protocol duplicates frames.
  return (m_type == FrameFormat::CyrustekES51962 && idx >= 12 && ringByte(data, idx, 1) == 0x0d && data[idx] == 0x0a);
}

size_t DecoderCyrusTekES51962::getPacketLength()
{
  return (m_type == FrameFormat::CyrustekES51962 ? 11 : 0);
}

std::optional<DmmDecoder::DmmResponse> DecoderCyrusTekES51962::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id  = id;
  m_result.hold       = false;
  m_result.range      = bit(data,8,1) ? "AUTO" : "MANU";
  m_result.main().val = makeValue(data,1,4,bit(data,6,2));
  m_result.showBar= true;
  m_result.main().special="";

  if (bit(data,8,2))
    m_result.main().special+= "AC";
  if (bit(data,8,3))
    m_result.main().special+= "DC";


  if (!memcmp(data.constData() + 1, "4000", 4) || (data[6] & 1) != 0)
    m_result.main().val = "  0L";

  // Function
  switch (data[5])
  {
    case 0x31:
      m_result.main().unit = "D";
      break;
    case 0x32:

      switch (data[0]) // Range
      {
        case '0': formatResultValue(1,"k",bit(data,6,3)?"RPM":"Hz"); break;
        case '1': formatResultValue(2,"k",bit(data,6,3)?"RPM":"Hz"); break;
        case '2': formatResultValue(3,"k",bit(data,6,3)?"RPM":"Hz"); break;
        case '3': formatResultValue(1,"M",bit(data,6,3)?"RPM":"Hz"); break;
        case '4': formatResultValue(2,"M",bit(data,6,3)?"RPM":"Hz"); break;
        case '5': formatResultValue(3,"M",bit(data,6,3)?"RPM":"Hz"); break;
      }
      break;
    case 0x33:
      switch (data[0]) // Range
      {
        case '0': formatResultValue(3,"","Ohm"); break;
        case '1': formatResultValue(1,"k","Ohm"); break;
        case '2': formatResultValue(2,"k","Ohm"); break;
        case '3': formatResultValue(3,"k","Ohm"); break;
        case '4': formatResultValue(1,"M","Ohm"); break;
        case '5': formatResultValue(2,"M","Ohm"); break;
      }
      break;
    case 0x34:
      m_result.main().special = "TE";
      formatResultValue(0,"",(data[6] & 8) ? "C" : "dF"); break;
      break;
    case 0x35:
      m_result.main().special = "BUZ";
      formatResultValue(3,"","Ohm");
      break;
    case 0x36:
      // Capacitance ranges per the table in docs/protocols/sources/UT70B.log:
      // 4n 40n 400n 4µ 40µ 400µ 4m 40m, one decade per range code with no gap.
      // heha's ut.cpp encodes the same as a linear scalebase + range offset.
      // The previous table had no range '2' and was shifted by one from there.
      switch (data[0]) // Range
      {
        case '0': formatResultValue(1,"n","F"); break;
        case '1': formatResultValue(2,"n","F"); break;
        case '2': formatResultValue(3,"n","F"); break;
        case '3': formatResultValue(1,"u","F"); break;
        case '4': formatResultValue(2,"u","F"); break;
        case '5': formatResultValue(3,"u","F"); break;
        case '6': formatResultValue(1,"m","F"); break;
        case '7': formatResultValue(2,"m","F"); break;
      }
      break;
    case 0x39:
      switch (data[0]) // Range
      {
        case '0': formatResultValue(3,"m","A"); break;
        case '1': formatResultValue(1,"m","A"); break;
      }
      break;
    case 0x3b:
      switch (data[0]) // Range
      {
        case '0': formatResultValue(3,"m","V"); break;
        case '1': formatResultValue(1,"","V"); break;
        case '2': formatResultValue(2,"","V"); break;
        case '3': formatResultValue(3,"","V"); break;
        case '4': formatResultValue(0,"","V"); break;   // 4000 V, UT70B.log
      }
      break;
    case 0x3d:
      switch (data[0]) // Range
      {
        case '0':  formatResultValue(3,"u","A"); break;
        // 4000 µA range per the log's table (and heha's ut.cpp, which derives
        // it from the range code); was missing, leaving the unit empty.
        case '1':  formatResultValue(4,"u","A"); break;
      }
      break;
    case 0x3f:
      // docs/protocols/sources/UT70B.log lists byte[5] == '?' (0x3f) as the A range,
      // completing the set next to 0x39 (mA) and 0x3d (uA). This used to read
      // formatResultValue(2,"A","Hz"), i.e. "A" sat in the SI prefix argument
      // where it was meant as the unit, and the "Hz" was left over from the
      // frequency branch - yielding the unit "AHz".
      formatResultValue(2,"","A");
      break;
  }

  noValueWithoutUnit();
  return m_result;
}
