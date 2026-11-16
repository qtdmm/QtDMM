// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph, the graph on QCustomPlot: the curve against the store's
// readings, a reading at the end, gaps, clear, the time and the left axis,
// hold; GraphModel's settings texts.
#include <QApplication>
#include <QDateTime>
#include <QTest>
#include <cmath>

#include "core/reading.h"
#include "recording/recordingstore.h"
#include "recording/seriesdecimator.h"
#include "ui/graphmodel.h"
#include "ui/views/qcpgraph.h"
#include "testutil.h"

// The readings a MeterController gives a store, on a clock of the test's
// own: value() is a main reading in V now, then the clock moves on.
struct Feed
{
  RecordingStore *store;
  qint64 now = 0;
  qint64 step;

  explicit Feed(RecordingStore *s, qint64 stepMs = 100) : store(s), step(stepMs)
  {
    store->setClock([this] { return now; }, [this] { return wall(now); });
  }
  Feed(const Feed &) = delete;
  Feed &operator=(const Feed &) = delete;

  static QDateTime wall(qint64 t) { return QDateTime(QDate(2026, 11, 16), QTime(12, 0, 0)).addMSecs(t); }
  void value(double v)
  {
    Reading r;
    r.overload = std::isnan(v);
    r.value = v;
    r.text = r.overload ? QStringLiteral("OL") : QString::number(v);
    r.unit = r.baseUnit = QStringLiteral("V");
    r.port.quantity = Quantity::Voltage;
    r.t = now;
    r.msecs = wall(now).toMSecsSinceEpoch();
    store->setReading(r);
    now += step;
  }
};

static bool samePoints(const QList<QPointF> &a, const QList<QPointF> &b)
{
  if (a.size() != b.size())
    return false;
  for (int i = 0; i < a.size(); ++i)
  {
    const bool same = a[i].y() == b[i].y() || (std::isnan(a[i].y()) && std::isnan(b[i].y()));
    if (a[i].x() != b[i].x() || !same)
      return false;
  }
  return true;
}

// what SeriesDecimator makes of the whole store with the graph's bucket
static QList<QPointF> expected(const RecordingStore &store, qint64 bucket)
{
  QList<QPointF> points;
  SeriesDecimator::append(store.series(), 0, store.count() - 1, bucket, points);
  return points;
}

static void testModel()
{
  GraphModel model;
  int valuesChanged = 0, windowChanged = 0;
  QObject::connect(&model, &GraphModel::valuesChanged, [&] { valuesChanged++; });
  QObject::connect(&model, &GraphModel::windowChanged, [&] { windowChanged++; });
  check(model.values().isEmpty() && model.valuesSetting().isEmpty(), "a new model has no values");
  check(model.windowSeconds() == GraphModel::kDefaultWindowSeconds, "a new model has the default window");

  model.setValuesSetting("0@device:a 1@device:b");
  check(model.values() == QList<ValueRef>{ ValueRef(0, "device:a"), ValueRef(1, "device:b") }, "values read from the setting");
  check(model.valuesSetting() == "0@device:a 1@device:b", "values written as read");
  model.setValuesSetting("0@device:a 1@device:b");
  check(valuesChanged == 1, "the same values again: no signal");

  model.setWindowSeconds(60);
  model.setWindowSeconds(60);
  check(model.windowSeconds() == 60 && windowChanged == 1, "the window changes once");
  model.setWindowSeconds(0);
  check(model.windowSeconds() == 1, "the window is at least a second");
}

static void testCurve()
{
  RecordingStore store;
  Feed feed(&store);
  store.live();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  graph.refreshNow();
  check(graph.curvePoints().isEmpty() && graph.drawnPoints().isEmpty(), "an empty store: no points");

  for (int i = 0; i < 50; ++i)
    feed.value(1 + 0.01 * i);
  graph.refreshNow();
  check(graph.curvePoints().size() == 50, QString("50 readings in 5 s are 50 points, not %1").arg(graph.curvePoints().size()));
  check(samePoints(graph.curvePoints(), expected(store, 8)), "the points are the store's readings");
  check(graph.drawnPoints().size() == 50, "all of them are in the time shown");
  check(graph.leftAxisTitle() == "[V]", "the left axis in V, not \"" + graph.leftAxisTitle() + "\"");

  // a reading at the end adds a point and leaves the others
  const QList<QPointF> before = graph.curvePoints();
  feed.value(2);
  check(graph.curvePoints().size() == 51 && samePoints(graph.curvePoints().mid(0, 50), before),
        "a new reading: one more point, the others as they were");
  check(graph.drawnPoints().size() == 50, "drawn at the drawing pace, not with the reading");
  check(waitFor([&] { return graph.drawnPoints().size() == 51; }), "the new reading is drawn soon after");

  // small values: the axis takes the prefix
  store.clear();
  graph.refreshNow();
  check(graph.curvePoints().isEmpty(), "cleared: the curve is empty");
  for (int i = 0; i < 20; ++i)
    feed.value(0.001 + 0.0001 * i);
  graph.refreshNow();
  check(graph.leftAxisTitle() == "[mV]", "millivolts on the left axis, not \"" + graph.leftAxisTitle() + "\"");
  const QList<QPointF> drawn = graph.drawnPoints();
  check(drawn.size() == 20 && std::abs(drawn.first().y() - 1.0) < 1e-9, "the drawn values are in mV");
}

