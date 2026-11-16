// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph, the graph on QCustomPlot: the curve against the store's
// readings, a reading at the end, gaps, clear, the time and the left axis,
// hold; scrolling, Live held, "All", zoom, a file loaded; GraphModel's settings texts.
#include <QApplication>
#include <QDateTime>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTest>
#include <cmath>

#include "core/reading.h"
#include "recording/recordingfile.h"
#include "recording/recordingstore.h"
#include "recording/seriesdecimator.h"
#include "service/devicesession.h"
#include "ui/graphmodel.h"
#include "ui/qcpgraphview.h"
#include "ui/views/graphtimebar.h"
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

  int allChanged = 0;
  QObject::connect(&model, &GraphModel::followAllChanged, [&] { allChanged++; });
  model.setFollowAll(true);
  model.setFollowAll(true);
  check(model.followAll() && allChanged == 1, "\"All\" changes once");

  // the window of "All", as GraphWidget::allTarget() counts
  check(GraphModel::allTarget(3, false, 0) == 10, "at least 10 s");
  check(GraphModel::allTarget(42.2, false, 0) == 43, "whole seconds, rounded up");
  check(GraphModel::allTarget(42.2, true, 0) == 53, "growing: a quarter more");
  check(GraphModel::allTarget(61, false, 0) == 120, "whole minutes from one minute on");
  check(GraphModel::allTarget(100, true, 0) == 180, "125 s are 3 minutes");
  check(GraphModel::allTarget(1000, true, 600) == 600, "at most the recording's length");
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

// the readings of a recording: one a second for @p seconds
static void record(Feed &feed, int seconds)
{
  for (int i = 0; i < seconds; ++i)
    feed.value(i % 10);
}

static void testScroll()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.start();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  record(feed, 30);
  graph.refreshNow();
  check(graph.scrollBar()->maximum() == 0, "a recording shorter than the window: nothing to scroll");
  check(graph.stateText().startsWith("Recording"), "the state says Recording, not \"" + graph.stateText() + "\"");

  record(feed, 270);   // 300 s
  graph.refreshNow();
  check(graph.following() && graph.scrollBar()->maximum() == 2400 && graph.scrollBar()->value() == 2400,
        QString("5 minutes, 1 minute shown: 240 s to scroll in tenths (%1 of %2)")
        .arg(graph.scrollBar()->value()).arg(graph.scrollBar()->maximum()));

  // back: the window stays where it was put while the recording goes on
  graph.scrollBar()->setValue(1000);
  check(!graph.following() && graph.timeAxisStart() == 100 && graph.timeAxisEnd() == 160, "scrolled to 100 s");
  record(feed, 20);
  graph.refreshNow();
  check(graph.timeAxisStart() == 100 && graph.scrollBar()->value() == 1000 && graph.scrollBar()->maximum() == 2600,
        "new readings do not move a window put back");
  // to the end: it follows again
  graph.scrollBar()->setValue(graph.scrollBar()->maximum());
  check(graph.following(), "the scrollbar at its end follows again");
  record(feed, 10);
  graph.refreshNow();
  check(graph.timeAxisEnd() == store.duration() / 1000.0, "following: the newest reading at the right edge");

  // the keys
  graph.setFocus();
  QTest::keyClick(&graph, Qt::Key_Home);
  check(graph.timeAxisStart() == 0 && !graph.following(), "Home: the start of the recording");
  QTest::keyClick(&graph, Qt::Key_PageDown);
  check(graph.timeAxisStart() == 60, "Page down: one window on");
  QTest::keyClick(&graph, Qt::Key_Right);
  check(graph.timeAxisStart() == 66, "Right: a tenth of the window");
  QTest::keyClick(&graph, Qt::Key_Left, Qt::ShiftModifier);
  check(graph.timeAxisStart() == 36, "Shift+Left: half a window back");
  QTest::keyClick(&graph, Qt::Key_End);
  check(graph.following(), "End: at the newest reading");

  // stopped: the recording stands, all of it can be scrolled
  store.stop();
  graph.refreshNow();
  check(graph.stateText().startsWith("Stopped"), "the state says Stopped, not \"" + graph.stateText() + "\"");
  graph.showFrom(-5000);
  check(graph.timeAxisStart() == 0, "not before the start");
  graph.showFrom(store.duration());
  check(graph.timeAxisEnd() == store.duration() / 1000.0, "not past the end");
}

