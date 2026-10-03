// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/readingadapter.h"

#include <QRegularExpression>

#include <cmath>
#include <limits>

#include "core/siprefix.h"

namespace
{
// Units that start with a prefix letter but are not prefixed ("min" is not
// milli-"in"); SiPrefix::split() would take them apart.
bool isWholeUnit(const QString &unit)
{
  static const QStringList kWhole = { "min", "mol", "dB", "dBm", "dBV", "cosphi", "hFE", "HFE" };
  return kWhole.contains(unit);
}

// The decoders' mode codes, as one word: "AC", "DC", "ACDC", "DI", "BUZ",
// "TE", ... The Metex frames carry it with padding, some decoders write
// "Diode".
QString modeCode(const QString &special)
{
  const QString s = special.trimmed().toUpper();
  return s == QLatin1String("DIODE") ? QStringLiteral("DI") : s;
}

struct Meaning
{
  Quantity quantity = Quantity::Unknown;
  QString  unit;   ///< the port's unit in one spelling
};

// What a value measures, from the decoder's base unit and mode code. The
// unit decides first; the mode code where the unit is ambiguous or missing
// (kern_spezifikation §2.1: "aus der Einheit").
Meaning meaning(const QString &base, const QString &mode, FrameFormat::DataFormat format)
{
  const bool victron = format == FrameFormat::VictronBLE;
  const bool tfa = format == FrameFormat::TfaAirControl;

  if (mode == QLatin1String("DI"))
    return { Quantity::Voltage, QStringLiteral("V") };
  if (mode == QLatin1String("BUZ"))
    return { Quantity::Continuity, base == QLatin1String("Ohm") ? QStringLiteral("Ω") : base };
  // temperature: "C" and "dF" in QtDMM's spelling; "F" is farad unless the
  // mode says temperature (ES51986)
  if (base == QLatin1String("C") || base == QStringLiteral("°C"))
    return { Quantity::Temperature, QStringLiteral("°C") };
  if (base == QLatin1String("dF") || base == QStringLiteral("°F") || (mode == QLatin1String("TE") && base == QLatin1String("F")))
    return { Quantity::Temperature, QStringLiteral("°F") };
  if (mode == QLatin1String("HFE") || base.compare(QLatin1String("hFE"), Qt::CaseInsensitive) == 0)
    return { Quantity::Gain, QString() };
  // a time in the frequency position is the pulse width (Fluke "ms")
  if (base == QLatin1String("s") && (mode == QLatin1String("FR") || mode == QLatin1String("HZ")))
    return { Quantity::PulseWidth, QStringLiteral("s") };

  struct Row { const char *base; Quantity quantity; const char *unit; };
  static const Row kRows[] = {
    { "V",      Quantity::Voltage,           "V" },
    { "A",      Quantity::Current,           "A" },
    { "Ohm",    Quantity::Resistance,        "Ω" },
    { "Ω",      Quantity::Resistance,        "Ω" },
    { "F",      Quantity::Capacitance,       "F" },
    { "Hz",     Quantity::Frequency,         "Hz" },
    { "RPM",    Quantity::Frequency,         "rpm" },
    { "%",      Quantity::DutyCycle,         "%" },
    { "S",      Quantity::Conductance,       "S" },
    { "W",      Quantity::Power,             "W" },
    { "VA",     Quantity::ApparentPower,     "VA" },
    { "dBm",    Quantity::Power,             "dBm" },
    { "dBV",    Quantity::Voltage,           "dBV" },
    { "dB",     Quantity::Gain,              "dB" },
    { "s",      Quantity::Time,              "s" },
    { "min",    Quantity::Time,              "min" },
    { "h",      Quantity::Time,              "h" },
    { "Ah",     Quantity::ElectricCharge,    "Ah" },
    { "Wh",     Quantity::Energy,            "Wh" },
    { "cosphi", Quantity::PowerFactor,       "" },
    { "D",      Quantity::DissipationFactor, "" },
  };
  for (const Row &r : kRows)
    if (base == QString::fromUtf8(r.base))
    {
      Meaning m{ r.quantity, QString::fromUtf8(r.unit) };
      if (victron && m.quantity == Quantity::DutyCycle)
        m.quantity = Quantity::StateOfCharge;
      if (tfa && m.quantity == Quantity::DutyCycle)   // the % of the TFA sensor is humidity
        m.quantity = Quantity::RelativeHumidity;
      if (victron && m.quantity == Quantity::Time)
        m.quantity = Quantity::TimeToGo;
      return m;
    }
  return { Quantity::Unknown, base };
}

// The flags that make a port: AC/DC (and RMS) only where a quantity has
// them, so the Hz a meter shows in its V DC position is one frequency port.
quint32 definingFlags(Quantity q, quint32 flags)
{
  quint32 keep = 0;
  switch (q)
  {
    case Quantity::Voltage:
      keep = SampleFlag::AC | SampleFlag::DC | SampleFlag::RMS | SampleFlag::Diode;
      break;
    case Quantity::Current:
    case Quantity::Power:
    case Quantity::ApparentPower:
      keep = SampleFlag::AC | SampleFlag::DC | SampleFlag::RMS;
      break;
    case Quantity::Resistance:
      keep = SampleFlag::FourWire;
      break;
    default:
      break;
  }
  return flags & keep;
}

// A temperature, a duty cycle or a state of charge has no measuring range:
// the display count says nothing about their scale.
bool hasRange(Quantity q)
{
  switch (q)
  {
    case Quantity::Unknown:
    case Quantity::Temperature:
    case Quantity::RelativeHumidity:
    case Quantity::DutyCycle:
    case Quantity::StateOfCharge:
    case Quantity::TimeToGo:
    case Quantity::Continuity:
      return false;
    default:
      return true;
  }
}
}

