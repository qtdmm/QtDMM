// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph, the graph on QCustomPlot: the curve against the store's
// readings, a reading at the end, gaps, clear, the time and the left axis,
// hold; scrolling, Live held, "All", zoom, a file loaded; several values of
// several meters on the list of axes; the colour variants (GraphLook), pens,
// markers and divisions; the crosshair on a reading, the time cursors and
// the statistics (GraphStats); the reference lines; marks, notes and their
// flags; the overview, the states' strip and the bands; GraphModel's settings texts.
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QLineEdit>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTest>
#include <QTimer>
#include <cmath>

#include "core/reading.h"
#include "recording/recordingfile.h"
#include "recording/recordingstore.h"
#include "recording/seriesdecimator.h"
#include "service/devicesession.h"
#include "ui/graphmodel.h"
#include "ui/qcpgraphview.h"
#include "ui/views/graphband.h"
#include "ui/views/graphminimap.h"
#include "ui/views/graphstats.h"
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
  /// Another value of the meter (a second display) now, without moving the clock.
  void other(int id, double v, const QString &unit, Quantity quantity)
  {
    Reading r;
    r.id = id;
    r.value = v;
    r.text = QString::number(v);
    r.unit = r.baseUnit = unit;
    r.port.quantity = quantity;
    r.port.slot = Slot(id);
    r.t = now;
    r.msecs = wall(now).toMSecsSinceEpoch();
    store->setReading(r);
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

  // the axis of a value, in the words of the graph on QtCharts
  int axesChanged = 0;
  QObject::connect(&model, &GraphModel::valueAxesChanged, [&] { axesChanged++; });
  check(model.valueAxis(ValueRef(1, "device:b")) == GraphModel::kAxisAuto && model.valueAxesSetting().isEmpty(), "Auto is not written");
  model.setValueAxis(ValueRef(1, "device:b"), 1);
  model.setValueAxis(ValueRef(0, "device:a"), 2);
  model.setValueAxis(ValueRef(0, "device:a"), 2);
  check(axesChanged == 2, "an axis set twice changes once");
  check(model.valueAxesSetting() == "0@device:a:third 1@device:b:right", "the axes as written: " + model.valueAxesSetting());
  model.setValueAxesSetting("1@device:b:left 0@device:c:axis4 0@device:a:nonsense");
  check(model.valueAxis(ValueRef(1, "device:b")) == 0 && model.valueAxis(ValueRef(0, "device:c")) == 3
        && model.valueAxis(ValueRef(0, "device:a")) == GraphModel::kAxisAuto, "the axes as read, a fourth one too");
  check(model.valueAxesSetting() == "1@device:b:left 0@device:c:axis4", "a fourth axis is written by its number: " + model.valueAxesSetting());
  model.setValueAxis(ValueRef(1, "device:b"), GraphModel::kAxisAuto);
  check(model.valueAxesSetting() == "0@device:c:axis4", "back on Auto: gone");

  int rangesChanged = 0;
  QObject::connect(&model, &GraphModel::fixedRangesChanged, [&] { rangesChanged++; });
  check(model.fixedRangesSetting().isEmpty(), "no fixed range: an empty text");
  model.setFixedRange("Hz", 49.5, 50.5);
  model.setFixedRange("Hz", 49.5, 50.5);
  model.setFixedRange("V", 5, 1);
  check(rangesChanged == 1 && model.fixedRangesSetting() == "{\"Hz\":[49.5,50.5]}", "a fixed range as written: " + model.fixedRangesSetting());
  model.setFixedRangesSetting("{\"V\":[0,10],\"A\":[3,1]}");
  check(model.fixedRanges().size() == 1 && model.fixedRanges().value("V") == GraphModel::Range(0, 10), "fixed ranges as read, a turned one left out");
  model.clearFixedRange("V");
  check(model.fixedRanges().isEmpty(), "cleared");

  int lookChanged = 0;
  QObject::connect(&model, &GraphModel::lookChanged, [&] { lookChanged++; });
  model.setVariant("scope");
  model.setVariant("scope");
  model.setPatternedLines(true);
  check(lookChanged == 2 && model.variant() == "scope" && model.patternedLines(), "variant and patterns change once each");
}

static void testLook()
{
  using namespace GraphLook;
  for (Variant v : variants())
    check(fromName(name(v)) == v && !title(v).isEmpty(), "a variant's name reads back: " + name(v));
  check(fromName("nonsense") == Neutral && fromName(QString()) == Neutral, "an unknown name is Neutral");

  const PlotTheme design{ QColor("#202428"), QColor("#606870"), QColor("#d0d4d8"), QColor("#40a0ff") };
  Settings settings;
  const Look neutral = look(Neutral, design, settings);
  check(neutral.theme.background == design.background && neutral.theme.labels == design.labels
        && neutral.theme.data == design.data && !neutral.divisions && !neutral.oneHue, "Neutral follows the design");
  check(curveColor(neutral, { 0, 0, -1 }) == design.data && curveColor(neutral, { 1, 1, -1 }) == QColor("#d95926")
        && curveColor(neutral, { 1, 0, 0 }) == QColor("#5b8def"), "a dark design: the stepped hues, others for another meter");
  const Look paper = look(Neutral, PlotTheme{ QColor(Qt::white), QColor(Qt::gray), QColor(Qt::black), QColor() }, settings);
  check(paper.theme.data == QColor(Qt::black) && curveColor(paper, { 1, 1, -1 }) == QColor("#eb6834"), "a light design: the plain hues, the lettering as data colour");

  const Look scope = look(ScopeBlue, design, settings);
  check(scope.theme.background.color() == QColor("#0b1633") && scope.theme.labels == QColor("#b9c9e8")
        && scope.theme.data == QColor("#ffe14d") && scope.divisions && scope.plot.style() == Qt::LinearGradientPattern,
        "the scope: blue screen, yellow curve, divisions");
  check(curveColor(scope, { 1, 1, -1 }) == QColor("#ff8a5c") && curveColor(scope, { 3, 3, -1 }) == QColor("#d9a6ff")
        && curveColor(scope, { 4, 4, -1 }) == curveColor(scope, { 1, 1, -1 }), "a meter's other values: a hue by id, round again after three");
  check(curveColor(scope, { 1, 0, 0 }) == QColor("#5ce1e6") && curveColor(scope, { 2, 1, 1 }) == QColor("#ff7eb6")
        && curveColor(scope, { 5, 0, 4 }) == curveColor(scope, { 1, 0, 0 }), "other meters' curves: a hue by their turn, round again after four");
  const Look recorder = look(ChartRecorder, design, settings);
  check(recorder.minorGrid.style() != Qt::NoPen && recorder.theme.data == QColor("#1f4e9c") && recorder.divisions, "the recorder: paper with a fine grid");
  const Look contrast = look(HighContrast, design, settings);
  check(contrast.lineWidth == 3 && contrast.theme.background.color() == QColor(Qt::black) && !contrast.divisions, "high contrast: thicker lines");
  const Look blind = look(ColourBlind, design, settings);
  check(blind.theme.data == QColor("#0072b2") && curveColor(blind, { 1, 1, -1 }) == QColor("#d55e00"), "colour-blind safe: the Okabe-Ito hues");
  const Look green = look(PhosphorGreen, design, settings);
  check(green.oneHue && green.divisions && green.centreTicks.style() != Qt::NoPen && curveColor(green, { 1, 1, -1 }) == curveColor(green, { 2, 0, 0 }),
        "phosphor: one hue for the other curves, fine ticks");

  // the settings: a data colour of one's own goes for every variant, Custom takes all
  settings.data = QColor("#ff0000");
  check(look(ScopeBlue, design, settings).theme.data == QColor("#ff0000"), "a data colour chosen goes for the scope too");
  settings.background = QColor("#101010");
  settings.grid = QColor("#333333");
  const Look custom = look(Custom, design, settings);
  check(custom.theme.background.color() == QColor("#101010") && custom.grid.color() == QColor("#333333")
        && custom.theme.labels.lightness() > 128, "Custom: the settings' colours, lettering that reads");

  // the pens
  settings = Settings();
  settings.lineWidth = 4;
  const Look l = look(ScopeBlue, design, settings);
  check(curvePen(l, { 0, 0, -1 }, false).widthF() == 4 && curvePen(l, { 0, 0, -1 }, false).style() == Qt::SolidLine, "the first curve: the settings' width, solid");
  check(curvePen(l, { 1, 0, 0 }, false).widthF() == 3 && curvePen(l, { 1, 0, 0 }, false).style() == Qt::SolidLine, "another meter's main value: thinner, solid");
  check(curvePen(l, { 1, 1, -1 }, false).style() == Qt::DashLine && curvePen(l, { 2, 2, -1 }, false).style() == Qt::DotLine
        && curvePen(l, { 3, 3, -1 }, false).style() == Qt::DashDotLine, "a meter's other values: dashes, dots, dash-dots");
  check(curvePen(l, { 1, 0, 0 }, true).style() == Qt::CustomDashLine && curvePen(l, { 1, 0, 0 }, true).dashPattern() != curvePen(l, { 2, 0, 1 }, true).dashPattern()
        && curvePen(l, { 0, 0, -1 }, true).style() == Qt::SolidLine, "patterned: each curve after the first its own dashes");
  check(curvePen(green, { 1, 0, 0 }, false).style() == Qt::CustomDashLine, "phosphor: dashes also without the switch");
  settings.lineMode = 2;
  check(curvePen(look(Neutral, design, settings), { 0, 0, -1 }, false).style() == Qt::DotLine, "dotted line");
  settings.lineMode = 0;
  check(curvePen(look(Neutral, design, settings), { 0, 0, -1 }, false).style() == Qt::NoPen
        && curvePen(look(Neutral, design, settings), { 1, 0, 0 }, false).style() == Qt::NoPen, "no line");
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
    check(graph.zoomed() && std::abs(graph.windowSeconds() - 50) <= 1, QString("Shift and a drag zoom on the frame (%1 s)").arg(graph.windowSeconds()));
    graph.endZoom(true);
    const double start = graph.timeAxisStart();
    QTest::mousePress(plotWidget, Qt::LeftButton, Qt::NoModifier, plot.center());
    QTest::mouseMove(plotWidget, plot.center() + QPoint(plot.width() / 4, 0));
    QTest::mouseMove(plotWidget, plot.center() + QPoint(plot.width() / 2, 0));
    QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::NoModifier, plot.center() + QPoint(plot.width() / 2, 0));
    check(std::abs(graph.timeAxisStart() - (start - 50)) < 2, QString("a drag to the right shows what was before (%1 s)").arg(graph.timeAxisStart()));
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