static void testLiveHeld()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.setLiveWidth(300);
  store.live();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  record(feed, 200);
  graph.refreshNow();
  check(graph.stateText() == "Live" && graph.following(), "Live follows");
  graph.showFrom(50000);
  check(graph.stateText() == "Live · held" && graph.timeAxisStart() == 50, "put back in Live: held at 50 s");
  record(feed, 20);
  graph.refreshNow();
  check(graph.timeAxisStart() == 50, "the store runs on, the window stays");
  graph.followEnd();
  check(graph.stateText() == "Live" && graph.timeAxisEnd() == store.duration() / 1000.0, "following again");
}

static void testAll()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.start();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  int windows = 0;
  QObject::connect(&graph, &QcpGraph::windowChanged, [&] { windows++; });
  record(feed, 20);
  graph.timeBar()->button(0)->click();
  check(graph.followAll() && graph.windowSeconds() == 20 && graph.timeAxisStart() == 0,
        QString("All: the window is the recording (%1 s)").arg(graph.windowSeconds()));
  check(graph.timeBar()->button(0)->isChecked() && !graph.timeBar()->button(60)->isChecked(), "the All button is down");
  record(feed, 1);   // 21 s: outgrown
  graph.refreshNow();
  check(graph.windowSeconds() == 27, QString("outgrown: a quarter more (%1 s)").arg(graph.windowSeconds()));
  windows = 0;
  record(feed, 5);
  graph.refreshNow();
  check(windows == 0 && graph.timeAxisStart() == 0, "within the window: it stays");
  record(feed, 60);  // 86 s
  graph.refreshNow();
  check(graph.windowSeconds() == 120, QString("above a minute: whole minutes (%1 s)").arg(graph.windowSeconds()));

  // a time button ends All and shows the last minute
  graph.timeBar()->button(60)->click();
  check(!graph.followAll() && graph.windowSeconds() == 60 && graph.following()
        && graph.timeAxisEnd() == store.duration() / 1000.0, "1 min: the last minute");
  check(graph.timeBar()->button(60)->isChecked() && !graph.timeBar()->button(0)->isChecked(), "the 1 min button is down");
  // the whole recording once, not growing
  graph.zoomFit();
  check(!graph.followAll() && graph.windowSeconds() == 120, "whole recording: its length, not All");
  store.stop();
}

static void testZoom()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.start();
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(100);
  graph.setStore(&store);
  record(feed, 400);
  store.stop();
  graph.refreshNow();

  // the wheel and the keys: narrower and wider, the end stays
  graph.zoomBy(1.25);
  check(graph.windowSeconds() == 80 && graph.timeAxisEnd() == 400, "zoom in at the end: 80 s up to the end");
  graph.zoomBy(1 / 1.25);
  check(graph.windowSeconds() == 100, "zoom out: 100 s again");
  // put back: around the middle
  graph.showFrom(100000);
  graph.zoomBy(2);
  check(graph.windowSeconds() == 50 && graph.timeAxisStart() == 125, "zoom in the middle: around 150 s");
  graph.zoomBy(0.5);
  graph.showFrom(100000);

  // a frame: its time and its values, until it is taken back
  const double before = graph.leftAxisMax();
  const QRect plot = graph.plotRect();
  const QRect frame(plot.left() + plot.width() / 4, plot.top() + plot.height() / 4, plot.width() / 2, plot.height() / 2);
  graph.zoomToRect(frame);
  check(graph.zoomed() && graph.windowSeconds() == 50, QString("the frame is half the window (%1 s)").arg(graph.windowSeconds()));
  check(std::abs(graph.timeAxisStart() - 125) < 1, QString("it starts a quarter in (%1 s)").arg(graph.timeAxisStart()));
  const double lo = graph.leftAxisMin(), hi = graph.leftAxisMax();
  check(hi < before && hi - lo > 0, "the left axis is the frame's");
  graph.showFrom(0);
  check(graph.leftAxisMin() == lo && graph.leftAxisMax() == hi, "the left axis stays while the time moves");
  // a second frame keeps what was before the first
  graph.zoomToRect(frame);
  check(graph.windowSeconds() == 25, "a frame in the frame");
  graph.setFocus();
  QTest::keyClick(&graph, Qt::Key_Escape);
  check(!graph.zoomed() && graph.windowSeconds() == 100 && graph.timeAxisStart() == 100,
        QString("Escape: the window as before the first frame (%1 s from %2)").arg(graph.windowSeconds()).arg(graph.timeAxisStart()));
  check(std::abs(graph.leftAxisMax() - before) < 1e-9, "the left axis fits again");

  // the mouse: Shift and a drag draw the frame, a drag moves the time
  QWidget *plotWidget = graph.childAt(plot.center() + QPoint(0, graph.timeBar()->height()));
  check(plotWidget != nullptr, "the plot under the mouse");
  if (plotWidget)
  {
    QTest::mousePress(plotWidget, Qt::LeftButton, Qt::ShiftModifier, frame.topLeft());
    QTest::mouseMove(plotWidget, frame.center());
    QTest::mouseMove(plotWidget, frame.bottomRight());
    QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::ShiftModifier, frame.bottomRight());
    check(graph.zoomed() && graph.windowSeconds() == 50, "Shift and a drag zoom on the frame");
    graph.endZoom(true);
    const double start = graph.timeAxisStart();
    QTest::mousePress(plotWidget, Qt::LeftButton, Qt::NoModifier, plot.center());
    QTest::mouseMove(plotWidget, plot.center() + QPoint(plot.width() / 4, 0));
    QTest::mouseMove(plotWidget, plot.center() + QPoint(plot.width() / 2, 0));
    QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::NoModifier, plot.center() + QPoint(plot.width() / 2, 0));
    check(std::abs(graph.timeAxisStart() - (start - 50)) < 1, QString("a drag to the right shows what was before (%1 s)").arg(graph.timeAxisStart()));
  }
}

