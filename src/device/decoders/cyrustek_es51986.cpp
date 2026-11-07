#include "device/decoders/cyrustek_es51986.h"

// same as es51981, but less features. no collisons between features

static const bool registered = []() {
  DmmDecoder::addConfig({"Iso-Tech", "IDM 73", "", 19200, FrameFormat::CyrustekES51986, 7, 1, 1, 2, 6000, 0, 0, 1});
  // ES51986 per the chip datasheet, settings as the IDM 73 (same family); "*" =
  // not confirmed on hardware yet
  DmmDecoder::addConfig({"APPA", "71 *", "", 19200, FrameFormat::CyrustekES51986, 7, 1, 1, 2, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"APPA", "73 *", "", 19200, FrameFormat::CyrustekES51986, 7, 1, 1, 2, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Tenma", "72-1016"  , "", 19200, FrameFormat::CyrustekES51986, 7, 1, 1, 2, 6000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend","UT803" , "", 19200, FrameFormat::CyrustekES51986, 7, 1, 1, 2, 6000, 0, 0, 1});
  return true;
}();

bool DecoderCyrusTekES51986::checkFormat(const char* data, size_t idx)
{
  // The 12 is deliberate and must stay above the 11-byte packet length: the
  // UT803 sends every telegram twice in a row (see docs/protocols/sources/UT803.log),
  // so the first copy's terminator lands at idx == 10 and is skipped, and only
  // the second copy at idx == 21 matches. That yields one reading per
  // measurement instead of two identical ones. Do not "simplify" this to
  // getPacketLength() - unlike vc940, this protocol duplicates its frames.
  return (m_type == FrameFormat::CyrustekES51986 && idx >= 12 && ringByte(data, idx, 1) == 0x0d && data[idx] == 0x0a);
}

size_t DecoderCyrusTekES51986::getPacketLength()
{
  return  (m_type == FrameFormat::CyrustekES51986 ? 11 : 0);
}

std::optional<DmmDecoder::DmmResponse> DecoderCyrusTekES51986::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id     = id;
  m_result.showBar= true;
  m_result.hold       = bit(data,7,3);
  m_result.range      = bit(data,8,1) ? "AUTO":"MANU";
  m_result.main().val = makeValue(data,1,4, bit(data,6,2));
  m_result.main().special="";
  bool ovl        = bit(data,6,0);
  QString unit_temp= bit(data,6,3) ? "C" : "F";
  int function = data[5] & 0x0f; //15
  int range    = data[0] & 0x0f; //10

  if (bit(data,8,2))
    m_result.main().special+= "AC";
  if (bit(data,8,3))
    m_result.main().special+= "DC";

  switch (function)
  {
    case 0xB:
      switch (range)
      {
        case 0: formatResultValue(1,"","V"); break;
        case 1: formatResultValue(2,"","V"); break;
        case 2: formatResultValue(3,"","V"); break;
        case 3: formatResultValue(0,"","V"); break;
        case 4: formatResultValue(3,"m","V");break;
      }
      break;
    case 0x3:
      m_result.main().special = "OH";
      switch (range)
      {
        case 0: formatResultValue(3,"","Ohm");  break;
        case 1: formatResultValue(1,"k","Ohm"); break;
        case 2: formatResultValue(2,"k","Ohm"); break;
        case 3: formatResultValue(3,"k","Ohm"); break;
        case 4: formatResultValue(1,"M","Ohm"); break;
        case 5: formatResultValue(2,"M","Ohm"); break;
      }
      break;
    case 0x4:
      m_result.main().special = "TE";
      m_result.showBar= false;
      formatResultValue(0,"",unit_temp); break;
      break;
    case 0x6:
      m_result.main().special = "CA";
      switch (range)
      {
        case 0: formatResultValue(1,"n","F"); break;
        case 1: formatResultValue(2,"n","F"); break;
        case 2: formatResultValue(3,"n","F"); break;
        case 3: formatResultValue(1,"u","F"); break;
        case 4: formatResultValue(2,"u","F"); break;
        case 5: formatResultValue(3,"u","F"); break;
        case 6: formatResultValue(1,"m","F"); break;
      }
      break;
    case 0xD:
      switch (range)
      {
        case 0: formatResultValue(3,"u","A"); break;
        case 1: formatResultValue(4,"u","A"); break;
      }
      break;
    case 0xF:
      switch (range)
      {
        case 0: formatResultValue(2,"m","A"); break;
        case 1: formatResultValue(3,"m","A"); break;
      }
      break;
    case 0x9:
      formatResultValue(2,"","A");
      break;
    case 0x5:   // buzzer
      m_result.main().special = "BUZ";
      formatResultValue(3,"","Ohm");
      break;
    case 0x1:
      m_result.main().special = "DI";
      formatResultValue(1,"","V");
      break;
    case 0x2:
      m_result.main().special = "FR";
      switch (range)
      {
        case 0: formatResultValue(0,"","Hz"); break;
        case 1: formatResultValue(2,"k","Hz"); break;
        case 2: formatResultValue(3,"k","Hz"); break;
        case 3: formatResultValue(1,"M","Hz"); break;
        case 4: formatResultValue(2,"M","Hz"); break;
      }
      break;
    case 0xE:
      m_result.main().special = "HFE";
      formatResultValue(4,"m","A");
      break;
  }

  if (ovl) {
    m_result.main().val  = "OL";
    m_result.main().dval = 0;
  }
  noValueWithoutUnit();
  return m_result;
}
