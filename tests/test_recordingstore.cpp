// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// RecordingStore on its own: QCoreApplication only, no widgets. What it
// records against a stream of readings over time (the grid export, the
// integral, the triggers) is in test_recording_golden; this is the rest:
// the points and their quality, the limits, marks by time, load, gaps, one
// function per recording and the readings series.
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <cmath>

#include "core/limittest.h"
#include "core/readingadapter.h"
#include "recording/recordingstore.h"
#include "testutil.h"

// A reading as the MeterController makes it: through ReadingAdapter
static Reading reading(double value, const QString &text, const QString &special, bool hold = false,
                       const QString &range = "AUTO")
{
  static ReadingAdapter adapter;
  Reading r = ReadingAdapter::reading(adapter.adaptValue(value, text, "mV", special, range, hold, true, false, 0,
                                                         QDateTime::currentMSecsSinceEpoch()));
  r.value = value;   // the tests give SI values together with display texts of their own
  return r;
}

// The meter shows no number ("----") or a fault word ("FUSE"): @p shown is
// Quality::NoValue or Quality::Fault, as DmmChannel says it
static Reading noNumber(const QString &text, Quality shown)
{
  static ReadingAdapter adapter;
  return ReadingAdapter::reading(adapter.adaptValue(0, text, "", "", "AUTO", false, false, false, 0,
                                                    QDateTime::currentMSecsSinceEpoch(), shown));
}

// Another value of the meter (a second display): @p id, in @p unit; an empty
// @p text is a display that is off
static Reading other(int id, double value, const QString &text, const QString &unit, const QString &special = "")
{
  static ReadingAdapter adapter;
  Reading r = ReadingAdapter::reading(adapter.adaptValue(value, text, unit, special, "AUTO", false, false, false, id,
                                                         QDateTime::currentMSecsSinceEpoch()));
  r.value = value;
  return r;
}

