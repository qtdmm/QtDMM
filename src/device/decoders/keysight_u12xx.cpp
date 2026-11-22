// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/keysight_u12xx.h"

#include "core/siprefix.h"

// The models libsigrok's agilent-dmm knows; no meter at hand, so "*", but
// the U1282A is confirmed by a user (issue #603).
// Display counts from the data sheets' "counts" (U123x 6000, U124x 10000,
// U125x 50000, U127x 30000, U128x 60000).
static const bool registered = []() {
  DmmDecoder::addConfig({"Agilent", "U1231A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1232A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1233A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1241A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 10000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1241B *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 10000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1242A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 10000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1242B *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 10000, 0, 0, 0});
  DmmDecoder::addConfig({"Keysight", "U1241C *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 10000, 0, 0, 0});
  DmmDecoder::addConfig({"Keysight", "U1242C *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 10000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1251A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 50000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1251B *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 50000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1252A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 50000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1252B *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 50000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1253A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 50000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1253B *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 50000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1271A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 30000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1272A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 30000, 0, 0, 0});
  DmmDecoder::addConfig({"Agilent", "U1273A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 30000, 0, 0, 0});
  DmmDecoder::addConfig({"Keysight", "U1273AX *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 30000, 0, 0, 0});
  DmmDecoder::addConfig({"Keysight", "U1281A *", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 60000, 0, 0, 0});
  DmmDecoder::addConfig({"Keysight", "U1282A", "", 9600, FrameFormat::KeysightU12xx, 8, 1, 1, 0, 60000, 0, 0, 0});
  return true;
}();

QByteArray DecoderKeysightU12xx::pollRequest() const
{
  // the function now and then even without a switch message, in case one
  // got lost; the terminator as libsigrok sends it. Its answer is no frame
  // of its own (it comes with the next value), so asking once is enough.
  const int n = m_polls++;
  if (m_needConf.exchange(false) || n % 10 == 0)
    return QByteArrayLiteral("CONF?\n\r\n");
  return QByteArrayLiteral("FETC?\n\r\n");
}

bool DecoderKeysightU12xx::checkFormat(const char *data, size_t idx)
{
  // A frame ends with the line of a FETC? answer, an NR3 number. Other lines
  // (the CONF? answer, a switch message "*3", an error "*E") stay in the
  // buffer and come with the next value; the frame starts at index 0.
  if (data[idx] != '\n')
    return false;
  size_t end = idx;
  if (end > 0 && data[end - 1] == '\r')
    --end;
  size_t start = end;
  while (start > 0 && data[start - 1] != '\n')
    --start;
  if (end - start < 6)
    return false;
  if (data[start] != '+' && data[start] != '-')
    return false;
  bool exponent = false;
  for (size_t i = start + 1; i < end; ++i)
  {
    const char c = data[i];
    if (c == 'E')
      exponent = true;
    else if (!(c >= '0' && c <= '9') && c != '.' && c != '+' && c != '-')
      return false;
  }
  return exponent;
}

bool DecoderKeysightU12xx::parseConf(const QString &conf, Function *f)
{
  *f = Function();
  // U123x/U127x: "V,0,DC" - function, resolution, coupling
  if (!conf.contains(' ') && conf.contains(','))
  {
    const QStringList p = conf.split(',');
    const QString name = p[0];
    const QString coupling = p.size() > 2 ? p[2] : QString();
    struct Row { const char *name; const char *unit; double scale; };
    static const Row rows[] = {
      { "V", "V", 1 }, { "MV", "V", 1e-3 }, { "A", "A", 1 }, { "MA", "A", 1e-3 }, { "UA", "A", 1e-6 },
      { "FREQ", "Hz", 1 }, { "RES", "Ohm", 1 }, { "CAP", "F", 1 },
    };
    for (const Row &r : rows)
      if (name == QLatin1String(r.name))
      {
        f->unit = r.unit;
        f->scale = r.scale;
      }
    if (f->unit.isEmpty())
      return false;
    if (f->unit == "Ohm")
      f->special = "OH";
    else if (f->unit == "F")
      f->special = "CA";
    else if (f->unit == "Hz")
      f->special = "FR";
    else
      f->special = coupling == "AC" ? "AC" : coupling == "ACDC" ? "ACDC" : "DC";
    return true;
  }

  // U124x/U125x/U128x: "VOLT:AC +6.000000E+00,+1.000000E-03", "TEMP:K CEL"
  const QString name = conf.section(' ', 0, 0);
  const QString arg = conf.section(' ', 1);
  const QString sub = name.section(':', 1);
  auto coupling = [&](const QString &s) { return s == "AC" ? "AC" : s.startsWith("ACDC") ? "ACDC" : "DC"; };
  if (name == "DIOD")
  {
    f->unit = "V";
    f->special = "DI";
  }
  else if (name.startsWith("VOLT") && sub != "HRAT")
  {
    f->unit = "V";
    f->special = coupling(sub);
  }
  else if (name.startsWith("CURR"))
  {
    f->unit = "A";
    f->special = coupling(sub);
  }
  else if (name == "RES")
  {
    f->unit = "Ohm";
    f->special = "OH";
  }
  else if (name == "CONT")
  {
    f->unit = "Ohm";
    f->special = "BUZ";
  }
  else if (name == "COND")
  {
    f->unit = "S";
  }
  else if (name == "CAP")
  {
    f->unit = "F";
    f->special = "CA";
  }
  else if (name.startsWith("FREQ") || name == "FC1" || name == "FC100")
  {
    f->unit = "Hz";
    f->special = "FR";
  }
  else if (name.startsWith("PULS:PWID"))
  {
    f->unit = "s";
    f->special = "FR";   // a time in the frequency position: the pulse width
  }
  else if (name == "PULS:PDUT" || name == "PULS:NDUT" || name.startsWith("CPER") || name == "VOLT:HRAT")
  {
    f->unit = "%";
  }
  else if (name.startsWith("TEMP") || name.startsWith("T1") || name.startsWith("T2"))
  {
    f->unit = arg == "FAR" ? "dF" : "C";
    f->special = "TE";
  }
  else
    return false;   // square wave out, switch counter, NCV: no reading
  return true;
}

std::optional<DmmDecoder::DmmResponse> DecoderKeysightU12xx::decode(const QByteArray &data, int id)
{
  QString value;
  for (const QByteArray &raw : data.split('\n'))
  {
    const QString line = QString::fromLatin1(raw).trimmed();
    if (line.isEmpty())
      continue;
    if (line.startsWith('*'))
    {
      // "*3": the rotary switch turned - another function, unknown until
      // CONF? says it; "*E": an error, nothing to read
      if (line.size() > 1 && line.at(1).isDigit())
      {
        m_function = Function();
        m_needConf = true;
      }
      continue;
    }
    if (line.startsWith('"') && line.endsWith('"') && line.size() > 2)
    {
      const QString conf = line.mid(1, line.size() - 2);
      // STAT? would answer digits only; CONF? starts with a letter
      if (conf.at(0).isLetter())
      {
        // an unknown function (square wave out, NCV) shows nothing until
        // the switch turns again
        parseConf(conf, &m_function);
        m_needConf = false;
      }
      continue;
    }
    value = line;
  }
  if (value.isEmpty() || m_function.unit.isEmpty())
    return std::nullopt;

  m_result = {};
  m_result.main().id = id;
  m_result.showBar = true;
  m_result.main().special = m_function.special;

  if (value == "+9.90000000E+37" || value == "-9.90000000E+37")
  {
    m_result.main().val = value.startsWith('-') ? "-OL" : "OL";
    m_result.main().unit = m_function.unit;
    m_result.main().dval = 0;
    return m_result;
  }
  bool ok = false;
  const double v = value.toDouble(&ok) * m_function.scale;
  if (!ok)
    return std::nullopt;
  m_result.main().dval = v;
  // temperatures and percentages without a prefix, a diode in volts as
  // the meter shows it
  if (m_function.unit == "C" || m_function.unit == "dF" || m_function.unit == "%" || m_function.special == "DI")
  {
    m_result.main().val = QString::number(v, 'f', m_function.special == "DI" ? 3 : 2);
    m_result.main().unit = m_function.unit;
    return m_result;
  }
  QString prefix;
  m_result.main().val = SiPrefix::format(v, &prefix);
  m_result.main().unit = prefix + m_function.unit;
  return m_result;
}
