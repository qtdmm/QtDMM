// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cmath>
#include <limits>

/// @file
/// The core's base types: what a value measures (Quantity), how (Flags),
/// in which unit, on which port of a device (PortKey), and the measured value
/// itself (Sample), in the vocabulary of libsigrok. Only QtCore: the core
/// built on them later runs without widgets. ReadingAdapter makes them from
/// what the decoders deliver.

/// How good a stored value is.
enum class Quality : quint8
{
  Valid,      ///< a number the meter showed
  Stale,      ///< no reading arrived for longer than the timeout
  Overload,   ///< the meter showed OL (or the like); the value is not a measurement
  Inactive    ///< the function is not being measured right now
};

/// What is measured. The values and their order are libsigrok's sr_mq
/// (0.5.2), so a mapping to sigrok is a cast. QtDMM's own quantities, which
/// that version does not have, start at 1000.
enum class Quantity : quint16
{
  Unknown = 0,
  Voltage = 10000,
  Current,
  Resistance,
  Capacitance,
  Temperature,
  Frequency,
  DutyCycle,
  Continuity,
  PulseWidth,
  Conductance,
  Power,
  Gain,
  SoundPressureLevel,
  CarbonMonoxide,
  RelativeHumidity,
  Time,
  WindSpeed,
  Pressure,
  ParallelInductance,
  ParallelCapacitance,
  ParallelResistance,
  SeriesInductance,
  SeriesCapacitance,
  SeriesResistance,
  DissipationFactor,
  QualityFactor,
  PhaseAngle,
  Difference,
  Count,
  PowerFactor,
  ApparentPower,
  Mass,
  HarmonicRatio,

  Energy = 1000,        ///< Wh (newer libsigrok has it, under another number)
  ElectricCharge,       ///< Ah (dito)
  StateOfCharge,        ///< % of a battery (Victron)
  TimeToGo,             ///< remaining time of a battery (Victron)
  CarbonDioxide
};

/// State and mode of a value, a bit mask after libsigrok's sr_mqflag, plus
/// LowBattery and Peak, which QtDMM meters report and sigrok does not have.
namespace SampleFlag
{
enum : quint32
{
  AC        = 1u << 0,
  DC        = 1u << 1,
  RMS       = 1u << 2,
  Diode     = 1u << 3,
  Hold      = 1u << 4,
  Max       = 1u << 5,
  Min       = 1u << 6,
  Autorange = 1u << 7,
  Relative  = 1u << 8,
  // 1u << 9 .. 1u << 16: the sound level weightings, not used by QtDMM
  Duration  = 1u << 17,
  Avg       = 1u << 18,
  Reference = 1u << 19,
  Unstable  = 1u << 20,
  FourWire  = 1u << 21,
  LowBattery = 1u << 28,   ///< QtDMM
  Peak       = 1u << 29,   ///< QtDMM

  /// The flags that tell two ports of a meter apart (V DC and V AC are two
  /// ports); the others describe a state of the value.
  Defining = AC | DC | RMS | Diode | FourWire
};
}

/// "AC", "DC", "AC+DC" or empty: the coupling in @p flags as meters write it.
inline QString couplingText(quint32 flags)
{
  const bool ac = flags & SampleFlag::AC;
  const bool dc = flags & SampleFlag::DC;
  return ac && dc ? QStringLiteral("AC+DC") : ac ? QStringLiteral("AC") : dc ? QStringLiteral("DC") : QString();
}