static void testFile(const QString &dataDir)
{
  // a store loaded from a recording: View, all of it to scroll, gaps kept
  Recording rec;
  rec.start = Feed::wall(0);
  rec.sampleTimeTenths = 10;
  rec.unit = "V";
  for (int i = 0; i < 120; ++i)
    rec.values << (i >= 50 && i < 55 ? qQNaN() : i * 0.1);
  RecordingStore store;
  store.load(rec);
  QcpGraph graph;
  graph.resize(640, 400);
  graph.show();
  graph.setWindowSeconds(600);
  graph.setStore(&store);
  graph.setSourceName("bench.qtdmm-rec");
  graph.refreshNow();
  const QList<QPointF> drawn = graph.drawnPoints();
  const bool gap = std::any_of(drawn.cbegin(), drawn.cend(), [](const QPointF &p) { return std::isnan(p.y()); });
  check(drawn.size() >= 100 && gap, QString("the file's values with their gap (%1 points)").arg(drawn.size()));
  check(graph.stateText().startsWith("bench.qtdmm-rec"), "the state names the file, not \"" + graph.stateText() + "\"");
  check(graph.leftAxisTitle() == "[V]", "the file's unit on the left axis");

  // through the view: a file instead of the meter, and back
  DeviceSession session;
  QcpGraph viewGraph;
  QcpGraphView view(&session, &viewGraph, "qgraph-2", nullptr);
  QString error;
  check(!view.loadFile(dataDir + "/no_such_file.csv", &error) && !error.isEmpty() && !view.showsFile(),
        "a file that is not there: refused with a reason");
  check(view.loadFile(dataDir + "/new_dpoint.csv", &error), "a recording is loaded: " + error);
  viewGraph.refreshNow();
  check(view.showsFile() && viewGraph.store() && viewGraph.store()->count() > 0 && !viewGraph.curvePoints().isEmpty(),
        "the graph shows the file");
  check(viewGraph.stateText().startsWith("new_dpoint.csv"), "its name in the state");
  view.closeFile();
  viewGraph.refreshNow();
  check(!view.showsFile() && viewGraph.store() == nullptr && viewGraph.stateText().isEmpty(), "closed: no meter, an empty graph");

  // window and All go to the model and come from it
  view.model()->setWindowSeconds(120);
  check(viewGraph.windowSeconds() == 120, "the model's window is the graph's");
  viewGraph.zoomBy(2);
  check(view.model()->windowSeconds() == 60, "a zoom is in the model");
  check(view.loadFile(dataDir + "/new_dpoint.csv"), "the file again");
  viewGraph.setFollowAll(true);
  check(view.model()->followAll(), "All is in the model");
  view.model()->setFollowAll(false);
  check(!viewGraph.followAll(), "and comes from it");
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
  if (argc != 2)
  {
    qCritical() << "Usage: test_qcpgraph <data-dir>";
    return 1;
  }
  testModel();
  testCurve();
  testThinning();
  testGaps();
  testTimeAxis();
  testStoreGone();
  testScroll();
  testLiveHeld();
  testAll();
  testZoom();
  testFile(QString::fromLocal8Bit(argv[1]));
  shot();
  return failed ? 1 : 0;
}
