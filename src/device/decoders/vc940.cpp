#include "device/decoders/vc940.h"
// ES51966

static const bool registered = []() {
  DmmDecoder::addConfig({"Tenma", "72-7732"    , "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 40000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT71B"  , "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 200000, 0, 0, 1});
  // ut71x family per libsigrok; "*" = not confirmed on hardware yet
  DmmDecoder::addConfig({"Uni-Trend", "UT71A *", "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 20000, 0, 0, 1});
  DmmDecoder::addConfig({"Tenma", "72-7730 *", "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 20000, 0, 0, 1});
  DmmDecoder::addConfig({"Tenma", "72-9380A *", "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 40000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT71CDE", "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 40000, 0, 0, 1});
  DmmDecoder::addConfig({"Uni-Trend", "UT804"  , "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 40000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 920" , "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 40000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 940" , "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 40000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 960" , "", 2400, FrameFormat::VC940Continuous, 7, 1, 1, 2, 40000, 0, 0, 1});
  return true;
}();

size_t DecoderVC940::getPacketLength()
{
  return  (m_type == FrameFormat::VC940Continuous ? 11 : 0);
}

bool DecoderVC940::checkFormat(const char* data, size_t idx)
{
  // The 12 is deliberate: it skips the first of two identical telegrams so one
  // measurement yields one reading (same scheme as es51986/es51962). Evidence is
  // the original 2015 readerthread.cpp, which used ">= 10" for the M9803R but
  // ">= 12" for the VC940 at the identical 11-byte packet length - the author had
  // the device and chose to differ. Open question: docs/protocols/sources/UT71BCDE.log
  // and UT804.log mention no duplication for the Uni-T models later added to this
  // decoder, so those may be losing every second reading here. Resolving that
  // needs per-device handling, not a change to this guard.
  return (m_type==FrameFormat::VC940Continuous && idx >= 12 && ringByte(data, idx, 1) == 0x0d && data[idx] == 0x0a);
}

std::optional<DmmDecoder::DmmResponse> DecoderVC940::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id     = id;
  m_result.showBar = true;
  m_result.hold   = false;
  m_result.range  = bit(data,8,0) ? "AUTO":"MANU";
  m_result.main().special= bit(data,7,0) ? "AC"  :"DC";
  m_result.main().special+= bit(data,7,1) ? "DC" : "";

  int function = data[6] & 0x0f;
  int range    = data[5] & 0x0f;
  int mode2    = data[8];
  // Byte 8 carries NEG, MAN and AUTO in bits 2, 1 and 0 (field table in
  // docs/protocols/sources/UT71BCDE.log; heha's ut.cpp: rbuf[8]&4). Bit 3 was
  // read before, so negative readings came out positive.
  bool neg     = bit(data,8,2);

  Q_UNUSED(mode2)

  m_result.main().val = makeValue(data,0,3, (function != 0xC && neg));
  // a fifth digit; ':' is its blank (the nibble A) on four digit ranges
  if (data[4] != ':')
    m_result.main().val += data[4];

  switch (function)
  {
    case 0x0:formatResultValue(3,"m","V"); break;
    case 0x1:formatResultValue(range,"","V");  m_result.main().special = "DC"; break;
    // V AC; byte 7 says whether it is AC+DC (UT71/UT804 "A+D"), keep that
    case 0x2:formatResultValue(range,"","V");  if (m_result.main().special != "ACDC") m_result.main().special = "AC"; break;
    // mV has a single 400.00 range whose code is 0; the code is not the
    // decimal position here as it is for the V ranges.
    case 0x3:formatResultValue(3,"m","V");     m_result.main().special = "DC"; break;
    case 0x4:
      m_result.main().special = "OH";
      switch (range)
      {
        case 1:formatResultValue(3,"","Ohm");  break;
        case 2:formatResultValue(1,"k","Ohm"); break;
        case 3:formatResultValue(2,"k","Ohm"); break;
        case 4:formatResultValue(3,"k","Ohm"); break;
        case 5:formatResultValue(1,"M","Ohm"); break;
        case 6:formatResultValue(2,"M","Ohm"); break;
      }
      break;
    case 0x5:
      m_result.main().special = "CA";
      switch (range)
      {
        case 1:formatResultValue(2,"n","F"); break;
        case 2:formatResultValue(3,"n","F"); break;
        case 3:formatResultValue(1,"u","F"); break;
        case 4:formatResultValue(2,"u","F"); break;
        case 5:formatResultValue(3,"u","F"); break;
        case 6:formatResultValue(1,"m","F"); break;   // 4.000 mF per the range table
        case 7:formatResultValue(2,"m","F"); break;   // 40.00 mF
      }
      break;
    case 0x6:formatResultValue(4,"","C");  m_result.main().special = "TE"; break;
    case 0x7:
      switch (range)
      {
        case 0:formatResultValue(3,"u","A"); break;
        case 1:formatResultValue(4,"u","A"); break;
      }
      break;
    case 0x8:
      switch (range)
      {
        case 0:formatResultValue(2,"m","A"); break;
        case 1:formatResultValue(3,"m","A"); break;
      }
      break;
    case 0x9: formatResultValue(2,"","A"); break;
    case 0xA:
      m_result.main().special = "BUZ";
      formatResultValue(3,"","Ohm");
      break;
    case 0xB:
      m_result.main().special = "DI";
      formatResultValue(1,"","V");
      break;
    case 0xC:
      if (neg)
      {
        formatResultValue(3,"","%");
        m_result.main().special = "PC";
      }
      else
      {
        m_result.main().special = "FR";
        switch (range)
        {
          case 0:formatResultValue(2,"","Hz"); break;
          case 1:formatResultValue(3,"","Hz"); break;
          case 2:formatResultValue(1,"k","Hz"); break;
          case 3:formatResultValue(2,"k","Hz"); break;
          case 4:formatResultValue(3,"k","Hz"); break;
          case 5:formatResultValue(1,"M","Hz"); break;
          case 6:formatResultValue(2,"M","Hz"); break;  // 40.00 MHz per the range table
          case 7:formatResultValue(3,"M","Hz"); break;  // 400.0 MHz
        }
      }
      break;
    case 0xD:
      m_result.main().special = "TE";
      formatResultValue(4,"","dF"); break;
    case 0xF:
      // 4-20 mA loop tester, shown in percent (function table in UT71BCDE.log;
      // heha's ut.cpp mode 15). Full scale "100" on five digits -> 100.00.
      m_result.main().special = "PC";
      formatResultValue(3,"","%");
      break;
      break;
  }

  // no digits on the display: nibble A in the first place, the second says
  // which word (UT804.log, UT71BCDE.log)
  if (data[0] == ':')
  {
    DmmChannel &main = m_result.main();
    main.dval = 0;
    if (data[1] == '<')
    {
      main.val = QStringLiteral("LO");   // below the 4-20 mA loop: no reading, no overload
      main.noValue = true;
    }
    else if (data[1] == '?')
      main.val = QStringLiteral("HI");
    else
      main.val = function != 0xC && neg ? QStringLiteral("-OL") : QStringLiteral("OL");
  }

  noValueWithoutUnit();
  return m_result;
}