// two meters and their stores, as the session gives them to a graph
struct Bench
{
  RecordingStore a, b;
  Feed feedA{ &a, 1000 }, feedB{ &b, 1000 };
  QcpGraph graph;

  Bench()
  {
    graph.resize(900, 400);
    graph.show();
    graph.setWindowSeconds(60);
    graph.setSources([this](const QString &source) -> RecordingStore *
    { return source == "device:a" ? &a : source == "device:b" ? &b : nullptr; },
    [](const QString &source) { return source == "device:a" ? QString("Meter A") : QString("Meter B"); });
  }
};

static void testValues()
{
  Bench bench;
  QcpGraph &graph = bench.graph;
  bench.a.live();
  bench.b.live();
  graph.setValues({ ValueRef(0, "device:a"), ValueRef(1, "device:a"), ValueRef(0, "device:b"), ValueRef(0, "device:gone") });
  check(graph.curveCount() == 3 && graph.store() == &bench.a, "a curve per value of an open meter, the first one gives the time");
  for (int i = 0; i < 20; ++i)
  {
    // the main value first: the others count from its first reading
    bench.feedA.value(1 + 0.1 * i);
    bench.feedA.now -= 1000;
    bench.feedA.other(1, 50 + 0.01 * i, "Hz", Quantity::Frequency);
    bench.feedA.now += 1000;
    bench.feedB.value(2 + 0.1 * i);
  }
  graph.refreshNow();
  check(graph.curvePoints(0).size() == 20 && graph.curvePoints(1).size() == 20 && graph.curvePoints(2).size() == 20,
        QString("20 readings each (%1, %2, %3)").arg(graph.curvePoints(0).size()).arg(graph.curvePoints(1).size()).arg(graph.curvePoints(2).size()));
  // V left, Hz right, the other meter's V on the left too
  check(graph.curveAxis(0) == 0 && graph.curveAxis(1) == 1 && graph.curveAxis(2) == 0 && graph.axisCount() == 2,
        QString("Auto: V left, Hz right, V of the other meter left (%1 %2 %3)").arg(graph.curveAxis(0)).arg(graph.curveAxis(1)).arg(graph.curveAxis(2)));
  check(graph.axisTitle(0) == "[V]" && graph.axisTitle(1) == "[Hz]", "the axes' units: " + graph.axisTitle(0) + " " + graph.axisTitle(1));
  check(graph.axisMin(0) < 1 && graph.axisMax(0) > 3.9, "the left axis covers both meters' volts");
  check(graph.axisMin(1) > 49 && graph.axisMax(1) < 51, "the right axis the frequency");
  check(graph.curveColor(0) != graph.curveColor(1) && graph.curveColor(1) != graph.curveColor(2), "each curve its colour");
  // with room for the long form, whatever the font: in less the meters' names go first
  graph.resize(2400, 400);
  QCoreApplication::processEvents();
  check(graph.legendText().contains("Meter A") && graph.legendText().contains("Meter B") && graph.legendText().contains("Hz"),
        "the legend names meters and values: " + graph.legendText());
  graph.resize(900, 400);
  QCoreApplication::processEvents();
  check(graph.legendText().contains("Hz"), "in a narrow window the legend still names the values: " + graph.legendText());
  check(graph.notice().isEmpty(), "all drawn: no notice");

  // readings one by one give what a rebuild gives, for each curve
  QList<QList<QPointF>> grown;
  for (int i = 0; i < 3; ++i)
    grown << graph.curvePoints(i);
  graph.clear();
  check(samePoints(grown[0], graph.curvePoints(0)) && samePoints(grown[1], graph.curvePoints(1)) && samePoints(grown[2], graph.curvePoints(2)),
        "appended one by one: the same points as built anew");

  // a chosen axis: the frequency on the third, the other meter's volts on the right
  graph.setValueAxes({ { ValueRef(1, "device:a"), 2 }, { ValueRef(0, "device:b"), 1 } });
  graph.refreshNow();
  check(graph.curveAxis(1) == 2 && graph.curveAxis(2) == 1 && graph.axisCount() == 3, "the axes chosen");
  check(graph.axisTitle(1) == "[V]" && graph.axisTitle(2) == "[Hz]", "their units: " + graph.axisTitle(1) + " " + graph.axisTitle(2));
  check(graph.axisMin(1) > 1.5, QString("the right axis fits the other meter alone (%1 .. %2)").arg(graph.axisMin(1)).arg(graph.axisMax(1)));
  // an axis that has another unit: the value is not drawn, and says so
  graph.setValueAxes({ { ValueRef(0, "device:a"), 0 }, { ValueRef(1, "device:a"), 0 } });
  graph.refreshNow();
  check(graph.curveAxis(0) == 0 && graph.curveAxis(1) == -1 && graph.notice().contains("Hz") && graph.stateText().contains("Hz"),
        "Hz chosen onto the volts' axis: not drawn, with a notice: " + graph.notice());
  check(graph.drawnPoints(1).isEmpty(), "nothing of it is handed to the plot");
  graph.setValueAxes({});
  graph.refreshNow();

  // a fixed range by unit, also a narrow one far from zero (R19-06)
  graph.setFixedRanges({ { "Hz", { 49.95, 50.05 } } });
  check(graph.axisMin(1) == 49.95 && graph.axisMax(1) == 50.05 && graph.axisTitle(1) == "[Hz]", "the fixed range of Hz");
  check(graph.axisMin(0) < 1, "the volts fit themselves");
  graph.setFixedRanges({});
  check(graph.axisMin(1) < 49.999, "without it the axis fits again");

  // a frame zooms every axis
  const QRect plot = graph.plotRect();
  const double hz = graph.axisMax(1) - graph.axisMin(1), volt = graph.axisMax(0) - graph.axisMin(0);
  graph.zoomToRect(QRect(plot.left(), plot.top() + plot.height() / 4, plot.width(), plot.height() / 2));
  check(std::abs((graph.axisMax(1) - graph.axisMin(1)) / hz - 0.5) < 0.02 && std::abs((graph.axisMax(0) - graph.axisMin(0)) / volt - 0.5) < 0.02,
        "a frame of half the height: half of each axis");
  graph.endZoom(true);

  // the meter of the first value closes: the next one gives the time
  bench.graph.setSources([&bench](const QString &source) -> RecordingStore * { return source == "device:b" ? &bench.b : nullptr; },
                         [](const QString &) { return QString("Meter B"); });
  graph.refreshNow();
  check(graph.curveCount() == 1 && graph.store() == &bench.b && graph.curveAxis(0) == 0 && graph.legendText().isEmpty(),
        "one meter left: its curve on the left, no legend");
  check(graph.curveColor(0).isValid(), "it has a colour");
}

static void testFourUnits()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.live();
  QcpGraph graph;
  graph.resize(900, 400);
  graph.show();
  graph.setSources([&store](const QString &) { return &store; }, [](const QString &) { return QString("Meter"); });
  graph.setValues({ ValueRef(0, "m"), ValueRef(1, "m"), ValueRef(2, "m"), ValueRef(3, "m") });
  for (int i = 0; i < 10; ++i)
  {
    feed.other(1, 50, "Hz", Quantity::Frequency);
    feed.other(2, 0.5, "A", Quantity::Current);
    feed.other(3, 25, "°C", Quantity::Temperature);
    feed.value(i);
  }
  graph.refreshNow();
  check(graph.curveAxis(0) == 0 && graph.curveAxis(1) == 1 && graph.curveAxis(2) == 2 && graph.curveAxis(3) == -1,
        QString("four units, %1 axes: the fourth is not drawn").arg(QcpGraph::kMaxAxes));
  check(graph.axisCount() == QcpGraph::kMaxAxes && graph.notice().contains("°C"), "and is named: " + graph.notice());
  // one goes: the fourth takes its axis
  graph.setValues({ ValueRef(0, "m"), ValueRef(2, "m"), ValueRef(3, "m") });
  graph.refreshNow();
  check(graph.curveAxis(1) == 1 && graph.curveAxis(2) == 2 && graph.notice().isEmpty() && graph.axisTitle(2) == "[°C]",
        "a value less: the temperature has the third axis");
}

