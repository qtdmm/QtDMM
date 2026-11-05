#include "device/decoders/ascii.h"

#include "core/sampletypes.h"
#include "core/siprefix.h"

static const bool registered = []() {
  DmmDecoder::addConfig({"Digitech", "QM1350", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"MASTECH", "MAS-343", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"MASTECH", "MAS-345", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"McVoice", "M-345pro", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "M-3360D *", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});   // CDMM table; unconfirmed
  DmmDecoder::addConfig({"Metex", "M-3640D *", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});   // libsigrok metex14; unconfirmed
  DmmDecoder::addConfig({"Metex", "Universal System 9140 *", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});   // CDMM table
  DmmDecoder::addConfig({"Voltcraft", "MS-9140 *", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});   // CDMM table
  DmmDecoder::addConfig({"Voltcraft", "MS-9160 *", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});   // CDMM table
  DmmDecoder::addConfig({"Metex", "M-3660D", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "M-3830D", "", 1200, FrameFormat::Metex14, 7, 2, 4, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "M-3840D", "", 1200, FrameFormat::Metex14, 7, 2, 4, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "M-3850D", "", 1200, FrameFormat::Metex14, 7, 2, 4, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "M-3850M", "", 9600, FrameFormat::Metex14, 7, 2, 4, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "M-3870D", "", 1200, FrameFormat::Metex14, 7, 1, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "M-4650C", "", 1200, FrameFormat::Metex14, 7, 2, 4, 0, 20000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "ME-11", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "ME-22", "", 2400, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "ME-32", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "ME-21 *", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});   // libsigrok-master metex14; unconfirmed
  DmmDecoder::addConfig({"Metex", "ME-42", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Metex", "universal system 9160", "", 1200, FrameFormat::Metex14, 7, 2, 4, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"PeakTech", "4010", "", 9600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"PeakTech", "4015A", "", 9600, FrameFormat::Metex14, 7, 2, 4, 0, 100000, 0, 0, 1});
  DmmDecoder::addConfig({"PeakTech", "4360", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"PeakTech", "4390", "", 9600, FrameFormat::Metex14, 7, 2, 4, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Radioshack", "22-805 DMM", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Radioshack", "RS22-168A", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Sinometer", "MAS-343", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "M-3610D", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "M-3650D", "", 1200, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "M-3860", "", 9600, FrameFormat::Metex14, 7, 2, 4, 0, 20000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "M-4660", "", 1200, FrameFormat::Metex14, 7, 2, 4, 0, 50000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "ME-11", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "ME-22T", "", 2400, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "ME-32", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "ME-42", "", 600, FrameFormat::Metex14, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "M-4660A", "", 9600, FrameFormat::Metex14, 7, 2, 4, 0, 50000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "M-4660M", "", 9600, FrameFormat::Metex14, 7, 2, 4, 0, 50000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "MXD-4660A", "", 9600, FrameFormat::Metex14, 7, 2, 4, 0, 50000, 0, 0, 1});
  DmmDecoder::addConfig({"PeakTech", "451", "", 600, FrameFormat::PeakTech10, 7, 2, 1, 0, 4000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "M-4650CR", "", 1200, FrameFormat::Voltcraft14Continuous, 7, 2, 1, 0, 20000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 670", "", 4800, FrameFormat::Voltcraft14Continuous, 7, 1, 1, 0, 50000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 630", "", 4800, FrameFormat::Voltcraft14Continuous, 7, 1, 1, 0, 50000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 650", "", 4800, FrameFormat::Voltcraft14Continuous, 7, 1, 1, 0, 50000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 635", "", 2400, FrameFormat::Voltcraft15Continuous, 7, 1, 1, 0, 50000, 0, 0, 1});
  DmmDecoder::addConfig({"Voltcraft", "VC 655", "", 2400,  FrameFormat::Voltcraft15Continuous, 7, 1, 1, 0, 50000, 0, 0, 1});
  return true;
}();

bool DecoderAscii::checkFormat(const char* data, size_t idx)
{
  switch(m_type)
  {
    // "#VVVVVVUUUU": complete when the '#' is ten bytes back. Only once ten
    // bytes are in: before that the look-back lands on stale bytes of the ring
    case FrameFormat::PeakTech10: return idx >= 10 && ringByte(data, idx, 10) == '#';
    case FrameFormat::Metex14:
    case FrameFormat::Voltcraft14Continuous: return (data[idx] == 0x0d);
    case FrameFormat::Sigrok:                return (data[idx] == 0x0a);
    case FrameFormat::Voltcraft15Continuous: return (ringByte(data, idx, 1) == 0x0d && data[idx] == 0x0a);
    default: return false;
  }
}

size_t DecoderAscii::getPacketLength()
{
  switch (m_type)
  {
    case FrameFormat::Sigrok:                return 30;
    case FrameFormat::PeakTech10:            return 11;
    case FrameFormat::Metex14:               return 14;
    case FrameFormat::Voltcraft14Continuous: return 14;
    case FrameFormat::Voltcraft15Continuous: return 15;
    default: return 0;
  }
}

bool DecoderAscii::decodeSigrok(QString str)
{
  // "<channel>: <value> <unit> <flags...>". Unitless readings (continuity,
  // SR_UNIT_BOOLEAN) stop after the value: "P1: 0.00".
  QStringList list = str.trimmed().split(" ", Qt::SkipEmptyParts);
  if (list.size()<2)
    return false;
  // the channel: P1 the main value, P2 ... the meter's others (a sigrok
  // meter with several channels, CalcDevice with several values), each in
  // a line of its own; past kMaxValues none of them
  static const QRegularExpression channel("^P(\\d+):$");
  const QRegularExpressionMatch ch = channel.match(list[0]);
  if (ch.hasMatch())
  {
    const int n = ch.captured(1).toInt();
    if (n > kMaxValues)
      return false;
    m_result.main().id = qMax(1, n) - 1;
  }
  m_result.main().val = list[1];
  if (m_result.main().val == "inf")
    m_result.main().val = " OL ";
  else if (m_result.main().val == "-inf")
    m_result.main().val = "-OL ";

  // libsigrok spells the units the same for every driver; map the ones the
  // other decoders spell differently, so units compare equal across protocols
  m_result.main().unit = list.size() > 2 ? list[2] : QString();
  // libsigrok prints a value without unit only for continuity (SR_UNIT_BOOLEAN)
  if (m_result.main().unit.isEmpty())
    m_result.main().special = "BUZ";
  m_result.main().unit.replace(QStringLiteral("Ω"), QStringLiteral("Ohm"));
  if (m_result.main().unit == QStringLiteral("°C"))
    m_result.main().unit = QStringLiteral("C");
  else if (m_result.main().unit == QStringLiteral("°F"))
    m_result.main().unit = QStringLiteral("dF");

  // all fields: CalcDevice puts the coupling where sigrok has the channel
  // ("DC 6 W AUTO"), and no unit or value is spelled like a flag
  m_result.range = "MANU";
  for(auto const& item : list)
  {
    if (item == "HOLD") m_result.hold = true;
    else if (item == "AUTO") m_result.range = "AUTO";
    else if (item == "DC") m_result.main().special = "DC";
    else if (item == "AC") m_result.main().special = "AC";
    else if (item == "DIODE") m_result.main().special = "DI";
  }
  return true;
}


std::optional<DmmDecoder::DmmResponse> DecoderAscii::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id = id;
  m_result.range = "";
  m_result.hold = false;
  m_result.showBar = true;
    QString str(data);

  switch (m_type)
  {
    case FrameFormat::Metex14:
    case FrameFormat::Voltcraft14Continuous:
    case FrameFormat::Voltcraft15Continuous:
      m_result.main().val     = str.mid(2, 7).trimmed();
      m_result.main().unit    = str.mid(9, 4).trimmed();
      m_result.main().special = str.left(3).trimmed();
      break;
    case FrameFormat::PeakTech10:
      m_result.main().val  = str.mid(1, 6).trimmed();
      m_result.main().unit = str.mid(7, 4).trimmed();
      break;
    case FrameFormat::Sigrok:
      if (!decodeSigrok(str))
        return std::nullopt;
      break;
    default:
      return std::nullopt;
  }

  m_result.main().dval = QString(m_result.main().val).remove(' ').toDouble();   // "- 0.24"

  switch (m_result.main().unit.length())
  {
    case 0:
      if (m_type!=FrameFormat::Sigrok) return std::nullopt;
        formatResultValue(0, "", m_result.main().unit);
        break;
    case 1:
      formatResultValue(0, "", m_result.main().unit);
      break;
    default:
    {
      // Metex meters send kilo as a capital K ("KOhm", "KHz"; see
      // docs/protocols/sources/metex-22t.log, and heha's Metex.cpp lists
      // "pnum KMG" as the prefix alphabet). Normalise it so the value is scaled
      // and the unit matches what LcdWidget knows.
      QString unit = m_result.main().unit;
      if (unit.startsWith('K'))
        unit[0] = 'k';
      // split() keeps units of their own whole: 415 ppm, 5 min, 3.5 m/s
      const SiPrefix::Split split = SiPrefix::split(unit);
      if (split.prefix.isEmpty())
        formatResultValue(0, "", m_result.main().unit);
      else
        formatResultValue(0, split.prefix, split.baseUnit);
      break;
    }
  }
  return m_result;
}