// A clock of the test's own for a store: ms on the core clock, the wall
// clock from a fixed day
struct TestClock
{
  qint64 now = 0;
  static QDateTime wall(qint64 t) { return QDateTime(QDate(2026, 10, 4), QTime(12, 0, 0)).addMSecs(t); }
  void attach(RecordingStore &store)
  {
    store.setClock([this] { return now; }, [this] { return wall(now); });
  }
  /// @p r as it arrives at @p t (the clock moves there)
  Reading at(qint64 t, Reading r)
  {
    now = t;
    r.t = t;
    r.msecs = wall(t).toMSecsSinceEpoch();
    return r;
  }
};

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // --- 1. the points: value, flags and quality of each main reading, with
  //        its time; secondary readings (id != 0) are not recorded ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.start();
    store.setReading(clock.at(250, reading(0.01234, "12.34", "DC")));
    Reading second = reading(50, "50.00", "AC", false, "MANU");
    second.id = 1;
    store.setReading(clock.at(260, second));
    check(store.count() == 1, QString("point: expected 1, got %1").arg(store.count()));
    const RawPoint &p = store.series().last();
    // the time starts with the first reading, not with start()
    check(p.t == 0 && p.value == 0.01234 && store.startDateTime() == TestClock::wall(250),
          QString("point: t %1 value %2").arg(p.t).arg(p.value));
    check(p.flags == (SampleFlag::DC | SampleFlag::Autorange), QString("point: flags 0x%1").arg(p.flags, 0, 16));
    check(p.quality == Quality::Valid, "point: quality not Valid");

    // a new recording starts with the value the meter shows
    store.start();
    check(store.count() == 1 && store.series().first().t == 0 && store.series().first().value == 0.01234,
          "start: the first point is the current value");
    store.setReading(clock.at(500, reading(0.005, "5.000", "ACDC", true, "MANU")));
    check(store.series().last().flags == (SampleFlag::AC | SampleFlag::DC | SampleFlag::Hold),
          QString("point: ACDC + hold flags 0x%1").arg(store.series().last().flags, 0, 16));
    // nothing for a while: no start value
    clock.now = 10000;
    store.start();
    check(store.count() == 0, "start: a stale value is no start value");
  }

  // --- 1c. the other values (id > 0): series of their own on the same time
  //         axis, one per value and port; the recording goes on when a
  //         second display changes what it shows ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    QSignalSpy began(&store, &RecordingStore::extrasChanged);
    QSignalSpy added(&store, &RecordingStore::extraAppended);
    store.start();
    // before the main value's first reading there is no time axis
    store.setReading(clock.at(100, other(1, 50, "50.00", "Hz")));
    check(store.extraCount() == 0, "other value before the main one: dropped");
    store.setReading(clock.at(250, reading(0.01234, "12.34", "DC")));
    store.setReading(clock.at(260, other(1, 50, "50.00", "Hz")));
    check(store.count() == 1 && store.extraCount() == 1, QString("other value: %1 series").arg(store.extraCount()));
    const RecordingSeries &hz = store.extra(0);
    check(hz.id == 1 && hz.unit == "Hz" && hz.port.quantity == Quantity::Frequency && hz.count() == 1
            && hz.last().t == 10 && hz.last().value == 50 && hz.last().integral == 0,
          QString("other value: id %1 unit %2 t %3").arg(hz.id).arg(hz.unit).arg(hz.count() ? hz.last().t : -1));
    check(began.count() == 1 && added.count() == 0, "other value: a series began");
    store.setReading(clock.at(500, other(1, 51, "51.00", "Hz")));
    check(store.extra(0).count() == 2 && added.count() == 1 && added.last().at(0).toInt() == 0,
          "other value: the next reading appended");
    // its integral over time, as the main value's: 50 Hz for 0.24 s
    check(std::abs(store.extra(0).last().integral - 12) < 1e-9,
          QString("other value: integral %1, expected 12").arg(store.extra(0).last().integral));
    // its stats as the main value's: up to now, nothing before its first reading
    {
      const RecordingStore::Stats st = store.statsOf(store.extra(0), 0, 250);
      check(st.count == 1 && st.mean == 50 && std::abs(st.integral - 12) < 1e-9 && st.covered == 240,
            QString("other value: stats %1 readings, mean %2, %3 ms").arg(st.count).arg(st.mean).arg(st.covered));
    }

    // Hz -> %: the series ends with a gap, a new one begins, still recording
    store.setReading(clock.at(700, other(1, 25, "25.0", "%")));
    check(store.isRunning() && store.extraCount() == 2 && store.extra(0).last().gap()
            && store.extra(0).last().t == 450 && store.extra(1).first().t == 450 && store.extra(1).unit == "%",
          QString("Hz -> %: %1 series, running %2").arg(store.extraCount()).arg(store.isRunning()));
    // the display goes off: one gap, not one per frame
    store.setReading(clock.at(800, other(1, 0, "", "%")));
    store.setReading(clock.at(900, other(1, 0, "", "%")));
    check(store.extra(1).count() == 2 && store.extra(1).last().gap()
            && store.extra(1).last().quality == Quality::Stale,
          QString("display off: one gap, %1 points").arg(store.extra(1).count()));
    // an overload is a gap that says so, and keeps the series
    store.setReading(clock.at(1000, other(1, 0, "OL", "%")));
    check(store.extraCount() == 2 && store.extra(1).last().quality == Quality::Overload, "other value OL: Overload");
    // the meter falls silent: a gap in every series that has a value
    store.setReading(clock.at(1100, other(1, 30, "30.0", "%")));
    clock.now = 10000;
    store.setStale(true);
    check(store.extra(1).last().gap() && store.extra(1).last().t == 850 + 3000,
          QString("stale: gap at %1").arg(store.extra(1).last().t));
    // a second other value: a series of its own
    store.setReading(clock.at(10100, reading(0.02, "20.00", "DC")));
    store.setReading(clock.at(10110, other(2, 21.5, "21.5", "°C")));
    check(store.extraCount() == 3 && store.extra(2).id == 2, "a second other value: its own series");
    // clear empties them, a new recording starts with what the displays show
    // (°C, fresh; the % of value 1 is too old)
    store.clear();
    check(store.extraCount() == 0, "clear: no other series");
    store.start();
    check(store.count() == 1 && store.extraCount() == 1 && store.extra(0).id == 2 && store.extra(0).first().t == 0,
          QString("start: the other values shown, %1 series").arg(store.extraCount()));
    // a recording that stopped takes nothing more
    store.stop();
    store.setReading(clock.at(10500, other(2, 22, "22.0", "°C")));
    check(store.extra(0).count() == 1, "View: no other value appended");
    // a file loaded has none
    Recording rec;
    rec.start = TestClock::wall(0);
    rec.sampleTimeTenths = 10;
    rec.values = { 1, 2 };
    store.load(rec);
    check(store.extraCount() == 0, "load: no other series");
  }

  // --- 1d. the other values in Live: they keep what the main one keeps, and
  //         a pre-triggered recording takes them along ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setSampleLength(20);   // Live keeps 2 s
    store.live();
    for (qint64 t = 0; t <= 6000; t += 100)
    {
      store.setReading(clock.at(t, reading(0.001, "1.000", "DC")));
      store.setReading(clock.at(t + 10, other(1, double(t), "1.0", "Hz")));
    }
    const qint64 origin = store.origin();
    check(store.extraCount() == 1 && store.extra(0).first().t <= origin && store.extra(0).first().t > origin - 1200
            && store.extra(0).count() < 40,
          QString("Live: other series from %1 (origin %2), %3 points")
            .arg(store.extra(0).first().t).arg(origin).arg(store.extra(0).count()));
    // an old series of another port goes once it ended before what is kept
    store.setReading(clock.at(6100, other(1, 3, "3.0", "%")));
    for (qint64 t = 6200; t <= 9000; t += 100)
    {
      store.setReading(clock.at(t, reading(0.001, "1.000", "DC")));
      store.setReading(clock.at(t + 10, other(1, 3, "3.0", "%")));
    }
    check(store.extraCount() == 1 && store.extra(0).unit == "%",
          QString("Live: the ended series went, %1 left").arg(store.extraCount()));

    // pre-trigger: the recording reaches back 1 s, the other value with it
    store.setPreTrigger(1000);
    store.setStartMode(RecordingStore::Raising);
    store.setThresholds(0, 0.005);
    store.setReading(clock.at(9100, reading(0.01, "10.00", "DC")));
    check(store.isRunning(), "pre-trigger: started");
    check(store.extraCount() == 1 && store.extra(0).count() >= 9 && store.extra(0).first().t >= 0
            && store.extra(0).first().t <= 100 && store.extra(0).unit == "%",
          QString("pre-trigger: other series %1 points from %2")
            .arg(store.extraCount() ? store.extra(0).count() : 0).arg(store.extraCount() ? store.extra(0).first().t : -1));
  }

  // --- 1e. the other values over time: one that stops coming is held only
  //         up to the stale limit; nothing after the recording length; a
  //         display switched back and forth goes on in its series ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.start();
    for (qint64 t = 0; t <= 2000; t += 100)
    {
      store.setReading(clock.at(t, reading(0.001, "1.000", "DC")));
      store.setReading(clock.at(t + 10, other(1, 50, "50.00", "Hz")));
    }
    // the second display's frames end without an "off" (iDMM bit 7), the
    // main value goes on: a gap at the stale limit; the integral counts the
    // last value up to it, as for the main value, then stays
    for (qint64 t = 2100; t <= 8000; t += 100)
      store.setReading(clock.at(t, reading(0.001, "1.000", "DC")));
    const RecordingSeries &hz = store.extra(0);
    check(store.extraCount() == 1 && hz.last().gap() && hz.last().quality == Quality::Stale && hz.last().t == 2010 + 3000,
          QString("silent value: last point t %1 gap %2").arg(hz.last().t).arg(hz.last().gap()));
    const double integral = hz.at(hz.count() - 2).integral;
    store.setReading(clock.at(8010, other(1, 50, "50.00", "Hz")));
    check(store.extraCount() == 1 && !store.extra(0).last().gap()
            && std::abs(store.extra(0).last().integral - (integral + 50 * 3.0)) < 1e-9,
          QString("silent value back: same series, integral %1 (was %2)").arg(store.extra(0).last().integral).arg(integral));
    // only one gap for the silence
    int gaps = 0;
    for (int i = 0; i < store.extra(0).count(); ++i)
      gaps += store.extra(0).at(i).gap();
    check(gaps == 1, QString("silent value: %1 gaps").arg(gaps));

    // Hz, %, Hz, % ...: two series, the newest last
    QSignalSpy changed(&store, &RecordingStore::extrasChanged);
    for (int k = 0; k < 40; ++k)
    {
      const qint64 t = 8100 + k * 100;
      store.setReading(clock.at(t, reading(0.001, "1.000", "DC")));
      store.setReading(clock.at(t + 10, k % 2 ? other(1, 50, "50.00", "Hz") : other(1, 25, "25.0", "%")));
    }
    check(store.extraCount() == 2 && store.extra(1).unit == "Hz" && store.extra(0).unit == "%",
          QString("Hz/% back and forth: %1 series").arg(store.extraCount()));
    check(store.extra(1).last().t == 8100 + 39 * 100 + 10 && !store.extra(1).last().gap(),
          "Hz/% back and forth: the newest series has the newest point");
    bool ordered = true;
    for (int i = 0; i < store.extraCount(); ++i)
      for (int j = 1; j < store.extra(i).count(); ++j)
        ordered = ordered && store.extra(i).at(j).t >= store.extra(i).at(j - 1).t;
    check(ordered, "Hz/% back and forth: each series in time order");

    // a value taken along at the start stands at 0 however old it was: its
    // gap never lies after now
    RecordingStore late;
    TestClock c3;
    c3.attach(late);
    late.setReading(c3.at(0, reading(0.001, "1.000", "DC")));
    late.setReading(c3.at(10, other(1, 50, "50.00", "Hz")));
    late.setReading(c3.at(2000, reading(0.001, "1.000", "DC")));
    late.start();
    for (qint64 t = 2100; t <= 3500; t += 100)
      late.setReading(c3.at(t, reading(0.001, "1.000", "DC")));
    check(late.extraCount() == 1 && late.extra(0).last().gap() && late.extra(0).last().t <= late.series().last().t,
          QString("old value at the start: gap at %1, the recording at %2")
            .arg(late.extraCount() ? late.extra(0).last().t : -1).arg(late.series().last().t));

    // the recording length: the main value stops it, the other values add
    // nothing after it
    RecordingStore len;
    TestClock c2;
    c2.attach(len);
    len.setSampleLength(10);   // 1 s
    len.start();
    // the main value every 300 ms: 900, then the other one at 1050, after
    // the length but before the main value at 1200 stops
    for (qint64 t = 0; t <= 1500; t += 300)
    {
      len.setReading(c2.at(t, reading(0.001, "1.000", "DC")));
      len.setReading(c2.at(t + 150, other(1, 50, "50.00", "Hz")));
    }
    check(!len.isRunning() && len.extraCount() == 1 && len.extra(0).last().t < 1000,
          QString("length: the other value ends at %1").arg(len.extraCount() ? len.extra(0).last().t : -1));
  }

  // --- 1f. another meter: live() starts with the value the meter shows,
  //         but not with the last one of the meter before (switching in
  //         the sidebar), nor with its other displays ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.live();
    store.setReading(clock.at(1000, reading(1755, "1755", "")));
    store.setReading(clock.at(1010, other(1, 20.85, "20.85", "°C")));
    clock.now = 1500;
    store.live();
    check(store.count() == 1 && store.extraCount() == 1, "live(): starts with the value shown and the other display");
    store.forgetReading();
    store.live();
    check(store.count() == 0 && store.extraCount() == 0,
          QString("another meter: %1 points, %2 other values").arg(store.count()).arg(store.extraCount()));
    store.setReading(clock.at(2000, reading(0.063, "63.0", "")));
    check(store.count() == 1 && store.series().at(0).value == 0.063, "another meter: its first reading is the first point");
  }

  // --- 1b. the time starts with the first reading: Live started while the
  //         meter was still connecting, or the graph cleared, begins at 0 ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.live();
    store.setReading(clock.at(5000, reading(1, "1.000", "DC")));
    store.setReading(clock.at(5500, reading(2, "2.000", "DC")));
    check(store.series().first().t == 0 && store.series().last().t == 500 && store.duration() == 500
            && store.startDateTime() == TestClock::wall(5000),
          QString("first reading: t %1, duration %2").arg(store.series().first().t).arg(store.duration()));
    // a recording with the value shown begins with it at 0 anyway
    clock.now = 6000;
    store.start();
    store.setReading(clock.at(6500, reading(3, "3.000", "DC")));
    check(store.series().first().t == 0 && store.series().last().t == 500, "start: from the value shown");
  }

  // --- 2. quality: an overload is a point without a value, Overload; the
  //        value going stale a point without one, Stale, where the last
  //        one stopped holding; the grid takes the worst in each step ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setSampleTime(10);
    store.setStaleAfter(1500);
    store.start();
    store.setReading(clock.at(0, reading(1, "1.000", "DC")));
    store.setReading(clock.at(100, reading(1, "1.000", "DC")));
    store.setReading(clock.at(600, reading(0, "OL", "DC")));
    check(store.series().last().gap() && store.series().last().quality == Quality::Overload,
          "quality: OL should be a gap point, Overload");
    store.setReading(clock.at(900, reading(1, "1.000", "DC")));
    clock.now = 2500;
    store.setStale(true);
    check(store.series().last().gap() && store.series().last().quality == Quality::Stale
            && store.series().last().t == 2400,
          QString("quality: stale gap at %1, expected 2400").arg(store.series().last().t));
    store.setStale(true);   // once
    check(store.count() == 5, QString("quality: %1 points, expected 5").arg(store.count()));
    store.setReading(clock.at(4100, reading(2, "2.000", "DC")));
    clock.now = 5000;
    store.stop();
    const QVector<GridPoint> g = store.grid(10);
    QStringList q;
    for (const GridPoint &p : g)
      q << QString::number(int(p.quality));
    // 0: the first reading, 1: the OL, 2: valid, 3: stale from 2.4 s, 4: none, 5: 2 V from 4.1 s
    check(g.size() == 6 && g[0].quality == Quality::Valid && g[1].quality == Quality::Overload
            && g[2].quality == Quality::Valid && g[3].quality == Quality::Stale && std::isnan(g[4].value)
            && g[5].value == 2 && g[5].quality == Quality::Stale,
          "quality: grid qualities " + q.join(' '));
    check(g[2].value == 1, QString("quality: step 2 the held 1 V, got %1").arg(g[2].value));
  }

  // --- 3. the limits: a recording keeps all of it, from its start (a
  //        recording longer than the old maximum length lost its start);
  //        beyond the most points the oldest go in steps (appended(true)
  //        says so), and the origin moves to what is left ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setLiveWidth(2);   // Live's width, nothing to do with a recording
    store.start();
    for (int i = 0; i < 100; i++)
      store.setReading(clock.at(i * 100, reading(i, QString::number(i), "DC")));
    check(store.count() == 100 && store.origin() == 0 && store.series().first().value == 0,
          QString("keeps all: %1 points, origin %2").arg(store.count()).arg(store.origin()));
  }
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setMaxPoints(20);
    QSignalSpy appended(&store, &RecordingStore::appended);
    store.start();
    for (int i = 0; i < 100; i++)
      store.setReading(clock.at(i * 100, reading(i, QString::number(i), "DC")));
    check(store.duration() == 9900 && store.origin() == store.series().first().t && store.origin() >= 7900,
          QString("limit: duration %1 origin %2").arg(store.duration()).arg(store.origin()));
    check(store.count() <= 20,
          QString("limit: %1 points from %2").arg(store.count()).arg(store.series().first().t));
    int shifts = 0;
    for (const auto &args : appended)
      shifts += args.first().toBool() ? 1 : 0;
    check(shifts > 0, QString("limit: %1 shifts").arg(shifts));
    check(store.series().last().value == 99, "limit: the newest is kept");
  }

  // --- 3b. a fast meter keeps every reading (50 a second); at the most a
  //         series keeps, the oldest go ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.start();
    for (int i = 0; i < 100; i++)
      store.setReading(clock.at(i * 20, reading(i, QString::number(i), "DC")));
    check(store.count() == 100, QString("fast: %1 of 100 readings").arg(store.count()));

    Reading r = reading(1, "1.000", "DC");
    store.setReadingsPaused(true);   // the table's series is not what this is about
    for (int i = 100; i < RecordingStore::kMaxPoints + RecordingStore::kMaxPoints / 50; i++)
    {
      clock.now = r.t = i * 20;   // without the wall clock: QDateTime is slow
      store.setReading(r);
    }
    check(store.count() <= RecordingStore::kMaxPoints + RecordingStore::kMaxPoints / 100
            && store.count() >= RecordingStore::kMaxPoints && store.series().first().t > 0,
          QString("most: %1 readings kept, from %2 ms").arg(store.count()).arg(store.series().first().t));
  }

  // --- 3c. pre-trigger: a recording started by a threshold reaches back by
  //         the pre-trigger time, with the readings of that time; a mark
  //         shows the trigger, the recording length counts from it ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setStartMode(RecordingStore::Raising);
    store.setThresholds(0, 5);
    store.setPreTrigger(2000);
    store.setSampleLength(10);   // 1 s after the trigger
    store.setReadingsPaused(true);
    store.live();                // the triggers wait in Live
    for (int i = 0; i < 10; i++)   // 0 .. 4.5 s, below the threshold
      store.setReading(clock.at(i * 500, reading(1 + i * 0.1, "1", "DC")));
    store.setReading(clock.at(5000, reading(6, "6.000", "DC")));   // the trigger
    check(store.isRunning(), "pre-trigger: not started");
    QStringList t;
    for (int i = 0; i < store.count(); ++i)
      t << QString::number(store.series().at(i).t);
    check(t.join(' ') == "0 500 1000 1500 2000", "pre-trigger: points at " + t.join(' '));
    check(store.series().first().value == 1.6 && store.series().last().value == 6,
          QString("pre-trigger: from %1 to %2").arg(store.series().first().value).arg(store.series().last().value));
    check(store.startDateTime() == TestClock::wall(3000), "pre-trigger: the recording starts 2 s before the trigger");
    check(store.marks().size() == 1 && store.marks().first().t == 2000 && store.marks().first().color == RecordingStore::kTriggerColor,
          "pre-trigger: a mark at the trigger");
    store.setReading(clock.at(5500, reading(6, "6.000", "DC")));
    check(store.isRunning() && store.remainingLength() == 5, QString("pre-trigger: %1 tenths left").arg(store.remainingLength()));
    store.setReading(clock.at(6000, reading(6, "6.000", "DC")));
    check(!store.isRunning() && store.duration() == 3000, QString("pre-trigger: stopped after %1 ms").arg(store.duration()));

    // the trigger on another value: a recording of the voltage started by
    // the current (value 2) crossing 5 A; the voltage's own 6 does nothing
    {
      RecordingStore by;
      TestClock c;
      c.attach(by);
      by.setStartMode(RecordingStore::Raising);
      by.setThresholds(0, 5);
      by.setTriggerValue(ValueRef(1));
      by.setReadingsPaused(true);
      by.live();
      by.setReading(c.at(0, reading(1, "1", "DC")));
      by.setReading(c.at(0, other(1, 1, "1.000", "A", "DC")));
      by.setReading(c.at(500, reading(6, "6.000", "DC")));
      check(!by.isRunning(), "trigger on value 2: the main value's 6 does not start it");
      by.setReading(c.at(1000, other(1, 4, "4.000", "A", "DC")));
      by.setReading(c.at(1500, other(1, 7, "7.000", "A", "DC")));
      check(by.isRunning(), "trigger on value 2: its 7 A starts it");
      by.stop();
      by.live();
      by.setTriggerValue(ValueRef(1, "device:other"));
      by.setReading(c.at(2000, other(1, 1, "1.000", "A", "DC")));
      by.setReading(c.at(2500, other(1, 7, "7.000", "A", "DC")));
      check(!by.isRunning(), "trigger on another meter's value: this meter's 7 A does not start it");
    }

    // a value gone stale before the trigger is a gap in it
    RecordingStore gap;
    TestClock gc;
    gc.attach(gap);
    gap.setStartMode(RecordingStore::Falling);
    gap.setThresholds(0, 0);
    gap.setPreTrigger(10000);
    gap.setStaleAfter(1500);
    gap.live();
    gap.setReading(gc.at(0, reading(3, "3", "DC")));
    gc.now = 2000;
    gap.setStale(true);
    gap.setReading(gc.at(4000, reading(2, "2", "DC")));
    gap.setReading(gc.at(4500, reading(-1, "-1", "DC")));   // the trigger
    check(gap.count() == 4 && gap.series().at(1).gap() && gap.series().at(1).t == 1500,
          QString("pre-trigger: %1 points, the gap at %2").arg(gap.count()).arg(gap.series().at(1).t));

    // a manual start does not reach back
    RecordingStore manual;
    TestClock mc;
    mc.attach(manual);
    manual.setPreTrigger(2000);
    manual.setReading(mc.at(0, reading(1, "1", "DC")));
    manual.start();
    check(manual.count() == 1 && manual.marks().isEmpty(), "pre-trigger: not for a manual start");
  }

  // --- 4. marks by time: now while recording; a mark goes with the
  //        readings before what the store keeps ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setMaxPoints(20);
    store.start();
    store.setReading(clock.at(0, reading(1, "1", "DC")));
    store.setReading(clock.at(100, reading(1, "1", "DC")));
    clock.now = 350;
    store.addMark(0xffff0000u, "alarm");
    check(store.marks().size() == 1 && store.marks().first().t == 350, "marks: not at the time it came");
    QSignalSpy changed(&store, &RecordingStore::marksChanged);
    for (int i = 5; i < 60; i++)
      store.setReading(clock.at(i * 100, reading(i, QString::number(i), "DC")));
    check(store.marks().isEmpty() && changed.size() == 1, "marks: did not go with the old readings");
  }

  // --- 5. load and write: an import replaces the readings, a write clears
  //        dirty, the unit and start time go with the recording; a loaded
  //        file on its own grid exports as it was ---
  {
    Recording rec;
    rec.start = QDateTime(QDate(2026, 9, 30), QTime(21, 0, 0));
    rec.sampleTimeTenths = 10;
    rec.unit = "V";
    rec.values = { 1.0, 2.0, qQNaN(), 3.0 };

    RecordingStore store;
    store.setUnit("V");
    store.load(rec);
    check(store.count() == 4 && store.series().last().value == 3.0 && store.series().last().t == 3000, "load: points");
    check(store.sampleTime() == 10 && store.startDateTime() == rec.start, "load: sample time or start");
    check(!store.dirty(), "load: an import is not unsaved data");
    const Recording again = store.toRecording();
    check(again.values.size() == 4 && again.values[1] == 2.0 && std::isnan(again.values[2]) && again.start == rec.start,
          "load: exported as it came");

    TestClock clock;
    clock.attach(store);
    store.start();
    store.setReading(clock.at(0, reading(7, "7.000", "DC")));
    store.stop();
    check(store.dirty(), "write: recording should be dirty");
    QTemporaryDir dir;
    QString err;
    check(store.write(dir.path() + "/r.csv", &err), "write: " + err);
    check(!store.dirty(), "write: still dirty after writing");
    const Recording back = store.toRecording();
    check(back.unit == "V" && back.values.size() == 1 && back.values.first() == 7, "write: recording content");
  }

  // --- 5a0. no number shown, a fault word: gaps of their own kind, written
  //          as none and fault, read back; neither ends the recording ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setUnit("V");
    store.setSampleTime(10);
    store.start();
    store.setReading(clock.at(100, reading(1.5, "1.500", "DC")));
    store.setReading(clock.at(600, noNumber("----", Quality::NoValue)));
    store.setReading(clock.at(1100, noNumber("FUSE", Quality::Fault)));
    store.setReading(clock.at(1600, reading(2.5, "2.500", "DC")));
    check(store.isRunning() && store.count() == 4, "no number: the recording goes on");
    check(store.series().at(1).gap() && store.series().at(1).quality == Quality::NoValue
            && store.series().at(2).quality == Quality::Fault,
          "no number: gaps with their quality");
    clock.now = 2000;
    store.stop();
    QTemporaryDir dir;
    QString err;
    const QString file = dir.path() + "/none.csv";
    check(store.write(file, &err, true), "no number: write: " + err);
    QFile f(file);
    check(f.open(QIODevice::ReadOnly | QIODevice::Text), "no number: cannot open " + file);
    const QString text = QString::fromUtf8(f.readAll());
    check(text.contains(";nan;V;none\n") && text.contains(";nan;V;fault\n"), "no number: the file is\n" + text);
    const std::optional<Recording> rec = RecordingFile::read(file, &err);
    RecordingStore back;
    if (rec)
      back.load(*rec);
    check(back.count() == 4 && back.series().at(1).quality == Quality::NoValue
            && back.series().at(2).quality == Quality::Fault,
          "no number: read back with its kind");
    const QList<GridPoint> grid = back.grid(5);
    check(grid.size() == 4 && grid[1].quality == Quality::NoValue && std::isnan(grid[1].value)
            && grid[2].quality == Quality::Fault && grid[3].value == 2.5,
          "no number: the grid's steps keep their kind");
  }

  // --- 5a. the export of every reading: each at its time, a gap a row of
  //         its own; it imports with its times, and exports again the same;
  //         the grid of an imported one holds each value to the next ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setUnit("V");
    store.setSampleTime(10);
    store.start();
    store.setReading(clock.at(130, reading(1.5, "1.500", "DC")));
    store.setReading(clock.at(610, reading(2.5, "2.500", "DC")));
    store.setReading(clock.at(1090, reading(0, "OL", "DC")));
    store.setReading(clock.at(1570, reading(0.004, "4.000", "DC")));
    clock.now = 2000;
    store.stop();
    QTemporaryDir dir;
    const QString raw1 = dir.path() + "/raw1.csv", raw2 = dir.path() + "/raw2.csv";
    QString err;
    check(store.write(raw1, &err, true), "raw: write: " + err);
    QFile f(raw1);
    check(f.open(QIODevice::ReadOnly | QIODevice::Text), "raw: cannot open file: " + raw1);
    const QString text = QString::fromUtf8(f.readAll());
    check(text == "timestamp;time (s);value;unit;status\n"
                  "2026-10-04T12:00:00,130;0;1.5;V;ok\n"
                  "2026-10-04T12:00:00,610;0.48;2.5;V;ok\n"
                  "2026-10-04T12:00:01,090;0.96;nan;V;OL\n"
                  "2026-10-04T12:00:01,570;1.44;4;mV;ok\n",
          "raw: the file is\n" + text);

    const std::optional<Recording> rec = RecordingFile::read(raw1, &err);
    check(rec && rec->times.size() == 4 && rec->times[3] == 1440 && !rec->onGrid(), "raw: read with its times");
    RecordingStore back;
    if (rec)
      back.load(*rec);
    check(back.count() == 4 && back.series().at(1).t == 480 && back.series().at(2).gap(), "raw: loaded at its times");
    check(back.write(raw2, &err, true), "raw: write again: " + err);
    QFile f2(raw2);
    check(f2.open(QIODevice::ReadOnly | QIODevice::Text), "raw: cannot open file: " + raw2);
    check(QString::fromUtf8(f2.readAll()) == text, "raw: export, import, export is not the same file");
    // on the grid of 0.5 s: the value at the start, then each step's mean
    QStringList g;
    for (const GridPoint &p : back.grid(5))
      g << QString::number(p.value);
    check(g.join(' ') == "1.5 1.54 2.5", "raw: grid of the import got " + g.join(' '));

    // what a QtDMM recording says of its devices goes along when it is saved
    // again, a new recording drops it; the writer's own keys are its own
    if (rec)
    {
      Recording withInfo = *rec;
      withInfo.meta = QJsonObject{ { "devices", QJsonArray{ QJsonObject{ { "model", "UNI-T UT61E" } } } },
                                   { "written", "2020-01-01T00:00:00" } };
      RecordingStore file;
      file.load(withInfo);
      const Recording again = file.toRecording(true);
      check(again.meta.value("devices").toArray().first().toObject().value("model").toString() == "UNI-T UT61E"
              && !again.meta.contains("written"),
            "meta: the devices of a loaded file go along, not what the writer adds");
      const Recording over = file.toRecording(true, {}, {}, {}, QJsonObject{ { "devices", QJsonArray() } });
      check(over.meta.value("devices").toArray().isEmpty(), "meta: info given replaces the file's");
      file.clear();
      check(file.toRecording().meta.isEmpty(), "meta: a new recording has none of the file's");
    }

    // an old export on its grid stays one: the gaps where they were
    const std::optional<Recording> legacy = RecordingFile::read(QString(argc > 1 ? argv[1] : "") + "/legacy_nan.txt", &err);
    check(legacy.has_value(), "legacy: read: " + err);
    if (legacy)
    {
      RecordingStore old;
      old.load(*legacy);
      const Recording out = old.toRecording();
      bool same = out.values.size() == legacy->values.size();
      for (int i = 0; same && i < out.values.size(); ++i)
        same = (std::isnan(out.values[i]) && std::isnan(legacy->values[i])) || out.values[i] == legacy->values[i];
      check(same, "legacy: the export of an old file has its values and gaps");
    }
  }

  // --- 5a1. the limit test's verdict goes with the readings into the
  //          export; a step with a PASS and a FAIL HI is FAIL HI ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setUnit("V");
    store.setSampleTime(10);
    store.start();
    auto judged = [](Reading r, quint32 verdict) { r.flags |= verdict; return r; };
    store.setReading(clock.at(0, judged(reading(5.0, "5.000", "DC"), SampleFlag::LimitPass)));
    store.setReading(clock.at(400, judged(reading(5.2, "5.200", "DC"), SampleFlag::LimitHigh)));
    store.setReading(clock.at(900, judged(reading(5.0, "5.000", "DC"), SampleFlag::LimitPass)));
    clock.now = 2000;
    store.stop();
    const Recording grid = store.toRecording();
    check(grid.limits.size() == 3 && grid.limits[0] == SampleFlag::LimitPass
          && (grid.limits[1] & SampleFlag::LimitHigh) && grid.limits[2] == SampleFlag::LimitPass,
          "limit: on the grid, the FAIL of a step stays");
    QTemporaryDir dir;
    const QString raw = dir.path() + "/limit_raw.csv";
    QString err;
    check(store.write(raw, &err, true), "limit: raw write: " + err);
    QFile f(raw);
    const QStringList lines = f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()).split('\n') : QStringList();
    check(lines.value(0).endsWith(";status;limit") && lines.value(2).endsWith(";ok;FAIL HI"), "limit: raw rows: " + lines.join(" | "));
    RecordingStore back;
    if (const auto rec = RecordingFile::read(raw, &err))
      back.load(*rec);
    check(back.count() == 3 && (back.series().at(1).flags & SampleFlag::LimitHigh), "limit: loaded with the verdicts");
  }

  // --- 5a1b. what the meter said about a reading (HOLD, REL, MAX) goes
  //           into the file too, in a flags column of its own beside the
  //           verdict, and comes back with the readings ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setUnit("V");
    store.setSampleTime(10);
    store.start();
    auto flagged = [](Reading r, quint32 flags) { r.flags |= flags; return r; };
    store.setReading(clock.at(0, flagged(reading(5.0, "5.000", "DC"), SampleFlag::LimitPass)));
    store.setReading(clock.at(400, flagged(reading(5.0, "5.000", "DC"), SampleFlag::LimitPass | SampleFlag::Hold)));
    store.setReading(clock.at(900, flagged(reading(0.2, "0.200", "DC"), SampleFlag::Relative | SampleFlag::Max)));
    clock.now = 2000;
    store.stop();
    QTemporaryDir dir;
    const QString raw = dir.path() + "/flags_raw.csv";
    QString err;
    check(store.write(raw, &err, true), "flags: raw write: " + err);
    QFile f(raw);
    const QStringList lines = f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()).split('\n') : QStringList();
    check(lines.value(0).endsWith(";status;limit;flags") && lines.value(1).endsWith(";ok;PASS;") && lines.value(2).endsWith(";ok;PASS;HOLD")
            && lines.value(3).endsWith(";ok;;MAX REL"),
          "flags: raw rows: " + lines.join(" | "));
    RecordingStore back;
    if (const auto rec = RecordingFile::read(raw, &err))
      back.load(*rec);
    check(back.count() == 3 && back.series().at(0).flags == SampleFlag::LimitPass
            && back.series().at(1).flags == (SampleFlag::LimitPass | SampleFlag::Hold)
            && back.series().at(2).flags == (SampleFlag::Relative | SampleFlag::Max),
          "flags: loaded with verdicts and states: " + err);
    // on the grid a state in a step stays, as a FAIL does
    const Recording grid = store.toRecording();
    check(grid.limits.size() == 3 && (grid.limits[1] & SampleFlag::Hold) && (grid.limits[2] & SampleFlag::Relative),
          "flags: on the grid, the states of a step stay");
    // without a verdict: the flags column alone; a file without states has none
    RecordingStore plain;
    TestClock clock2;
    clock2.attach(plain);
    plain.setUnit("V");
    plain.start();
    plain.setReading(clock2.at(0, flagged(reading(1.0, "1.000", "DC"), SampleFlag::Hold)));
    clock2.now = 1000;
    plain.stop();
    const QString only = dir.path() + "/flags_only.csv";
    check(plain.write(only, &err, true), "flags: write without a verdict: " + err);
    QFile g(only);
    const QString head = g.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(g.readAll()).section('\n', 0, 0) : QString();
    check(head.endsWith(";status;flags"), "flags: alone after the status: " + head);
    RecordingStore back2;
    if (const auto rec = RecordingFile::read(only, &err))
      back2.load(*rec);
    check(back2.count() == 1 && back2.series().at(0).flags == SampleFlag::Hold, "flags: loaded from the flags column alone: " + err);
  }

  // --- 5a1c. why a gap is one goes with it: the connection lost, the port
  //           released on purpose; known later, the gap in hand gets it;
  //           the file keeps it ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setUnit("V");
    store.start();
    store.setReading(clock.at(0, reading(1.0, "1.000", "DC")));
    clock.now = 1000;
    store.setStale(true, SampleFlag::GapLost);
    store.setReading(clock.at(2000, reading(1.0, "1.000", "DC")));
    clock.now = 3000;
    store.setStale(true);
    store.setGapCause(SampleFlag::GapPause);
    store.setReading(clock.at(4000, reading(1.0, "1.000", "DC")));
    store.setGapCause(SampleFlag::GapLost);   // no gap in hand: nothing
    clock.now = 5000;
    store.stop();
    const RecordingSeries &s = store.series();
    check(s.count() == 5 && s.at(1).gap() && s.at(1).flags == SampleFlag::GapLost && s.at(3).gap()
            && s.at(3).flags == SampleFlag::GapPause && !(s.at(4).flags & (SampleFlag::GapLost | SampleFlag::GapPause)),
          QString("gap cause: lost and paused on their gaps, %1 points, flags %2 %3 %4").arg(s.count())
            .arg(s.count() > 1 ? s.at(1).flags : 0).arg(s.count() > 3 ? s.at(3).flags : 0).arg(s.count() > 4 ? s.at(4).flags : 0));
    QTemporaryDir dir;
    const QString raw = dir.path() + "/gap_cause.csv";
    QString err;
    check(store.write(raw, &err, true), "gap cause: write: " + err);
    QFile f(raw);
    const QStringList lines = f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()).split('\n') : QStringList();
    check(lines.value(0).endsWith(";status;flags") && lines.value(2).endsWith(";gap;LOST") && lines.value(4).endsWith(";gap;PAUSE"),
          "gap cause: in the file: " + lines.join(" | "));
    RecordingStore back;
    if (const auto rec = RecordingFile::read(raw, &err))
      back.load(*rec);
    check(back.count() == 5 && back.series().at(1).flags == SampleFlag::GapLost && back.series().at(3).flags == SampleFlag::GapPause,
          "gap cause: loaded again: " + err);
  }

  // --- 5a2. the other values in the export: a column group each, on the
  //          grid and raw; empty before the value began; the status tells
  //          OL, -OL and a gap apart; import and export again the same ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setUnit("V");
    store.setSampleTime(10);
    store.start();
    Reading negative = reading(0, "-OL", "DC");
    negative.value = std::copysign(qQNaN(), -1.0);   // as ReadingAdapter makes it
    store.setReading(clock.at(100, reading(1.0, "1.000", "DC")));
    store.setReading(clock.at(600, negative));
    store.setReading(clock.at(1200, reading(2.0, "2.000", "DC")));
    store.setReading(clock.at(1200, other(1, 50, "50.00", "Hz")));
    store.setReading(clock.at(1700, reading(2.0, "2.000", "DC")));
    store.setReading(clock.at(1700, other(1, 0, "OL", "Hz")));
    clock.now = 2500;
    store.stop();
    check(store.extraCount() == 1, QString("extras: one other value, got %1").arg(store.extraCount()));
    QTemporaryDir dir;
    auto fileText = [](const QString &path)
    {
      QFile f(path);
      return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()) : QString();
    };
    QString err;
    const QString raw1 = dir.path() + "/raw1.csv", raw2 = dir.path() + "/raw2.csv";
    check(store.write(raw1, &err, true, { 0 }), "extras raw: write: " + err);
    const QString raw = fileText(raw1);
    check(raw == "timestamp;time (s);value;unit;status;value 2;unit 2;status 2\n"
                 "2026-10-04T12:00:00,100;0;1;V;ok;;;\n"
                 "2026-10-04T12:00:00,600;0.5;nan;V;-OL;;;\n"
                 "2026-10-04T12:00:01,200;1.1;2;V;ok;50;Hz;ok\n"
                 "2026-10-04T12:00:01,700;1.6;2;V;ok;nan;Hz;OL\n",
          "extras raw: the file is\n" + raw);
    // the grid (from the first reading): the step that ends before the
    // value came has none of it; a step with a value and -OL has its mean
    const QString grid1 = dir.path() + "/grid1.csv", grid2 = dir.path() + "/grid2.csv";
    check(store.write(grid1, &err, false, { 0 }), "extras grid: write: " + err);
    const QString grid = fileText(grid1);
    check(grid == "timestamp;time (s);value;unit;status;value 2;unit 2;status 2\n"
                  "2026-10-04T12:00:00,100;0;1;V;ok;;;\n"
                  "2026-10-04T12:00:01,100;1;1;V;ok;;;\n"
                  "2026-10-04T12:00:02,100;2;2;V;ok;50;Hz;ok\n",
          "extras grid: the file is\n" + grid);
    // without the other values: the main value alone, as before
    const QString main1 = dir.path() + "/main.csv";
    check(store.write(main1, &err, true) && fileText(main1).startsWith("timestamp;time (s);value;unit;status\n")
            && !fileText(main1).contains("Hz"),
          "extras: not asked for, not written");

    const std::optional<Recording> rec = RecordingFile::read(raw1, &err);
    check(rec && rec->extra.size() == 1 && rec->extra[0].unit == "Hz" && rec->extra[0].states.size() == 4
            && rec->extra[0].states[0] == CellState::None && rec->extra[0].states[3] == CellState::Overload,
          "extras raw: read back with its states: " + err);
    check(rec && rec->states.value(1) == CellState::NegativeOverload && std::signbit(rec->values.value(1)),
          "extras raw: -OL read back, its NaN negative");
    RecordingStore back;
    if (rec)
      back.load(*rec);
    check(back.extraCount() == 1 && back.extra(0).count() == 2 && back.extra(0).id == 1 && back.extra(0).first().t == 1100
            && back.extra(0).last().quality == Quality::Overload,
          "extras raw: loaded as a series from its first row");
    check(back.series().at(1).quality == Quality::Overload && std::signbit(back.series().at(1).value),
          "extras raw: the main value's -OL loaded");
    check(back.write(raw2, &err, true, { 0 }) && fileText(raw2) == raw, "extras raw: export, import, export the same\n" + fileText(raw2));
    // the grid file on its own grid exports as it came
    const std::optional<Recording> g = RecordingFile::read(grid1, &err);
    RecordingStore backGrid;
    if (g)
      backGrid.load(*g);
    check(g && backGrid.write(grid2, &err, false, { 0 }) && fileText(grid2) == grid,
          "extras grid: export, import, export the same\n" + fileText(grid2));
  }

  // --- 5b. gaps and the integral: over time (V s) for the values above
  //         the threshold; a gap adds nothing, the integral carries over
  //         it; the start triggers compare with the last value there was ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setIntegrationThreshold(0);
    store.start();
    store.setReading(clock.at(0, reading(2, "2.000", "DC")));
    store.setReading(clock.at(1000, reading(4, "4.000", "DC")));
    check(store.series().last().integral == 2, QString("integral: 2 V for 1 s, got %1").arg(store.series().last().integral));
    store.setReading(clock.at(1500, reading(0, "OL", "DC")));
    check(std::isnan(store.series().last().integral), "integral: a gap has none");
    store.setReading(clock.at(3000, reading(6, "6.000", "DC")));
    check(store.series().last().integral == 4, QString("integral: carried over the gap, 2 + 4 x 0.5, got %1")
                                                  .arg(store.series().last().integral));
    store.setReading(clock.at(4000, reading(-1, "-1.000", "DC")));
    check(store.series().last().integral == 0, "integral: at or below the threshold it is 0");

    RecordingStore trig;
    TestClock tc;
    tc.attach(trig);
    trig.setStartMode(RecordingStore::Raising);
    trig.setThresholds(0, 1);
    trig.live();
    trig.setReading(tc.at(0, reading(0, "0.000", "DC")));
    trig.setReading(tc.at(500, reading(0, "OL", "DC")));
    check(!trig.isRunning(), "gap: an OL crosses no threshold");
    trig.setReading(tc.at(1000, reading(2, "2.000", "DC")));
    check(trig.isRunning(), "gap: 0, OL, 2 crosses 1");
    check(trig.count() == 1 && trig.series().first().value == 2 && trig.series().first().t == 0,
          "trigger: the reading that crossed is the first point");
  }

  // --- 5c. one recording, one function: another port (V DC -> Ohm, DC ->
  //          AC) or unit (°C -> °F) stops it and says so; a prefix (mV ->
  //          V), an overload or hold do not. The samples keep their unit ---
  {
    auto rd = [](const QString &text, const QString &unit, const QString &special)
    {
      static ReadingAdapter adapter;
      return ReadingAdapter::reading(adapter.adaptValue(text.toDouble(), text, unit, special, "AUTO", false, true,
                                                        false, 0, QDateTime::currentMSecsSinceEpoch()));
    };
    RecordingStore store;
    store.setUnit("V");
    QSignalSpy changed(&store, &RecordingStore::functionChanged);
    store.start();
    store.setReading(rd("12.34", "mV", "DC"));
    store.setReading(rd("1.234", "V", "DC"));   // a prefix
    store.setReading(rd("OL", "V", "DC"));
    store.setReading(rd("OL", "", ""));         // an overload without a unit
    check(store.isRunning() && changed.isEmpty(), "function: mV -> V, OL: the recording goes on");

    store.setUnit("Ohm");                       // MeterController: unitChanged before the reading
    check(store.unit() == "V", "function: the samples keep their unit");
    store.setReading(rd("4.700", "kOhm", "OH"));
    check(!store.isRunning() && changed.size() == 1, "function: V DC -> Ohm stops the recording");
    if (!changed.isEmpty())
      check(changed[0][0].toString() == "Voltage DC (V)" && changed[0][1].toString() == "Resistance (Ω)",
            "function: says what changed, got " + changed[0][0].toString() + " -> " + changed[0][1].toString());
    check(store.count() == 4 && store.unit() == "V" && store.toRecording().unit == "V",
          QString("function: the stopped recording (%1 points) and its export stay in V").arg(store.count()));
    store.start();
    check(store.unit() == "Ohm", "function: the next recording is in Ohm");

    store.setUnit("V");
    store.start();
    store.setReading(rd("1.000", "V", "DC"));
    store.setReading(rd("1.000", "V", "AC"));
    check(!store.isRunning() && changed.size() == 2, "function: DC -> AC is another function");

    store.setUnit("C");
    store.start();
    store.setReading(rd("21.5", "C", "TE"));
    store.setReading(rd("70.7", "dF", "TE"));
    check(!store.isRunning() && changed.size() == 3, "function: °C -> °F (one port, two units) stops it");

    store.start();
    store.setReading(rd("21.5", "C", "TE"));
    store.stop();
    store.setReading(rd("1.000", "V", "DC"));
    check(changed.size() == 3, "function: no recording, nothing to stop");
  }

  // --- 5d. waiting for a trigger: another function is no crossing (0 V, then
  //          1000 Ohm over a threshold of 5), and the pre-trigger readings of
  //          the function before go ---
  {
    auto rd = [](const QString &text, const QString &unit, const QString &special)
    {
      static ReadingAdapter adapter;
      return ReadingAdapter::reading(adapter.adaptValue(text.toDouble(), text, unit, special, "AUTO", false, true,
                                                        false, 0, QDateTime::currentMSecsSinceEpoch()));
    };
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setStartMode(RecordingStore::Raising);
    store.setThresholds(0, 5);
    store.setPreTrigger(2000);
    store.live();
    store.setReading(clock.at(0, rd("0.000", "V", "DC")));
    store.setReading(clock.at(500, rd("1000", "Ohm", "OH")));
    check(!store.isRunning(), "trigger: V -> Ohm crosses nothing");
    store.setReading(clock.at(1000, rd("2", "Ohm", "OH")));
    store.setReading(clock.at(1500, rd("6", "Ohm", "OH")));   // a crossing in Ohm
    check(store.isRunning(), "trigger: a crossing in the new function starts it");
    QStringList v;
    for (int i = 0; i < store.count(); ++i)
      v << QString::number(store.series().at(i).value);
    check(v.join(' ') == "1000 2 6", "trigger: only Ohm before the trigger, got " + v.join(' '));
  }

  // --- 2. the readings series: every reading of every value, whether or
  //        not a recording runs, with its own capacity and pause ---
  {
    RecordingStore store;
    int inserted = 0, removed = 0, cleared = 0;
    QObject::connect(&store, &RecordingStore::readingsInserted, [&] { ++inserted; });
    QObject::connect(&store, &RecordingStore::readingsRemoved, [&] { ++removed; });
    QObject::connect(&store, &RecordingStore::readingsCleared, [&] { ++cleared; });

    Reading second = reading(50, "50.00", "AC", false, "MANU");
    second.id = 1;
    store.setReading(reading(0.01234, "12.34", "DC", true));
    store.setReading(second);
    store.setReading(reading(0, "OL", "DC"));
    check(!store.isRunning() && store.count() == 0, "readings: no recording must be needed");
    check(store.readingCount() == 3 && inserted == 3,
          QString("readings: expected 3 rows, got %1").arg(store.readingCount()));
    const LoggedReading &first = store.readingAt(0);
    check(first.text == "12.34" && first.unit == "mV" && first.port.toString() == "voltage.dc" && first.range == "AUTO" &&
          first.hold() && first.value == 0.01234 && first.id == 0,
          "readings: the full tuple of the first row");
    check(store.readingAt(1).id == 1 && store.readingAt(1).range == "MANU", "readings: a secondary value is a row too");
    check(store.readingAt(2).quality == Quality::Overload, "readings: OL is quality Overload");

    // pause: the readings series stops, the recording does not
    store.setReadingsPaused(true);
    store.start();
    store.setReading(reading(1, "1.000", "DC"));
    check(store.readingCount() == 3, "readings: a paused series takes nothing");
    check(store.count() == 2, "readings: pausing the series must not stop the recording (start value and 1 V)");
    store.setReadingsPaused(false);
    // and the other way round: a stopped recording does not stop the series
    store.stop();
    store.setReading(reading(2, "2.000", "DC"));
    check(store.readingCount() == 4, "readings: a stopped recording must not stop the series");

    // capacity: the oldest go, one removal per change
    store.setReadingCapacity(3);
    check(store.readingCount() == 3 && removed == 1 && store.readingAt(0).id == 1,
          "readings: shrinking drops the oldest");
    store.setReading(reading(3, "3.000", "DC"));
    check(store.readingCount() == 3 && removed == 2 && store.readingAt(2).text == "3.000",
          "readings: a full series drops one for each new reading");
    store.setReadingCapacity(0);
    check(store.readingCapacity() == 1 && store.readingCount() == 1, "readings: capacity is at least 1");

    // marks
    store.markLastReading(0xffd82222u, "Alarm 1");
    check(store.readingAt(0).alarmArgb == 0xffd82222u && store.readingAt(0).alarmName == "Alarm 1",
          "readings: the newest row gets the mark");

    // clear: the recording is left alone
    store.setReadingCapacity(100);
    store.clearReadings();
    check(store.readingCount() == 0 && cleared == 1, "readings: clear");
    check(store.count() == 2, "readings: clearing the series must not clear the recording");
    store.clearReadings();
    check(cleared == 1, "readings: clearing an empty series says nothing");
    // logReading() feeds the series only, not the recording
    store.logReading(reading(5, "5.000", "AC"));
    check(store.readingCount() == 1, "readings: logReading");
  }

  // --- 6. the states: View at first; Live runs through the live window and
  //        is never dirty; a recording starts from it cleared and ends in
  //        View; the triggers wait in Live only ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    QList<RecordingStore::State> states;
    QObject::connect(&store, &RecordingStore::stateChanged, [&](RecordingStore::State st) { states << st; });
    QSignalSpy running(&store, &RecordingStore::runningChanged);
    check(store.state() == RecordingStore::View, "states: a new store views");
    store.setReading(clock.at(0, reading(1, "1", "DC")));
    check(store.count() == 0, "states: View records nothing");

    // without a recording length Live keeps the last hour, not only the
    // window: 1 min for a while and back to All still has the start
    {
      RecordingStore kept;
      TestClock c;
      c.attach(kept);
      kept.setLiveWidth(120);
      kept.live();
      for (int i = 0; i <= 120; ++i)   // 2 min at 1 Hz
        kept.setReading(c.at(i * 1000, reading(i, QString::number(i), "DC")));
      kept.setLiveWidth(60);            // 1 min ...
      for (int i = 121; i <= 300; ++i)  // ... for 3 min
        kept.setReading(c.at(i * 1000, reading(i, QString::number(i), "DC")));
      kept.setLiveWidth(300);           // All
      check(kept.liveKept() == RecordingStore::kLiveMaxMs && kept.liveWindow() == 300000,
            QString("live kept: an hour, the window %1 ms").arg(kept.liveWindow()));
      check(kept.origin() == 0 && kept.series().first().t == 0 && kept.count() == 301,
            QString("live kept: from 0 on after 1 min, origin %1, first %2, %3 readings")
              .arg(kept.origin()).arg(kept.series().first().t).arg(kept.count()));
      for (int i = 301; i <= 3900; ++i) // past the hour
        kept.setReading(c.at(i * 1000, reading(i, QString::number(i), "DC")));
      const qint64 slack = RecordingStore::kLiveMaxMs / 20 + 1000;
      check(kept.origin() == 3900000 - RecordingStore::kLiveMaxMs && kept.series().first().t >= kept.origin() - slack,
            QString("live kept: the last hour, origin %1, first %2").arg(kept.origin()).arg(kept.series().first().t));
    }

    // stats between two times: each value for as long as it held, a gap
    // counts nothing, the last value up to now
    {
      RecordingStore st;
      TestClock c;
      c.attach(st);
      st.setLiveWidth(120);
      st.live();
      st.setReading(c.at(0, reading(2, "2", "DC")));
      st.setReading(c.at(1000, reading(4, "4", "DC")));
      st.setReading(c.at(2000, reading(0, "OL", "DC")));
      st.setReading(c.at(3000, reading(-1, "-1", "DC")));
      c.now = 5000;
      auto text = [](const RecordingStore::Stats &s)
      {
        return QString("%1 readings, %2..%3, mean %4, integral %5, %6 ms")
          .arg(s.count).arg(s.min).arg(s.max).arg(s.mean).arg(s.integral).arg(s.covered);
      };
      RecordingStore::Stats s = st.stats(0, 5000);
      check(s.count == 3 && s.min == -1 && s.max == 4 && s.integral == 4 && s.covered == 4000 && s.mean == 1,
            "stats over all: 3 readings, -1..4, mean 1, integral 4, 4000 ms; got " + text(s));
      s = st.stats(1500, 500);
      check(s.count == 2 && s.min == 2 && s.max == 4 && s.mean == 3 && s.integral == 3 && s.covered == 1000,
            "stats across a reading, either order: 2 and 4 half each; got " + text(s));
      s = st.stats(2100, 2900);
      check(s.count == 0 && std::isnan(s.mean) && std::isnan(s.min) && s.covered == 0 && s.integral == 0,
            "stats in a gap: nothing; got " + text(s));
      s = st.stats(4000, 9000);
      check(s.count == 1 && s.mean == -1 && s.covered == 1000, "stats past now: up to now; got " + text(s));
      s = st.stats(2500, 2500);
      check(s.count == 0 && s.covered == 0, "stats of no time: nothing; got " + text(s));
      // σ weighted by time: 2 and 4 for 1 s, -1 for 2 s around the mean 1: √4.5
      s = st.stats(0, 5000);
      check(std::abs(s.sigma - std::sqrt(4.5)) < 1e-12, QString("stats: σ √4.5, got %1").arg(s.sigma));
      s = st.stats(1500, 500);
      check(std::abs(s.sigma - 1) < 1e-12, QString("stats: σ of 2 and 4 half each is 1, got %1").arg(s.sigma));
    }
    // a steady large value has no spread, not a rounding error's
    {
      RecordingStore st;
      TestClock c;
      c.attach(st);
      st.setLiveWidth(120);
      st.live();
      for (int i = 0; i < 100; ++i)
        st.setReading(c.at(i * 100, reading(1000.001, "1000.001", "DC")));
      c.now = 10000;
      const RecordingStore::Stats s = st.stats(0, 10000);
      check(s.sigma == 0 && std::abs(s.mean - 1000.001) < 1e-9, QString("stats: steady 1000.001: σ 0, got %1").arg(s.sigma));
    }

    // stats of a loaded file: the last reading counts although nothing comes
    // after it; a file on its own grid weighs each row over the step before it
    {
      auto text = [](const RecordingStore::Stats &s)
      {
        return QString("%1 readings, %2..%3, mean %4, %5 ms").arg(s.count).arg(s.min).arg(s.max).arg(s.mean).arg(s.covered);
      };
      Recording file;
      file.start = TestClock::wall(0);
      file.sampleTimeTenths = 10;
      file.values = { 1, 2, 3, 10 };
      RecordingStore st;
      st.load(file);
      RecordingStore::Stats s = st.stats(0, st.duration());
      check(s.count == 4 && s.min == 1 && s.max == 10 && s.mean == 5 && s.covered == 3000,
            "loaded grid: 4 rows, 1..10, mean 5 (2, 3, 10 a step each); got " + text(s));
      s = st.stats(0, 1000000);
      check(s.count == 4 && s.max == 10 && s.mean == 5, "loaded grid, cursor past the end; got " + text(s));
      s = st.stats(2500, 3000);
      check(s.count == 1 && s.min == 10 && s.mean == 10 && s.covered == 500, "loaded grid: the last step; got " + text(s));

      file.times = { 0, 700, 1900, 2500 };
      st.load(file);
      s = st.stats(0, st.duration());
      check(s.count == 4 && s.max == 10 && s.covered == 2500 && qAbs(s.mean - 1.96) < 1e-9,
            "loaded readings: 4, the last up to 10, mean over time 1.96; got " + text(s));

      file.times.clear();
      file.values = { 7 };
      st.load(file);
      s = st.stats(0, st.duration());
      check(s.count == 1 && s.min == 7 && s.max == 7 && s.mean == 7, "a file of one row; got " + text(s));
    }

    // the live window: the recording length, or the graph's window
    store.setLiveWidth(60);
    store.setSampleLength(20);   // 2 s
    check(store.liveWindow() == 2000, QString("live window: %1 ms, expected the length").arg(store.liveWindow()));
    store.setPreTrigger(5000);
    check(store.liveWindow() == 5000, "live window: at least the pre-trigger");
    store.setPreTrigger(0);
    store.setSampleLength(0);
    check(store.liveWindow() == 60000, "live window: until stopped - the graph's window");
    store.setLiveWidth(7200);
    check(store.liveWindow() == RecordingStore::kLiveMaxMs, "live window: until stopped - at most an hour");
    store.setLiveWidth(60);
    store.setSampleLength(20);

    clock.now = 100;
    store.live();
    check(store.state() == RecordingStore::Live && states == QList<RecordingStore::State>{ RecordingStore::Live },
          "live: Live, said once");
    check(running.isEmpty() && !store.isRunning(), "live: is no recording");
    check(store.count() == 1 && store.series().first().value == 1, "live: starts with the value shown");
    for (int i = 1; i <= 100; ++i)   // 10 s
      store.setReading(clock.at(100 + i * 100, reading(i, QString::number(i), "DC")));
    check(!store.dirty(), "live: nothing unsaved");
    check(store.duration() == 10000 && store.origin() == 8000,
          QString("live: covers %1 ms from %2 on").arg(store.duration()).arg(store.origin()));
    check(store.series().first().t <= 8000 && store.series().first().t >= 8000 - 1100,
          QString("live: keeps from %1 ms, the window and a step of slack").arg(store.series().first().t));
    check(store.remainingLength() == 0, "live: no length left to show");

    // saved while live: the window up to now, and still live
    const Recording rec = store.toRecording(true);
    check(!rec.values.isEmpty() && rec.values.last() == 100 && rec.start == TestClock::wall(100 + 8000),
          QString("live export: %1 values from %2").arg(rec.values.size()).arg(rec.start.toString("hh:mm:ss.zzz")));
    QTemporaryDir dir;
    check(store.write(dir.filePath("live.csv")) && store.state() == RecordingStore::Live,
          "live export: written, Live goes on");

    // another function in Live: the window starts anew, no recording stopped
    QSignalSpy changed(&store, &RecordingStore::functionChanged);
    static ReadingAdapter ohms;
    Reading ohm = ReadingAdapter::reading(ohms.adaptValue(1000, "1000", "Ohm", "OH", "AUTO", false, true, false, 0,
                                                          QDateTime::currentMSecsSinceEpoch()));
    store.setReading(clock.at(10200, ohm));
    check(store.state() == RecordingStore::Live && changed.isEmpty() && store.count() == 1
            && store.series().first().value == 1000,
          QString("live: another function starts the window anew, %1 points").arg(store.count()));

    // a recording starts cleared, from the value shown, and ends in View
    store.start();
    check(store.state() == RecordingStore::Record && store.isRunning() && running.size() == 1,
          "record: started");
    check(store.count() == 1 && store.series().first().t == 0, "record: cleared, the value shown first");
    store.setReading(clock.at(10700, ohm));
    check(store.dirty(), "record: unsaved");
    check(store.remainingLength() == 15, QString("record: %1 tenths left").arg(store.remainingLength()));
    store.setReading(clock.at(12300, ohm));   // past the length
    check(store.state() == RecordingStore::View && running.size() == 2 && store.dirty(),
          "record: the length ends it in View, unsaved");
    const int kept = store.count();
    store.setReading(clock.at(12500, ohm));
    check(store.count() == kept && store.duration() == 2000, "view: stands");
    store.stop();
    check(running.size() == 2, "view: stop says nothing");

    // back to Live: cleared, nothing unsaved
    store.live();
    check(store.state() == RecordingStore::Live && !store.dirty() && store.count() == 1 && store.marks().isEmpty(),
          "live again: cleared");
    check(states == QList<RecordingStore::State>({ RecordingStore::Live, RecordingStore::Record, RecordingStore::View,
                                                   RecordingStore::Live }),
          "states: Live, Record, View, Live");

    // stopped by hand: View, unsaved; a load views too
    store.start();
    store.setReading(clock.at(13000, ohm));
    store.stop();
    check(store.state() == RecordingStore::View && store.dirty(), "stop: View, unsaved");
    store.live();
    Recording file;
    file.start = TestClock::wall(0);
    file.sampleTimeTenths = 10;
    file.values = { 1, 2, 3 };
    store.load(file);
    check(store.state() == RecordingStore::View && !store.dirty() && store.count() == 3, "load: View");
  }

  // --- 6b. the triggers wait in Live, not in View: a recording viewed is
  //         not overwritten; the pre-trigger reaches into the live window ---
  {
    RecordingStore store;
    TestClock clock;
    clock.attach(store);
    store.setStartMode(RecordingStore::Raising);
    store.setThresholds(0, 5);
    store.setReading(clock.at(0, reading(1, "1", "DC")));
    store.setReading(clock.at(500, reading(6, "6", "DC")));
    check(!store.isRunning(), "trigger: not in View");
    store.setStartMode(RecordingStore::Time);
    store.setStartTime(TestClock::wall(1000).time());
    clock.now = 1000;
    store.poll();
    check(!store.isRunning(), "clock time: not in View");

    // Live: the crossing starts it; the pre-trigger takes the window's readings
    store.setStartMode(RecordingStore::Raising);
    store.setPreTrigger(1000);
    store.setSampleLength(100);   // 10 s
    clock.now = 2000;
    store.live();
    for (int i = 1; i <= 20; ++i)   // 2 s at 1 V
      store.setReading(clock.at(2000 + i * 100, reading(1, "1", "DC")));
    store.setReading(clock.at(4100, reading(6, "6", "DC")));
    check(store.isRunning() && store.marks().size() == 1 && store.marks().first().t == 1000,
          "pre-trigger from Live: started, the mark 1 s in");
    check(store.series().first().t == 0 && store.series().last().value == 6 && store.series().last().t == 1000,
          QString("pre-trigger from Live: %1 points, the crossing at %2 ms")
            .arg(store.count()).arg(store.series().last().t));
    check(store.startDateTime() == TestClock::wall(3100), "pre-trigger from Live: starts 1 s before the trigger");
    // the length counts from the trigger
    check(store.remainingLength() == 100, QString("pre-trigger from Live: %1 tenths left").arg(store.remainingLength()));
    // ended: no trigger until Live again
    store.stop();
    store.setReading(clock.at(5000, reading(1, "1", "DC")));
    store.setReading(clock.at(5100, reading(6, "6", "DC")));
    check(store.state() == RecordingStore::View, "trigger: not again in View after a recording");
  }

  // --- 7. one time base: a store started as another one was (startAt)
  //         counts from the same moment, from what its Live holds since then;
  //         the time does not move to the first reading ---
  {
    RecordingStore lead, follow;
    TestClock clock;
    clock.attach(lead);
    clock.attach(follow);
    clock.now = 1000;
    lead.live();
    follow.live();
    follow.setReading(clock.at(1200, reading(2, "2", "DC")));
    follow.setReading(clock.at(1400, reading(3, "3", "DC")));
    clock.now = 1300;
    lead.start();
    lead.holdOrigin();
    clock.now = 1500;
    follow.startAt(lead.startClock(), lead.startDateTime());
    check(follow.isRunning() && follow.startDateTime() == TestClock::wall(1300) && follow.count() == 2
            && follow.series().first().t == 0 && follow.series().last().t == 100,
          QString("startAt: %1 points from 1300 ms, the last at %2").arg(follow.count()).arg(follow.series().last().t));
    lead.setReading(clock.at(1800, reading(5, "5", "DC")));
    follow.setReading(clock.at(1800, reading(4, "4", "DC")));
    check(lead.series().last().t == 500 && follow.series().last().t == 500 && lead.startDateTime() == TestClock::wall(1300),
          QString("one time base: %1 and %2 ms").arg(lead.series().last().t).arg(follow.series().last().t));
    // started without a fresh reading: the reading that held, as start() has it
    RecordingStore late;
    clock.attach(late);
    late.live();
    clock.now = 2000;
    late.startAt(1300, TestClock::wall(1300));
    late.setReading(clock.at(2100, reading(1, "1", "DC")));
    check(late.count() == 1 && late.series().first().t == 800, QString("startAt, no reading yet: the first at %1 ms")
                                                                   .arg(late.count() ? late.series().first().t : -1));
    // not Live, the start further back (the pre-trigger): the reading
    // shown at the time it came, not at the start
    RecordingStore view;
    clock.attach(view);
    clock.now = 3000;
    view.setReading(clock.at(3000, reading(6, "6", "DC")));
    clock.now = 3100;
    view.startAt(1000, TestClock::wall(1000));
    check(view.count() == 1 && view.series().first().t == 2000,
          QString("startAt from View: the reading at %1 ms").arg(view.count() ? view.series().first().t : -1));
    // a store alone: the time begins with the first reading
    RecordingStore alone;
    clock.attach(alone);
    clock.now = 4000;
    alone.start();
    alone.setReading(clock.at(4500, reading(1, "1", "DC")));
    check(alone.series().first().t == 0 && alone.startClock() == 4500,
          QString("alone: the first reading at %1 ms, from %2").arg(alone.series().first().t).arg(alone.startClock()));
  }

  // --- 7a. another meter's values in the export (one time base): a column
  //          group each after its name, empty before it began; read back
  //          as its own part, loaded and exported again the same ---
  {
    RecordingStore lead, follow;
    TestClock clock;
    clock.attach(lead);
    clock.attach(follow);
    lead.setUnit("V");
    follow.setUnit("A");
    lead.setSampleTime(10);
    follow.setSampleTime(10);
    clock.now = 1000;
    lead.start();
    lead.holdOrigin();
    follow.startAt(lead.startClock(), lead.startDateTime());
    lead.setReading(clock.at(1000, reading(1, "1", "DC")));
    follow.setReading(clock.at(1500, reading(2, "2", "DC")));
    follow.setReading(clock.at(1500, other(1, 20, "20.0", "°C")));
    lead.setReading(clock.at(2000, reading(2, "2", "DC")));
    follow.setReading(clock.at(2500, reading(3, "3", "DC")));
    lead.setReading(clock.at(3000, reading(3, "3", "DC")));
    clock.now = 3200;
    lead.stop();
    follow.stop();
    const QList<RecordingStore::Foreign> sim{ { &follow, 0, "Sim B" }, { &follow, 1, "Sim B" } };
    QTemporaryDir dir;
    auto fileText = [](const QString &path)
    {
      QFile f(path);
      return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()) : QString();
    };
    QString err;
    const QString raw1 = dir.path() + "/raw1.csv", raw2 = dir.path() + "/raw2.csv";
    check(follow.dirty() && lead.write(raw1, &err, true, {}, sim) && !follow.dirty(),
          "another meter raw: written, its recording saved too: " + err);
    const QString raw = fileText(raw1);
    check(raw.startsWith("timestamp;time (s);value;unit;status;Sim B: value;Sim B: unit;Sim B: status;"
                         "Sim B: value 2;Sim B: unit 2;Sim B: status 2\n"
                         "2026-10-04T12:00:01,000;0;1;V;ok;;;;;;\n"
                         "2026-10-04T12:00:01,500;0.5;1;V;ok;2;A;ok;20;°C;ok\n"
                         "2026-10-04T12:00:02,000;1;2;V;ok;2;A;ok;20;°C;ok\n")
            && raw.count('\n') == 6,
          "another meter raw: the file is\n" + raw);
    const QString grid1 = dir.path() + "/grid1.csv";
    check(lead.write(grid1, &err, false, {}, sim), "another meter grid: written: " + err);
    const QString grid = fileText(grid1);
    check(grid.count('\n') == 4 && grid.section('\n', 1, 1) == "2026-10-04T12:00:01,000;0;1;V;ok;;;;;;"
            && grid.section('\n', 3, 3).startsWith("2026-10-04T12:00:03,000;2;2;V;ok;2.5;A;ok;20;°C;ok"),
          "another meter grid: empty before it began, then the mean over the step\n" + grid);

    const std::optional<Recording> rec = RecordingFile::read(raw1, &err);
    check(rec && RecordingFile::devices(*rec) == QStringList{ "Sim B" } && rec->extra.size() == 2
            && rec->extra[0].device == "Sim B" && rec->extra[0].id == 0 && rec->extra[1].id == 1,
          "another meter: read back by its name: " + err);
    if (rec)
    {
      const Recording own = RecordingFile::devicePart(*rec, QString());
      const Recording other = RecordingFile::devicePart(*rec, "Sim B");
      check(own.extra.isEmpty() && own.unit == "V" && other.unit == "A" && other.extra.size() == 1
              && other.extra[0].id == 1 && other.extra[0].device.isEmpty() && other.states.value(0) == CellState::None,
            "another meter: its part, the main value and value 2");
      RecordingStore back, backSim;
      back.load(own);
      backSim.load(other);
      check(backSim.count() == 4 && backSim.series().first().t == 500 && back.offsetOf(backSim) == 0,
            QString("another meter: loaded from its first row, %1 points").arg(backSim.count()));
      check(back.write(raw2, &err, true, {}, { { &backSim, 0, "Sim B" }, { &backSim, 1, "Sim B" } }) && fileText(raw2) == raw,
            "another meter: export, import, export the same\n" + fileText(raw2));
    }
    // this meter's columns after its name too (26.4): read back as its
    // part, the other meter apart
    {
      const QString named = dir.path() + "/named.csv";
      check(lead.write(named, &err, true, {}, { { &follow, 0, "Sim B" } }, "Sim A")
              && fileText(named).startsWith("timestamp;time (s);Sim A: value;Sim A: unit;Sim A: status;Sim B: value;"),
            "this meter after its name\n" + fileText(named).section('\n', 0, 0));
      const std::optional<Recording> rec = RecordingFile::read(named, &err);
      check(rec && rec->device == "Sim A" && RecordingFile::devices(*rec) == QStringList{ "Sim B" }
              && RecordingFile::devicePart(*rec, "Sim A").unit == "V" && RecordingFile::devicePart(*rec, "Sim B").unit == "A",
            "read back: the main value's meter, the other apart");
    }
    // a limit test on the other meter's value: its column after its name
    Recording limits;
    limits.start = TestClock::wall(0);
    limits.unit = "V";
    limits.values = { 1, 2 };
    RecordingColumn col;
    col.device = "Sim B";
    col.unit = "A";
    col.values = { 1, 2 };
    col.states = { CellState::Ok, CellState::Ok };
    col.limits = { LimitTest::flags(LimitTest::Pass), LimitTest::flags(LimitTest::High) };
    limits.extra << col;
    const QString lim = dir.path() + "/limit.csv";
    check(RecordingFile::write(limits, lim, &err) && fileText(lim).section('\n', 0, 0).endsWith(";Sim B: limit")
            && fileText(lim).section('\n', 2, 2).endsWith(";FAIL HI"),
          "another meter's limit column\n" + fileText(lim));
    const std::optional<Recording> limBack = RecordingFile::read(lim, &err);
    check(limBack && limBack->extra.size() == 1 && limBack->limits.isEmpty()
            && limBack->extra[0].limits.value(1) == LimitTest::flags(LimitTest::High),
          "another meter's limit column read back: " + err);
  }

  // --- many rows at once: one insert for the table, the newest ones kept ---
  {
    RecordingStore store;
    store.setReadingCapacity(4);
    QSignalSpy about(&store, &RecordingStore::readingsAboutToInsert);
    QList<LoggedReading> rows;
    for (int i = 0; i < 6; ++i)
    {
      LoggedReading row;
      row.value = i;
      rows << row;
    }
    store.logRows(rows);
    check(about.size() == 1 && about.at(0).at(0).toInt() == 0 && about.at(0).at(1).toInt() == 3,
          "logRows: one insert of the rows that fit");
    check(store.readingCount() == 4 && store.readingAt(0).value == 2 && store.readingAt(3).value == 5,
          "logRows: the newest rows stay");
    store.logRows(rows.mid(0, 2));
    check(store.readingCount() == 4 && store.readingAt(0).value == 4 && store.readingAt(3).value == 1,
          "logRows: the oldest rows make room");
    store.logRows({});
    check(about.size() == 2, "logRows: no rows, no insert");
  }

  if (failed)
  {
    qWarning() << failed << "RecordingStore check(s) failed";
    return 1;
  }
  qInfo() << "All RecordingStore tests passed.";
  return 0;
}