static void testOtherTime()
{
  // the second meter starts 10 s after the first: its readings 10 s later on the graph
  Bench bench;
  QcpGraph &graph = bench.graph;
  bench.a.live();
  bench.feedB.now = 10000;
  bench.b.live();
  graph.setValues({ ValueRef(0, "device:a"), ValueRef(0, "device:b") });
  for (int i = 0; i < 15; ++i)
    bench.feedA.value(i);
  for (int i = 0; i < 5; ++i)
    bench.feedB.value(100 + i);
  graph.refreshNow();
  const QList<QPointF> b = graph.curvePoints(1);
  check(b.size() == 5 && b.first().x() == 10 && b.last().x() == 14,
        QString("the other meter's readings on the first one's time (%1 .. %2 s)").arg(b.value(0).x()).arg(b.isEmpty() ? 0 : b.last().x()));
  // a reading more goes where it belongs
  bench.feedB.value(105);
  check(graph.curvePoints(1).size() == 6 && graph.curvePoints(1).last().x() == 15, "a new reading of the other meter at 15 s");
}

static void testParts()
{
  // a second display that goes from Hz to %: two series, a gap between them
  RecordingStore store;
  Feed feed(&store, 1000);
  store.live();
  QcpGraph graph;
  graph.resize(900, 400);
  graph.show();
  graph.setSources([&store](const QString &) { return &store; }, [](const QString &) { return QString(); });
  graph.setValues({ ValueRef(0, "m"), ValueRef(1, "m") });
  for (int i = 0; i < 5; ++i)
  {
    feed.other(1, 50, "Hz", Quantity::Frequency);
    feed.value(i);
  }
  for (int i = 0; i < 5; ++i)
  {
    feed.other(1, 40 + i, "%", Quantity::DutyCycle);
    feed.value(i);
  }
  graph.refreshNow();
  const QList<QPointF> points = graph.curvePoints(1);
  const int gaps = int(std::count_if(points.cbegin(), points.cend(), [](const QPointF &p) { return std::isnan(p.y()); }));
  check(store.extraCount() == 2 && gaps >= 1 && points.size() >= 11,
        QString("two series of the value, a gap between (%1 series, %2 points, %3 gaps)").arg(store.extraCount()).arg(points.size()).arg(gaps));
  check(graph.axisTitle(1) == "[%]", "the axis in the newest unit: " + graph.axisTitle(1));
}

static void testGraphLook()
{
  Bench bench;
  QcpGraph &graph = bench.graph;
  const PlotTheme design{ QColor("#202428"), QColor("#606870"), QColor("#d0d4d8"), QColor("#40a0ff") };
  graph.setTheme(design);
  bench.a.live();
  bench.b.live();
  graph.setValues({ ValueRef(0, "device:a"), ValueRef(1, "device:a"), ValueRef(0, "device:b") });
  for (int i = 0; i < 20; ++i)
  {
    bench.feedA.value(1 + 0.1 * i);
    bench.feedA.now -= 1000;
    bench.feedA.other(1, 50 + 0.01 * i, "Hz", Quantity::Frequency);
    bench.feedA.now += 1000;
    bench.feedB.value(2 + 0.1 * i);
  }
  graph.refreshNow();
  check(graph.curveColor(0) == design.data && graph.curvePen(0).style() == Qt::SolidLine && graph.curvePen(1).style() == Qt::DashLine
        && graph.curvePen(2).style() == Qt::SolidLine, "Neutral: the design's colour, the second display dashed");
  // the frequency alone on its axis: the axis in its colour; two curves on the left: the lettering
  check(graph.axisColor(1) == graph.curveColor(1) && graph.axisColor(0) == design.labels,
        "an axis with one curve has its colour, one with two the lettering's");
  check(!graph.curveMarkers(0), "no markers by default");

  graph.setVariant(GraphLook::ScopeBlue);
  graph.refreshNow();
  check(graph.look().theme.background.color() == QColor("#0b1633") && graph.curveColor(0) == QColor("#ffe14d")
        && graph.curveColor(1) == QColor("#ff8a5c") && graph.curveColor(2) == QColor("#5ce1e6"),
        "the scope's colours: the second display orange, the other meter cyan");
  // 8 divisions on every axis: whole steps, the values inside
  QTest::qWait(30);   // the ticks are made with the picture
  for (int axis = 0; axis < 2; ++axis)
  {
    const double span = graph.axisMax(axis) - graph.axisMin(axis), step = span / 8;
    const double mantissa = step / std::pow(10.0, std::floor(std::log10(step) + 1e-9));
    check(graph.axisTickCount(axis) == 9 && (std::abs(mantissa - 1) < 1e-6 || std::abs(mantissa - 2) < 1e-6 || std::abs(mantissa - 5) < 1e-6),
          QString("axis %1: 8 divisions of a 1-2-5 step (%2 .. %3, %4 ticks)").arg(axis).arg(graph.axisMin(axis)).arg(graph.axisMax(axis)).arg(graph.axisTickCount(axis)));
  }
  check(graph.axisMin(0) <= 1 && graph.axisMax(0) >= 3.9 && graph.axisMin(1) <= 50 && graph.axisMax(1) >= 50.19, "the values are inside");
  // a fixed range and a frame are not widened
  graph.setFixedRanges({ { "Hz", { 49.95, 50.05 } } });
  check(graph.axisMin(1) == 49.95 && graph.axisMax(1) == 50.05, "a fixed range stays as given");
  graph.setFixedRanges({});

  graph.setVariant(GraphLook::PhosphorGreen);
  graph.refreshNow();
  check(graph.curveColor(1) == graph.curveColor(2) && graph.curvePen(1).dashPattern() != graph.curvePen(2).dashPattern(),
        "phosphor: one hue, the curves apart by their dashes");
  graph.setVariant(GraphLook::Neutral);
  graph.setPatternedLines(true);
  check(graph.curvePen(2).style() == Qt::CustomDashLine && graph.curvePen(0).style() == Qt::SolidLine, "patterned lines");
  graph.setPatternedLines(false);

  // the settings' line, points and width
  GraphLook::Settings settings;
  settings.lineWidth = 3;
  settings.pointMode = 2;
  graph.setLookSettings(settings);
  graph.refreshNow();
  check(graph.curvePen(0).widthF() == 3 && graph.curvePen(1).widthF() == 2.25, "the width, the other curves three quarters of it");
  check(graph.curveMarkers(0) && graph.curveMarkers(2), "20 readings on 800 pixels: markers");
  settings.lineMode = 0;
  graph.setLookSettings(settings);
  graph.refreshNow();
  check(graph.curvePen(0).style() == Qt::NoPen && graph.curveMarkers(0), "no line, the markers stay");
  settings.lineMode = 1;
  graph.setLookSettings(settings);
  // dense readings: no markers
  bench.feedA.step = bench.feedB.step = 100;
  for (int i = 0; i < 500; ++i)
  {
    bench.feedA.value(1 + 0.001 * i);
    bench.feedB.value(2 + 0.001 * i);
  }
  graph.refreshNow();
  check(!graph.curveMarkers(0), QString("%1 readings on the plot: no markers").arg(graph.drawnPoints(0).size()));

  // the legend shows the pens
  check(!graph.legendText().isEmpty(), "the legend is there");
}

static void testStoreDeleted()
{
  // a store that goes without a word from the sources: its curves go, the rest stays
  auto *first = new RecordingStore;
  RecordingStore second;
  Feed feedFirst(first, 1000), feedSecond(&second, 1000);
  first->live();
  second.live();
  int asked = 0;
  QcpGraph graph;
  graph.resize(900, 400);
  graph.show();
  graph.setSources([&](const QString &source) -> RecordingStore * { ++asked; return source == "one" ? first : &second; },
                   [](const QString &source) { return source; });
  graph.setValues({ ValueRef(0, "one"), ValueRef(0, "two") });
  for (int i = 0; i < 5; ++i)
  {
    feedFirst.value(i);
    feedSecond.value(10 + i);
  }
  graph.refreshNow();
  check(graph.curveCount() == 2 && graph.store() == first, "two curves, the first store gives the time");
  asked = 0;
  delete first;
  check(asked == 0, "the sources are not asked while a store goes");
  graph.refreshNow();
  check(graph.curveCount() == 1 && graph.store() == &second && graph.curvePoints(0).size() == 5, "its curve is gone, the other one stays and gives the time");
  feedSecond.value(20);
  graph.refreshNow();
  check(graph.curvePoints(0).size() == 6, "and goes on");
}

