// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The core's base types and ReadingAdapter: port keys and their text form,
// and the cases of the special/unit table the decoder fixtures do not cover.
// StaleRule, the time rule beside it.

#include <QCoreApplication>
#include <QDebug>
#include <cmath>

#include "core/readingadapter.h"
#include "core/stalerule.h"
#include "testutil.h"

static PortSample one(ReadingAdapter &a, const QString &text, const QString &unit, const QString &special,
                      const QString &range = "AUTO", bool showBar = true, int id = 0)
{
  return a.adaptValue(text.toDouble(), text, unit, special, range, false, showBar, false, id, 0);
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // --- PortKey text form ---
  {
    PortKey k;
    k.quantity = Quantity::Voltage;
    k.defining = SampleFlag::DC;
    check(k.toString() == "voltage.dc", "voltage.dc: " + k.toString());
    k.channel = 2;
    k.slot = Slot::Sub;
    check(k.toString() == "2:voltage.dc.sub", "2:voltage.dc.sub: " + k.toString());
    check(PortKey::fromString("2:voltage.dc.sub") == k, "round trip with channel and slot");
    check(PortKey::fromString("voltage.ac.dc").defining == (SampleFlag::AC | SampleFlag::DC), "ac.dc");
    check(PortKey::fromString("duty_cycle").quantity == Quantity::DutyCycle, "duty_cycle");
    check(!PortKey::fromString("voltage.xyz").isValid(), "unknown part: invalid key");
    check(!PortKey::fromString("bogus").isValid(), "unknown quantity: invalid key");
    check(Quantities::fromId(Quantities::id(Quantity::StateOfCharge)) == Quantity::StateOfCharge, "own quantities round trip");
    // every quantity has its row in the table: one without fell back on
    // "unknown" (CarbonDioxide of the TFA AirControl did)
    for (int q = int(Quantity::Voltage); q <= int(Quantity::HarmonicRatio); ++q)
      check(Quantities::id(Quantity(q)) != QLatin1String("unknown"), QString("table row for sr_mq %1").arg(q));
    for (int q = int(Quantity::Energy); q <= int(Quantity::Illuminance); ++q)
      check(Quantities::id(Quantity(q)) != QLatin1String("unknown"), QString("table row for own quantity %1").arg(q));
    check(Quantities::id(Quantity::CarbonDioxide) == QLatin1String("carbon_dioxide"), "carbon_dioxide");
    check(int(Quantity::Voltage) == 10000 && int(Quantity::HarmonicRatio) == 10032, "sr_mq numbering");
  }

  ReadingAdapter a;
  a.setCounts(4000);

  // --- a text that is neither word nor number: no value, not a valid 0 ---
  {
    const PortSample r = one(a, "1.:34", "V", "DC");
    check(r.sample.quality == Quality::NoValue && std::isnan(r.sample.value), "damaged digits: no value");
    check(one(a, "8 0.0", "V", "DC").sample.quality == Quality::NoValue, "a damaged digit read as a space");
    check(one(a, " 1.234", "V", "DC").sample.quality == Quality::Valid, "a space is no damage");
    check(one(a, "- 0.24", "V", "DC").sample.quality == Quality::Valid, "nor after the sign");
    check(one(a, ".500", "V", "DC").sample.quality == Quality::Valid, "a point first");
    check(one(a, "-0.012", "V", "DC").sample.quality == Quality::Valid, "a sign neither");
  }

  // --- unit spelling, prefix, flags ---
  {
    const PortSample r = one(a, "4.700", "kOhm", "OH");
    check(r.port.toString() == "resistance" && r.unit.base == "Ω" && r.sample.displayPrefix == "k",
          "kOhm: resistance in Ω, prefix k");
    check(r.decoderUnit == "kOhm" && r.decoderBaseUnit == "Ohm", "the decoder's spelling is kept beside it");
    check(r.sample.flags & SampleFlag::Autorange, "AUTO: autorange flag");
    const PortSample u = one(a, "12.3", "uA", "DC");
    check(u.sample.displayPrefix == "µ" && u.port.toString() == "current.dc", "uA: prefix µ, current.dc");
    const PortSample m = one(a, "15", "min", "DC");
    check(m.port.quantity == Quantity::Time && m.sample.displayPrefix.isEmpty(), "min is minutes, not milli-in");
  }

  // --- the mode codes ---
  {
    check(one(a, "0.512", "V", "DI").port.toString() == "voltage.diode", "DI");
    check(one(a, "0.512", "V", "Diode").port.toString() == "voltage.diode", "Diode (DO3122)");
    check(one(a, "12.3", "Ohm", "BUZ").port.toString() == "continuity", "BUZ");
    check(one(a, "1.0", "V", "ACDC").port.toString() == "voltage.ac.dc", "ACDC");
    check(one(a, "50.0", "Hz", "DC").port.toString() == "frequency", "Hz in the V DC position: one frequency port");
    check(one(a, "50.0", "Hz", "DC").sample.flags & SampleFlag::DC, "... the DC flag stays on the sample");
    check(one(a, "23.5", "C", "TE").port.toString() == "temperature" && one(a, "23.5", "C", "TE").unit.base == "°C", "C");
    check(one(a, "74.3", "F", "TE").unit.base == "°F", "F with TE is Fahrenheit (ES51986)");
    check(one(a, "4.7", "uF", "").port.quantity == Quantity::Capacitance, "F without TE is farad");
    check(one(a, "74.3", "dF", "").unit.base == "°F", "dF");
    check(one(a, "1.25", "ms", "FR").port.quantity == Quantity::PulseWidth, "ms in the frequency position: pulse width");
  }

  // --- time: the frame's arrival, not the time of adapting ---
  {
    DmmDecoder::DmmResponse r;
    r.main().val = "1.000";
    r.main().unit = "V";
    r.main().special = "DC";
    r.received = 42;
    r.receivedWall = 1700000000000;
    const PortSample ps = a.adapt(r, 5).first();
    check(ps.sample.t == 42 && ps.sample.wall == 1700000000000, "sample time from the frame's arrival");
    r.received = r.receivedWall = 0;
    check(a.adapt(r, 5).first().sample.wall == 5, "no arrival time: the time passed in");
  }

  // --- protocol: a Victron's % and min ---
  {
    ReadingAdapter v(FrameFormat::VictronBLE);
    check(v.adaptValue(71.5, "71.5", "%", "DC", "", false, true, false, 0, 0).port.toString() == "state_of_charge",
          "Victron %: state of charge, no DC");
    check(v.adaptValue(300, "300", "min", "DC", "", false, true, false, 0, 0).port.quantity == Quantity::TimeToGo,
          "Victron min: time to go");
    // from an hour on shown in h, the value still in min: one unit for graph and min/max
    const PortSample h = v.adaptValue(90, "1.5", "h", "DC", "", false, true, false, 0, 0);
    check(h.port.quantity == Quantity::TimeToGo && h.decoderBaseUnit == "min" && h.unit.base == "min"
            && qFuzzyCompare(h.sample.value, 90.0) && h.decoderUnit == "h",
          "Victron h: shown in h, minutes for graph and min/max");
    const PortSample none = v.adaptValue(std::nan(""), "--", "h", "DC", "", false, true, false, 0, 0);
    check(std::isnan(none.sample.value) && none.port.quantity == Quantity::TimeToGo, "Victron --: no number, a gap");
    check(one(a, "1.5", "h", "").decoderBaseUnit == "h", "another meter's h stays h");
    check(one(a, "50.0", "%", "DC").port.toString() == "duty_cycle", "a meter's %: duty cycle");
    const PortSample rh = one(a, "45.2", "%RH", "");
    check(rh.port.toString() == "relative_humidity" && rh.unit.base == "%" && std::isnan(rh.sample.range.full),
          "%RH of any meter: relative humidity in %, no range");
    const PortSample ws = one(a, "3.47", "m/s", "");
    check(ws.port.toString() == "wind_speed" && ws.sample.displayPrefix.isEmpty() && qFuzzyCompare(ws.sample.value, 3.47),
          "m/s: wind speed, not milli-/s");
    const PortSample spl = one(a, "101.1", "dBA", "");
    check(spl.port.toString() == "sound_pressure_level" && spl.unit.base == "dBA" && std::isnan(spl.sample.range.full),
          "dBA: sound pressure level, no range");
  }

  // --- quality, range, bar ---
  {
    const PortSample ol = one(a, " OL ", "MOhm", "OH");
    check(ol.sample.quality == Quality::Overload, "OL: quality Overload");
    check(std::isnan(ol.sample.value) && !std::signbit(ol.sample.value), "OL: no value, NaN (the decoder said 0)");
    const PortSample neg = one(a, "-OL", "V", "DC");
    check(std::isnan(neg.sample.value) && std::signbit(neg.sample.value), "-OL: a negative NaN");
    const Reading r = ReadingAdapter::reading(neg);
    check(std::isnan(r.value) && r.overload && r.text == "-OL", "Reading: NaN, overload, the text as shown");
    check(std::isnan(ol.sample.range.full) && std::isnan(ol.sample.bar), "OL: no range, no bar");
    const PortSample v = one(a, "3.856", "V", "DC");
    check(v.sample.quality == Quality::Valid, "a number: Valid");
    check(qFuzzyCompare(v.sample.range.full, 4.0), QString("3.856 @4000: 4 V range, got %1").arg(v.sample.range.full));
    check(qFuzzyCompare(v.sample.bar, 3856.f / 4000.f), "bar 3856/4000");
    const PortSample mv = one(a, "123.4", "mV", "DC");
    check(qFuzzyCompare(mv.sample.range.full, 0.4), "123.4 mV @4000: 0.4 V in SI");
    const PortSample t = one(a, "37.2", "C", "TE");
    check(!t.showBar && std::isnan(t.sample.range.full), "temperature: no bar, no range");
    check(one(a, "1.234", "V", "DC", "AUTO", false).showBar == false, "the decoder's showBar is kept");
  }

  // --- the stale rule (kern_spezifikation §4.3): three intervals, 1..30 s ---
  {
    StaleRule s;
    check(s.stale(0) && s.maxAgeMs() == 3000, "no value yet: stale, 3 s");
    s.arrived(0);
    check(!s.stale(3000) && s.stale(3001), "one value: the 3 s of the link timeout");
    for (qint64 t = 500; t <= 10000; t += 500)
      s.arrived(t);
    check(s.maxAgeMs() == 1500 && !s.stale(11500) && s.stale(11501),
          QString("every 0.5 s: stale after 1.5 s, got %1").arg(s.maxAgeMs()));
    s.arrived(30000);   // an outage, not the rhythm
    check(s.maxAgeMs() == 1500, QString("a gap does not count as an interval, got %1").arg(s.maxAgeMs()));

    StaleRule fast;
    for (qint64 t = 0; t <= 5000; t += 100)
      fast.arrived(t);
    check(fast.maxAgeMs() == 1000, QString("every 0.1 s: at least 1 s, got %1").arg(fast.maxAgeMs()));

    StaleRule slow;   // a slow meter: the limit grows with each interval it allows
    qint64 t = 0;
    for (int i = 0; i < 60; ++i, t += qMin<qint64>(12000, slow.maxAgeMs()))
      slow.arrived(t);
    check(slow.maxAgeMs() == 30000, QString("every 12 s: at most 30 s, got %1").arg(slow.maxAgeMs()));
    slow.reset();
    check(slow.stale(t) && slow.maxAgeMs() == 3000, "reset: no value, 3 s");
  }

  // --- the second value: its own port, or a slot of the main one ---
  {
    DmmDecoder::DmmResponse r{};
    DmmDecoder::DmmChannel &m = r.main();
    m.dval = 230.1; m.val = "230.1"; m.unit = "V"; m.special = "AC"; r.range = "AUTO"; r.showBar = true;
    r.addChannel(1) = { 1, 50.0, "50.00", "Hz", "", 0 };
    QList<PortSample> s = a.adapt(r, 1234);
    check(s.size() == 2 && s[0].port.toString() == "voltage.ac" && s[1].port.toString() == "frequency",
          "V AC + Hz: two ports");
    check(s[1].id == 1 && s[1].sample.wall == 1234, "second value: id 1, the frame's time");
    r.main().unit = "C"; r.main().special = "TE"; r.channels[1].unit = "dF"; r.channels[1].val = "254.1";
    s = a.adapt(r, 0);
    check(s[1].port.toString() == "temperature.sub", "°C + °F: the same quantity, slot sub: " + s[1].port.toString());
    r.channels[1].val.clear(); r.channels[1].unit.clear();
    s = a.adapt(r, 0);
    check(s.size() == 2 && s[1].sample.text.isEmpty(), "an empty second value comes along, to clear the display");
    r.lowBat = true;
    check(a.adapt(r, 0)[0].sample.flags & SampleFlag::LowBattery, "low battery flag");
  }

  // --- no number shown ("----") or a fault word ("FUSE"): no value, and no
  //     overload although the text has letters ---
  {
    ReadingAdapter a;
    DmmDecoder::DmmResponse r;
    r.main().val = "FUSE";
    r.main().unit = "mA";
    r.main().special = "DC";
    r.main().fault = "FUSE";
    DmmDecoder::DmmChannel &dash = r.addChannel(1);
    dash.val = "----";
    dash.unit = "V";
    dash.noValue = true;
    const QList<PortSample> s = a.adapt(r, 0);
    check(s.size() == 2 && s[0].sample.quality == Quality::Fault && std::isnan(s[0].sample.value)
            && s[1].sample.quality == Quality::NoValue && std::isnan(s[1].sample.value),
          "a fault word and dashes: no value");
    const Reading fuse = ReadingAdapter::reading(s[0]);
    check(!fuse.overload && fuse.noNumber() && fuse.fault() && fuse.text == "FUSE", "FUSE: no overload, a fault");
    check(one(a, "OL", "V", "DC").sample.quality == Quality::Overload, "OL stays an overload");
  }

  if (failed == 0)
    qInfo() << "All adapter tests passed.";
  else
    qWarning() << failed << "adapter test(s) failed.";
  return failed == 0 ? 0 : 1;
}
