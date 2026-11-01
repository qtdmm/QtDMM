#include "device/decoders/brymen_bm52x.h"

// Brymen BM52x / BM82x over the BU-86X adapter (libsigrok "brymen-bm52x",
// "brymen-bm82x", "hid/bu86x"). "*": not confirmed on hardware. A Minipa
// ET-2615A (BM525s) and ET-2517A (BM827s) were, by their captures for
// dmm-tools (issues #35, #36), now the real vectors of the specs.
static const bool registered = []() {
  DmmDecoder::addConfig({"Brymen", "BM525s", "", 0, FrameFormat::BrymenBM52x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Minipa", "ET-2615A", "", 0, FrameFormat::BrymenBM52x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Brymen", "BM829s *", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  // the same meters under other names, and their smaller siblings
  // (HKJ's TestController device list, "BrymenDMM": protocol 525 or 829)
  DmmDecoder::addConfig({"Brymen", "BM521s *", "", 0, FrameFormat::BrymenBM52x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Elma", "BM525s *", "", 0, FrameFormat::BrymenBM52x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Brymen", "BM821s *", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Brymen", "BM822s *", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Brymen", "BM827s", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Minipa", "ET-2517A", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Elma", "BM821s *", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Elma", "BM829s *", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Greenlee", "DM-810A *", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Greenlee", "DM-820A *", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Greenlee", "DM-830A *", "", 0, FrameFormat::BrymenBM82x, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  return true;
}();

size_t DecoderBrymenBM52x::getPacketLength()
{
  return (m_type == FrameFormat::BrymenBM52x || m_type == FrameFormat::BrymenBM82x) ? 24 : 0;
}

QByteArray DecoderBrymenBM52x::pollRequest() const
{
  QByteArray r = QByteArray::fromHex("005266");
  r[1] = static_cast<char>(modelId());
  return r;
}

bool DecoderBrymenBM52x::checkFormat(const char *data, size_t idx)
{
  for (int back = 4; back <= 7; back++)
    if (ringByte(data, idx, back) != modelId())
      return false;
  return true;
}

char DecoderBrymenBM52x::digit(unsigned char b)
{
  switch (b & ~0x10)
  {
    case 0x40: return '-';
    case 0xaf: return '0';
    case 0xa0: return '1';
    case 0xcb: return '2';
    case 0xe9: return '3';
    case 0xe4: return '4';
    case 0x6d: return '5';
    case 0x6f: return '6';
    case 0xa8: return '7';
    case 0xef: return '8';
    case 0xed: return '9';
    case 0x0f: return 'C';
    case 0x4e: return 'F';
    case 0x07: return 'L';
    case 0xe3: return 'd';
    case 0x20: return 'i';
    case 0x63: return 'o';
    case 0xee: return 'A';
    case 0x23: return 'u';
    case 0x47: return 't';
    default:   return '\0';
  }
}

QString DecoderBrymenBM52x::digits(const unsigned char *pkt, unsigned char signFlag, QChar *tempUnit)
{
  QString text;
  if (pkt[0] & signFlag)
    text += '-';
  for (int pos = 0; pos < 4; pos++)
  {
    const unsigned char byte = pkt[1 + pos];
    const char c = digit(byte);
    if (pos == 3 && (c == 'C' || c == 'F'))
    {
      if (tempUnit)
        *tempUnit = QChar(c);
    }
    else if (c)
      text += c;
    if (pos < 3 && (byte & 0x10))
      text += '.';
  }
  return text;
}

std::optional<DmmDecoder::DmmResponse> DecoderBrymenBM52x::decode(const QByteArray &data, int /*id*/)
{
  if (data.size() != 24)
    return std::nullopt;
  const unsigned char *buf = reinterpret_cast<const unsigned char *>(data.constData());
  for (int i = 16; i <= 19; i++)
    if (buf[i] != modelId())
      return std::nullopt;

  m_result = {};
  // one frame carries both displays: main is id 0, the second id 1 (and
  // channels 2 in the table, so the display shows it)
  m_result.main().id = 0;
  DmmChannel &sub = m_result.addChannel(1);
  m_result.showBar = true;
  m_result.hold = (buf[20] & 0x80) != 0;
  m_result.range = (buf[20] & 0x10) ? "AUTO" : "MANU";

  // --- main display ---
  QChar tempUnit;
  const QString second = digits(&buf[7], 0, nullptr);
  const bool isDiode = second == "diod";
  QString text = digits(&buf[2], 0x80, &tempUnit);
  const bool overload = text.contains("0L") || text.contains("0.L");
  const bool isDb = (buf[6] & 0x10) != 0;
  bool mainMilli = (buf[14] & 0x40) != 0;

  const bool dc = (buf[1] & 0x20) != 0;
  const bool ac = (buf[1] & 0x10) != 0;
  QString unit, special = dc && ac ? "ACDC" : dc ? "DC" : (ac ? "AC" : "");
  if (buf[14] & 0x20)      { unit = "V"; if (isDiode) special = "DI"; }
  else if (buf[14] & 0x10) { unit = "A"; }
  else if (buf[14] & 0x01) { unit = "F"; special = "CA"; }
  else if (buf[14] & 0x02) { unit = "S"; }
  else if (buf[13] & 0x10) { unit = "Hz"; special = "HZ"; }
  else if (buf[7] & 0x01)  { unit = "Ohm"; special = "BUZ"; }
  else if (buf[13] & 0x20) { unit = "Ohm"; special = "OH"; }
  else if (isDb && mainMilli) { unit = "dBm"; special = "PO"; }
  else if (buf[14] & 0x04) { unit = "%"; special = "DU"; }
  else if ((buf[2] & 0x09) && !tempUnit.isNull())
  {
    unit = tempUnit == 'F' ? "dF" : "C";
    special = "TE";
  }
  if (isDb)
    mainMilli = false;
  QString prefix;
  if (buf[14] & 0x08) prefix = "n";
  if (buf[14] & 0x80) prefix = "u";
  if (mainMilli)      prefix = "m";
  if (buf[13] & 0x80) prefix = "k";
  if (buf[13] & 0x40) prefix = "M";
  if (unit == "dBm" || special == "TE")
    prefix.clear();

  bool ok = false;
  const double value = text.toDouble(&ok);
  m_result.main().unit = prefix + unit;
  m_result.main().special = special;
  if (overload)
  {
    m_result.main().val = QStringLiteral(" OL ");
    m_result.main().dval = 0.0;
  }
  else if (!ok)
  {
    // no reading: dashes (a thermocouple missing, a range or function
    // being changed, also beside a lit unit), a lone point, "Auto" while
    // AutoCheck waits (dmm-tools, captures of a BM827s and a BM525s)
    text.remove('.');
    m_result.main().val = text.isEmpty() ? QStringLiteral("----") : text;
    m_result.main().noValue = true;
  }
  else
  {
    m_result.main().val = text;
    m_result.main().dval = value * prefixFactor(prefix);
  }

  // --- secondary display ---
  const QString text2 = digits(&buf[7], 0x20, nullptr);
  bool ok2 = false;
  const double value2 = text2.toDouble(&ok2);
  if (ok2 && !isDiode && second != "Auto" && !text2.contains("---"))
  {
    QString unit2;
    if (buf[12] & 0x10)      unit2 = "V";
    else if (buf[12] & 0x20) unit2 = (buf[11] & 0x10) ? "%" : "A";
    else if (buf[13] & 0x02) unit2 = "Ohm";
    else if (buf[12] & 0x02) unit2 = "S";
    else if (buf[12] & 0x01) unit2 = "F";
    else if (buf[7] & 0x06)  unit2 = tempUnit.isNull() ? QString() : tempUnit == 'F' ? "dF" : "C";
    else if (buf[13] & 0x01) unit2 = "Hz";
    else if (buf[11] & 0x08) unit2 = "%";
    QString prefix2;
    if (buf[12] & 0x04) prefix2 = "n";
    if (buf[12] & 0x40) prefix2 = "u";
    if (buf[12] & 0x80) prefix2 = "m";
    if (buf[13] & 0x04) prefix2 = "k";
    if (buf[13] & 0x08) prefix2 = "M";
    if (!unit2.isEmpty())
    {
      sub.dval = value2 * prefixFactor(prefix2);
      sub.val = text2;
      sub.unit = prefix2 + unit2;
    }
  }
  m_result.lowBat = (buf[7] & 0x08) != 0;
  return m_result;
}