static void testStatsText()
{
  using namespace GraphStats;
  check(withUnit(0.001234, "V") == "1.234 mV" && withUnit(qQNaN(), "V") == "--", "a value with its prefix, none as --");
  check(spanText(250) == "250 ms" && spanText(5000) == "5.000 s" && spanText(12500) == "12.50 s" && spanText(3909000) == "1:05:09",
        "spans: " + spanText(250) + ", " + spanText(5000) + ", " + spanText(12500) + ", " + spanText(3909000));
  check(timeText(80250) == "0:01:20.250", "a time of the recording: " + timeText(80250));
  const ColumnScale c = columnScale({ 0.0123, 0.0101 }, { 0.00002 }, "V");
  check(c.unit == "mV" && c.text(0.0123) == "12.300 mV" && c.text(0.00002) == "0.020 mV", "one scale for a column: " + c.text(0.0123) + ", " + c.text(0.00002));
  check(columnScale({ 0, 0 }, {}, "V").text(0) == "0 V", "nothing but zeros");
  check(columnScale({ 25.5 }, {}, "C").text(25.5) == QString::fromUtf8("25.50 °C") && withUnit(25.5, "C") == QString::fromUtf8("25.5 °C"),
        "a temperature in degrees: " + columnScale({ 25.5 }, {}, "C").text(25.5));

  // a curve of a store: the value that holds, the nearer reading, a gap
  RecordingStore store;
  Feed feed(&store, 1000);
  store.start();
  for (int i = 0; i < 10; ++i)
    feed.value(i == 5 ? qQNaN() : i);
  store.stop();
  Curve curve;
  curve.name = "Voltage";
  curve.unit = "V";
  curve.store = &store;
  curve.parts = { &store.series() };
  check(curve.valueAt(3400) == 3 && curve.valueAt(3999) == 3 && curve.valueAt(4000) == 4, "the value holds until the next reading");
  check(std::isnan(curve.valueAt(5500)) && std::isnan(curve.valueAt(-5)) && std::isnan(curve.valueAt(store.duration() + 1000)),
        "none in a gap, before the first reading, after the recording");
  qint64 at = -1;
  check(curve.readingNear(3400, &at) && at == 3000 && curve.readingNear(3600, &at) && at == 4000, "the nearer reading of the two");
  check(curve.readingNear(4600, &at) && at == 4000 && curve.readingNear(5200, &at)->gap(), "next to a gap: the one that holds; in it: the gap");
  // on another time: the store's 2 s later on the graph's
  curve.shift = 2000;
  check(curve.valueAt(5400) == 3 && curve.readingNear(5400, &at) && at == 5000, "a store that started later, on the graph's time");
  curve.shift = 0;

  Input input;
  input.curves << curve;
  input.from = 0;
  input.to = 60000;
  input.end = store.duration();
  QString text = GraphStats::text(input);
  check(text.contains("Click: cursor A") && text.contains("Shown") && text.contains("peak-peak") && !text.contains("A – B"),
        "no cursors: the hint and the time shown");
  input.cursor[0] = 2000;
  input.cursor[1] = 8000;
  text = GraphStats::text(input);
  const RecordingStore::Stats st = store.stats(2000, 8000);
  const ColumnScale scale = columnScale({ 6.0, st.mean, st.min, st.max }, { st.sigma }, "V");
  check(text.contains("A 0:00:02.000") && text.contains("B 0:00:08.000") && text.contains("A – B") && text.contains("6.000 s"),
        "the cursors' times and their distance");
  check(text.contains(scale.text(st.mean)) && text.contains(scale.text(st.sigma)) && text.contains(scale.text(6.0))
        && text.contains(QString::number(st.count)), "from A to B: the store's statistics (mean " + scale.text(st.mean) + ")");
  check(!text.contains(QChar(0x222b)), "no integral for volts");
  input.curves[0].unit = "A";
  text = GraphStats::text(input);
  check(text.contains(QChar(0x222b)) && text.contains(withUnit(st.integral / 3600, "Ah")), "a current: its integral in Ah: " + withUnit(st.integral / 3600, "Ah"));
}

static void testCrosshairAndCursors()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.start();
  QcpGraph graph;
  graph.resize(900, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  for (int i = 0; i < 20; ++i)
  {
    if (i == 12)
    {
      Reading r;
      r.value = 12;
      r.text = "12";
      r.unit = r.baseUnit = "V";
      r.port.quantity = Quantity::Voltage;
      r.hold = true;
      r.flags = quint32(SampleFlag::Hold);
      r.t = feed.now;
      r.msecs = Feed::wall(feed.now).toMSecsSinceEpoch();
      store.setReading(r);
      feed.now += 1000;
    }
    else
      feed.value(i == 8 ? qQNaN() : i);
  }
  store.stop();
  graph.refreshNow();
  QTest::qWait(30);
  const QRect plot = graph.plotRect();
  // a pixel of a time: the time axis is 0 .. 60 s over the plot's width
  auto pixel = [&](double seconds) { return QPoint(plot.left() + qRound(seconds / 60.0 * plot.width()), plot.center().y()); };

  // the crosshair: on the reading that holds, on the next one when nearer
  QString text = graph.crosshairTextAt(pixel(3.3));
  check(text.startsWith("0:00:03.000") && text.contains("3 V"), "the reading that holds at 3.3 s: " + text);
  text = graph.crosshairTextAt(pixel(3.8));
  check(text.startsWith("0:00:04.000") && text.contains("4 V"), "the nearer one at 3.8 s: " + text);
  text = graph.crosshairTextAt(pixel(8.4));
  check(text.contains("OL") && !text.contains(" V"), "an overload says so: " + text);
  text = graph.crosshairTextAt(pixel(12.1));
  check(text.contains("12 V") && text.contains("HOLD"), "a held reading says so: " + text);
  text = graph.crosshairTextAt(pixel(40));
  check(!text.contains("V"), "after the recording: the time alone: " + text);

  // the cursors: set, bound to the recording, their levels
  int changed = 0;
  QObject::connect(&graph, &QcpGraph::cursorsChanged, [&] { changed++; });
  graph.setTimeCursor(0, 3400);
  check(graph.timeCursor(0) == 3400 && graph.cursorLevel(0) == 3 && changed == 1, "A at 3.4 s, its level on the value that holds");
  graph.setTimeCursor(1, 999999);
  check(graph.timeCursor(1) == store.duration(), "B past the end: at the end");
  graph.setTimeCursor(1, 8500);
  check(std::isnan(graph.cursorLevel(1)), "B in the overload: no level");
  graph.clearTimeCursors();
  check(graph.timeCursor(0) < 0 && graph.timeCursor(1) < 0 && std::isnan(graph.cursorLevel(0)), "removed");

  // the mouse: a click is A, Shift and a click B, a cursor's line is dragged, a drag still moves the time
  QWidget *plotWidget = graph.childAt(plot.center() + QPoint(0, graph.timeBar()->height()));
  check(plotWidget != nullptr, "the plot under the mouse");
  if (plotWidget)
  {
    QTest::mouseClick(plotWidget, Qt::LeftButton, Qt::ShiftModifier, pixel(6));
    check(std::abs(graph.timeCursor(0) - 6000) < 100 && graph.timeCursor(1) < 0, QString("Shift+click without A: A (%1)").arg(graph.timeCursor(0)));
    QTest::mouseClick(plotWidget, Qt::LeftButton, Qt::NoModifier, pixel(4));
    check(std::abs(graph.timeCursor(0) - 4000) < 100, QString("a click: A at 4 s (%1)").arg(graph.timeCursor(0)));
    QTest::mouseClick(plotWidget, Qt::LeftButton, Qt::ShiftModifier, pixel(10));
    check(std::abs(graph.timeCursor(1) - 10000) < 100 && std::abs(graph.timeCursor(0) - 4000) < 100,
          QString("Shift+click: B at 10 s (%1)").arg(graph.timeCursor(1)));
    // dragging A's line
    const QPoint onA(graph.cursorPixel(0), plot.center().y());
    QTest::mousePress(plotWidget, Qt::LeftButton, Qt::NoModifier, onA);
    QTest::mouseMove(plotWidget, onA + QPoint(20, 0));
    QTest::mouseMove(plotWidget, pixel(15));
    QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::NoModifier, pixel(15));
    check(std::abs(graph.timeCursor(0) - 15000) < 100 && graph.timeAxisStart() == 0,
          QString("A dragged to 15 s, the time did not move (%1)").arg(graph.timeCursor(0)));
    // Escape: first the frame zoomed on, then the cursors
    graph.zoomToRect(QRect(plot.left(), plot.top(), plot.width() / 2, plot.height() / 2));
    graph.setFocus();
    QTest::keyClick(&graph, Qt::Key_Escape);
    check(!graph.zoomed() && graph.timeCursor(0) >= 0, "Escape takes the zoom back first");
    QTest::keyClick(&graph, Qt::Key_Escape);
    check(graph.timeCursor(0) < 0 && graph.timeCursor(1) < 0, "then the cursors");
  }

  // the statistics column: its switch, its text, the room it takes
  check(!graph.statsVisible(), "no statistics by default");
  const int width = graph.plotRect().width();
  int toggled = 0;
  QObject::connect(&graph, &QcpGraph::statsVisibleChanged, [&] { toggled++; });
  graph.setStatsVisible(true);
  QTest::qWait(30);
  check(graph.statsVisible() && toggled == 1 && graph.plotRect().width() < width - 100,
        QString("the column takes room from the plot (%1 of %2)").arg(graph.plotRect().width()).arg(width));
  graph.setTimeCursor(0, 2000);
  graph.setTimeCursor(1, 6000);
  const RecordingStore::Stats st = store.stats(2000, 6000);
  const QString stats = graph.statsText();
  check(stats.contains("A 0:00:02.000") && stats.contains("4.000 s") && stats.contains(QString::number(st.count))
        && stats.contains(GraphStats::columnScale({ 4.0, st.mean, st.min, st.max }, { st.sigma }, "V").text(st.mean)),
        "the graph's statistics from A to B are the store's");
  check(stats.contains("Shown") && stats.contains("1:00") && stats.contains("peak-peak"), "and over the time shown: " + stats.section("Shown", 1).left(40));
  graph.setStatsVisible(false);
  QTest::qWait(30);
  check(graph.plotRect().width() == width, "switched off: the plot has its width again");
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

  // three values of two meters on three axes
  Bench bench;
  bench.graph.setThemeColors(QColor(20, 24, 28), QColor(90, 100, 110), QColor(200, 205, 210), QColor(90, 200, 120));
  bench.feedA.step = bench.feedB.step = 100;
  bench.a.live();
  bench.b.live();
  bench.graph.setValues({ ValueRef(0, "device:a"), ValueRef(1, "device:a"), ValueRef(0, "device:b") });
  for (int i = 0; i < 450; ++i)
  {
    bench.feedA.other(1, 50 + 0.02 * std::sin(i * 0.02), "Hz", Quantity::Frequency);
    bench.feedA.value(0.02 * std::sin(i * 0.05));
    Reading r;
    r.value = 0.3 + 0.1 * std::cos(i * 0.03);
    r.text = QString::number(r.value);
    r.unit = r.baseUnit = "A";
    r.port.quantity = Quantity::Current;
    r.t = bench.feedB.now;
    r.msecs = Feed::wall(r.t).toMSecsSinceEpoch();
    bench.b.setReading(r);
    bench.feedB.now += 100;
  }
  bench.graph.refreshNow();
  QTest::qWait(50);
  bench.graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/qcpgraph_axes.png");
  // cursors and the statistics column
  bench.graph.resize(1100, 520);
  bench.graph.setStatsVisible(true);
  bench.graph.setTimeCursor(0, 12000);
  bench.graph.setTimeCursor(1, 31000);
  bench.graph.refreshNow();
  QTest::qWait(300);
  bench.graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/qcpgraph_stats.png");
  bench.graph.setStatsVisible(false);
  bench.graph.clearTimeCursors();
  bench.graph.resize(900, 400);
  // the same in every colour variant
  bench.graph.setTheme(PlotTheme{ QColor("#202428"), QColor("#606870"), QColor("#d0d4d8"), QColor("#40a0ff") });
  for (GraphLook::Variant v : GraphLook::variants())
  {
    bench.graph.setVariant(v);
    bench.graph.refreshNow();
    QTest::qWait(50);
    bench.graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/qcpgraph_look_" + GraphLook::name(v) + ".png");
  }
}