static void testThinning()
{
  // 10 minutes at 10 readings a second in a 10 minute window: thinned to
  // minimum and maximum of each bucket, as SeriesDecimator says
  RecordingStore store;
  Feed feed(&store);
  store.start();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(600);
  graph.setStore(&store);
  for (int i = 0; i < 6000; ++i)
    feed.value(std::sin(i * 0.37) + i * 0.001);
  graph.refreshNow();
  const QList<QPointF> all = graph.curvePoints();
  check(all.size() < 3000 && all.size() > 300, QString("6000 readings are thinned (%1 points)").arg(all.size()));
  bool matched = false;
  for (qint64 bucket = 1; bucket <= 4096 && !matched; bucket *= 2)
    matched = samePoints(all, expected(store, bucket));
  check(matched, "the thinned points are SeriesDecimator's for one bucket size");

  // readings one by one give the same points as building anew
  for (int i = 0; i < 200; ++i)
    feed.value(std::cos(i * 0.2));
  const QList<QPointF> grown = graph.curvePoints();
  graph.clear();
  check(samePoints(grown, graph.curvePoints()), "appended one by one: the same points as built anew");
  store.stop();
}

static void testGaps()
{
  RecordingStore store;
  Feed feed(&store);
  store.live();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  feed.value(1);
  feed.value(2);
  feed.value(qQNaN());
  feed.value(3);
  graph.refreshNow();
  const QList<QPointF> drawn = graph.drawnPoints();
  check(drawn.size() == 4 && std::isnan(drawn[2].y()) && drawn[3].y() == 3, "an overload is a gap in the line");

  store.clear();
  graph.refreshNow();
  check(graph.curvePoints().isEmpty() && graph.drawnPoints().isEmpty(), "cleared: no points");
  feed.value(5);
  graph.refreshNow();
  check(graph.drawnPoints().size() == 1, "a reading after clear is drawn");
}

static void testTimeAxis()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.start();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  for (int i = 0; i < 30; ++i)
    feed.value(i);
  graph.refreshNow();
  check(graph.timeAxisStart() == 0 && graph.timeAxisEnd() == 60, "a recording shorter than the window starts at 0");
  for (int i = 0; i < 90; ++i)
    feed.value(i);
  graph.refreshNow();
  check(std::abs(graph.timeAxisEnd() - store.duration() / 1000.0) < 1e-9 && graph.timeAxisEnd() - graph.timeAxisStart() == 60,
        QString("the window follows the newest reading (%1..%2)").arg(graph.timeAxisStart()).arg(graph.timeAxisEnd()));
  const QList<QPointF> drawn = graph.drawnPoints();
  check(drawn.size() < 70 && drawn.size() >= 60, QString("only the time shown is handed to the plot (%1 points)").arg(drawn.size()));

  graph.setWindowSeconds(10);
  graph.refreshNow();
  check(graph.timeAxisEnd() - graph.timeAxisStart() == 10, "a shorter window");

  // hold: the picture stays, the readings since show when it is released
  graph.setHold(true);
  const double end = graph.timeAxisEnd();
  for (int i = 0; i < 5; ++i)
    feed.value(i);
  QTest::qWait(80);
  check(graph.timeAxisEnd() == end, "hold: the time axis stays");
  graph.setHold(false);
  check(graph.timeAxisEnd() > end, "released: the time axis is at the newest reading");
  store.stop();
}

static void testStoreGone()
{
  auto *store = new RecordingStore;
  Feed feed(store);
  store->live();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setStore(store);
  feed.value(1);
  graph.refreshNow();
  check(graph.drawnPoints().size() == 1, "a reading of the store");
  graph.setStore(nullptr);
  graph.refreshNow();
  check(graph.drawnPoints().isEmpty(), "no store: an empty graph");
  graph.setStore(store);
  graph.refreshNow();
  check(graph.drawnPoints().size() == 1, "the store again: its reading");
  delete store;
  graph.refreshNow();
  check(graph.store() == nullptr && graph.drawnPoints().isEmpty(), "the store deleted: an empty graph");
}

static void shot()
{
  // a picture to look at: QTDMM_SHOT=<dir>
  if (!qEnvironmentVariableIsSet("QTDMM_SHOT"))
    return;
  RecordingStore store;
  Feed feed(&store);
  store.live();
  QcpGraph graph;
  graph.resize(800, 400);
  graph.setThemeColors(QColor(20, 24, 28), QColor(90, 100, 110), QColor(200, 205, 210), QColor(90, 200, 120));
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  for (int i = 0; i < 450; ++i)
    feed.value(i > 200 && i < 215 ? qQNaN() : 0.02 * std::sin(i * 0.05) + 0.002 * std::sin(i * 1.3));
  graph.refreshNow();
  QTest::qWait(50);
  graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/qcpgraph_curve.png");
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  testModel();
  testCurve();
  testThinning();
  testGaps();
  testTimeAxis();
  testStoreGone();
  shot();
  return failed ? 1 : 0;
}