namespace Quantities
{
struct Entry
{
  Quantity    quantity;
  const char *id;      ///< the port key's text form: "voltage", "duty_cycle"
  const char *name;    ///< for people, translated in context "Quantity"
};

inline const Entry *table(int *count)
{
  static const Entry kTable[] = {
    { Quantity::Unknown,             "unknown",              QT_TRANSLATE_NOOP("Quantity", "Unknown") },
    { Quantity::Voltage,             "voltage",              QT_TRANSLATE_NOOP("Quantity", "Voltage") },
    { Quantity::Current,             "current",              QT_TRANSLATE_NOOP("Quantity", "Current") },
    { Quantity::Resistance,          "resistance",           QT_TRANSLATE_NOOP("Quantity", "Resistance") },
    { Quantity::Capacitance,         "capacitance",          QT_TRANSLATE_NOOP("Quantity", "Capacitance") },
    { Quantity::Temperature,         "temperature",          QT_TRANSLATE_NOOP("Quantity", "Temperature") },
    { Quantity::Frequency,           "frequency",            QT_TRANSLATE_NOOP("Quantity", "Frequency") },
    { Quantity::DutyCycle,           "duty_cycle",           QT_TRANSLATE_NOOP("Quantity", "Duty cycle") },
    { Quantity::Continuity,          "continuity",           QT_TRANSLATE_NOOP("Quantity", "Continuity") },
    { Quantity::PulseWidth,          "pulse_width",          QT_TRANSLATE_NOOP("Quantity", "Pulse width") },
    { Quantity::Conductance,         "conductance",          QT_TRANSLATE_NOOP("Quantity", "Conductance") },
    { Quantity::Power,               "power",                QT_TRANSLATE_NOOP("Quantity", "Power") },
    { Quantity::Gain,                "gain",                 QT_TRANSLATE_NOOP("Quantity", "Gain") },
    { Quantity::SoundPressureLevel,  "sound_pressure_level", QT_TRANSLATE_NOOP("Quantity", "Sound pressure level") },
    { Quantity::CarbonMonoxide,      "carbon_monoxide",      QT_TRANSLATE_NOOP("Quantity", "Carbon monoxide") },
    { Quantity::RelativeHumidity,    "relative_humidity",    QT_TRANSLATE_NOOP("Quantity", "Relative humidity") },
    { Quantity::Time,                "time",                 QT_TRANSLATE_NOOP("Quantity", "Time") },
    { Quantity::WindSpeed,           "wind_speed",           QT_TRANSLATE_NOOP("Quantity", "Wind speed") },
    { Quantity::Pressure,            "pressure",             QT_TRANSLATE_NOOP("Quantity", "Pressure") },
    { Quantity::ParallelInductance,  "parallel_inductance",  QT_TRANSLATE_NOOP("Quantity", "Parallel inductance") },
    { Quantity::ParallelCapacitance, "parallel_capacitance", QT_TRANSLATE_NOOP("Quantity", "Parallel capacitance") },
    { Quantity::ParallelResistance,  "parallel_resistance",  QT_TRANSLATE_NOOP("Quantity", "Parallel resistance") },
    { Quantity::SeriesInductance,    "series_inductance",    QT_TRANSLATE_NOOP("Quantity", "Series inductance") },
    { Quantity::SeriesCapacitance,   "series_capacitance",   QT_TRANSLATE_NOOP("Quantity", "Series capacitance") },
    { Quantity::SeriesResistance,    "series_resistance",    QT_TRANSLATE_NOOP("Quantity", "Series resistance") },
    { Quantity::DissipationFactor,   "dissipation_factor",   QT_TRANSLATE_NOOP("Quantity", "Dissipation factor") },
    { Quantity::QualityFactor,       "quality_factor",       QT_TRANSLATE_NOOP("Quantity", "Quality factor") },
    { Quantity::PhaseAngle,          "phase_angle",          QT_TRANSLATE_NOOP("Quantity", "Phase angle") },
    { Quantity::Difference,          "difference",           QT_TRANSLATE_NOOP("Quantity", "Difference") },
    { Quantity::Count,               "count",                QT_TRANSLATE_NOOP("Quantity", "Count") },
    { Quantity::PowerFactor,         "power_factor",         QT_TRANSLATE_NOOP("Quantity", "Power factor") },
    { Quantity::ApparentPower,       "apparent_power",       QT_TRANSLATE_NOOP("Quantity", "Apparent power") },
    { Quantity::Mass,                "mass",                 QT_TRANSLATE_NOOP("Quantity", "Mass") },
    { Quantity::HarmonicRatio,       "harmonic_ratio",       QT_TRANSLATE_NOOP("Quantity", "Harmonic ratio") },
    { Quantity::Energy,              "energy",               QT_TRANSLATE_NOOP("Quantity", "Energy") },
    { Quantity::ElectricCharge,      "electric_charge",      QT_TRANSLATE_NOOP("Quantity", "Electric charge") },
    { Quantity::StateOfCharge,       "state_of_charge",      QT_TRANSLATE_NOOP("Quantity", "State of charge") },
    { Quantity::TimeToGo,            "time_to_go",           QT_TRANSLATE_NOOP("Quantity", "Time to go") },
  };
  *count = int(sizeof(kTable) / sizeof(kTable[0]));
  return kTable;
}

inline const Entry *find(Quantity q)
{
  int n = 0;
  const Entry *t = table(&n);
  for (int i = 0; i < n; ++i)
    if (t[i].quantity == q)
      return &t[i];
  return &t[0];
}

/// "voltage", "duty_cycle": the port key's text form.
inline QString id(Quantity q) { return QString::fromLatin1(find(q)->id); }

/// The quantity behind an id(), Unknown for anything else.
inline Quantity fromId(const QString &id)
{
  int n = 0;
  const Entry *t = table(&n);
  for (int i = 0; i < n; ++i)
    if (id == QLatin1String(t[i].id))
      return t[i].quantity;
  return Quantity::Unknown;
}

/// "Voltage", "Duty cycle", translated.
inline QString name(Quantity q) { return QCoreApplication::translate("Quantity", find(q)->name); }
}

/// The unit of a port: the base unit only, in one spelling ("V", "Ω", "°C",
/// "Hz", "%"). The SI prefix the meter shows is presentation and belongs to
/// the value (Sample::displayPrefix).
struct Unit
{
  QString base;