// GraphReference: the texts of the settings, crossings, where a drag puts the line
static void testReferenceRules()
{
  using namespace GraphReference;
  for (const QString &text : { QString(), QString("mean"), QString("min"), QString("max"), QString("off"), QString("alarm 2.5"), QString("-0.75") })
    check(setting(fromSetting(text)) == text, "the reference as read: '" + text + "' -> '" + setting(fromSetting(text)) + "'");
  check(fromSetting("nonsense").kind == Zero && fromSetting("0").kind == Zero, "nothing readable and 0: at 0");
  check(fromSetting("alarm 2.5").kind == Alarm && fromSetting("alarm 2.5").value == 2.5, "an alarm threshold keeps its value");
  check(line(Mean, 3) == line(Mean, 7) && !(line(Value, 3) == line(Value, 7)), "only Value and Alarm differ by their value");
  check(linesFromSetting("1.2 x 2.5 0 0.5 0.7") == QList<double>({ 1.2, 2.5, 0, 0.5 }), "added lines: the numbers, four at most");
  check(linesSetting({ 1.2, -2 }) == "1.2 -2", "added lines as text");

  // four periods of 1 + sin over 20 s: two crossings of 1.2 in each
  QList<QPointF> points;
  for (int i = 0; i < 200; ++i)
    points << QPointF(i * 0.1, 1.0 + std::sin((i + 0.5) * 2 * M_PI / 50));
  QList<Crossing> found = crossings(points, 0, 30, 0.1, 1.2);
  check(found.size() == 8 && found.first().up && !found.at(1).up, QString("1.2 crossed 8 times, up first: %1").arg(found.size()));
  check(crossings(points, 0, 30, 0.1, 2.5).isEmpty(), "2.5 never");
  check(crossings(points, 5, 10, 0.1, 1.2).size() == 2, "only those of the time shown");
  // a gap is no crossing; a noisy edge is one
  points = { QPointF(0, 0), QPointF(1, qQNaN()), QPointF(2, 2), QPointF(3, 0.9), QPointF(3.01, 1.1), QPointF(3.02, 0.9), QPointF(3.03, 1.1) };
  found = crossings(points, 0, 10, 0.5, 1);
  check(found.size() == 1 && found.first().up, QString("no crossing over a gap, a noisy edge once: %1").arg(found.size()));

  // a drag: snaps within 6 pixels, else a round value of its own
  const QList<Snap> snaps{ Snap{ Zero, 0 }, Snap{ Mean, 1.234 }, Snap{ Alarm, 2 } };
  check(dragged(0.05, 4, 0.01, snaps).kind == Zero, "snaps to 0");
  check(dragged(1.2, 4, 0.01, snaps).kind == Mean, "snaps to the mean");
  const Line alarm = dragged(1.95, 4, 0.01, snaps);
  check(alarm.kind == Alarm && alarm.value == 2, "snaps to an alarm threshold");
  const Line own = dragged(1.5678, 4, 0.01, snaps);
  check(own.kind == Value && std::abs(own.value - 1.55) < 1e-9, QString("else a value on a round step: %1").arg(own.value));

  GraphModel model;
  int changed = 0;
  QObject::connect(&model, &GraphModel::referenceChanged, [&] { changed++; });
  check(model.referenceSetting().isEmpty() && model.crossingsVisible() && model.referenceLines().isEmpty(), "a new model: the line at 0, crossings on");
  model.setReferenceSetting("mean");
  model.setReferenceSetting("mean");
  model.setReferenceLinesSetting("1.5 -2");
  model.setCrossingsVisible(false);
  check(changed == 3 && model.reference().kind == Mean && model.referenceLinesSetting() == "1.5 -2", "the model keeps them and says a change once");
}

