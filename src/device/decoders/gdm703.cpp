#include "device/decoders/gdm703.h"
#include "core/siprefix.h"

// Voltcraft GDM 703 family (ablage/CDMM/src/dmmclass.cpp): 9600 8N1, DTR,
// two values per frame. Marked "*": ported from CDMM without a capture.
static const bool registered = []() {
  DmmDecoder::addConfig({"Voltcraft", "GDM 703 *", "", 9600, FrameFormat::GDM703Continuous, 8, 1, 1, 0, 4000, 0, 0, 1, "", "", 2});
  DmmDecoder::addConfig({"Voltcraft", "GDM 704 *", "", 9600, FrameFormat::GDM703Continuous, 8, 1, 1, 0, 4000, 0, 0, 1, "", "", 2});
  DmmDecoder::addConfig({"Voltcraft", "GDM 705 *", "", 9600, FrameFormat::GDM703Continuous, 8, 1, 1, 0, 4000, 0, 0, 1, "", "", 2});
  return true;
}();

size_t DecoderGDM703::getPacketLength()
{
  return (m_type == FrameFormat::GDM703Continuous ? 26 : 0);
}

bool DecoderGDM703::checkFormat(const char *data, size_t idx)
{
  // frame ends with ETX; the STX 25 bytes earlier confirms the alignment
  return idx >= 25 && data[idx] == 0x03 && ringByte(data, idx, 25) == 0x02;
}

bool DecoderGDM703::parseValue(const QString &value, const QString &prefixUnit, QString &val, double &dval, QString &unit)
{
  val = value.trimmed();
  const QString pu = prefixUnit.trimmed();
  const SiPrefix::Split split = SiPrefix::split(pu);
  unit = pu;
  bool ok = false;
  dval = val.toDouble(&ok);
  if (!ok)
    return false;
  dval *= SiPrefix::factor(split.prefix);
  return true;
}

std::optional<DmmDecoder::DmmResponse> DecoderGDM703::decode(const QByteArray &data, int /*id*/)
{
  if (data.size() != 26 || data[0] != 0x02 || data[25] != 0x03)
    return std::nullopt;
  const QString frame = QString::fromLatin1(data);

  m_result = {};
  // one frame carries both displays: main is id 0, the second id 1 (and
  // channels 2 in the table, so the display shows it)
  m_result.main().id = 0;
  DmmChannel &second = m_result.addChannel(1);
  m_result.range = "";
  m_result.hold = false;
  m_result.showBar = true;

  // main display: mode (2), value (6), prefix + unit (4)
  const QString mode = frame.mid(1, 2).trimmed().toUpper();
  QString val, unit;
  double dval = 0;
  const bool numeric = parseValue(frame.mid(3, 6), frame.mid(9, 4), val, dval, unit);
  if (!numeric)
    val = " OL ";
  m_result.main().val = val;
  m_result.main().dval = numeric ? dval : 0.0;
  m_result.main().unit = unit;
  if (mode == "AC")       m_result.main().special = "AC";
  else if (mode == "DC")  m_result.main().special = "DC";
  else if (mode == "OH" || unit.endsWith("Ohm")) m_result.main().special = "OH";
  else if (mode == "DI")  m_result.main().special = "DI";
  else if (mode == "FR" || unit.endsWith("Hz")) m_result.main().special = "HZ";
  else                    m_result.main().special = mode;

  // secondary display: 'B', value (6), prefix + unit (4)
  QString val2, unit2;
  double dval2 = 0;
  if (frame[14] == 'B' && parseValue(frame.mid(15, 6), frame.mid(21, 4), val2, dval2, unit2))
  {
    second.dval = dval2;
    second.val = val2;
    second.unit = unit2;
  }
  return m_result;
}
