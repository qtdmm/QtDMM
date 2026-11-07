#include "device/decoders/cyrustek_es51922.h"

static const bool registered = []() {
  DmmDecoder::addConfig(  {"Uni-Trend","UT61E","", 19200, FrameFormat::CyrustekES51922, 7, 1, 1, 2, 22000, 0, 0, 1});
  DmmDecoder::addConfig(  {"Wintex","TD2200","", 19200, FrameFormat::CyrustekES51922, 7, 1, 1, 0, 22000, 0, 0, 1});
  return true;
}();

bool DecoderCyrusTekES51922::checkFormat(const char* data, size_t idx)
{
  return (m_type == FrameFormat::CyrustekES51922 && (idx) && (data[idx - 1] == 0x0d) && (data[idx] == 0x0a));
}

size_t DecoderCyrusTekES51922::getPacketLength()
{
  return  (m_type == FrameFormat::CyrustekES51922 ? 14 : 0);
}

std::optional<DmmDecoder::DmmResponse> DecoderCyrusTekES51922::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id     = id;
  m_result.showBar = true;
  m_result.hold   = bit(data,11,1);
  m_result.range  = bit(data,10,1) ? "AUTO":"MANU";
  m_result.main().special= bit(data,10,2) ? "AC"  :"DC";
  m_result.main().val    = makeValue(data,1,5, bit(data,7,2));
  bool ul         = bit(data,9,3);
  bool ovl        = bit(data,7,0);
  bool hz         = bit(data,10,0);
  bool duty       = bit(data,7,3);
  int function    = data[6] & 0x0f;
  int range       = data[0] & 0x0f;

  // Frequency ranges per the range table in docs/protocols/sources/UT61E.log
  // ('0'=220.00, '1'=2200.0, no '2', '3'=22.000k ... '7'=220.00M). The same
  // table applies when the Hz button is pressed in the V or A functions: the
  // log notes the Hz reading is independent of the V/mV range, and heha's
  // DMM.EXE (ut.cpp) switches to its frequency mode outright in that case.
  auto formatFrequency = [this](int r)
  {
    switch (r)
    {
      case 0: formatResultValue(3,"","Hz");  break;
      case 1: formatResultValue(4,"","Hz");  break;
      case 3: formatResultValue(2,"k","Hz"); break;
      case 4: formatResultValue(3,"k","Hz"); break;
      case 5: formatResultValue(1,"M","Hz"); break;
      case 6: formatResultValue(2,"M","Hz"); break;
      case 7: formatResultValue(3,"M","Hz"); break;
    }
  };

  if (duty)
  {
    // Duty cycle is always shown as 100.0-style, whatever the range code
    // (the table's '%' column reads '100.0*' for every range).
    if (function == 0x2) m_result.main().special = "FR";
    formatResultValue(4,"","%");
  }
  else if (hz && function != 0x2)
  {
    formatFrequency(range);
  }
  else switch (function)
  {
    case 0xB:
      switch (range)
      {
        case 0: formatResultValue(1,"","V"); break;
        case 1: formatResultValue(2,"","V"); break;
        case 2: formatResultValue(3,"","V"); break;
        case 3: formatResultValue(4,"","V"); break;
        case 4: formatResultValue(3,"m","V");break;
      }
      break;
    case 0x1:
      // Diode test reads in volts with a 2.2000-style format: the log shows
      // the display as '.0L V', and heha's ut.cpp uses the V unit code here.
      m_result.main().special = "DI";
      formatResultValue(1,"","V");
      break;
    case 0x2:
      m_result.main().special = "FR";
      formatFrequency(range);
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
        case 6: formatResultValue(3,"M","Ohm"); break;
      }      break;
    case 0x5:
      m_result.main().special = "BUZ";
      formatResultValue(3,"","Ohm");
      break;
    case 0x6:
      m_result.main().special = "CA";
      switch (range)
      {
        case 0: formatResultValue(2,"n","F"); break;
        case 1: formatResultValue(3,"n","F"); break;
        case 2: formatResultValue(1,"u","F"); break;
        case 3: formatResultValue(2,"u","F"); break;
        case 4: formatResultValue(3,"u","F"); break;
        case 5: formatResultValue(1,"m","F"); break;
        case 6: formatResultValue(2,"m","F"); break;
        case 7: formatResultValue(3,"m","F"); break;
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
    case 0x0:
      formatResultValue(2,"","A");
      break;
  }

  if (ul) {
    m_result.main().val  = "UL";
    m_result.main().dval = 0;
  }
  if (ovl) {
    m_result.main().val  = "OL";
    m_result.main().dval = 0;
  }
  noValueWithoutUnit();
  return m_result;
}