static void testLevels()
{
  Bench bench;
  QcpGraph &graph = bench.graph;
  bench.a.live();
  bench.b.live();
  graph.setValues({ ValueRef(0, "device:a"), ValueRef(0, "device:b") });
  graph.setValueAxes({ { ValueRef(0, "device:b"), 1 } });
  Feed &feed = bench.feedA;
  feed.step = 100;
  bench.feedB.step = 100;
  // four periods of 1 + sin over 20 s, as the test of the graph on QtCharts
  for (int i = 0; i < 200; ++i)
  {
    feed.value(1.0 + std::sin((i + 0.5) * 2 * M_PI / 50));
    bench.feedB.value(5);
  }
  graph.setFixedRanges({ { "V", { -1.0, 3.0 } } });
  graph.refreshNow();
  check(graph.leftValue() == ValueRef(0, "device:a"), "the left value is the first one's");
  check(graph.reference().kind == GraphReference::Zero && graph.referenceValue() == 0 && graph.referenceLabel().startsWith("0 V"),
        "the reference line at 0 by default: " + graph.referenceLabel());

  int changed = 0;
  QObject::connect(&graph, &QcpGraph::referenceChanged, [&] { changed++; });
  graph.setReference(GraphReference::line(GraphReference::Off));   // the minimum touches 0 V
  check(std::isnan(graph.referenceValue()) && changed == 1, "off: no line");
  graph.setReferenceLines({ 1.2, 2.5, 0.0, 0.5, 0.7, 0.9 });
  check(graph.referenceLines() == QList<double>({ 1.2, 2.5, 0.0, 0.5 }), "added lines: four at most");
  check(graph.crossingCount(0) == 8 && graph.crossingCount(1) == 0,
        QString("1.2 V crossed 8 times, 2.5 V never: %1, %2").arg(graph.crossingCount(0)).arg(graph.crossingCount(1)));
  check(graph.referenceLabel(0) == QString::fromUtf8("1.2 V · 8×") && graph.referenceLabel(1) == "2.5 V",
        "the label counts them: " + graph.referenceLabel(0) + " / " + graph.referenceLabel(1));
  graph.setCrossingsVisible(false);
  check(graph.crossingCount(0) == 0 && graph.referenceLabel(0) == "1.2 V", "crossings off: none counted");
  graph.setCrossingsVisible(true);

  // the lines that follow the readings; none counted on them
  graph.setReference(GraphReference::line(GraphReference::Mean));
  const RecordingStore::Stats st = bench.a.stats(0, 60000);
  check(std::abs(graph.referenceValue() - st.mean) < 1e-9 && graph.crossingCount(-1) == 0 && graph.referenceLabel().startsWith("mean "),
        "the mean of the time shown, not counted: " + graph.referenceLabel());
  graph.setReference(GraphReference::line(GraphReference::Max));
  check(std::abs(graph.referenceValue() - st.max) < 1e-9, "the maximum");
  feed.value(2.4);
  bench.feedB.value(5);
  graph.refreshNow();
  check(std::abs(graph.referenceValue() - 2.4) < 1e-9, QString("it follows the readings: %1").arg(graph.referenceValue()));
  graph.setReference(GraphReference::line(GraphReference::Value, 0.5));
  check(graph.crossingCount(-1) == 8, QString("the reference line at 0.5 V is counted too: %1").arg(graph.crossingCount(-1)));
  graph.setReference(GraphReference::line(GraphReference::Value, 7));
  check(std::isnan(graph.referenceValue()), "a line outside the scale is not drawn");

  // alarm thresholds: a line on one that went stays where it is
  graph.setAlarmThresholds({ 2.0, -0.5 });
  graph.setReference(GraphReference::line(GraphReference::Alarm, 2.0));
  check(graph.referenceLabel().startsWith("alarm 2 V"), "on an alarm threshold: " + graph.referenceLabel());
  graph.setAlarmThresholds({ -0.5 });
  check(graph.reference().kind == GraphReference::Value && graph.reference().value == 2.0, "the threshold went: a value of its own");

  // the limit band: of the left value only, an open side to the edge
  double low = 0, high = 0;
  check(!graph.limitBand(&low, &high), "no limit band by default");
  graph.setLimits(ValueRef(0, "device:a"), 0.5, 1.5);
  check(graph.limitBand(&low, &high) && low == 0.5 && high == 1.5, "the limit band of the left value");
  graph.setLimits(ValueRef(0, "device:a"), qQNaN(), 1.5);
  check(graph.limitBand(&low, &high) && std::isinf(low) && high == 1.5, "an open side reaches the edge");
  graph.setLimits(ValueRef(0, "device:a"), 5, 6);
  check(!graph.limitBand(&low, &high), "a band outside the scale: none");
  graph.setLimits(ValueRef(0, "device:b"), 0.5, 1.5);
  check(!graph.limitBand(&low, &high), "the band of another value: none");
  graph.setLimits(ValueRef(-1));

  // the start threshold of a recording waiting for it
  check(std::isnan(graph.triggerLevel()), "no start threshold while the start is manual");
  bench.a.setThresholds(0.25, 1.75);
  bench.a.setStartMode(RecordingStore::Raising);
  graph.refreshNow();
  check(graph.triggerLevel() == 1.75, QString("the raising threshold: %1").arg(graph.triggerLevel()));
  bench.a.setStartMode(RecordingStore::Falling);
  graph.refreshNow();
  check(graph.triggerLevel() == 0.25, "the falling one");
  bench.a.setStartMode(RecordingStore::Manual);
  graph.refreshNow();

  // the mouse: the reference line snaps, an added line takes a round value, no cursor is set
  const QRect plot = graph.plotRect();
  QTest::qWait(30);
  QWidget *plotWidget = graph.childAt(plot.center() + QPoint(0, graph.timeBar()->height()));
  check(plotWidget != nullptr, "the plot under the mouse");
  if (plotWidget)
  {
    auto drag = [&](int line, int toY)
    {
      const QPoint from(plot.left() + 40, graph.referencePixel(line)), to(plot.left() + 40, toY);
      QTest::mousePress(plotWidget, Qt::LeftButton, Qt::NoModifier, from);
      QTest::mouseMove(plotWidget, from + QPoint(0, toY > from.y() ? 10 : -10));
      QTest::mouseMove(plotWidget, to);
      QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::NoModifier, to);
    };
    // the axis is -1 .. 3 V over the plot's height
    auto pixelOf = [&](double v) { return plot.bottom() - qRound((v + 1) / 4 * plot.height()); };
    graph.setReference(GraphReference::line(GraphReference::Value, 2.0));
    drag(-1, pixelOf(0) + 3);
    check(graph.reference().kind == GraphReference::Zero, "dragged near 0: at 0");
    drag(-1, pixelOf(-0.5) - 2);
    check(graph.reference().kind == GraphReference::Alarm && graph.reference().value == -0.5, "dragged near an alarm threshold: on it");
    drag(-1, pixelOf(2.6));
    check(graph.reference().kind == GraphReference::Value && std::abs(graph.reference().value - 2.6) < 0.06,
          QString("dragged elsewhere: a value of its own (%1)").arg(graph.reference().value));
    drag(1, pixelOf(1.2));
    check(std::abs(graph.referenceLines().value(1) - 1.2) < 0.06 && graph.crossingCount(1) == 9,
          QString("an added line dragged to %1, crossed %2 times (8 of the sine, one of the 2.4 V after it)").arg(graph.referenceLines().value(1)).arg(graph.crossingCount(1)));
    check(graph.timeCursor(0) < 0 && graph.timeAxisStart() == 0, "dragging a line sets no cursor and moves no time");
    check(graph.roundedValueAt(plotWidget->mapToGlobal(QPoint(plot.left() + 40, pixelOf(1.0)))) == 1.0, "a line added under the mouse: a round value there");
  }

  if (qEnvironmentVariableIsSet("QTDMM_SHOT"))
  {
    graph.setThemeColors(QColor(20, 24, 28), QColor(90, 100, 110), QColor(200, 205, 210), QColor(90, 200, 120));
    graph.setReference(GraphReference::line(GraphReference::Value, 0.5));
    graph.setReferenceLines({ 1.2, 2.5 });
    graph.setLimits(ValueRef(0, "device:a"), 0.2, 1.8);
    bench.a.setStartMode(RecordingStore::Raising);
    graph.refreshNow();
    QTest::qWait(50);
    graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/qcpgraph_levels.png");
    bench.a.setStartMode(RecordingStore::Manual);
  }

  // a file has no left value of a meter: no alarms, no band
  RecordingStore file;
  Feed fileFeed(&file, 1000);
  file.start();
  for (int i = 0; i < 5; ++i)
    fileFeed.value(i);
  file.stop();
  graph.setStore(&file);
  graph.refreshNow();
  check(graph.leftValue().id < 0, "a file: no value of a meter on the left");
}