PortSample ReadingAdapter::adaptValue(double dval, const QString &text, const QString &unit, const QString &special,
                                      const QString &range, bool hold, bool showBar, bool lowBattery, int id, qint64 wall)
{
  PortSample ps;
  ps.id = id;
  ps.decoderUnit = unit;
  ps.rangeText = range;

  SiPrefix::Split split = isWholeUnit(unit) ? SiPrefix::Split{ QString(), unit } : SiPrefix::split(unit);
  if (split.prefix == QLatin1String("u"))
    split.prefix = QStringLiteral("µ");
  ps.decoderBaseUnit = split.baseUnit;

  const QString mode = modeCode(special);
  const Meaning m = meaning(split.baseUnit, mode, m_format);
  ps.unit.base = m.unit;

  quint32 flags = 0;
  if (mode == QLatin1String("AC"))
    flags |= SampleFlag::AC;
  else if (mode == QLatin1String("DC"))
    flags |= SampleFlag::DC;
  else if (mode == QLatin1String("ACDC"))
    flags |= SampleFlag::AC | SampleFlag::DC;
  else if (mode == QLatin1String("DI"))
    flags |= SampleFlag::Diode;
  if (hold)
    flags |= SampleFlag::Hold;
  if (range == QLatin1String("AUTO"))
    flags |= SampleFlag::Autorange;
  if (lowBattery)
    flags |= SampleFlag::LowBattery;

  // a bar graph is a share of the display count, which a temperature (no
  // range) does not fill: 37.2 °C of 50000 counts is an empty bar
  ps.showBar = showBar && m.quantity != Quantity::Temperature;

  ps.port.quantity = m.quantity;
  ps.port.defining = definingFlags(m.quantity, flags);

  // a second display showing the main display's quantity again is its own
  // slot of that port; any other quantity is a port of its own
  if (id == 0)
    m_mainKey = ps.port;
  else if (ps.port == m_mainKey)
    ps.port.slot = Slot(qBound(1, id, 3));

  Sample &s = ps.sample;
  s.t = Sample::now();
  s.wall = wall;
  s.value = dval;
  s.text = text;
  s.displayPrefix = split.prefix;
  s.flags = flags;
  // decoders mark overload and similar states with letters in the value
  // text ("OL", "-OL", "0.L", "EFLO"); a number never has one
  static const QRegularExpression letters("[A-Za-z]");
  s.quality = text.contains(letters) ? Quality::Overload : Quality::Valid;
  // an overload has no value (kern_spezifikation §2.4): whatever number the
  // decoder made of "OL" is not one. The sign of "-OL" stays on the NaN,
  // for SCPI's -9.9E37.
  if (s.quality == Quality::Overload)
    s.value = std::copysign(std::numeric_limits<double>::quiet_NaN(),
                            text.trimmed().startsWith(QLatin1Char('-')) ? -1.0 : 1.0);
  s.range.autorange = range == QLatin1String("AUTO");

  // full scale and bar from the display count and the decimals shown:
  // "3.856" at 4000 counts is the 4 V range, the bar at 3856/4000
  static const QRegularExpression number("^\\s*([-+]?)\\s*(\\d+)(?:\\.(\\d*))?\\s*$");
  const QRegularExpressionMatch nm = number.match(text);
  if (m_counts > 0 && nm.hasMatch() && hasRange(m.quantity))
  {
    const int decimals = nm.captured(3).size();
    s.range.full = m_counts / std::pow(10.0, decimals) * SiPrefix::factor(split.prefix);
    if (ps.showBar)
    {
      const double digits = (nm.captured(2) + nm.captured(3)).toDouble();
      s.bar = float(qBound(0.0, digits / m_counts, 1.0));
    }
  }
  return ps;
}

Reading ReadingAdapter::reading(const PortSample &ps)
{
  Reading r;
  r.value = ps.sample.value;
  r.text = ps.sample.text;
  r.unit = ps.decoderUnit;
  r.prefix = ps.sample.displayPrefix;
  r.baseUnit = ps.decoderBaseUnit;
  r.range = ps.rangeText;
  r.hold = ps.sample.flags & SampleFlag::Hold;
  r.showBar = ps.showBar;
  r.overload = ps.sample.quality == Quality::Overload;
  r.id = ps.id;
  r.msecs = ps.sample.wall;
  r.port = ps.port;
  r.portUnit = ps.unit;
  r.flags = ps.sample.flags;
  r.quality = ps.sample.quality;
  r.measuringRange = ps.sample.range;
  return r;
}

QList<PortSample> ReadingAdapter::adapt(const DmmDecoder::DmmResponse &r, qint64 wall)
{
  QList<PortSample> out;
  out << adaptValue(r.dval, r.val, r.unit, r.special, r.range, r.hold, r.showBar, r.lowBat, r.id, wall);
  if (r.id2 > 0)
    out << adaptValue(r.dval2, r.val2, r.unit2, r.special2, r.range, r.hold, r.showBar, r.lowBat, r.id2, wall);
  return out;
}
