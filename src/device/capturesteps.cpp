// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/capturesteps.h"

#include <QCoreApplication>
#include <QStringList>

#include "core/readingadapter.h"

namespace CaptureSteps
{
namespace
{
// the meter's own annunciators that make a state; AC/DC/Diode are in defining
constexpr quint32 kStateFlags = SampleFlag::Hold | SampleFlag::Max | SampleFlag::Min | SampleFlag::Avg
                                | SampleFlag::Relative | SampleFlag::Autorange | SampleFlag::Peak
                                | SampleFlag::Reference;
constexpr quint32 kDefining = SampleFlag::AC | SampleFlag::DC | SampleFlag::Diode;
// below this many counts the sign is noise: open leads flicker between
// -0.0002 and 0.0002 (UT61E)
constexpr qint64 kSignCounts = 100;

Expect quantity(Quantity q, quint32 defining = 0)
{
  Expect e;
  e.quantity = q;
  e.defining = defining;
  return e;
}

Expect flag(quint32 any)
{
  Expect e;
  e.anyFlag = any;
  return e;
}

Step step(const char *id, const char *instruction, Expect expect = {})
{
  return Step{ QString::fromLatin1(id), instruction, QString(), nullptr, expect };
}

Step key(const char *id, const char *instruction, const char *key, const char *label, Expect expect = {})
{
  return Step{ QString::fromLatin1(id), instruction, QString::fromLatin1(key), label, expect };
}

QList<Step> multimeter(bool clamp)
{
  Expect negative = quantity(Quantity::Voltage, SampleFlag::DC);
  negative.negative = true;
  Expect open = quantity(Quantity::Resistance);
  open.overload = true;
  Expect manual;
  manual.manualRange = true;

  QList<Step> s{
    step("dc_v", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to DC voltage and measure a battery."),
         quantity(Quantity::Voltage, SampleFlag::DC)),
    step("dc_v_neg", QT_TRANSLATE_NOOP("CaptureSteps", "Swap the test leads on the battery, so the meter shows a negative voltage."),
         negative),
    step("ac_v", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to AC voltage and measure a source that is safe to measure, "
                                                   "such as the AC output of a small transformer. "
                                                   "Without one, skip this step."),
         quantity(Quantity::Voltage, SampleFlag::AC)),
    step("ohm", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to resistance and measure a resistor."),
         quantity(Quantity::Resistance)),
    step("ohm_open", QT_TRANSLATE_NOOP("CaptureSteps", "Leave the test leads open, so the meter shows OL."), open),
    step("diode", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to diode test and measure a diode in forward direction."),
         quantity(Quantity::Unknown, SampleFlag::Diode)),
    step("continuity", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to continuity and short the test leads."),
         quantity(Quantity::Continuity)),
    step("capacitance", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to capacitance and measure a discharged capacitor."),
         quantity(Quantity::Capacitance)),
    step("frequency", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to frequency (Hz)."), quantity(Quantity::Frequency)),
    step("dc_ma", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to DC current in mA, put the red test lead into the mA socket "
                                                    "and measure a small current, such as an LED with its resistor. "
                                                    "Never measure a voltage source directly in this setting. "
                                                    "Without such a circuit, skip this step."),
         quantity(Quantity::Current, SampleFlag::DC)),
  };
  if (clamp)
    s << step("ac_a_clamp", QT_TRANSLATE_NOOP("CaptureSteps", "Set the meter to AC current and clamp one wire of a running appliance."),
              quantity(Quantity::Current, SampleFlag::AC));
  s << key("hold", QT_TRANSLATE_NOOP("CaptureSteps", "Measure something and press HOLD."), "hold", "HOLD", flag(SampleFlag::Hold))
    << key("rel", QT_TRANSLATE_NOOP("CaptureSteps", "Release HOLD, then press REL (relative)."), "rel", "REL",
           flag(SampleFlag::Relative))
    << key("minmax", QT_TRANSLATE_NOOP("CaptureSteps", "Release REL, then press MIN/MAX."), "minmax", "MIN/MAX",
           flag(SampleFlag::Max | SampleFlag::Min))
    << key("range", QT_TRANSLATE_NOOP("CaptureSteps", "Release MIN/MAX, then press RANGE to choose the range by hand."),
           "range", "RANGE", manual);
  return s;
}
}

bool Expect::isSet() const
{
  return quantity != Quantity::Unknown || defining || anyFlag || overload || negative || manualRange;
}