static void testMarks()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.start();
  QcpGraph graph;
  graph.resize(900, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  for (int i = 0; i < 20; ++i)
  {
    feed.value(i);
    if (i == 4)
      store.addMark(0xffff0000, "Alarm Peak");
  }
  store.stop();
  graph.refreshNow();
  const QRect plot = graph.plotRect();
  QWidget *plotWidget = graph.childAt(plot.center() + QPoint(0, graph.timeBar()->height()));

  // an alarm's mark: a line at its time, its flag below the plot
  check(graph.markCount() == 1 && graph.markTime(0) == 5000 && graph.markColor(0) == QColor(Qt::red) && !graph.markIsNote(0),
        QString("the store's mark at its time (%1 marks, %2 ms)").arg(graph.markCount()).arg(graph.markTime(0)));
  graph.setTimeCursor(0, 5000);
  const int at5 = graph.cursorPixel(0);
  check(graph.markPixel(0) == at5 && at5 > plot.left(), "its line where the time axis has 5 s");
  // the cursor far away: the name gets the room to the nearest line, and how much it needs is the font's
  graph.setTimeCursor(0, 40000);
  check(graph.markFlagText(0) == QString("1 · Peak"), "the flag: its number and its name without \"Alarm\": " + graph.markFlagText(0));
  graph.setTimeCursor(0, 9000);
  QRect flag = graph.markFlagRect(0);
  check(graph.markFlagText(0).startsWith("1"), "the flag: its number and its name without \"Alarm\": " + graph.markFlagText(0));
  check(flag.top() >= plot.bottom() && flag.left() <= at5 && flag.right() >= at5, "the flag below the plot, under its line");
  // the cursor's letter in the same row, not on the mark's flag
  const QRect letter = graph.cursorFlagRect(0);
  check(!letter.isEmpty() && letter.top() == flag.top() && !letter.intersects(flag) && graph.cursorFlagRect(1).isEmpty(),
        "the cursor's letter is a flag in the row of the marks'");
  check(graph.markAt(flag.center()) == 0 && graph.markAt(QPoint(at5, plot.center().y())) == -1,
        "an alarm's flag takes the click, its line does not");

  // a note: set, told, moved
  const int note = graph.addNote(12000, "Probe", "moved the probe\nto pin 3");
  check(note == 1 && graph.markCount() == 2 && graph.markIsNote(1) && graph.markTime(1) == 12000
        && store.marks().at(1).kind == RecordingStore::MarkKind::Note, "a note at 12 s is the second mark");
  check(graph.markTip(1).contains("2 · Probe") && graph.markTip(1).contains("to pin 3") && graph.markTip(1).contains("12:00:12"),
        "its tooltip: number, title, clock time, text: " + graph.markTip(1));
  check(graph.markAt(QPoint(graph.markPixel(1), plot.center().y())) == 1, "a note's line takes the click");
  const int movedTo = graph.moveMark(1, 2000);
  // the flag's text is cut to the room between the lines, which depends on the font: only its number
  check(movedTo == 0 && graph.markTime(0) == 2000 && graph.markIsNote(0) && !graph.markIsNote(1) && graph.markFlagText(1).startsWith("2"),
        QString("the note moved before the alarm: the marks are numbered by time (%1, %2 ms, \"%3\")").arg(movedTo).arg(graph.markTime(0)).arg(graph.markFlagText(1)));
  check(graph.moveMark(1, 8000) == 1 && graph.markTime(1) == 5000, "an alarm's mark stays where it is");
  check(graph.moveMark(0, 99000) == 1 && graph.markTime(1) == store.duration() && store.duration() >= 19000, "a note goes no further than the recording");
  graph.moveMark(1, 12000);

  // dragged with the mouse: by its line, Escape puts it back
  if (plotWidget)
  {
    int moved = 0;
    QDateTime from, to;
    const QMetaObject::Connection heard = QObject::connect(&graph, &QcpGraph::noteMoved, &graph, [&](const QDateTime &a, const QDateTime &b, const QString &)
    {
      ++moved;
      from = a;
      to = b;
    });
    const QPoint on(graph.markPixel(1), plot.center().y());
    QTest::mousePress(plotWidget, Qt::LeftButton, Qt::NoModifier, on);
    QTest::mouseMove(plotWidget, on + QPoint(40, 0));
    check(graph.markTime(1) > 12000, QString("the note follows the mouse (%1 ms)").arg(graph.markTime(1)));
    QTest::keyClick(&graph, Qt::Key_Escape);
    QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::NoModifier, on + QPoint(40, 0));
    check(graph.markTime(1) == 12000 && moved == 0 && graph.timeCursor(0) == 9000, "Escape: the note is back, nothing else happened");
    // by its flag, to where the mouse is let go
    const QPoint onFlag = graph.markFlagRect(1).center();
    QTest::mousePress(plotWidget, Qt::LeftButton, Qt::NoModifier, onFlag);
    QTest::mouseMove(plotWidget, onFlag + QPoint(20, 0));
    QTest::mouseMove(plotWidget, onFlag + QPoint(40, 0));
    QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::NoModifier, onFlag + QPoint(40, 0));
    check(graph.markTime(1) > 12000 && moved == 1 && from == Feed::wall(12000) && to == Feed::wall(graph.markTime(1)),
          QString("dragged by its flag: noteMoved() from 12 s to %1 ms").arg(graph.markTime(1)));
    // an alarm's flag does not move
    const QPoint onAlarm = graph.markFlagRect(0).center();
    QTest::mousePress(plotWidget, Qt::LeftButton, Qt::NoModifier, onAlarm);
    QTest::mouseMove(plotWidget, onAlarm + QPoint(40, 0));
    QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::NoModifier, onAlarm + QPoint(40, 0));
    check(graph.markTime(0) == 5000 && moved == 1, "an alarm's flag cannot be dragged");
    QObject::disconnect(heard);   // it writes to this block's variables
  }
  else
    check(false, "the plot under the mouse");

  // asked for: the dialog's title becomes the note, the table hears of it
  {
    QString added;
    QDateTime when;
    const QMetaObject::Connection heard = QObject::connect(&graph, &QcpGraph::noteAdded, &graph, [&](const QDateTime &t, const QString &title)
    {
      when = t;
      added = title;
    });
    QTimer::singleShot(0, &graph, []
    {
      auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
      if (!dialog)
        return;
      dialog->findChild<QLineEdit *>()->setText("Asked");
      dialog->accept();
    });
    const int asked = graph.askNote(3000);
    check(asked == 0 && graph.markName(0) == "Asked" && added == "Asked" && when == Feed::wall(3000) && graph.markCount() == 3,
          QString("askNote(): a note at 3 s, noteAdded() (mark %1, \"%2\")").arg(asked).arg(added));
    // another title through the same dialog
    QTimer::singleShot(0, &graph, []
    {
      auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
      if (!dialog)
        return;
      dialog->findChild<QLineEdit *>()->setText("Renamed");
      dialog->accept();
    });
    check(graph.editMark(0) && store.marks().at(0).name == "Renamed", "editMark(): another title");
    graph.removeMark(0);
    check(graph.markCount() == 2 && graph.markName(0) == "Alarm Peak", "removeMark(): the note is gone");
    QObject::disconnect(heard);
  }
  check(graph.timeBar()->findChild<QToolButton *>("ui_graphNoteButton") != nullptr, "the time bar has the note button");

  // a mark that comes before the note while it is dragged: the drag keeps its note
  {
    int probe = -1;
    for (int i = 0; i < graph.markCount(); ++i)
      if (graph.markName(i) == "Probe")
        probe = i;
    const QPoint on(graph.markPixel(probe), plot.center().y());
    QTest::mousePress(plotWidget, Qt::LeftButton, Qt::NoModifier, on);
    QTest::mouseMove(plotWidget, on + QPoint(10, 0));
    store.addNote(1000, "Early", QString());
    QTest::mouseMove(plotWidget, on + QPoint(30, 0));
    QTest::mouseRelease(plotWidget, Qt::LeftButton, Qt::NoModifier, on + QPoint(30, 0));
    check(graph.markName(0) == "Early" && graph.markTime(0) == 1000 && graph.markName(probe + 1) == "Probe"
          && graph.markPixel(probe + 1) >= on.x() + 28,
          QString("a note coming meanwhile stays, the dragged one goes on (%1 px from %2)").arg(graph.markPixel(probe + 1)).arg(on.x()));
    graph.removeMark(0);
  }
  // a recording only looked at: no notes, the marks stay as they are
  graph.setMarksEditable(false);
  const int marksBefore = graph.markCount();
  graph.removeMark(0);
  check(graph.addNote(4000, "No", QString()) == -1 && graph.askNote(4000) == -1 && graph.markCount() == marksBefore
        && graph.moveMark(1, 1000) == 1 && !graph.timeBar()->findChild<QToolButton *>("ui_graphNoteButton")->isEnabled(),
        "not editable: no note, nothing removed or moved, the button is off");
  graph.setMarksEditable(true);

  // close marks: the flags do not cover each other, the later one loses its name first
  graph.addNote(5300, "Second", QString());
  graph.addNote(5600, "Third", QString());
  graph.refreshNow();
  bool apart = true;
  int shown = 0;
  for (int i = 0; i < graph.markCount(); ++i)
    for (int k = i + 1; k < graph.markCount(); ++k)
      apart = apart && !graph.markFlagRect(i).intersects(graph.markFlagRect(k));
  for (int i = 0; i < graph.markCount(); ++i)
    shown += !graph.markFlagRect(i).isEmpty();
  check(apart && shown >= 2 && graph.markCount() == 4, QString("flags of close marks stand apart (%1 of 4 shown)").arg(shown));
  if (qEnvironmentVariableIsSet("QTDMM_SHOT"))
  {
    graph.setTimeCursor(1, 15000);
    graph.refreshNow();
    QCoreApplication::processEvents();
    graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/qcpgraph_marks.png");
  }
  // out of the time shown: neither line nor flag
  graph.setWindowSeconds(5);
  graph.followEnd();
  graph.refreshNow();
  check(graph.markPixel(0) == -1 && graph.markFlagRect(0).isEmpty(), "a mark outside the time shown has no line and no flag");

  // two meters, the second 10 s later: its mark on the first one's time, its meter in the tooltip
  Bench bench;
  bench.a.live();
  bench.feedB.now = 10000;
  bench.b.live();
  bench.graph.setValues({ ValueRef(0, "device:a"), ValueRef(0, "device:b") });
  for (int i = 0; i < 15; ++i)
    bench.feedA.value(i);
  bench.a.addMark(0xffff0000, "Alarm A");
  for (int i = 0; i < 5; ++i)
    bench.feedB.value(100 + i);
  bench.b.addMark(0xff0000ff, "Alarm B");
  bench.graph.refreshNow();
  check(bench.graph.markCount() == 2 && bench.graph.markTime(0) == 15000 && bench.graph.markTime(1) == bench.b.marks().at(0).t + 10000,
        QString("the marks of both meters on the graph's time (%1, %2 ms)").arg(bench.graph.markTime(0)).arg(bench.graph.markTime(1)));
  check(bench.graph.markTip(0).contains("Meter A") && bench.graph.markTip(1).contains("Meter B"), "the tooltip says whose mark it is");
  // a note goes to the left value's recording
  check(bench.graph.addNote(4000, "Left", QString()) == 0 && bench.a.marks().size() == 2 && bench.b.marks().size() == 1,
        "a note goes to the recording of the left axis' value");
  // Live goes on: the mark moves left with the readings
  const int before = bench.graph.markPixel(2);
  for (int i = 0; i < 60; ++i)
    bench.feedA.value(i);
  bench.graph.refreshNow();
  check(before > 0 && bench.graph.markPixel(2) > 0 && bench.graph.markPixel(2) < before,
        QString("in Live the mark moves with its reading (%1 -> %2 px)").arg(before).arg(bench.graph.markPixel(2)));
  // held: a mark that comes is there all the same, by its place among them
  bench.graph.setHold(true);
  bench.a.addMark(0xffff0000, "Alarm held");
  check(bench.graph.markCount() == 4, "a mark coming while the picture is held is counted");
}