  bool operator==(const Unit &o) const { return base == o.base; }
  bool operator!=(const Unit &o) const { return base != o.base; }
  /// Values in a and b can share an axis. Only a hint: the UI decides
  /// whether it warns.
  static bool compatible(const Unit &a, const Unit &b) { return a.base == b.base; }
};

/// The measuring range of a value.
struct Range
{
  bool   autorange = false;
  double full = std::numeric_limits<double>::quiet_NaN();   ///< full scale in SI, NaN when not known
};

/// Which value of a meter: main display or the n-th display showing the same
/// quantity again.
enum class Slot : quint8 { Main, Sub, Sub2, Sub3 };

/// The key of a meter's port: channel, quantity, the defining flags (AC, DC,
/// RMS, Diode, FourWire) and the slot. Text form "voltage.dc",
/// "2:current.ac", "voltage.dc.sub"; the channel is left out when it is 1.
struct PortKey
{
  int      channel = 1;
  Quantity quantity = Quantity::Unknown;
  quint32  defining = 0;
  Slot     slot = Slot::Main;

  bool operator==(const PortKey &o) const
  {
    return channel == o.channel && quantity == o.quantity && defining == o.defining && slot == o.slot;
  }
  bool operator!=(const PortKey &o) const { return !(*this == o); }
  bool isValid() const { return quantity != Quantity::Unknown || defining != 0; }

  QString toString() const
  {
    QStringList parts{Quantities::id(quantity)};
    for (const FlagName &f : kFlagNames)
      if (defining & f.flag)
        parts << QLatin1String(f.text);
    static const char *const kSlots[] = { nullptr, "sub", "sub2", "sub3" };
    if (slot != Slot::Main)
      parts << QLatin1String(kSlots[int(slot)]);
    const QString text = parts.join('.');
    return channel == 1 ? text : QString::number(channel) + ':' + text;
  }

  /// The key of a toString() text; an invalid key (Unknown, no flags) for
  /// anything it does not know.
  static PortKey fromString(const QString &text)
  {
    PortKey k;
    QString rest = text.trimmed();
    const int colon = rest.indexOf(':');
    if (colon > 0)
    {
      bool ok = false;
      k.channel = rest.left(colon).toInt(&ok);
      if (!ok || k.channel < 1)
        return PortKey{};
      rest = rest.mid(colon + 1);
    }
    const QStringList parts = rest.split('.');
    k.quantity = Quantities::fromId(parts.value(0));
    if (k.quantity == Quantity::Unknown && parts.value(0) != QLatin1String("unknown"))
      return PortKey{};
    for (int i = 1; i < parts.size(); ++i)
    {
      bool known = false;
      for (const FlagName &f : kFlagNames)
        if (parts[i] == QLatin1String(f.text))
        {
          k.defining |= f.flag;
          known = true;
        }
      if (parts[i] == QLatin1String("sub"))  { k.slot = Slot::Sub;  known = true; }
      if (parts[i] == QLatin1String("sub2")) { k.slot = Slot::Sub2; known = true; }
      if (parts[i] == QLatin1String("sub3")) { k.slot = Slot::Sub3; known = true; }
      if (!known)
        return PortKey{};
    }
    return k;
  }

private:
  struct FlagName { quint32 flag; const char *text; };
  static constexpr FlagName kFlagNames[] = {
    { SampleFlag::AC, "ac" }, { SampleFlag::DC, "dc" }, { SampleFlag::RMS, "rms" },
    { SampleFlag::Diode, "diode" }, { SampleFlag::FourWire, "4w" } };
};

inline size_t qHash(const PortKey &k, size_t seed = 0)
{
  return qHashMulti(seed, k.channel, quint16(k.quantity), k.defining, quint8(k.slot));
}

/// One measured value, as it flows from a meter's port. The port carries the
/// key and the unit, the sample does not.
struct Sample
{
  qint64  t = 0;            ///< core clock, ms, monotonic (Sample::now())
  qint64  wall = 0;         ///< wall clock, ms since the epoch, for export and display
  double  value = 0;        ///< SI base unit
  Quality quality = Quality::Valid;
  quint32 flags = 0;        ///< SampleFlag, the state flags and the port's defining ones
  QString text;             ///< as the meter shows it: "-006.52", "OL"
  QString displayPrefix;    ///< "m", "k", "" as the meter shows it
  Range   range;
  float   bar = std::numeric_limits<float>::quiet_NaN();   ///< the meter's bar graph 0..1, NaN = none

  /// The core clock: ms since the first call, monotonic.
  static qint64 now()
  {
    static const QElapsedTimer clock = [] { QElapsedTimer c; c.start(); return c; }();
    return clock.elapsed();
  }
};

Q_DECLARE_METATYPE(PortKey)
Q_DECLARE_METATYPE(Sample)