QList<Step> forType(DeviceType::Type type)
{
  switch (type)
  {
  case DeviceType::HandheldDmm:
  case DeviceType::BenchDmm:
    return multimeter(false);
  case DeviceType::ClampMeter:
    return multimeter(true);
  case DeviceType::Thermometer:
  {
    Expect open = quantity(Quantity::Temperature);
    open.overload = true;
    return {
      step("t1", QT_TRANSLATE_NOOP("CaptureSteps", "Plug a probe into the first input (T1) and measure the room temperature."),
           quantity(Quantity::Temperature)),
      step("t1_open", QT_TRANSLATE_NOOP("CaptureSteps", "Unplug the probe, so the meter shows that no probe is there."), open),
      step("t2", QT_TRANSLATE_NOOP("CaptureSteps", "Plug probes into the first and the second input (T1 and T2). "
                                                   "With one input only, skip this step.")),
      key("unit", QT_TRANSLATE_NOOP("CaptureSteps", "Switch the unit between °C and °F."), "unit", "°C/°F"),
      key("hold", QT_TRANSLATE_NOOP("CaptureSteps", "Switch back to °C and press HOLD."), "hold", "HOLD", flag(SampleFlag::Hold)),
      key("minmax", QT_TRANSLATE_NOOP("CaptureSteps", "Release HOLD, then press MAX/MIN."), "minmax", "MAX/MIN",
          flag(SampleFlag::Max | SampleFlag::Min)),
    };
  }
  case DeviceType::Luxmeter:
    return {
      step("lux", QT_TRANSLATE_NOOP("CaptureSteps", "Point the sensor at a lamp."), quantity(Quantity::Illuminance)),
      key("range", QT_TRANSLATE_NOOP("CaptureSteps", "Change the range, with the RANGE key or by covering the sensor."), "range",
          "RANGE"),
      key("unit", QT_TRANSLATE_NOOP("CaptureSteps", "Switch the unit between lx and fc."), "unit", "lx/fc"),
      key("hold", QT_TRANSLATE_NOOP("CaptureSteps", "Switch back to lx and press HOLD."), "hold", "HOLD", flag(SampleFlag::Hold)),
      key("max", QT_TRANSLATE_NOOP("CaptureSteps", "Release HOLD, then press MAX."), "max", "MAX", flag(SampleFlag::Max)),
    };
  case DeviceType::SoundLevel:
    return {
      step("spl", QT_TRANSLATE_NOOP("CaptureSteps", "Measure the sound level of the room."),
           quantity(Quantity::SoundPressureLevel)),
      key("weighting", QT_TRANSLATE_NOOP("CaptureSteps", "Switch the weighting between dBA and dBC."), "weighting", "A/C"),
      step("fast_slow", QT_TRANSLATE_NOOP("CaptureSteps", "Switch the time weighting between FAST and SLOW.")),
      key("hold", QT_TRANSLATE_NOOP("CaptureSteps", "Press HOLD."), "hold", "HOLD", flag(SampleFlag::Hold)),
      key("max", QT_TRANSLATE_NOOP("CaptureSteps", "Release HOLD, then press MAX."), "max", "MAX", flag(SampleFlag::Max)),
    };
  default:
    return {};
  }
}

QString instruction(const Step &step)
{
  return QCoreApplication::translate("CaptureSteps", step.instruction);
}

bool State::operator==(const State &o) const
{
  if (understood != o.understood)
    return false;
  if (!understood)
    return length == o.length;
  return quantity == o.quantity && defining == o.defining && flags == o.flags && unit == o.unit && prefix == o.prefix
         && decimals == o.decimals && negative == o.negative && quality == o.quality && values == o.values;
}

State stateOf(const CaptureLog::Frame &frame, FrameFormat::DataFormat format)
{
  State s;
  s.length = int(frame.bytes.size());
  if (!frame.response)
    return s;
  ReadingAdapter adapter(format);
  const QList<PortSample> samples = adapter.adapt(*frame.response, frame.wall);
  if (samples.isEmpty())
    return s;
  const PortSample &ps = samples.first();
  s.understood = true;
  s.quantity = ps.port.quantity;
  s.defining = ps.port.defining & kDefining;
  s.flags = ps.sample.flags & kStateFlags;
  s.unit = ps.unit.base;
  s.prefix = ps.sample.displayPrefix;
  s.quality = ps.sample.quality;
  const QString text = ps.sample.text.trimmed();
  QString digits;
  for (const QChar c : text)
    if (c.isDigit())
      digits += c;
  s.negative = text.startsWith('-') && digits.toLongLong() >= kSignCounts;
  const qsizetype point = text.indexOf('.');
  s.decimals = point < 0 ? 0 : int(text.size() - point - 1);
  for (const PortSample &p : samples)
    if (!p.sample.text.isEmpty())
      ++s.values;
  return s;
}

bool matches(const Expect &e, const State &s)
{
  if (!s.understood)
    return !e.isSet();
  if (e.quantity != Quantity::Unknown && s.quantity != e.quantity)
    return false;
  if ((s.defining & e.defining) != e.defining)
    return false;
  if (e.anyFlag && !(s.flags & e.anyFlag))
    return false;
  if (e.overload && s.quality != Quality::Overload)
    return false;
  if (e.negative && !s.negative)
    return false;
  return !(e.manualRange && (s.flags & SampleFlag::Autorange));
}

QString describe(const Expect &e)
{
  QStringList parts;
  if (e.quantity != Quantity::Unknown)
    parts << Quantities::name(e.quantity);
  if (e.defining & SampleFlag::DC)
    parts << "DC";
  if (e.defining & SampleFlag::AC)
    parts << "AC";
  if (e.defining & SampleFlag::Diode)
    parts << QCoreApplication::translate("CaptureSteps", "diode");
  if (e.overload)
    parts << "OL";
  if (e.negative)
    parts << QCoreApplication::translate("CaptureSteps", "negative");
  if (e.anyFlag & SampleFlag::Hold)
    parts << "HOLD";
  if (e.anyFlag & SampleFlag::Relative)
    parts << "REL";
  if (e.anyFlag & (SampleFlag::Max | SampleFlag::Min))
    parts << ((e.anyFlag & SampleFlag::Min) ? "MAX/MIN" : "MAX");
  if (e.manualRange)
    parts << QCoreApplication::translate("CaptureSteps", "manual range");
  return parts.join(' ');
}
}

void CaptureWatcher::setBaseline(std::optional<CaptureSteps::State> state)
{
  m_baseline = std::move(state);
  // the state the meter is in now counts again against the new baseline
  m_proposed = false;
}

bool CaptureWatcher::add(const CaptureSteps::State &state, qint64 wall)
{
  if (!m_current || *m_current != state)
  {
    m_current = state;
    m_since = wall;
    m_steady.reset();
    m_proposed = false;
  }
  if (wall - m_since < kStable)
    return false;
  m_steady = m_current;
  if (m_proposed || (m_baseline && *m_baseline == state))
    return false;
  m_proposed = true;
  return true;
}