static void testBandRules()
{
  // a zigzag of 4 points per column over a plot 100 pixels wide showing 0 .. 100 s
  QList<QPointF> points;
  for (int i = 0; i < 400; ++i)
    points << QPointF(i * 0.25, i % 2 ? 10 + i / 100 : -10);
  const QRectF plot(50, 20, 100, 200);
  double lo = 0, hi = 0;
  check(GraphBand::columnAt(points, 0, 100, plot, 0, 42.5, &lo, &hi) && lo == -10 && hi == 11,
        QString("a column's band from its lowest to its highest point (%1 .. %2)").arg(lo).arg(hi));
  check(GraphBand::columnAt(points, 0, 100, plot, 4, 99.5, &lo, &hi) && lo == -10 && hi == 13,
        QString("widened by 4 columns it has their extremes (%1 .. %2)").arg(lo).arg(hi));
  QList<QPolygonF> band = GraphBand::columnBand(points, 0, 100, -20, 20, plot, 0);
  check(band.size() == 1 && band.first().boundingRect().top() >= plot.top() + 30 && band.first().boundingRect().bottom() <= plot.top() + 151,
        "one outline, between -10 and 13 on the plot");
  // a gap parts it, and no column takes extremes from across it
  points[200] = QPointF(50, qQNaN());
  points[199] = QPointF(49.75, 100);
  band = GraphBand::columnBand(points, 0, 100, -20, 20, plot, 4);
  check(band.size() == 2, QString("a gap parts the band (%1 outlines)").arg(band.size()));
  check(GraphBand::columnAt(points, 0, 100, plot, 4, 51.5, &lo, &hi) && hi < 100 && GraphBand::columnAt(points, 0, 100, plot, 4, 48.5, &lo, &hi)
        && hi == 100, "the spike before the gap widens only the columns on its side");
  check(GraphBand::columnBand({}, 0, 100, -20, 20, plot, 0).isEmpty() && GraphBand::columnBand(points, 0, 100, 5, 5, plot, 0).isEmpty(),
        "no points or no scale: no band");
}

static void testOverview()
{
  RecordingStore store;
  Feed feed(&store, 1000);
  store.start();
  QcpGraph graph;
  graph.resize(900, 400);
  graph.show();
  graph.setWindowSeconds(60);
  graph.setStore(&store);
  // 0 .. 99 V, HOLD from 30 to 35 s, an overload at 40 s
  for (int i = 0; i < 100; ++i)
  {
    if (i >= 30 && i < 35)
    {
      Reading r;
      r.value = i;
      r.text = QString::number(i);
      r.unit = r.baseUnit = "V";
      r.port.quantity = Quantity::Voltage;
      r.hold = true;
      r.flags = quint32(SampleFlag::Hold);
      r.t = feed.now;
      r.msecs = Feed::wall(feed.now).toMSecsSinceEpoch();
      store.setReading(r);
      feed.now += 1000;
    }
    else
      feed.value(i == 40 ? qQNaN() : i);
  }
  store.stop();
  graph.showFrom(20000);
  graph.refreshNow();

  // the overview: on by default instead of the scrollbar, the window between its brackets
  GraphMinimap *minimap = graph.minimap();
  check(graph.minimapVisible() && minimap->isVisible() && !graph.scrollBar()->isVisible(), "the overview is shown instead of the scrollbar");
  check(minimap->viewStart() == 20000 && minimap->viewWidth() == 60000, QString("it has the window shown (%1 + %2 ms)").arg(minimap->viewStart()).arg(minimap->viewWidth()));
  Q_EMIT minimap->moveRequested(5000);
  graph.refreshNow();
  check(graph.windowStart() == 5000 && minimap->viewStart() == 5000, "a drag in it moves the window");
  Q_EMIT minimap->widthRequested(10000, 30000, false);
  graph.refreshNow();
  check(graph.windowSeconds() == 30 && graph.windowStart() == 10000 && !graph.following(), "a bracket dragged: another width");
  Q_EMIT minimap->endRequested();
  graph.refreshNow();
  check(graph.following(), "brought to the end it follows the newest reading");
  graph.setMinimapVisible(false);
  check(!minimap->isVisible() && graph.scrollBar()->isVisible() && !graph.timeBar()->findChild<QToolButton *>("ui_graphOverviewButton")->isChecked(),
        "switched off: the scrollbar again, the button up");
  graph.timeBar()->findChild<QToolButton *>("ui_graphOverviewButton")->click();
  check(graph.minimapVisible() && minimap->isVisible(), "the button brings it back");

  // the states of the left value in the time shown
  graph.setWindowSeconds(60);
  graph.showFrom(20000);
  graph.refreshNow();
  check(graph.statesText() == "HOLD 30.0-35.0 | OL 40.0-41.0", "the states in the window: " + graph.statesText());
  auto *strip = graph.findChild<QWidget *>("ui_graphStates");
  // the layout after the overview came back, then the picture that places the strip
  QCoreApplication::processEvents();
  graph.refreshNow();
  QCoreApplication::processEvents();
  const QRect plot = graph.plotRect();
  check(strip && strip->isVisible() && strip->geometry().bottom() == plot.bottom() && strip->geometry().left() == plot.left()
        && strip->width() == plot.width(),
        QString("the strip lies at the bottom of the plot, as wide as it (%1 %2 %3 %4, plot %5 %6 %7)").arg(strip ? strip->isVisible() : -1)
          .arg(strip->geometry().left()).arg(strip->geometry().bottom()).arg(strip->width()).arg(plot.left()).arg(plot.bottom()).arg(plot.width()));
  graph.setWindowSeconds(30);
  graph.showFrom(50000);
  graph.refreshNow();
  check(graph.statesText().isEmpty(), "none in another window: " + graph.statesText());
  graph.setStatesVisible(false);
  check(strip && !strip->isVisible() && !graph.statesVisible(), "the strip switched off");
  graph.setStatesVisible(true);

  // the min/max band behind the curve: off by default, the extremes of 4 columns either side
  graph.showFrom(0);
  graph.refreshNow();
  double lo = 0, hi = 0;
  check(!graph.envelopeVisible() && !graph.bandAt(-1, 10000, &lo, &hi), "no min/max band by default");
  graph.setEnvelopeVisible(true);
  graph.refreshNow();
  const double perColumn = double(graph.windowSeconds()) / plot.width();
  check(graph.bandAt(-1, 10500, &lo, &hi) && lo <= 10.5 - 3.9 * perColumn && lo >= 10.5 - 6 * perColumn && hi >= 10.5 + 3.9 * perColumn
        && hi <= 10.5 + 6 * perColumn, QString("the min/max band at 10.5 s spans the curve 4 columns either side (%1 .. %2)").arg(lo).arg(hi));
  check(graph.timeBar()->findChild<QToolButton *>("ui_graphEnvelopeButton")->isChecked(), "its button is down");
  graph.setEnvelopeVisible(false);

  // dense: more points than columns is a band, a narrower window a line again
  RecordingStore fast;
  Feed fastFeed(&fast, 10);
  fast.start();
  for (int i = 0; i < 12000; ++i)
    fastFeed.value(i % 2 ? 5 + std::sin(i / 300.0) : -5);
  fast.stop();
  graph.setStore(&fast);
  graph.setWindowSeconds(120);
  graph.showFrom(0);
  graph.refreshNow();
  check(graph.curveBanded(0) && graph.drawnPoints(0).isEmpty(), QString("12000 readings on %1 columns are a band, no line").arg(plot.width()));
  check(graph.bandAt(0, 60000, &lo, &hi) && lo >= -5 && hi <= 6 && hi - lo > 5, QString("the band spans what the curve covers in its column (%1 .. %2)").arg(lo).arg(hi));
  if (qEnvironmentVariableIsSet("QTDMM_SHOT"))
  {
    QCoreApplication::processEvents();
    graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/qcpgraph_band.png");
  }
  graph.setWindowSeconds(2);
  graph.showFrom(60000);
  graph.refreshNow();
  check(!graph.curveBanded(0) && !graph.drawnPoints(0).isEmpty() && !graph.bandAt(0, 61000, &lo, &hi), "200 readings in the window: a line again");

  if (qEnvironmentVariableIsSet("QTDMM_SHOT"))
  {
    graph.setStore(&store);
    graph.setWindowSeconds(60);
    graph.showFrom(20000);
    graph.setEnvelopeVisible(true);
    graph.refreshNow();
    QCoreApplication::processEvents();
    graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/qcpgraph_overview.png");
  }
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
  testLook();
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
  testValues();
  testFourUnits();
  testOtherTime();
  testParts();
  testGraphLook();
  testStoreDeleted();
  testStatsText();
  testCrosshairAndCursors();
  testReferenceRules();
  testLevels();
  testMarks();
  testBandRules();
  testOverview();
  shot();
  return failed ? 1 : 0;
}
