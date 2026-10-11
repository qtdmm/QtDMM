// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// PoincareSeries on its own: the pairs, SD1 and SD2, gaps, port changes,
// capacity, lag and the axis.
#include <QCoreApplication>
#include <QDebug>
#include <cmath>

#include "core/poincare.h"
#include "core/readingadapter.h"

static int failed = 0;

static void check(bool cond, const QString &what)
{
  if (!cond)
  {
    qWarning() << "FAILED:" << what;
    failed++;
  }
}

static bool near(double a, double b)
{
  return std::abs(a - b) < 1e-9;
}

// a main reading as the MeterController makes it, value in SI
static Reading reading(double value, const QString &text, const QString &unit = "V", const QString &special = "DC",
                       bool hold = false)
{
  static ReadingAdapter adapter;
  Reading r = ReadingAdapter::reading(adapter.adaptValue(value, text, unit, special, "AUTO", hold, false, false, 0, 0));
  if (!r.overload)
    r.value = value;
  return r;
}

static QString pairsText(const PoincareSeries &s)
{
  QStringList out;
  for (const PoincareSeries::Pair &p : s.pairs())
    out << QString("%1,%2").arg(p.x).arg(p.y);
  return out.join(' ');
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // --- 1. pairs and the spread: 1 2 3 2 1 gives (1,2) (2,3) (3,2) (2,1);
  //        across the diagonal +-1/sqrt 2, along it +-1/sqrt 2 around the
  //        middle: SD1 = SD2 = sqrt(2/3) ---
  {
    PoincareSeries s;
    for (double v : { 1.0, 2.0, 3.0, 2.0, 1.0 })
      s.feed(reading(v, QString::number(v)));
    check(pairsText(s) == "1,2 2,3 3,2 2,1", "pairs: got " + pairsText(s));
    const PoincareSeries::Stats st = s.stats();
    check(st.count == 4 && near(st.meanX, 2) && near(st.meanY, 2), "stats: count or mean");
    check(near(st.sd1, std::sqrt(2.0 / 3)) && near(st.sd2, std::sqrt(2.0 / 3)),
          QString("stats: SD1 %1 SD2 %2").arg(st.sd1).arg(st.sd2));
    const QVector<PoincareSeries::Pair> p = s.pairs();
    check(p.first().age == 3 && p.last().age == 0, "pairs: the newest has age 0");
    check(s.unit() == "V", "unit: " + s.unit());
  }

  // --- 2. a steady value is a point: SD1 = SD2 = 0; a drift lies along the
  //        diagonal (SD1 0, SD2 > 0) ---
  {
    PoincareSeries steady, drift;
    for (int i = 0; i < 10; ++i)
    {
      steady.feed(reading(5, "5.000"));
      drift.feed(reading(i * 0.1, QString::number(i * 0.1)));
    }
    check(near(steady.stats().sd1, 0) && near(steady.stats().sd2, 0), "steady: SD1 and SD2 should be 0");
    check(near(drift.stats().sd1, 0) && drift.stats().sd2 > 0.2, QString("drift: SD1 %1 SD2 %2")
            .arg(drift.stats().sd1).arg(drift.stats().sd2));
  }

  // --- 3. gaps: no pair across an overload, a held display or a value
  //        gone stale ---
  {
    PoincareSeries s;
    s.feed(reading(1, "1"));
    s.feed(reading(2, "2"));
    s.feed(reading(0, "OL"));
    s.feed(reading(3, "3"));
    s.feed(reading(4, "4"));
    s.feed(reading(4, "4", "V", "DC", true));   // HOLD
    s.feed(reading(5, "5"));
    s.feed(reading(6, "6"));
    s.gap();
    s.feed(reading(7, "7"));
    s.feed(reading(8, "8"));
    check(pairsText(s) == "1,2 3,4 5,6 7,8", "gaps: got " + pairsText(s));
  }

  // --- 4. another port or unit starts afresh, a range change does not ---
  {
    PoincareSeries s;
    s.feed(reading(0.5, "500.0", "mV"));
    check(!s.feed(reading(1.5, "1.500", "V")), "range: mV -> V must not start afresh");
    check(pairsText(s) == "0.5,1.5", "range: got " + pairsText(s));
    check(s.feed(reading(1000, "1.000", "kOhm", "OH")), "port: V DC -> Ohm must start afresh");
    check(s.valueCount() == 1 && s.unit() == "Ω", "port: one value in Ω, got " + s.unit());
    check(!s.feed(reading(0, "OL", "kOhm", "OH")), "port: an overload says nothing about the port");
    s.feed(reading(21, "21.0", "C", "TE"));
    check(s.valueCount() == 1 && s.unit() == "°C", "port: °C afresh");
    check(s.feed(reading(70, "70.0", "dF", "TE")), "unit: °C -> °F must start afresh");
  }

  // --- 5. capacity: the oldest go and the numbers follow; lag 3 ---
  {
    PoincareSeries s;
    s.setCapacity(4);
    for (double v : { 9.0, 9.0, 1.0, 2.0, 3.0, 2.0 })
      s.feed(reading(v, QString::number(v)));
    check(s.valueCount() == 4 && pairsText(s) == "1,2 2,3 3,2", "capacity: got " + pairsText(s));
    check(near(s.stats().meanX, 2), "capacity: the mean of what is kept");
    s.setCapacity(1);
    check(s.capacity() == 2 && s.valueCount() == 2, "capacity: at least 2");

    PoincareSeries k;
    k.setLag(3);
    for (int i = 1; i <= 6; ++i)
      k.feed(reading(i, QString::number(i)));
    check(pairsText(k) == "1,4 2,5 3,6", "lag 3: got " + pairsText(k));
    k.setLag(0);
    check(k.lag() == 1, "lag: at least 1");
    k.setLag(99);
    check(k.lag() == PoincareSeries::kMaxLag, "lag: at most kMaxLag");
    k.clear();
    check(k.valueCount() == 0 && k.pairs().isEmpty() && std::isnan(k.stats().sd1), "clear");
  }

  {
    // a meter at 0: the axis follows the display, not 1e-12
    PoincareSeries z;
    z.feed(reading(0, "0.0", "mV"));
    check(near(z.resolution(), 1e-4), QString("resolution of 0.0 mV: %1").arg(z.resolution()));
    const PoincareSeries::Axis a = PoincareSeries::axis(0, 0, z.resolution());
    check(a.lo < 0 && a.hi > 0 && a.hi - a.lo >= 1e-4 && a.hi - a.lo <= 1e-2 && a.step >= 1e-4,
          QString("axis at 0: %1 .. %2 step %3").arg(a.lo).arg(a.hi).arg(a.step));
    z.feed(reading(1.234, "1.234", "V"));
    check(near(z.resolution(), 1e-3), "resolution of 1.234 V");
    const PoincareSeries::Axis b = PoincareSeries::axis(1.234, 1.234, z.resolution());
    check(b.lo < 1.234 && b.hi > 1.234 && b.hi - b.lo <= 0.1, QString("axis at 1.234: %1 .. %2").arg(b.lo).arg(b.hi));
    const PoincareSeries::Axis c = PoincareSeries::axis(0, 0, 0);
    check(c.lo < 0 && c.hi > 0 && c.step > 1e-3, "axis at 0 without a resolution");
    const PoincareSeries::Axis d = PoincareSeries::axis(1, 3, 1e-3);
    check(d.lo <= 1 && d.hi >= 3 && near(d.step, 0.5), QString("axis 1..3: step %1").arg(d.step));
    z.clear();
    check(z.resolution() == 0, "clear: no resolution");
  }

  if (failed)
  {
    qWarning() << failed << "Poincaré check(s) failed";
    return 1;
  }
  qInfo() << "All Poincaré tests passed.";
  return 0;
}
