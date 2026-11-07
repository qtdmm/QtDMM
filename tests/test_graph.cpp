// Baseline behavior tests for GraphWidget, written before the planned QtGraph-based
// rewrite so the current CSV import/export contract has a regression net to
// compare the replacement against.
#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>

#include "ui/frameclock.h"
#include "ui/views/graphwidget.h"
#include "ui/views/graphflags.h"
#include "ui/views/graphminimap.h"
#include "recording/recordingfile.h"
#include "recording/recordingstore.h"
#include <QChartView>
#include <QValueAxis>
#include <QGraphicsSimpleTextItem>
#include <QXYSeries>
#include <QScatterSeries>
#include <QScrollBar>
#include <QToolButton>
#include "core/nicestep.h"
#include "core/siprefix.h"
#include "ui/engnumbervalidator.h"
#include "core/settings.h"
#include "testutil.h"

// The lines of a path at least @p length long, horizontal (their y) or
// vertical (their x): phosphor's lines, not its ticks
static QList<double> pathLines(const QPainterPath &path, bool horizontal, double length)
{
  QList<double> out;
  for (int i = 1; i < path.elementCount(); ++i)
  {
    const QPainterPath::Element a = path.elementAt(i - 1), b = path.elementAt(i);
    if (!a.isMoveTo() || !b.isLineTo())
      continue;
    if (horizontal && std::abs(a.y - b.y) < 0.01 && std::abs(b.x - a.x) >= length)
      out << a.y;
    else if (!horizontal && std::abs(a.x - b.x) < 0.01 && std::abs(b.y - a.y) >= length)
      out << a.x;
  }
  return out;
}

// Reads a whole file as text (used to compare export output byte-for-byte).
// Text mode mirrors the exporter, which writes "\r\n" on Windows.
static QString readFile(const QString &fileName)
{
  QFile f(fileName);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return QString();
  return QString::fromUtf8(f.readAll());
}

// The readings a MeterController gives a store, on a clock of the test's
// own: value() is a main reading now, then the clock moves on by a step
// (0.1 s by default). NaN is an overload.
struct Feed
{
  RecordingStore *store;
  qint64 now = 0;
  qint64 step;

  explicit Feed(GraphWidget &graph, qint64 stepMs = 100) : Feed(graph.store(), stepMs) {}
  explicit Feed(RecordingStore *s, qint64 stepMs = 100) : store(s), step(stepMs)
  {
    store->setClock([this] { return now; }, [this] { return wall(now); });
  }
  Feed(const Feed &) = delete;
  Feed &operator=(const Feed &) = delete;

  static QDateTime wall(qint64 t) { return QDateTime(QDate(2026, 10, 4), QTime(12, 0, 0)).addMSecs(t); }
  void value(double v)
  {
    Reading r;
    r.overload = std::isnan(v);
    r.value = v;
    r.text = r.overload ? QStringLiteral("OL") : QString::number(v);
    r.t = now;
    r.msecs = wall(now).toMSecsSinceEpoch();
    store->setReading(r);
    now += step;
  }
  /// Another value of the meter (a second display) now, without moving the clock.
  void other(int id, double v, const QString &unit, Quantity quantity, const QString &name = {})
  {
    Reading r;
    r.id = id;
    r.name = name;
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

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");

  QApplication app(argc, argv);

  if (argc != 2)
  {
    qCritical() << "Usage: test_graph <data-dir>";
    return 1;
  }

  QString dataDir = QString::fromLocal8Bit(argv[1]);
  QTemporaryDir tmpDir;
  check(tmpDir.isValid(), "could not create temp dir for test settings");

  Settings settings("test_graph", tmpDir.path());

  // --- 1. all known fixture formats (legacy tab-separated, legacy with nan,
  //         new semicolon CSV, CSV with comma-decimal ms, larger recording)
  //         must import without error ---
  const QStringList fixtures = {
    "legacy.txt", "legacy_nan.txt", "new_dpoint.csv", "new_komma_ms.csv", "new_larger.csv"
  };

  for (const QString &name : fixtures)
  {
    GraphWidget graph(nullptr, &settings);
    QString path = dataDir + "/" + name;
    bool ok = graph.importCsvFile(path);
    check(ok, QString("importCsvFile() failed for fixture '%1'").arg(name));
  }

  // --- 1b. an import keeps all of the file and leaves the window as it
  //          was; the time buttons go up to the file's length ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setGraphSize(60);
    check(graph.importCsvFile(dataDir + "/new_larger.csv"), "import size: import failed");
    const int count = graph.store()->count();
    const double seconds = (count - 1) * graph.store()->sampleTime() / 10.0;
    const double limit = graph.store()->lengthLimit() / 1000.0;
    check(count > 1 && graph.store()->origin() == 0 && limit >= seconds && limit < seconds + 1,
          QString("import size: %1 points, limit %2 s for %3 s").arg(count).arg(limit).arg(seconds));
    check(graph.store()->state() == RecordingStore::View, "import size: viewed");
  }

  // --- 2. malformed input must be rejected, not crash or half-import ---
  {
    QTemporaryDir badDir;
    QString badFile = badDir.path() + "/broken.csv";
    QFile f(badFile);
    check(f.open(QIODevice::WriteOnly), "temp file opens for writing");
    QTextStream(&f) << "this is not a valid QtDMM export\n";
    f.close();

    GraphWidget graph(nullptr, &settings);
    bool ok = graph.importCsvFile(badFile);
    check(!ok, "importCsvFile() should reject a non-matching file, but reported success");
  }

  // --- 3. import -> export -> re-import must round-trip to the same CSV
  //         (this is the contract the QtGraph replacement needs to preserve) ---
  {
    GraphWidget graph(nullptr, &settings);
    check(graph.importCsvFile(dataDir + "/new_larger.csv"), "round-trip: initial import failed");

    QTemporaryDir outDir;
    QString exported1 = outDir.path() + "/export1.csv";
    check(graph.exportCsvFile(exported1), "round-trip: first export failed");

    GraphWidget graph2(nullptr, &settings);
    check(graph2.importCsvFile(exported1), "round-trip: re-import of exported file failed");

    QString exported2 = outDir.path() + "/export2.csv";
    check(graph2.exportCsvFile(exported2), "round-trip: second export failed");

    QString c1 = readFile(exported1);
    QString c2 = readFile(exported2);
    check(!c1.isEmpty() && c1 == c2,
          "round-trip: re-exporting a re-imported file produced different CSV content");
  }

  // --- 3a. the export has the other values the graph shows, a column
  //         group each; an import brings them back as series ---
  {
    Settings cfg("export-values", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.setUnit("V");
    Feed feed(graph);
    graph.startSLOT();
    // the first one comes before the main value: no time axis yet, dropped
    for (int i = 0; i < 20; ++i)
    {
      feed.other(1, 50 + i, "Hz", Quantity::Frequency);
      feed.value(1.0 + i * 0.1);
    }
    auto exported = [&](const QString &name)
    {
      const QString path = tmpDir.path() + "/" + name;
      QFile f(path);
      return graph.exportCsvFile(path, true) && f.open(QIODevice::ReadOnly | QIODevice::Text)
               ? QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts)
               : QStringList();
    };
    const QStringList hidden = exported("values_hidden.csv");
    check(hidden.value(0) == "timestamp;time (s);value;unit;status", "export: a hidden value is not exported, " + hidden.value(0));
    graph.setValueShown(1, true);
    const QStringList shown = exported("values_shown.csv");
    check(shown.value(0) == "timestamp;time (s);value;unit;status;value 2;unit 2;status 2" && shown.size() == 21
            && shown.value(1).endsWith(";1;V;ok;;;") && shown.value(2).endsWith(";51;Hz;ok"),
          "export: the value shown in a column group of its own, " + shown.value(0) + " | " + shown.value(1) + " | " + shown.value(2));
    GraphWidget back(nullptr, &cfg);
    back.resize(800, 500);
    check(back.importCsvFile(tmpDir.path() + "/values_shown.csv") && back.store()->extraCount() == 1
            && back.store()->extra(0).unit == "Hz" && back.store()->extra(0).count() == 19,
          "import: value 2 comes back as a series");
    // shown again, so the file goes out as it came in
    check(back.valueShown(ValueRef(1, back.baseSource())), "import: value 2 shown");
    const QString again = tmpDir.path() + "/values_again.csv";
    check(back.exportCsvFile(again) && readFile(again).section('\n', 0, 0) == shown.value(0),
          "import, export: the same columns, " + readFile(again).section('\n', 0, 0));
  }

  // --- 3b. time buttons (All / 1 min / 5 min / 30 min) top right ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(10);   // one sample per second
    graph.setGraphSize(300);
    const QList<QToolButton *> buttons = graph.findChildren<QToolButton *>();
    auto button = [&](const QString &text) -> QToolButton *
    {
      for (QToolButton *b : buttons)
        if (b->text() == text)
          return b;
      return nullptr;
    };
    QToolButton *all = button("All"), *one = button("1 min"), *five = button("5 min"), *thirty = button("30 min");
    check(all && one && five && thirty, "time buttons: All, 1 min, 5 min, 30 min exist");
    if (all && one && five && thirty)
    {
      check(five->isChecked() && !one->isChecked() && !all->isChecked(), "time buttons: a 300 s window marks 5 min");
      QSignalSpy spy(&graph, &GraphWidget::windowRequested);
      one->click();
      check(spy.size() == 1 && spy.last().at(0).toInt() == 60, "time buttons: 1 min asks for a 60 s window");
      // InstanceWidget applies the request through the settings, like a zoom
      graph.setGraphSize(60);
      check(one->isChecked() && !five->isChecked(), "time buttons: the applied window is marked");
      graph.setGraphSize(45);
      check(!one->isChecked() && !five->isChecked() && !thirty->isChecked(), "time buttons: a zoomed window marks none");
      graph.setGraphSize(60);
      graph.setSampleLength(6000);   // a recording of 10 min, waited for in Live
      graph.liveSLOT();
      check(thirty->isHidden() && !five->isHidden(), "time buttons: 30 min is hidden for a 10 min recording");
      graph.setSampleLength(0);
      check(!thirty->isHidden(), "time buttons: 30 min is back without a length");

      // All: the recording so far (at least 10 s), growing with it
      spy.clear();
      graph.setGraphSize(10);
      all->click();
      check(all->isChecked(), "time buttons: All stays marked");
      Feed feed(graph);
      graph.startSLOT();
      for (int i = 0; i < 150; ++i)   // 15 s
        feed.value(1.0);
      check(graph.windowSeconds() > 10 && graph.windowSeconds() <= 600,
            QString("time buttons: All grows the window with the recording (%1 s)").arg(graph.windowSeconds()));
      check(spy.isEmpty(), QString("time buttons: All keeps its window out of the settings (%1 requests)").arg(spy.size()));
      // a zoom starts from the window shown: it goes to the settings first
      const int shown = graph.windowSeconds();
      graph.zoomInSLOT();
      check(spy.size() == 1 && spy.last().at(0).toInt() == shown,
            QString("time buttons: a zoom from All hands its window to the settings (%1)").arg(spy.size()));
      graph.setGraphSize(8);
      check(!all->isChecked(), "time buttons: zooming ends All");
    }
  }

  // --- 3c. All on a longer recording: from one minute on it asks for whole
  //         minutes (the settings keep seconds only up to 99999; an odd
  //         value above was cut and asked for again with every sample) ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(10);   // one sample per second
    graph.setGraphSize(10);
    QToolButton *all = nullptr;
    for (QToolButton *b : graph.findChildren<QToolButton *>())
      if (b->text() == "All")
        all = b;
    check(all, "All on a long recording: button exists");
    if (all)
    {
      all->click();
      QList<int> requests;   // the windows All went through
      Feed feed(graph);
      graph.startSLOT();
      for (int i = 0; i < 3000; ++i)   // 300 s
      {
        feed.value(1.0);
        if (requests.isEmpty() || requests.last() != graph.windowSeconds())
          requests << graph.windowSeconds();
      }
      bool minutes = !requests.isEmpty();
      for (int seconds : requests)
        minutes = minutes && (seconds <= 60 || seconds % 60 == 0);
      check(minutes, QString("All on a long recording: whole minutes above 60 s (%1)").arg(
              [&] { QStringList l; for (int r : requests) l << QString::number(r); return l.join(' '); }()));
      check(requests.size() < 20, QString("All on a long recording: grows in steps, not per sample (%1 requests)").arg(requests.size()));
    }
  }

  // --- 4. regression test for the CSV-import sample-time bug (dmmgraph.cpp):
  //         m_sampleTime used to be computed by summing a growing offset on
  //         every row instead of once after the loop, inflating it with row
  //         count. For new_larger.csv (~271 rows over ~54s) the correct
  //         sample interval is on the order of the actual ~0.2s spacing,
  //         not something that grows with the number of rows. ---
  {
    GraphWidget graph(nullptr, &settings);
    QSignalSpy spy(&graph, &GraphWidget::sampleTime);
    check(graph.importCsvFile(dataDir + "/new_larger.csv"), "sampleTime regression: import failed");
    check(spy.count() == 1, "sampleTime regression: expected exactly one sampleTime() signal");
    if (spy.count() == 1)
    {
      int sampleTime = spy.at(0).at(0).toInt();
      // buggy version summed ~271 growing offsets -> computed a wildly larger
      // value than the true ~54s / 271 rows interval; a healthy value stays small.
      check(sampleTime == 2,
            QString("sampleTime regression: got sampleTime %1 for a 54s/272-row recording, expected 2 (0.2 s)").arg(sampleTime));
    }
  }

  // --- 4b. slower recordings: the sample time must follow the timestamps,
  //         not collapse to 0.1 s (a 5 min / 0.5 s recording used to import
  //         as one minute) ---
  {
    QTemporaryDir dir;
    const QString slow = dir.path() + "/slow.csv";
    QFile f(slow);
    check(f.open(QIODevice::WriteOnly | QIODevice::Text), "slow: create fixture");
    QTextStream out(&f);
    out << "timestamp;time (s);value;unit\n";
    QDateTime t0(QDate(2026, 9, 20), QTime(14, 2, 17, 385));
    for (int i = 0; i < 600; ++i)
      out << t0.addMSecs(i * 500).toString("yyyy-MM-ddTHH:mm:ss,zzz") << ";" << i * 0.5 << ";12.0;V\n";
    f.close();

    GraphWidget graph(nullptr, &settings);
    QSignalSpy spy(&graph, &GraphWidget::sampleTime);
    check(graph.importCsvFile(slow), "slow: import failed");
    check(spy.count() == 1 && spy.at(0).at(0).toInt() == 5,
          QString("slow: sample time %1, expected 5 (0.5 s)").arg(spy.count() ? spy.at(0).at(0).toInt() : -1));

    // and it exports with the original spacing
    const QString back = dir.path() + "/back.csv";
    check(graph.exportCsvFile(back), "slow: export failed");
    const QStringList lines = readFile(back).split('\n', Qt::SkipEmptyParts);
    check(lines.size() == 601 && lines.last().startsWith("2026-09-20T14:07:16,885"),
          QString("slow: exported %1 lines, last '%2'").arg(lines.size()).arg(lines.value(lines.size() - 1).left(23)));
  }

  // --- 5. smoke test for the live recording against the Qt Charts series
  //         sync (rebuildSeries()/append()): must survive the store
  //         dropping its oldest readings without crashing, and
  //         setGraphSize() must be callable again afterwards while data
  //         already exists. ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(1);
    graph.setGraphSize(5);
    graph.store()->setMaxPoints(10);   // drops quickly
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph, 500);
    graph.startSLOT();

    for (int i = 0; i < 20; i++)
      feed.value(i * 0.1);

    check(graph.dirty(), "ring-buffer smoke test: expected graph to be marked dirty after recording");
    check(graph.store()->origin() > 0, "ring-buffer smoke test: the store must have dropped old readings");

    graph.setGraphSize(10);
    feed.value(1.23);
  }

  // --- 5a. integration: over time (unit x s) of the readings above the
  //          threshold, 0 at or below it - the first reading, too (it used
  //          to be the threshold itself, a spike at the left edge). A value
  //          counts for the time it held, so 0.8 adds from the 0.9 on; with
  //          the scale 10 a reading of 0.1 s adds its value ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(1);
    graph.setGraphSize(100);
    graph.setIntegration(true, 10.0, 0.5, 0.0);
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph);
    graph.startSLOT();
    for (double v : { 0.1, 0.2, 0.8, 0.9, 0.1, 0.6 })
      feed.value(v);
    const QList<QAbstractSeries *> series = graph.findChild<QChartView *>()->chart()->series();
    auto *integral = series.size() > 2 ? qobject_cast<QXYSeries *>(series[2]) : nullptr;   // data line, data points, integration
    QStringList got;
    if (integral)
      for (const QPointF &p : integral->points())
        got << QString::number(p.y());
    check(got.join(' ') == "0 0 0 0.8 0 0",
          QString("integration: expected '0 0 0 0.8 0 0', got '%1'").arg(got.join(' ')));
  }

  // --- 5b. window size and sample time in either order: InstanceWidget used to set
  //          the window (counted in samples) before the sample time, and any
  //          later x axis update showed it scaled by the ratio of the two ---
  {
    Settings cfg("ordertest", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    auto *x = qobject_cast<QValueAxis *>(graph.findChild<QChartView *>()->chart()->axes(Qt::Horizontal).first());
    graph.setGraphSize(600);   // with the default sample time of 0.1 s
    graph.setSampleTime(10);         // then 1 s
    // the error showed on the next x axis update, e.g. scrolling
    auto *bar = graph.findChild<QScrollBar *>(QString(), Qt::FindDirectChildrenOnly);   // not the chart view's
    bar->setValue(1);
    bar->setValue(0);
    check(qAbs(x->max() - x->min() - 599) < 1.5,
          QString("window stays 600 s after a new sample time, got %1 s").arg(x->max() - x->min()));
    graph.setSampleTime(10);         // unchanged: nothing to do
    graph.setGraphSize(300);
    check(qAbs(x->max() - x->min() - 299) < 1.5, QString("300 s window, got %1 s").arg(x->max() - x->min()));
  }

  // --- 5c-5h: the recorder behind the graph, through its public API ---

  // y values of series @p index (0 data line, 2 integration) as "1 4 8"
  auto seriesY = [](GraphWidget &graph, int index) -> QString
  {
    const QList<QAbstractSeries *> series = graph.findChild<QChartView *>()->chart()->series();
    auto *s = series.size() > index ? qobject_cast<QXYSeries *>(series[index]) : nullptr;
    QStringList got;
    if (s)
      for (const QPointF &p : s->points())
        got << QString::number(p.y());
    return got.join(' ');
  };
  auto seriesX = [](GraphWidget &graph) -> QString
  {
    auto *s = qobject_cast<QXYSeries *>(graph.findChild<QChartView *>()->chart()->series().value(0));
    QStringList got;
    if (s)
      for (const QPointF &p : s->points())
        got << QString::number(p.x());
    return got.join(' ');
  };

  // --- 5c. every reading is drawn, at its time; the sample time is the
  //          grid of the export only ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(2);          // a grid of 0.2 s
    graph.setGraphSize(100);
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph);
    graph.startSLOT();
    for (double v : { 1.0, 3.0, 5.0, 7.0, 9.0 })
      feed.value(v);
    check(seriesY(graph, 0) == "1 3 5 7 9",
          QString("every reading: expected '1 3 5 7 9', got '%1'").arg(seriesY(graph, 0)));
    check(seriesX(graph) == "0 0.1 0.2 0.3 0.4",
          QString("every reading: expected x '0 0.1 0.2 0.3 0.4', got '%1'").arg(seriesX(graph)));
    // not recording: readings pass by
    graph.stopSLOT();
    feed.value(100);
    feed.value(100);
    check(seriesY(graph, 0) == "1 3 5 7 9", "every reading: stopped recorder must not store");
    // the export: the start value, then the mean of each 0.2 s
    const QVector<double> grid = graph.store()->toRecording().values;
    QStringList g;
    for (double v : grid)
      g << QString::number(v);
    check(g.join(' ') == "1 2 6", "every reading: grid export got " + g.join(' '));
  }

  // --- 5d. start trigger, rising: starts on the reading that crosses the
  //          threshold from below and records it as the first sample; the
  //          very first reading (nothing to compare with) never triggers ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(1);
    graph.setGraphSize(100);
    graph.setThresholds(0.0, 1.0);
    graph.setMode(GraphWidget::Raising);
    QSignalSpy running(&graph, &GraphWidget::running);
    Feed feed(graph);
    graph.liveSLOT();            // the triggers wait in Live
    feed.value(1.5);             // first reading, above: no start
    feed.value(0.5);
    feed.value(0.8);
    check(running.isEmpty(), "rising trigger: started before the crossing");
    feed.value(1.0);             // crosses (>=)
    check(running.size() == 1 && running.first().first().toBool(), "rising trigger: did not start on the crossing");
    feed.value(2.0);
    check(seriesY(graph, 0) == "1 2", QString("rising trigger: expected '1 2', got '%1'").arg(seriesY(graph, 0)));
  }

  // --- 5e. start trigger, falling ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(1);
    graph.setGraphSize(100);
    graph.setThresholds(-1.0, 5.0);
    graph.setMode(GraphWidget::Falling);
    QSignalSpy running(&graph, &GraphWidget::running);
    Feed feed(graph);
    graph.liveSLOT();            // the triggers wait in Live
    feed.value(-2.0);            // first reading, below: no start
    feed.value(0.0);
    feed.value(-0.5);
    check(running.isEmpty(), "falling trigger: started before the crossing");
    feed.value(-1.0);            // crosses (<=)
    feed.value(-3.0);
    check(running.size() == 1, "falling trigger: did not start on the crossing");
    check(seriesY(graph, 0) == "-1 -3", QString("falling trigger: expected '-1 -3', got '%1'").arg(seriesY(graph, 0)));
  }

  // --- 5f. start trigger, clock time: starts within two seconds after the
  //          start time (the store's clock looks every second), not before ---
  {
    GraphWidget later(nullptr, &settings);
    later.setGraphSize(100);
    Feed laterFeed(later);
    later.setStartTime(Feed::wall(0).time().addSecs(120));
    later.setMode(GraphWidget::Time);
    later.liveSLOT();
    QSignalSpy notYet(&later, &GraphWidget::running);
    later.store()->poll();
    check(notYet.isEmpty(), "time trigger: started before the start time");

    GraphWidget now(nullptr, &settings);
    now.setGraphSize(100);
    Feed nowFeed(now);
    now.setStartTime(Feed::wall(0).time());
    now.setMode(GraphWidget::Time);
    now.liveSLOT();
    QSignalSpy started(&now, &GraphWidget::running);
    nowFeed.now = 1000;
    now.store()->poll();
    check(started.size() == 1, "time trigger: did not start at the start time");
  }

  // --- 5g. integral and marks when the store is full: the window moves on
  //          with what the store keeps, the integral goes on summing, a
  //          mark falls off with the readings around it ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(1);
    graph.setGraphSize(1);
    graph.store()->setMaxPoints(10);  // keeps 1 s
    graph.setIntegration(true, 10.0, 0.0, 0.0);
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph);
    graph.startSLOT();
    for (int i = 1; i <= 5; i++)
      feed.value(i);
    feed.now -= 100;                 // the mark comes with the reading of 5
    graph.addMark(Qt::red, "m");
    feed.now += 100;
    check(graph.markCount() == 1, "marks: addMark() did not add");
    for (int i = 6; i <= 15; i++)
      feed.value(i);
    auto *x = qobject_cast<QValueAxis *>(graph.findChild<QChartView *>()->chart()->axes(Qt::Horizontal).first());
    // as of the newest reading; the clock has moved on by one step since
    check(graph.store()->origin() > 0 && qAbs(x->min() - graph.store()->origin() / 1000.0) <= 0.1 + 1e-9,
          QString("full: the window starts at %1 s, the store keeps from %2 ms").arg(x->min()).arg(graph.store()->origin()));
    check(seriesY(graph, 0).endsWith("12 13 14 15"), "full: the newest are drawn, got " + seriesY(graph, 0));
    // the integral: 1 + ... + 14, each held 0.1 s, scaled by 10
    check(seriesY(graph, 2).endsWith(" 105"), QString("full: integral got '%1'").arg(seriesY(graph, 2)));
    for (int i = 16; i <= 30; i++)
      feed.value(i);                 // the mark's readings leave the store
    check(graph.markCount() == 0, "marks: still there after its readings left the store");
    graph.addMark(Qt::red, "m2");
    graph.clearSLOT();
    check(graph.markCount() == 0, "marks: clearSLOT() kept a mark");
  }

  // --- 5h. recording length: stops on its own after setSampleLength()
  //          (tenths of a second); 0 records until stopped ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(1);
    graph.setGraphSize(100);
    graph.setSampleLength(5);
    graph.setMode(GraphWidget::Manual);
    QSignalSpy running(&graph, &GraphWidget::running);
    Feed feed(graph);
    graph.startSLOT();
    for (int i = 1; i <= 8; i++)
      feed.value(i);
    check(running.size() == 2 && !running.last().first().toBool(),
          QString("length: expected start and stop, got %1 signals").arg(running.size()));
    check(seriesY(graph, 0) == "1 2 3 4 5",
          QString("length: expected '1 2 3 4 5', got '%1'").arg(seriesY(graph, 0)));

    graph.setSampleLength(0);
    graph.startSLOT();
    for (int i = 1; i <= 50; i++)
      feed.value(i);
    check(running.size() == 3, "length 0: must record until stopped");
  }

  // --- 5j. a store from outside (the MeterController's recorder): the graph
  //          shows it, takes what it already holds, and follows its signals ---
  {
    RecordingStore store;
    Feed feed(&store);
    store.setStartMode(RecordingStore::Manual);
    store.start();
    feed.value(1);
    feed.value(2);

    GraphWidget graph(nullptr, &settings);
    graph.setStore(&store);
    check(graph.store() == &store, "setStore: store() must return the new store");
    check(seriesY(graph, 0) == "1 2",
          QString("setStore: expected the store's '1 2', got '%1'").arg(seriesY(graph, 0)));
    feed.value(3);
    check(seriesY(graph, 0) == "1 2 3",
          QString("setStore: expected '1 2 3' after a new reading, got '%1'").arg(seriesY(graph, 0)));
    graph.stopSLOT();
    check(!store.isRunning(), "setStore: the graph's stop must reach the shared store");
    store.clear();
    check(seriesY(graph, 0).isEmpty(), "setStore: a cleared store must empty the graph");
  }

  // --- 5k. thinning: more readings in the window than pixel columns are
  //          drawn as a minimum and a maximum per column (a spike survives),
  //          the store keeps every reading, and growing the series reading
  //          by reading gives what a rebuild gives ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.resize(300, 200);
    graph.setSampleTime(1);
    graph.setGraphSize(200);            // 2000 readings in the window
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph);
    graph.startSLOT();
    const int samples = 2000;
    for (int i = 0; i < samples; i++)
      feed.value(i == 777 ? 1000.0 : i == 1234 ? -500.0 : (i * 37) % 101);

    QChart *chart = graph.findChild<QChartView *>()->chart();
    const double plotWidth = chart->plotArea().width();
    const int columns = int(plotWidth > 0 ? plotWidth : graph.width());
    auto *series = qobject_cast<QXYSeries *>(chart->series().value(0));
    const int points = series ? series->count() : -1;
    check(points > 0 && points <= 2 * columns,
          QString("thinning: expected at most %1 points, got %2").arg(2 * columns).arg(points));
    check(graph.store()->count() == samples,
          QString("thinning: the store must keep all %1 samples, has %2").arg(samples).arg(graph.store()->count()));
    // the grid of 0.1 s: one step for each reading, and the end of the last
    check(int(graph.store()->toRecording().values.size()) == samples + 1, "thinning: the export must have every step");

    double lo = 1e9, hi = -1e9;
    if (series)
      for (const QPointF &p : series->points())
      {
        lo = qMin(lo, p.y());
        hi = qMax(hi, p.y());
      }
    check(hi == 1000.0 && lo == -500.0, QString("thinning: spike and dip must be drawn, got %1 .. %2").arg(lo).arg(hi));

    const QString grown = seriesY(graph, 0) + "|" + seriesY(graph, 2);
    graph.setGraphSize(200);            // rebuilds the series
    check(grown == seriesY(graph, 0) + "|" + seriesY(graph, 2),
          "thinning: appended series must equal the rebuilt one");
  }

  // --- 5l. thinning while the full store moves on: a reading stays in its
  //          bucket (columns of time from the start), so the drawn minima
  //          and maxima of the older readings do not change from one new
  //          reading to the next ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.resize(300, 200);
    graph.setSampleTime(1);
    graph.setGraphSize(200);
    graph.store()->setMaxPoints(2000);  // keeps 200 s
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph);
    graph.startSLOT();
    auto noise = [](int i) { return double((i * 37) % 101); };
    int i = 0;
    for (; i < 2500; i++)
      feed.value(noise(i));

    QChart *chart = graph.findChild<QChartView *>()->chart();
    auto *series = qobject_cast<QXYSeries *>(chart->series().value(0));
    const RecordingStore *store = graph.store();
    // the drawn points by time, without the oldest and the newest 20 s (a
    // bucket falls off or fills there)
    auto drawn = [&](double from, double to)
    {
      QStringList list;
      for (const QPointF &p : series->points())
        if (p.x() >= from && p.x() < to)
          list << QString("%1:%2").arg(p.x()).arg(p.y());
      return list.join(' ');
    };
    const double from = store->origin() / 1000.0 + 20, to = store->duration() / 1000.0 - 20;
    const QString before = drawn(from, to);
    feed.value(noise(i));
    check(store->origin() > 0, "scrolling: the store must be full and move on");
    check(!before.isEmpty() && before == drawn(from, to),
          "scrolling: the thinned points of the older readings must stay as they were");
  }

  // --- 5m. gaps: a reading without a value (NaN: overload, stale) breaks
  //          the line - Qt Charts would draw straight across it - and has no
  //          point; growing reading by reading gives what a rebuild gives,
  //          thinned or not ---
  {
    // the drawn lines: the y values of each line segment that has points
    auto lines = [](GraphWidget &graph)
    {
      QStringList out;
      for (QAbstractSeries *a : graph.findChild<QChartView *>()->chart()->series())
      {
        auto *line = qobject_cast<QLineSeries *>(a);
        if (!line || line->count() == 0)
          continue;
        QStringList ys;
        for (const QPointF &p : line->points())
          ys << QString::number(p.y());
        out << ys.join(' ');
      }
      return out;
    };
    auto dataPoints = [](GraphWidget &graph)
    {
      return qobject_cast<QScatterSeries *>(graph.findChild<QChartView *>()->chart()->series().value(1))->count();
    };

    GraphWidget graph(nullptr, &settings);
    graph.resize(800, 300);
    graph.setSampleTime(1);
    graph.setGraphSize(100);
    graph.setMode(GraphWidget::Manual);
    graph.setIntegration(true, 1.0, 0.0, 0.0);   // a hidden integral is not drawn
    Feed feed(graph);
    graph.startSLOT();
    for (int i = 0; i < 30; i++)
      feed.value(i >= 10 && i < 20 ? qQNaN() : i);
    const QStringList grown = lines(graph);
    // data and integral, two segments each
    check(grown.size() == 4 && grown.first() == "0 1 2 3 4 5 6 7 8 9" && grown.contains("20 21 22 23 24 25 26 27 28 29"),
          "gaps: two segments, got " + grown.join(" | "));
    check(dataPoints(graph) == 20, QString("gaps: no point in the gap, got %1").arg(dataPoints(graph)));
    graph.setGraphSize(100);   // rebuilds the series
    check(lines(graph) == grown, "gaps: rebuilt = grown, got " + lines(graph).join(" | "));
    graph.clearSLOT();
    check(lines(graph).isEmpty() && dataPoints(graph) == 0, "gaps: cleared");

    // thinned: a bucket with a value draws it, a bucket of gaps is a gap
    GraphWidget thin(nullptr, &settings);
    thin.resize(300, 200);
    thin.setSampleTime(1);
    thin.setGraphSize(200);
    thin.setMode(GraphWidget::Manual);
    thin.setIntegration(true, 1.0, 0.0, 0.0);
    Feed thinFeed(thin);
    thin.startSLOT();
    for (int i = 0; i < 2000; i++)
      thinFeed.value((i / 300) % 2 ? qQNaN() : i % 7 == 3 ? qQNaN() : (i * 37) % 101);
    const QStringList thinGrown = lines(thin);
    check(thinGrown.size() == 2 * 4, QString("gaps thinned: 4 runs, data and integral, got %1").arg(thinGrown.size()));
    thin.setGraphSize(200);
    check(lines(thin) == thinGrown, "gaps thinned: rebuilt = grown");
  }

  // --- 5n0. Live starts with "All": the window as long as what there is
  //           (at least 10 s), growing with it ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setGraphSize(600);
    int asked = -1;
    QObject::connect(&graph, &GraphWidget::windowRequested, [&](int s) { asked = s; });
    graph.liveSLOT();
    QToolButton *all = nullptr;
    for (QToolButton *b : graph.findChildren<QToolButton *>())
      if (b->text() == "All")
        all = b;
    check(all && all->isChecked() && graph.windowSeconds() == 10,
          QString("live: starts with All, window %1 s").arg(graph.windowSeconds()));
    // review R9-03: the window of All is not the settings' one - it was
    // saved with them, 600 s became 10 s by starting and closing QtDMM
    check(asked == -1, QString("live: All asks the settings for nothing (%1)").arg(asked));
    graph.setGraphSize(600);   // OK in the settings dialog, the window unchanged
    check(all->isChecked() && graph.windowSeconds() == 10,
          QString("live: OK with the same window keeps All (%1 s)").arg(graph.windowSeconds()));
    graph.setGraphSize(300);   // another window on the Graph page
    check(!all->isChecked() && graph.windowSeconds() == 300,
          QString("live: another window from the settings ends All (%1 s)").arg(graph.windowSeconds()));
  }

  // --- 5n. a meter silent for minutes (every 2 s, then 5.5 min nothing),
  //          Live with "All": thinned, the column of the last value also
  //          holds the gap - the line must not bridge it, whatever the
  //          column width; and a reading is not drawn twice when the window
  //          grows with it ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.resize(220, 300);   // columns of several seconds, wider than the stale limit
    graph.setSampleTime(10);
    graph.setSampleLength(300000);   // 8:20:00
    graph.setGraphSize(60);
    // the window comes back at once, as through the settings
    QObject::connect(&graph, &GraphWidget::windowRequested, [&](int s) { graph.setGraphSize(s); });
    Feed feed(graph, 2000);
    graph.liveSLOT();
    for (QToolButton *b : graph.findChildren<QToolButton *>())
      if (b->text() == "All")
        b->click();
    auto *data = qobject_cast<QLineSeries *>(graph.findChild<QChartView *>()->chart()->series().first());
    auto bridged = [&](double from, double to)
    {
      for (QAbstractSeries *a : graph.findChild<QChartView *>()->chart()->series())
      {
        auto *line = qobject_cast<QLineSeries *>(a);
        if (!line || line->count() == 0 || line->pen() != data->pen())
          continue;
        if (line->at(0).x() < from && line->at(line->count() - 1).x() > to)
          return true;
      }
      return false;
    };
    int bridges = 0, doubled = 0;
    QString first;
    for (int i = 0; i < 570; i++)
    {
      if (i == 285)
      {
        // silent from 9:30: stale after the meter's interval, nothing until 15:00
        feed.now += 6000;
        graph.store()->setStale(true);
        feed.now = 900000;
        continue;
      }
      if (i > 285 && i < 450)
        continue;
      feed.value(650 + i * 0.3);
      if (i > 450 && bridged(570, 899))
      {
        bridges++;
        if (first.isEmpty())
          first = QString("reading %1").arg(i);
      }
      // the newest point once
      if (data->count() >= 2 && data->at(data->count() - 1) == data->at(data->count() - 2))
        doubled++;
    }
    check(bridges == 0, QString("silent meter: the line bridged the gap %1 times, first at %2").arg(bridges).arg(first));
    check(doubled == 0, QString("silent meter: the newest reading drawn twice %1 times").arg(doubled));
  }

  // --- 5n. the axis names the recording's unit: a meter switched from V to
  //          Ohm after a recording does not relabel it; a new one does ---
  {
    auto yTitle = [](GraphWidget &graph)
    {
      for (QGraphicsItem *item : graph.findChild<QChartView *>()->chart()->childItems())
        if (auto *text = dynamic_cast<QGraphicsSimpleTextItem *>(item); text && text->text().startsWith('['))
          return text->text();
      return QString();
    };
    GraphWidget graph(nullptr, &settings);
    graph.setUnit("mV");
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph);
    graph.startSLOT();
    feed.value(0.5);
    graph.stopSLOT();
    graph.setUnit("kOhm");
    // 0.5 V of a meter that measures ohms now: the prefix that suits the values
    check(yTitle(graph) == "[mV]" && graph.store()->unit() == "V", "unit: the recorded V stay V, got " + yTitle(graph));
    graph.startSLOT();
    feed.value(4700);   // ohms now; the prefix by the axis
    check(yTitle(graph) == "[kΩ]", "unit: a new recording is in kΩ as the meter shows, got " + yTitle(graph));
    // a unit of its own takes no prefix: 1500 ppm, not 1.5 kppm
    graph.setUnit("ppm");
    graph.startSLOT();
    for (int i = 0; i < 5; ++i)
      feed.value(1500 + i);
    check(yTitle(graph) == "[ppm]", "unit: ppm without a prefix, got " + yTitle(graph));
  }

  // --- 5n2. the y labels in the meter's prefix: 350 mV, not 0.35 V ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.resize(800, 500);
    graph.setUnit("mV");
    graph.setScale(true, true, 0, 0);
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph);
    graph.startSLOT();
    for (double v : { 0.05, 0.2, 0.35, -0.04 })
      feed.value(v);
    graph.show();
    QTest::qWait(50);
    QChart *chart = graph.findChild<QChartView *>()->chart();
    QStringList labels;
    for (QGraphicsItem *item : chart->childItems())
      if (auto *text = dynamic_cast<QGraphicsSimpleTextItem *>(item); text && text->isVisible()
          && text->pos().x() + text->boundingRect().width() < chart->plotArea().left())
        labels << text->text();
    bool mv = !labels.isEmpty();
    double hi = 0;
    for (const QString &l : labels)
    {
      mv = mv && !l.startsWith("-0") && !l.contains("0.");
      hi = qMax(hi, l.toDouble());
    }
    check(mv && hi >= 300, "y labels in mV, got " + labels.join(' '));
    check(qobject_cast<QValueAxis *>(chart->axes(Qt::Vertical).first())->labelsBrush().color().alpha() == 0,
          "y labels: Qt's own invisible");
  }

  // --- 5n3. a meter that changes range keeps the axis: up to 16 V, the
  //          reading now in mV - the labels stay volts (they were mV with
  //          1.6e+04) ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.resize(800, 500);
    graph.setUnit("V");
    graph.setScale(true, true, 0, 0);
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph);
    graph.startSLOT();
    for (double v : { 5.0, 12.0, 15.5 })
      feed.value(v);
    graph.setUnit("mV");
    feed.value(0.004);
    graph.show();
    QTest::qWait(50);
    QChart *chart = graph.findChild<QChartView *>()->chart();
    QStringList labels;
    for (QGraphicsItem *item : chart->childItems())
      if (auto *text = dynamic_cast<QGraphicsSimpleTextItem *>(item); text && text->isVisible()
          && text->pos().x() + text->boundingRect().width() < chart->plotArea().left())
        labels << text->text();
    double hi = 0;
    for (const QString &l : labels)
      hi = qMax(hi, l.toDouble());
    check(!labels.isEmpty() && hi >= 15 && hi < 100 && !labels.join(' ').contains('e'),
          "range change: the y labels stay volts, got " + labels.join(' '));
  }

  // --- 5o. the integral's scale from before 26.2: once divided by the
  //          sample time, so the curve stays as it was - the sum of the
  //          0.5 s samples times 2 is the integral over time times 4 (up to
  //          the first sample, which had no time yet) ---
  {
    Settings old("intscale", tmpDir.path());
    old.setInt("Sample/rate", 5);
    old.setInt("Sample/rate-unit", 0);   // 0.5 s
    old.setString("Graph/int-scale", "2");
    old.save();
    check(GraphWidget::migrateIntegralScale(&old) && old.getString("Graph/int-scale") == "4",
          "int-scale: 2 at 0.5 s should become 4, got " + old.getString("Graph/int-scale"));
    check(!GraphWidget::migrateIntegralScale(&old) && old.getString("Graph/int-scale") == "4",
          "int-scale: converted twice");

    GraphWidget graph(nullptr, &old);
    graph.setSampleTime(5);
    graph.setGraphSize(100);
    graph.setIntegration(true, EngNumberValidator::value(old.getString("Graph/int-scale")), 0.0, 0.0);
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph, 500);
    graph.startSLOT();
    for (int i = 0; i < 4; ++i)
      feed.value(1.0);
    // before: 2 4 6 8 (the samples 1 1 1 1, summed, times 2)
    check(seriesY(graph, 2) == "0 2 4 6", "int-scale: the curve got " + seriesY(graph, 2));

    Settings fresh("intscale-new", tmpDir.path());   // 1 s: nothing to do
    check(!GraphWidget::migrateIntegralScale(&fresh) && fresh.getBool("Graph/int-scale-per-second"),
          "int-scale: a sample time of 1 s keeps the scale");
  }

  // --- 6. engineering-prefix export/import: setUnit() must strip a leading
  //         G or p prefix too (previously only n/u/m/k/M were recognized), and
  //         a value re-imported from a prefix-scaled export (e.g. "2.5;pF")
  //         must round-trip back to the same raw value, not get double-scaled
  //         into something like "ppF" on the next export. ---
  {
    QTemporaryDir outDir;

    auto exportedUnitFor = [&](const QString &unit, double rawValue) -> QString
    {
      GraphWidget graph(nullptr, &settings);
      graph.setUnit(unit);
      graph.setSampleTime(10);
      graph.setGraphSize(5);
      graph.setMode(GraphWidget::Manual);
      Feed feed(graph);
      graph.startSLOT();
      feed.value(rawValue);

      QString path = outDir.path() + "/prefix_probe.csv";
      if (!graph.exportCsvFile(path))
        return QString();

      QStringList lines = readFile(path).split('\n', Qt::SkipEmptyParts);
      if (lines.size() < 2)
        return QString();
      return lines[1].split(';').value(3); // timestamp;time;value;unit
    };

    check(exportedUnitFor("GHz", 2.5e9) == "GHz",
          "setUnit() should strip a leading 'G' prefix so re-exporting a GHz-range value stays 'GHz', not 'GGHz' or 'Hz'");
    check(exportedUnitFor("pF", 2.5e-12) == "pF",
          "setUnit() should strip a leading 'p' prefix so re-exporting a pF-range value stays 'pF', not doubled to 'ppF'");

    // Full round trip at an extreme prefix: import a pF-range export, export
    // again, and the two exports must be byte-for-byte identical.
    {
      GraphWidget graph(nullptr, &settings);
      graph.setUnit("F");
      graph.setSampleTime(10);
      graph.setGraphSize(5);
      graph.setMode(GraphWidget::Manual);
      Feed feed(graph);
      graph.startSLOT();
      feed.value(2.5e-12);

      QString exported1 = outDir.path() + "/pf_export1.csv";
      check(graph.exportCsvFile(exported1), "pF round-trip: first export failed");

      GraphWidget graph2(nullptr, &settings);
      check(graph2.importCsvFile(exported1), "pF round-trip: re-import failed");

      QString exported2 = outDir.path() + "/pf_export2.csv";
      check(graph2.exportCsvFile(exported2), "pF round-trip: second export failed");

      QString c1 = readFile(exported1);
      QString c2 = readFile(exported2);
      check(!c1.isEmpty() && c1 == c2,
            "pF round-trip: re-exporting a re-imported pF-range file produced different CSV content");
    }
  }

  // --- 7. micro: export writes "µ", and both "µ" and the ASCII "u" of older
  //         exports must import with the same 1e-6 factor. Before the shared
  //         SiPrefix table the importer only knew "u", so a "µA" file came back
  //         a million times too large. ---
  {
    QTemporaryDir outDir;

    {
      GraphWidget graph(nullptr, &settings);
      graph.setUnit("A");
      graph.setSampleTime(10);
      graph.setGraphSize(5);
      graph.setMode(GraphWidget::Manual);
      Feed feed(graph);
      graph.startSLOT();
      feed.value(2.5e-6);
      QString path = outDir.path() + "/micro_export.csv";
      check(graph.exportCsvFile(path), "micro: export failed");
      QStringList lines = readFile(path).split('\n', Qt::SkipEmptyParts);
      check(lines.size() >= 2 && lines[1].split(';').value(3) == QString::fromUtf8("µA"),
            "a 2.5e-6 A value should export with the unit 'µA'");
    }

    auto importedValue = [&](const QString &unitInFile) -> double
    {
      QString path = outDir.path() + "/micro_" + QString::number(qHash(unitInFile)) + ".csv";
      QFile f(path);
      if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return -1;
      QTextStream out(&f);
      out << "timestamp;time (s);value;unit\n"
          << "2026-09-19T10:00:00,000;0;2.5;" << unitInFile << "\n"
          << "2026-09-19T10:00:01,000;1;2.5;" << unitInFile << "\n";
      f.close();

      GraphWidget graph(nullptr, &settings);
      if (!graph.importCsvFile(path))
        return -1;
      QString exported = path + ".out.csv";
      if (!graph.exportCsvFile(exported))
        return -1;
      QStringList lines = readFile(exported).split('\n', Qt::SkipEmptyParts);
      if (lines.size() < 2)
        return -1;
      QStringList cols = lines[1].split(';');
      return cols.value(2).toDouble() * SiPrefix::factor(SiPrefix::split(cols.value(3)).prefix);
    };

    check(qFuzzyCompare(importedValue("µA"), 2.5e-6),
          "importing '2.5;µA' should yield 2.5e-6 A");
    check(qFuzzyCompare(importedValue("uA"), 2.5e-6),
          "importing the older ASCII spelling '2.5;uA' should yield 2.5e-6 A as well");
  }

  // --- 7b. image export: SVG and PDF keep the curve as vectors, the pixel
  //          formats come out with the asked-for size ---
  {
    Settings cfg("imgtest", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    check(graph.importCsvFile(dataDir + "/new_larger.csv"), "image export: import failed");
    // as in the program: the graph is on screen and laid out (a hidden
    // widget gets no resize event, and Qt Charts lays out one turn later)
    graph.show();
    QTest::qWait(100);

    const QString svg = tmpDir.filePath("graph.svg");
    check(graph.exportImageFile(svg, QSize(1000, 600)), "SVG written");
    QFile f(svg);
    check(f.open(QIODevice::ReadOnly), "SVG opens");
    const QByteArray content = f.readAll();
    f.close();
    QXmlStreamReader xml(content);
    while (!xml.atEnd())
      xml.readNext();
    check(!xml.hasError(), "SVG is well-formed XML: " + xml.errorString());
    // QSvgGenerator writes width/height in millimetres; the pixel size the
    // drawing was made for is the viewBox
    check(content.contains("<svg") && content.contains("viewBox=\"0 0 1000 600\""),
          "SVG carries the requested size: " + QString::fromUtf8(content.left(200)));
    // vector output, not a pixel dump: the curve and the axis labels are
    // paths and text, so there is no embedded raster image
    // the curve itself: Qt Charts writes a line series as one short
    // polyline per segment, so a curve of ~270 samples gives hundreds of
    // them; the frame and the grid alone are a few dozen
    const int shapes = content.count("<polyline") + content.count("<path");
    check(shapes >= 200, QString("SVG holds the curve as vectors (%1 polylines/paths)").arg(shapes));
    check(!content.contains("<image"), "SVG holds no embedded bitmap");
    check(content.contains("QtDMM"), "SVG names its origin in the title");

    const QString pdf = tmpDir.filePath("graph.pdf");
    check(graph.exportImageFile(pdf, QSize(1000, 600)), "PDF written");
    QFile pf(pdf);
    check(pf.open(QIODevice::ReadOnly) && pf.read(5) == "%PDF-", "PDF has its magic");
    pf.close();

    const QString png = tmpDir.filePath("graph.png");
    check(graph.exportImageFile(png, QSize(320, 200)), "PNG written");
    QImage image(png);
    check(!image.isNull() && image.size() == QSize(320, 200), "PNG has the requested size");

    check(!graph.exportImageFile(tmpDir.filePath("no/such/dir/graph.svg")), "an unwritable path fails");
  }

  // --- 7c. colour variants: 1-2-5 divisions and 10 x 8 for the scope-like
  //          ones, names round-trip ---
  {
    check(niceStep(0.3) == 0.5 && niceStep(1) == 1 && niceStep(1.01) == 2
          && niceStep(4.9) == 5 && niceStep(6) == 10 && qFuzzyCompare(niceStep(0.0021), 0.005),
          "niceStep follows 1-2-5");
    for (auto v : { GraphWidget::Neutral, GraphWidget::ScopeBlue, GraphWidget::PhosphorGreen, GraphWidget::PhosphorAmber,
                    GraphWidget::ChartRecorder, GraphWidget::Custom })
      check(GraphWidget::variantFromName(GraphWidget::variantName(v)) == v, "variant name round-trip " + GraphWidget::variantName(v));
    check(GraphWidget::variantFromName("nonsense") == GraphWidget::Neutral, "unknown variant is neutral");

    Settings cfg("varianttest", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.setScale(false, false, -0.3, 11.7);
    auto *y = graph.findChild<QChartView *>()->chart()->axes(Qt::Vertical).first();
    auto *yAxis = qobject_cast<QValueAxis *>(y);
    check(qFuzzyCompare(yAxis->min(), -0.3) && qFuzzyCompare(yAxis->max(), 11.7), "neutral keeps the scale as set");
    graph.setColorVariant(GraphWidget::Neutral, GraphWidget::PhosphorGreen);   // this graph only
    const double div = (yAxis->max() - yAxis->min()) / 8;
    check(yAxis->tickCount() == 9 && qFuzzyCompare(div, 2.0) && qFuzzyCompare(yAxis->min(), -2.0),
          QString("phosphor: 8 divisions of 2 from -2, got %1..%2").arg(yAxis->min()).arg(yAxis->max()));
    check(graph.colorOverride() == GraphWidget::PhosphorGreen, "the override is kept");
    graph.setColorVariant(GraphWidget::Custom);   // the default, no override
    check(graph.colorVariant() == GraphWidget::Custom && graph.colorOverride() == -1, "default without override");
    check(yAxis->tickCount() == 5 && qFuzzyCompare(yAxis->max(), 11.7), "custom goes back to the plain scale");
  }

  // --- 7d. the time axis: whole time steps, labelled in s, min or h; the
  //          scope-like variants make 600 s 10 x 1 min (not 10 x 100 s) ---
  {
    check(GraphWidget::timeStep(45) == 60 && GraphWidget::timeStep(61) == 120 && GraphWidget::timeStep(3) == 5
          && GraphWidget::timeStep(12) == 15 && GraphWidget::timeStep(400) == 600 && GraphWidget::timeStep(2000) == 3600
          && GraphWidget::timeStep(0.15) == 0.2 && GraphWidget::timeStep(100000) == 172800,
          "timeStep: 1, 2, 5, 10, 15, 30 s, 1, 2, 5, 10, 15, 30 min, h, days");
    Settings cfg("timeaxis", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.setSampleTime(10);
    graph.setGraphSize(600);
    graph.show();
    QTest::qWait(50);
    QChart *chart = graph.findChild<QChartView *>()->chart();
    auto *x = qobject_cast<QValueAxis *>(chart->axes(Qt::Horizontal).first());
    auto labels = [&]
    {
      QStringList out;
      for (QGraphicsItem *item : chart->childItems())
        if (auto *t = dynamic_cast<QGraphicsSimpleTextItem *>(item); t && t->isVisible()
            && t->pos().y() > chart->plotArea().bottom())
          out << t->text();
      return out;
    };
    check(x->tickType() == QValueAxis::TicksDynamic && x->tickInterval() == 120 && x->titleText() == "[min]",
          QString("neutral 600 s: a tick every 2 min, got %1 s, '%2'").arg(x->tickInterval()).arg(x->titleText()));
    check(labels().join(' ') == "0 2 4 6 8", "neutral 600 s: labels 0 2 4 6 8 min, got " + labels().join(' '));
    graph.setColorVariant(GraphWidget::ScopeBlue);
    check(x->tickInterval() == 60 && qFuzzyCompare(x->max() - x->min(), 600.0),
          QString("scope 600 s: 10 x 1 min, got %1 x %2 s").arg((x->max() - x->min()) / x->tickInterval()).arg(x->tickInterval()));
    check(labels().join(' ') == "0 1 2 3 4 5 6 7 8 9 10", "scope 600 s: labels 0..10 min, got " + labels().join(' '));
    graph.setGraphSize(20);
    check(x->tickInterval() == 2 && x->titleText() == "[sec]", QString("scope 20 s: 10 x 2 s, got %1").arg(x->tickInterval()));
    graph.setGraphSize(7200);
    check(x->tickInterval() == 900 && x->titleText() == "[min]", QString("scope 2 h: 10 x 15 min, got %1").arg(x->tickInterval()));
    // 1 min is not 10 x a whole step: 6 x 10 s over exactly the minute (was
    // 10 x 10 s, the readings in the left 60 % of the graph)
    graph.setGraphSize(60);
    check(x->tickInterval() == 10 && qFuzzyCompare(x->max() - x->min(), 60.0),
          QString("scope 1 min: 6 x 10 s over 60 s, got %1 s wide, a tick every %2 s").arg(x->max() - x->min()).arg(x->tickInterval()));
    // Live runs on smoothly: one reading later the axis is one second on,
    // not a whole step (it jumped every 10 s)
    {
      Feed feed(graph, 1000);
      graph.liveSLOT();
      for (QAbstractButton *b : graph.findChildren<QAbstractButton *>())
        if (b->text() == "1 min")
          b->click();
      graph.setGraphSize(60);   // InstanceWidget applies the request through the settings
      for (int i = 0; i < 95; ++i)
        feed.value(1.0);
      const double before = x->min();
      feed.value(1.0);
      check(qFuzzyCompare(x->min() - before + 1, 2.0) && qFuzzyCompare(x->max() - x->min(), 60.0),
            QString("scope Live: the axis moves with each reading, %1 -> %2 (%3 s wide)").arg(before).arg(x->min()).arg(x->max() - x->min()));
      // back to All: the curve from the start of Live, not only the last
      // minute shown before
      for (QAbstractButton *b : graph.findChildren<QAbstractButton *>())
        if (b->text() == "All")
          b->click();
      feed.value(1.0);
      const QString xs = seriesX(graph);
      check(xs.section(' ', 0, 0) == "0" && x->min() <= 0.0,
            QString("Live All after 1 min: from 0 on, first point %1, axis from %2").arg(xs.section(' ', 0, 0)).arg(x->min()));
    }
    graph.setGraphSize(7200);
    graph.setColorVariant(GraphWidget::Neutral);
    check(x->tickInterval() == 1800 && x->titleText() == "[min]", QString("neutral 2 h: every 30 min, got %1").arg(x->tickInterval()));
  }

  // --- 7d2. a graph without colours set (a test, a picture made
  //          offscreen) shows its x labels once: Qt's own stay invisible ---
  {
    GraphWidget graph(nullptr, &settings);
    auto *x = qobject_cast<QValueAxis *>(graph.findChild<QChartView *>()->chart()->axes(Qt::Horizontal).first());
    check(x && x->labelsBrush().color().alpha() == 0, "x labels: Qt's own must be invisible from the start");
  }

  // --- 7e. no value yet: the auto scale has no range (+-1e40, or 0..0
  //          with "include zero"), the axis still gets one, so grid and
  //          labels are there before the first reading ---
  {
    Settings cfg("emptyscale", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    auto *yAxis = qobject_cast<QValueAxis *>(graph.findChild<QChartView *>()->chart()->axes(Qt::Vertical).first());
    for (bool includeZero : { false, true })
    {
      graph.setScale(true, includeZero, 0, 0);
      graph.clearSLOT();
      check(yAxis->min() == 0 && yAxis->max() == 1,
            QString("empty, include zero %1: 0..1, got %2..%3")
              .arg(includeZero).arg(yAxis->min()).arg(yAxis->max()));
    }
  }

  // --- 7f. the reference line: across the plot at 0 while 0 lies inside
  //          the y range, "0 A" at its right end; phosphor draws it with
  //          its fine ticks, without 0 in range it has no line (no cross
  //          in the middle), the ticks sit on the edges ---
  {
    Settings cfg("reference", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.setUnit("A");
    graph.show();
    QTest::qWait(50);
    QChart *chart = graph.findChild<QChartView *>()->chart();
    QAbstractSeries *series = chart->series().first();
    QGraphicsLineItem *line = nullptr;
    QGraphicsSimpleTextItem *label = nullptr;
    QGraphicsPathItem *centre = nullptr;
    for (QGraphicsItem *item : chart->childItems())
    {
      if (item->data(0).toString() == "reference")
      {
        if (auto *l = dynamic_cast<QGraphicsLineItem *>(item))
          line = l;
        else if (auto *t = dynamic_cast<QGraphicsSimpleTextItem *>(item))
          label = t;
      }
      else if (auto *p = dynamic_cast<QGraphicsPathItem *>(item))
        centre = p;
    }
    check(line && label && centre, "reference: line, label and centre axis found");
    if (line && label && centre)
    {
      auto zeroY = [&] { return chart->mapToPosition(QPointF(0, 0), series).y(); };
      for (auto v : { GraphWidget::Neutral, GraphWidget::ScopeBlue, GraphWidget::ChartRecorder, GraphWidget::Custom })
      {
        graph.setColorVariant(v);
        graph.setScale(false, false, -2, 10);
        check(line->isVisible() && std::abs(line->line().y1() - zeroY()) < 0.5 && line->line().y1() == line->line().y2()
                && std::abs(line->line().x2() - line->line().x1() - chart->plotArea().width()) < 0.5,
              QString("reference %1: across the plot at 0 (%2), got y %3").arg(GraphWidget::variantName(v))
                .arg(zeroY()).arg(line->line().y1()));
        check(label->isVisible() && label->text() == "0 A", QString("reference %1: label '0 A', got '%2'")
              .arg(GraphWidget::variantName(v), label->isVisible() ? label->text() : "hidden"));
        check(label->pos().x() + label->boundingRect().width() <= chart->plotArea().right()
                && label->pos().x() > chart->plotArea().center().x(),
              QString("reference %1: the label at the right end").arg(GraphWidget::variantName(v)));
        graph.setScale(false, false, 1, 10);
        check(!line->isVisible() && !label->isVisible(),
              QString("reference %1: no line without 0 in range").arg(GraphWidget::variantName(v)));
        // 0 on the edge: the axis is the line
        graph.setScale(false, false, 0, 10);
        check(!line->isVisible(), QString("reference %1: no line on the axis").arg(GraphWidget::variantName(v)));
      }
      graph.setColorVariant(GraphWidget::PhosphorGreen);
      graph.setScale(false, false, -2, 10);
      const QRectF pa = chart->plotArea();
      QList<double> h = pathLines(centre->path(), true, pa.width() - 1);
      check(!line->isVisible() && h.size() == 1 && std::abs(h.value(0) - zeroY()) < 0.5,
            QString("reference phosphor: one line, at 0 (%1), got %2 lines").arg(zeroY()).arg(h.size()));
      check(pathLines(centre->path(), false, pa.height() - 1).isEmpty(), "phosphor: no vertical line in the middle");
      check(label->isVisible() && label->text() == "0 A", "reference phosphor: label '0 A'");
      graph.setScale(false, false, 1, 10);
      check(pathLines(centre->path(), true, pa.width() - 1).isEmpty() && !label->isVisible(),
            "reference phosphor: without 0 no line (no cross)");
      // the ticks then on the bottom edge
      check(centre->path().elementCount() > 0 && std::abs(centre->path().elementAt(0).y + 3 - pa.bottom()) < 0.5,
            QString("phosphor: the ticks on the bottom edge, got %1").arg(centre->path().elementAt(0).y));
      // a unit with a prefix and none at all
      graph.setColorVariant(GraphWidget::Neutral);
      graph.setScale(false, false, -0.002, 0.01);
      check(label->text() == "0 A", "reference: 0 takes no prefix, got " + label->text());
      graph.setUnit("");
      graph.setScale(false, false, -2, 10);
      check(label->text() == "0", "reference without unit: '0', got " + label->text());
    }
  }

  // --- 7g. the reference line moves: dragged it snaps to 0, the window's
  //          mean, minimum and maximum and the alarm thresholds, else takes
  //          the value; the context menu chooses the same; the setting
  //          round-trips ---
  {
    Settings cfg("referencedrag", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.setUnit("A");
    graph.setScale(false, false, -2, 6);
    graph.setGraphSize(60);
    graph.show();
    QTest::qWait(50);
    QChart *chart = graph.findChild<QChartView *>()->chart();
    QWidget *viewport = graph.findChild<QChartView *>()->viewport();
    QChartView *view = graph.findChild<QChartView *>();
    QGraphicsSimpleTextItem *label = nullptr;
    for (QGraphicsItem *item : chart->childItems())
      if (auto *t = dynamic_cast<QGraphicsSimpleTextItem *>(item); t && item->data(0).toString() == "reference")
        label = t;
    QSignalSpy changed(&graph, &GraphWidget::referenceChanged);
    QSignalSpy thresholds(&graph, &GraphWidget::thresholdChanged);
    // a minute of 1 A and 3 A, a second each: mean 2, min 1, max 3
    Feed feed(graph, 1000);
    graph.liveSLOT();
    graph.setGraphSize(60);
    for (int i = 0; i < 70; ++i)
      feed.value(i % 2 ? 3.0 : 1.0);
    const QRectF plot = chart->plotArea();
    auto yOf = [&](double v) { return plot.bottom() - (v + 2) / 8 * plot.height(); };
    auto mouse = [&](QEvent::Type type, double y, Qt::MouseButton button = Qt::NoButton)
    {
      const QPointF at = view->mapFromScene(QPointF(plot.center().x(), y));
      QMouseEvent ev(type, at, viewport->mapToGlobal(at), button,
                     type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(button), Qt::NoModifier);
      QCoreApplication::sendEvent(viewport, &ev);
    };
    auto drag = [&](double fromY, double toY)
    {
      mouse(QEvent::MouseMove, fromY);   // hovering finds the line
      mouse(QEvent::MouseButtonPress, fromY, Qt::LeftButton);
      mouse(QEvent::MouseMove, toY, Qt::LeftButton);
      mouse(QEvent::MouseButtonRelease, toY, Qt::LeftButton);
    };
    drag(yOf(0), yOf(2) + 3);
    check(graph.referenceKind() == GraphWidget::RefMean && std::abs(graph.referenceValue() - 2) < 0.1,
          QString("reference dragged near the mean: snaps to it, got kind %1 at %2").arg(graph.referenceKind()).arg(graph.referenceValue()));
    check(label && label->text().startsWith("mean ") && label->text().endsWith(" A"), "reference on the mean: 'mean 2 A', got "
          + (label ? label->text() : QString()));
    check(!changed.isEmpty() && thresholds.isEmpty(), "reference dragged: referenceChanged, no thresholdChanged");
    drag(yOf(2), yOf(3) - 2);
    check(graph.referenceKind() == GraphWidget::RefMax && graph.referenceValue() == 3, "reference: snaps to the maximum");
    drag(yOf(3), yOf(1) + 2);
    check(graph.referenceKind() == GraphWidget::RefMin && graph.referenceValue() == 1, "reference: snaps to the minimum");
    graph.setAlarmThresholds({ 5.0 });
    drag(yOf(1), yOf(5) + 4);
    check(graph.referenceKind() == GraphWidget::RefAlarm && graph.referenceValue() == 5, "reference: snaps to an alarm threshold");
    check(graph.referenceSetting() == "alarm 5", "reference setting: 'alarm 5', got " + graph.referenceSetting());
    graph.setAlarmThresholds({ 4.0 });
    check(graph.referenceKind() == GraphWidget::RefValue && graph.referenceValue() == 5,
          "reference: a threshold that went leaves the line where it is");
    // away from all of them: the value there, rounded to the scale
    drag(yOf(5), yOf(4.47));
    // (a hundredth of the 8 A scale is 0.08, the 1-2-5 step 0.1)
    check(graph.referenceKind() == GraphWidget::RefValue && std::abs(graph.referenceValue() - 4.5) < 1e-9,
          QString("reference dragged freely: 4.5, got %1").arg(graph.referenceValue(), 0, 'g', 12));
    check(graph.referenceSetting() == QString::number(graph.referenceValue(), 'g', 12), "reference setting: the value");
    drag(yOf(graph.referenceValue()), yOf(0) - 1);
    check(graph.referenceKind() == GraphWidget::RefZero && graph.referenceSetting().isEmpty(), "reference: back to 0, setting empty");

    // the limit test's band: two lines and the green between, on the left
    // axis' value; the reference line snaps to the lines
    {
      auto limitItems = [&]
      {
        int lines = 0, bands = 0;
        for (QGraphicsItem *item : chart->childItems())
          if (item->data(0).toString() == "limit" && item->isVisible())
            (dynamic_cast<QGraphicsLineItem *>(item) ? lines : bands)++;
        return std::pair{ lines, bands };
      };
      check(limitItems() == std::pair{ 0, 0 }, "no limit test: no limit lines");
      graph.setLimits(0, 4.5, 5.5);
      check(limitItems() == std::pair{ 2, 1 }, "limits 4.5..5.5: two lines and the band");
      drag(yOf(0), yOf(5.5) - 3);
      check(graph.referenceKind() == GraphWidget::RefValue && graph.referenceValue() == 5.5, "reference: snaps to a limit");
      graph.setLimits(0, 4.5, 100);
      check(limitItems() == std::pair{ 1, 1 }, "a limit beyond the scale: one line, the band to the edge");
      graph.setLimits(1, 4.5, 5.5);
      check(limitItems() == std::pair{ 0, 0 }, "limits of another value: none on this axis");
      graph.setLimits(-1);
      graph.setReferenceSetting(QString());
    }

    // the setting round-trips
    for (const QString &text : { QString("mean"), QString("min"), QString("max"), QString("alarm 4"), QString("1.25"), QString("off"),
                                 QString() })
    {
      graph.setReferenceSetting(text);
      check(graph.referenceSetting() == text, QString("reference setting '%1' round-trips, got '%2'").arg(text, graph.referenceSetting()));
    }
    graph.setReferenceSetting("nonsense");
    check(graph.referenceKind() == GraphWidget::RefZero, "reference setting: nonsense is 0");

    // the context menu: right click, "Mean of the window"
    mouse(QEvent::MouseButtonPress, yOf(1), Qt::RightButton);
    QAction *mean = nullptr;
    for (QAction *a : graph.findChildren<QAction *>())
      if (a->text().remove('&') == "Mean of the window")
        mean = a;
    check(mean != nullptr, "reference: the context menu has 'Mean of the window'");
    if (mean)
      mean->trigger();
    for (QMenu *m : graph.findChildren<QMenu *>())
      m->close();
    check(graph.referenceKind() == GraphWidget::RefMean, "reference: chosen in the context menu");

    // switched off in the context menu: no line, no label, nothing to drag
    mouse(QEvent::MouseButtonPress, yOf(1), Qt::RightButton);
    QAction *off = nullptr;
    for (QAction *a : graph.findChildren<QAction *>())
      if (a->text().remove('&') == "Off")
        off = a;
    check(off != nullptr, "reference: the context menu has 'Off'");
    if (off)
      off->trigger();
    for (QMenu *m : graph.findChildren<QMenu *>())
      m->close();
    QGraphicsLineItem *refLine = nullptr;
    for (QGraphicsItem *item : chart->childItems())
      if (auto *l = dynamic_cast<QGraphicsLineItem *>(item); l && item->data(0).toString() == "reference")
        refLine = l;
    check(graph.referenceKind() == GraphWidget::RefOff && graph.referenceSetting() == "off"
            && refLine && !refLine->isVisible() && label && !label->isVisible(),
          "reference off: no line, no label, setting 'off'");
    drag(yOf(0), yOf(3));
    check(graph.referenceKind() == GraphWidget::RefOff, "reference off: nothing to drag at 0");
    graph.setColorVariant(GraphWidget::PhosphorGreen);
    QGraphicsPathItem *centre = nullptr;
    for (QGraphicsItem *item : chart->childItems())
      if (auto *p = dynamic_cast<QGraphicsPathItem *>(item))
        centre = p;
    check(centre && pathLines(centre->path(), true, chart->plotArea().width() - 1).isEmpty(),
          "reference off: phosphor has no horizontal line either");
  }

  // --- 7h. time cursors: a click sets A, Shift+click B, both can be
  //          dragged; A says time and value, with B the change, the mean and
  //          the integral between them (Ah for a current); Esc clears ---
  {
    Settings cfg("timecursors", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.setUnit("A");
    graph.setScale(false, false, 0, 5);
    graph.show();
    QTest::qWait(50);
    QChartView *view = graph.findChild<QChartView *>();
    QChart *chart = view->chart();
    QWidget *viewport = view->viewport();
    QLabel *info = graph.findChild<QLabel *>("ui_graphCursors");
    // 2 A for 10 s, then 4 A for 10 s
    Feed feed(graph, 1000);
    graph.liveSLOT();
    graph.setGraphSize(60);
    for (int i = 0; i < 21; ++i)
      feed.value(i < 10 ? 2.0 : 4.0);
    feed.now -= 1000;   // now is the last reading
    check(graph.timeCursorText().isEmpty() && info && info->isHidden(), "time cursors: none at first");

    auto xOf = [&](qint64 t)
    {
      return chart->mapToPosition(QPointF(t / 1000.0, 1), chart->series().first()).x();
    };
    auto mouse = [&](QEvent::Type type, double x, Qt::MouseButton button = Qt::NoButton, Qt::KeyboardModifiers mods = Qt::NoModifier)
    {
      const QPointF at = view->mapFromScene(QPointF(x, chart->plotArea().center().y()));
      QMouseEvent ev(type, at, viewport->mapToGlobal(at), button,
                     type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(button), mods);
      QCoreApplication::sendEvent(viewport, &ev);
    };
    auto click = [&](double x, Qt::KeyboardModifiers mods = Qt::NoModifier)
    {
      mouse(QEvent::MouseMove, x);
      mouse(QEvent::MouseButtonPress, x, Qt::LeftButton, mods);
      mouse(QEvent::MouseButtonRelease, x, Qt::LeftButton, mods);
    };
    click(xOf(5000));
    check(std::abs(graph.timeCursor(0) - 5000) < 200 && graph.timeCursor(1) < 0,
          QString("time cursors: a click sets A at 5 s, got %1").arg(graph.timeCursor(0)));
    check(info && !info->isHidden() && graph.timeCursorText().count('\n') == 0
            && graph.timeCursorText().startsWith("A  12:00:0") && graph.timeCursorText().endsWith("  2 A"),
          "time cursors: A says time and value, got " + graph.timeCursorText());
    click(xOf(15000), Qt::ShiftModifier);
    check(std::abs(graph.timeCursor(1) - 15000) < 200 && std::abs(graph.timeCursor(0) - 5000) < 200,
          QString("time cursors: Shift+click sets B, A stays: %1, %2").arg(graph.timeCursor(0)).arg(graph.timeCursor(1)));

    // exactly: 5 s of 2 A, 5 s of 4 A
    graph.setTimeCursor(0, 5000);
    graph.setTimeCursor(1, 15000);
    const QStringList lines = graph.timeCursorText().split('\n');
    check(lines.size() == 4 && lines[1].endsWith("  4 A"), "time cursors: two lines for A and B, got " + lines.join(" | "));
    check(lines.size() == 4 && lines[2] == QString::fromUtf8("Δt 10.00 s"),
          "time cursors: 'Δt 10.00 s', got " + lines.value(2));
    // 30 A s = 8.333 mAh
    check(lines.size() == 4 && lines[3] == QString::fromUtf8("Δ 2 A  mean 3 A  ∫ 8.333 mAh"),
          "time cursors: 'Δ 2 A  mean 3 A  ∫ 8.333 mAh', got " + lines.value(3));

    // a dashed line across the plot at each cursor's value: at 2 A and 4 A,
    // the change their distance
    {
      QList<QGraphicsLineItem *> levels;
      for (QGraphicsItem *item : chart->childItems())
        if (item->data(0).toString() == "cursor-level" && item->isVisible())
          levels << static_cast<QGraphicsLineItem *>(item);
      auto yOf = [&](double v) { return chart->mapToPosition(QPointF(10, v), chart->series().first()).y(); };
      const QRectF plot = chart->plotArea();
      check(levels.size() == 2, QString("time cursors: two levels, got %1").arg(levels.size()));
      bool at2 = false, at4 = false;
      for (QGraphicsLineItem *l : levels)
      {
        const QLineF line = l->line();
        const bool across = line.y1() == line.y2() && line.x1() == plot.left() && line.x2() == plot.right()
                            && l->pen().style() == Qt::DashLine;
        at2 = at2 || (across && std::abs(line.y1() - yOf(2)) < 0.5);
        at4 = at4 || (across && std::abs(line.y1() - yOf(4)) < 0.5);
      }
      check(at2 && at4, "time cursors: dashed levels across the plot at 2 A and 4 A");
      // A and B as flags under the plot, their points up at their lines
      int flagsShown = 0;
      for (QGraphicsItem *item : chart->childItems())
        if (item->data(0).toString() == "cursor-flag" && item->isVisible())
        {
          auto *flag = static_cast<GraphFlagItem *>(item);
          const double x = flag->text() == "A" ? xOf(5000) : xOf(15000);
          flagsShown++;
          check(flag->tagRect().top() >= plot.bottom() && std::abs(flag->tagRect().center().x() - x) < 1,
                QString("time cursors: flag %1 under the plot, centred on its line").arg(flag->text()));
        }
      check(flagsShown == 2, QString("time cursors: two flags, got %1").arg(flagsShown));
      // an alarm mark: a flag with its number and name, in the row with A and B
      graph.store()->addMark(0xffd03030, "Alarm Above 3 A");
      GraphFlagItem *markFlag = nullptr;
      for (QGraphicsItem *item : chart->childItems())
        if (item->data(0).toString() == "mark-flag" && item->isVisible())
          markFlag = static_cast<GraphFlagItem *>(item);
      check(markFlag && markFlag->text() == QString::fromUtf8("1 · Above 3 A") && markFlag->tagRect().top() >= plot.bottom(),
            "time cursors: the mark's flag '1 · Above 3 A' (no 'Alarm') under the plot, got " + (markFlag ? markFlag->text() : "none"));
      // out of the scale: no level
      graph.setScale(false, false, 0, 3);
      int shown = 0;
      for (QGraphicsItem *item : chart->childItems())
        shown += item->data(0).toString() == "cursor-level" && item->isVisible();
      check(shown == 1, QString("time cursors: 4 A above a scale to 3, one level, got %1").arg(shown));
      graph.setScale(false, false, 0, 5);
    }

    // dragging B moves it, no new cursor
    mouse(QEvent::MouseMove, xOf(15000));
    mouse(QEvent::MouseButtonPress, xOf(15000), Qt::LeftButton);
    mouse(QEvent::MouseMove, xOf(18000), Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, xOf(18000), Qt::LeftButton);
    check(std::abs(graph.timeCursor(1) - 18000) < 200 && graph.timeCursor(0) == 5000,
          QString("time cursors: B dragged to 18 s, A stays: %1, %2").arg(graph.timeCursor(0)).arg(graph.timeCursor(1)));
    // a drag on empty ground sets nothing
    mouse(QEvent::MouseMove, xOf(9000));
    mouse(QEvent::MouseButtonPress, xOf(9000), Qt::LeftButton);
    mouse(QEvent::MouseMove, xOf(11000), Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, xOf(11000), Qt::LeftButton);
    check(graph.timeCursor(0) == 5000, "time cursors: a drag is no click");
    // a voltage: no integral, unless it is the integration curve's (in V s)
    graph.setUnit("V");
    check(!graph.timeCursorText().contains(QChar(0x222b)), "time cursors: a voltage has no integral, got "
          + graph.timeCursorText());
    graph.setIntegration(true, 1.0, 0.0, 0.0);
    check(graph.timeCursorText().contains(QString::fromUtf8("V·s")), "time cursors: the integrated voltage in V·s, got "
          + graph.timeCursorText());
    graph.setIntegration(false, 1.0, 0.0, 0.0);
    graph.setUnit("A");

    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(view, &esc);
    check(graph.timeCursor(0) < 0 && graph.timeCursor(1) < 0 && info->isHidden(), "time cursors: Esc takes them away");
    graph.setTimeCursor(0, 3000);
    graph.clearSLOT();
    check(graph.timeCursor(0) < 0, "time cursors: go with the graph cleared");
  }

  // --- 7h2. the flags' layout: centred on their lines, the next one
  //           slides right when they would touch, one that would leave its
  //           line is left out, at the edge a flag slides inward ---
  {
    const QRectF plot(10, 10, 200, 100);
    auto width = [](const QString &t) { return 10.0 * t.size(); };
    QList<GraphFlags::Flag> flags = { { 100, "A", {}, {}, {} }, { 112, "B", {}, {}, {} } };
    GraphFlags::layout(flags, plot, 14, width);
    // 18 px wide each: A 91..109, B would start at 103, slides to 112
    check(flags[0].rect == QRectF(91, 110 + GraphFlags::kPoint, 18, 14), "flags: A centred under its line");
    check(flags[1].rect.left() == 109 + GraphFlags::kAir && flags[1].rect.contains(QPointF(112, flags[1].rect.center().y())),
          "flags: B slides right, its line still on it");
    flags = { { 100, "A", {}, {}, {} }, { 101, "B", {}, {}, {} } };
    GraphFlags::layout(flags, plot, 14, width);
    check(!flags[0].rect.isEmpty() && flags[1].rect.isEmpty(), "flags: B would leave its line, left out");
    // notes as far as the neighbours leave room
    flags = { { 100, "1", "Above 5 V", {}, {} } };
    GraphFlags::layout(flags, plot, 14, width);
    check(flags[0].text == QString::fromUtf8("1 · Above 5 V"), "flags: alone, the whole note: " + flags[0].text);
    flags = { { 60, "1", "Above 5 V", {}, {} }, { 140, "2", "Below 1 V", {}, {} } };
    GraphFlags::layout(flags, plot, 14, width);
    check(flags[0].text == QString::fromUtf8("1 · A…") && flags[1].text == QString::fromUtf8("2 · B…"),
          "flags: 80 px apart, the notes cut: " + flags[0].text + " | " + flags[1].text);
    flags = { { 100, "1", "Above 5 V", {}, {} }, { 130, "2", "Below", {}, {} } };
    GraphFlags::layout(flags, plot, 14, width);
    check(flags[0].text == "1" && flags[1].text == "2", "flags: 30 px apart, the numbers alone");
    flags = { { 205, "long", {}, {}, {} } };
    GraphFlags::layout(flags, plot, 14, width);
    check(flags[0].rect.right() == 210 && flags[0].rect.contains(QPointF(205, flags[0].rect.center().y())),
          "flags: at the right edge it slides inward");
  }

  // --- 7i. phosphor: a vertical line only where a pre-triggered
  //          recording triggered ---
  {
    Settings cfg("phosphortrigger", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.setColorVariant(GraphWidget::PhosphorGreen);
    graph.show();
    QTest::qWait(50);
    QChart *chart = graph.findChild<QChartView *>()->chart();
    QGraphicsPathItem *centre = nullptr;
    for (QGraphicsItem *item : chart->childItems())
      if (auto *p = dynamic_cast<QGraphicsPathItem *>(item))
        centre = p;
    Feed feed(graph, 1000);
    graph.liveSLOT();
    graph.setGraphSize(60);
    for (int i = 0; i < 10; ++i)
      feed.value(1.0);
    auto vertical = [&] { return centre ? pathLines(centre->path(), false, chart->plotArea().height() - 1) : QList<double>(); };
    check(vertical().isEmpty(), "phosphor: no vertical line without a trigger");
    graph.store()->addMark(RecordingStore::kTriggerColor, "Trigger");
    feed.value(1.0);
    const double triggerX = chart->mapToPosition(QPointF(graph.store()->marks().last().t / 1000.0, 1),
                                                 chart->series().first()).x();
    check(vertical().size() == 1 && std::abs(vertical().value(0) - triggerX) < 0.5,
          QString("phosphor: the vertical line at the trigger (%1), got %2 lines").arg(triggerX).arg(vertical().size()));
  }

  // --- overview: around 0 the last digit stays small too ---
  {
    RecordingStore store;
    LoggedReading row;
    row.text = "0.001";
    row.unit = "V";
    row.value = 0.001;
    check(store.displayStep(0) == 0, "no readings: the step is not known");
    store.logRow(row);
    row.id = 1;
    row.text = "- 012.3";
    row.unit = "mV";
    row.value = -0.0123;
    store.logRow(row);
    row.text = "OL";
    row.quality = Quality::Overload;
    store.logRow(row);
    check(qFuzzyCompare(store.displayStep(0), 0.001), "step of 0.001 V");
    check(qFuzzyCompare(store.displayStep(1), 0.0001), "step of 012.3 mV, past the OL");
    check(store.displayStep(2) == 0, "another value: not known");
    check(qFuzzyCompare(GraphMinimap::leastSpan(0, 0.001, 0.001), 0.01), "0.000/0.001: ten digits");
    check(qFuzzyCompare(GraphMinimap::leastSpan(3, 3.0001, 0.0001), 3.0001 * GraphMinimap::kLeastSpan), "at 3 V: the part of the value");
    check(GraphMinimap::leastSpan(0, 5, 0.001) < 5, "0 to 5 V is wider, the signal fills the strip");
  }

  // --- overview: the last digit of a steady value stays a line, a real
  //          step fills the strip ---
  {
    const auto curveRows = [&](double step) {
      Settings cfg("minimapscale", tmpDir.path());
      GraphWidget graph(nullptr, &cfg);
      graph.resize(800, 500);
      graph.show();
      QTest::qWait(50);
      Feed feed(graph, 1000);
      graph.liveSLOT();
      for (int i = 0; i < 200; ++i)
        feed.value(13.27 + (i / 5 % 2) * step);
      GraphMinimap *map = graph.minimap();
      map->setColors(QBrush(Qt::black), Qt::white);
      map->setCurveColor(Qt::yellow);
      const QImage img = map->grab().toImage();
      int rows = 0;
      for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
          if (img.pixelColor(x, y).red() > 200 && img.pixelColor(x, y).blue() < 100)   // under the window's tint
          {
            ++rows;
            break;
          }
      return rows;
    };
    // the strip's scale moves in steps: most readings leave it alone
    {
      Settings cfg("minimapsteps", tmpDir.path());
      GraphWidget graph(nullptr, &cfg);
      graph.resize(800, 500);
      graph.show();
      QTest::qWait(50);
      Feed feed(graph, 1000);
      graph.liveSLOT();
      for (int i = 0; i < 500; ++i)
        feed.value(1.0);
      GraphMinimap *map = graph.minimap();
      int moves = 0;
      for (int i = 0; i < 60; ++i)
      {
        const double before = map->xOf(100000);
        feed.value(1.0);
        moves += map->xOf(100000) != before;
      }
      check(moves > 0 && moves < 12, QString("overview: 60 readings moved the scale %1 times").arg(moves));
    }
    const int steady = curveRows(0.01), stepping = curveRows(1.0);
    check(steady > 0 && steady <= 4, QString("overview: 10 mV at 13 V takes %1 rows").arg(steady));
    check(stepping >= 15, QString("overview: 1 V at 13 V takes %1 rows").arg(stepping));
  }

  // --- 7j. the overview below the graph: all the store keeps, the
  //          minimum to the maximum per column (a spike stays), overloads,
  //          gaps and marks, in the design's colours; dragging moves the
  //          window, in Live that pauses it until the right end; the
  //          brackets zoom; the columns are counted anew only when the
  //          strip doubles ---
  {
    Settings cfg("minimap", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    QObject::connect(&graph, &GraphWidget::windowRequested, [&](int s) { graph.setGraphSize(s); });
    GraphMinimap *map = graph.minimap();
    QScrollBar *bar = graph.findChild<QScrollBar *>();
    check(map && map->isVisible() && bar && !bar->isVisible(), "overview: shown in place of the scroll bar");
    Feed feed(graph, 1000);
    graph.liveSLOT();
    for (QToolButton *b : graph.findChildren<QToolButton *>())
      if (b->text() == "1 min")
        b->click();
    check(graph.windowSeconds() == 60, QString("overview: a window of 1 min, got %1 s").arg(graph.windowSeconds()));
    const int rebuilds = map->rebuilds();
    qint64 staleAt = -1;
    for (int i = 0; i < 1800; ++i)   // 30 min
    {
      if (i == 1200)
      {
        // silent for a minute
        feed.now += 1500;
        graph.store()->setStale(true);
        staleAt = graph.store()->series().last().t;
        feed.now += 60000;
      }
      feed.value(i == 700 ? 9.0 : i == 1000 ? qQNaN() : 1.0 + (i % 2) * 0.01);
      if (i == 500)
        graph.store()->addMark(0xffff00ff, "alarm");
    }
    check(map->rebuilds() - rebuilds < 20,
          QString("overview: 1800 readings counted anew %1 times").arg(map->rebuilds() - rebuilds));
    check(map->columnMs() == 4096, QString("overview: columns of 4096 ms, got %1").arg(map->columnMs()));
    const GraphMinimap::Column *spike = map->column(700000);
    check(spike && spike->max == 9.0 && spike->min >= 1.0 && spike->min <= 1.01,
          "overview: the spike stays in its column");
    const GraphMinimap::Column *ol = map->column(1000000);
    check(ol && ol->overload && !ol->gap, "overview: the overload in its column");
    const GraphMinimap::Column *gap = map->column(staleAt);
    const GraphMinimap::Column *silent = map->column(staleAt + 30000);
    check(gap && gap->gap && std::isnan(gap->last) && silent && !silent->any, "overview: the gap, nothing after it");
    // the window at the right end (the clock is a reading on)
    check(map->viewWidth() == 60000 && map->to() - (map->viewStart() + map->viewWidth()) <= 2000,
          QString("overview: the window %1 + %2 at the end %3").arg(map->viewStart()).arg(map->viewWidth()).arg(map->to()));

    // what is drawn: the spike up to the top, the mark as a dot on the
    // bottom edge
    {
      const QImage img = map->grab().toImage();
      const int x = int(map->xOf(700000));
      check(img.pixelColor(x, 6) != img.pixelColor(x + 40, 6), "overview: the spike is drawn");
      const int mx = int(std::round(map->xOf(graph.store()->marks().last().t)));
      const QColor dot = img.pixelColor(mx, img.height() - 3);
      check(dot.red() > 200 && dot.blue() > 200 && dot.green() < 80,
            "overview: the mark a dot on the bottom edge, got " + dot.name());
    }
    graph.setColorVariant(GraphWidget::PhosphorGreen);
    {
      const QImage img = map->grab().toImage();
      const QColor curve = img.pixelColor(int(map->xOf(700000)), 6);
      check(std::abs(curve.hue() - 135) < 20, QString("overview: the phosphor curve green, got hue %1").arg(curve.hue()));
    }
    graph.setColorVariant(GraphWidget::Neutral);

    auto mouse = [&](QEvent::Type type, double x)
    {
      const QPointF at(x, map->height() / 2.0);
      const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(Qt::LeftButton);
      QMouseEvent ev(type, at, map->mapToGlobal(at), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                     buttons, Qt::NoModifier);
      QCoreApplication::sendEvent(map, &ev);
    };
    auto drag = [&](double from, double to)
    {
      mouse(QEvent::MouseButtonPress, from);
      mouse(QEvent::MouseMove, (from + to) / 2);
      mouse(QEvent::MouseMove, to);
      mouse(QEvent::MouseButtonRelease, to);
    };
    const double perPixel = double(map->to() - map->from()) / map->width();
    // dragged back 300 px: Live pauses there
    const qint64 start = map->viewStart();
    const double centre = map->xOf(start + 30000);
    drag(centre, centre - 300);
    check(graph.livePaused() && graph.stateText() == "Live paused" && graph.store()->state() == RecordingStore::Live,
          "overview: dragged back, Live paused, got " + graph.stateText());
    check(std::abs(map->viewStart() - (start - 300 * perPixel)) < 2 * perPixel,
          QString("overview: the window moved 300 px back, %1 -> %2").arg(start).arg(map->viewStart()));
    const qint64 held = map->viewStart();
    for (int i = 0; i < 10; ++i)
      feed.value(1.0);
    check(map->viewStart() == held, "overview: paused, the window stays while readings come");
    // a click at the right end follows again
    mouse(QEvent::MouseButtonPress, map->width() - 2);
    mouse(QEvent::MouseButtonRelease, map->width() - 2);
    check(!graph.livePaused() && graph.stateText().isEmpty() && map->to() - (map->viewStart() + map->viewWidth()) <= 2000,
          "overview: a click at the right end, Live follows");

    // the left bracket 100 px out: a wider window, still following
    drag(map->xOf(map->viewStart()), map->xOf(map->viewStart()) - 100);
    check(graph.windowSeconds() > 250 && graph.windowSeconds() < 350 && !graph.livePaused(),
          QString("overview: the left bracket widens to %1 s, Live follows").arg(graph.windowSeconds()));
    bool anyChecked = false;
    for (QToolButton *b : graph.findChildren<QToolButton *>())
      if (b->isCheckable() && b->isChecked() && b->property("seconds").toInt() != graph.windowSeconds())
        anyChecked = true;
    check(!anyChecked, "overview: no time button for a window of its own");
    // the right bracket 50 px back: narrower, paused
    const int wide = graph.windowSeconds();
    const double right = map->xOf(map->viewStart() + map->viewWidth()) - 1;
    drag(right, right - 50);
    check(graph.windowSeconds() < wide && graph.livePaused(),
          QString("overview: the right bracket back, %1 s, Live paused").arg(graph.windowSeconds()));
    // a time button follows again
    for (QToolButton *b : graph.findChildren<QToolButton *>())
      if (b->text() == "1 min")
        b->click();
    check(!graph.livePaused() && graph.windowSeconds() == 60, "overview: a time button, Live follows");

    // a recording viewed: the overview moves the scroll bar's window
    graph.setMode(GraphWidget::Manual);
    graph.startSLOT();
    for (int i = 0; i < 600; ++i)   // 10 min
      feed.value(i % 60);
    graph.stopSLOT();
    check(graph.store()->state() == RecordingStore::View, "overview: the recording viewed");
    mouse(QEvent::MouseButtonPress, map->width() / 2.0);
    mouse(QEvent::MouseButtonRelease, map->width() / 2.0);
    const qint64 middle = (map->from() + map->to()) / 2;
    check(std::abs(map->viewStart() + 30000 - middle) < 2000 && bar->value() > 0 && bar->value() < bar->maximum(),
          QString("overview: a click in the middle centres the window, %1").arg(map->viewStart()));
    mouse(QEvent::MouseButtonPress, map->width() - 2);
    mouse(QEvent::MouseButtonRelease, map->width() - 2);
    check(bar->value() == bar->maximum(), "overview: the right end, the end of the recording");

    // off in the context menu: the scroll bar again
    graph.setMinimapVisible(false);
    check(!map->isVisible() && bar->isVisible() && graph.findChild<QChartView *>()->height() == graph.height() - 16,
          "overview: off, the scroll bar again");
    graph.setMinimapVisible(true);
    check(map->isVisible() && !bar->isVisible()
            && graph.findChild<QChartView *>()->height() == graph.height() - GraphMinimap::kHeight,
          "overview: on again");
  }

  // --- 7k. the min/max band: off by default; per pixel column the lowest
  //          and highest of the columns within four either side, so it
  //          spreads around a spike by that much and no further; a gap
  //          parts it ---
  {
    Settings cfg("envelope", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    QObject::connect(&graph, &GraphWidget::windowRequested, [&](int s) { graph.setGraphSize(s); });
    Feed feed(graph, 100);
    graph.liveSLOT();
    for (QToolButton *b : graph.findChildren<QToolButton *>())
      if (b->text() == "1 min")
        b->click();
    qint64 spikeT = -1;
    for (int i = 0; i < 900; ++i)   // 1.5 min of 1.0 and 2.0 in turn, a spike of 5 after a minute
    {
      if (i == 600)
        spikeT = feed.now;
      feed.value(i == 600 ? 5.0 : 1.0 + (i % 2));
    }
    check(!graph.envelopeVisible() && graph.envelopePath().isEmpty(), "band: off by default");
    graph.setEnvelopeVisible(true);
    const QPainterPath band = graph.envelopePath();
    QChart *chart = graph.findChild<QChartView *>()->chart();
    QAbstractSeries *curve = chart->series().first();
    auto at = [&](qint64 t, double v) { return chart->mapToPosition(QPointF(t / 1000.0, v), curve); };
    const qint64 middle = spikeT - 20000;
    check(!band.isEmpty() && band.contains(at(middle, 1.5)) && !band.contains(at(middle, 2.3)),
          "band: from 1 to 2 where the curve alternates");
    const QPointF spike = at(spikeT, 4.0);
    check(band.contains(spike + QPointF(3, 0)) && band.contains(spike - QPointF(3, 0)),
          "band: the spike spreads 3 px either side");
    check(!band.contains(spike + QPointF(7, 0)) && !band.contains(spike - QPointF(7, 0)),
          "band: ... and not 7 px");
    // a minute silent, then readings again: two pieces
    feed.now += 1500;
    graph.store()->setStale(true);
    feed.now += 20000;
    const qint64 resumed = feed.now;
    for (int i = 0; i < 100; ++i)
      feed.value(1.0 + (i % 2));
    check(graph.envelopePath().toSubpathPolygons().size() == 2,
          QString("band: the gap parts it, %1 pieces").arg(graph.envelopePath().toSubpathPolygons().size()));
    // Live runs on between the readings (FrameClock): the band goes along
    // with the curve, its shape the same (it lagged and flickered)
    {
      FrameClock *clock = FrameClock::instance();
      clock->tickNow();
      const QRectF before = graph.envelopePath().boundingRect();
      feed.now += 430;   // some pixels and a part of one
      clock->tickNow();
      const QPainterPath moved = graph.envelopePath();
      // the piece after the gap begins at its first reading, now some pixels further left
      const QPointF begin = at(resumed, 1.5);
      check(moved.contains(begin + QPointF(2, 0)) && !moved.contains(begin - QPointF(3, 0)),
            QString("band: Live moved on without a reading, the band with the curve (begins at %1)").arg(begin.x()));
      check(std::abs(moved.boundingRect().height() - before.height()) < 0.5,
            QString("band: its shape stays (height %1 -> %2)").arg(before.height()).arg(moved.boundingRect().height()));
    }
    graph.setEnvelopeVisible(false);
    check(graph.envelopePath().isEmpty(), "band: off again");
  }

  // --- 7m. values of another meter, from its own store: shifted by the
  //          starts of the stores, A on the free right axis, V with the
  //          main value on the left, the meter's name in the legend; its
  //          meter closed, no curve and the values kept ---
  {
    Settings cfg("othermeter", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    graph.setUnit("V");
    Feed feed(graph);
    graph.liveSLOT();
    RecordingStore other;
    Feed far(&other);
    far.now = 2000;   // opened two seconds after the window's
    other.setUnit("A");   // as its MeterController does
    other.live();
    bool open = true;
    graph.setOtherStores([&](const QString &source) { return open && source == "device:b" ? &other : nullptr; },
                         [](const QString &source) { return source == "device:b" ? QStringLiteral("Sim B") : QStringLiteral("Sim W"); });
    graph.setOtherValues({ ValueRef(0, "device:b"), ValueRef(1, "device:b") });
    for (int i = 0; i < 50; ++i)
    {
      feed.value(1.0 + i * 0.01);
      if (i >= 20)
      {
        far.other(1, 3.0, "V", Quantity::Voltage);
        Reading r;
        r.value = 0.5;
        r.text = "0.5";
        r.unit = r.baseUnit = "A";
        r.port.quantity = Quantity::Current;
        r.t = far.now;
        r.msecs = Feed::wall(far.now).toMSecsSinceEpoch();
        other.setReading(r);
        far.now += 100;
      }
    }
    QTest::qWait(400);
    check(graph.otherCurveCount() == 2 && graph.otherCurveShown(0) && graph.otherCurveOnRight(0)
            && graph.rightAxisTitle() == "[A]",
          QString("another meter's A: on the right, %1").arg(graph.rightAxisTitle()));
    check(graph.otherCurveShown(1) && !graph.otherCurveOnRight(1), "its V: on the left with the main value");
    const QList<QPointF> amps = graph.otherCurvePoints(0);
    check(!amps.isEmpty() && qAbs(amps.first().x() - 2.0) < 0.15,
          QString("its time: from 2 s on, %1").arg(amps.isEmpty() ? -1 : amps.first().x()));
    check(graph.legendText().contains("Sim B"), "its name in the legend: " + graph.legendText());
    // an axis chosen: its V on the right is no room (A is there), with a notice
    QSignalSpy notices(&graph, &GraphWidget::notice);
    graph.setOtherValueAxis(ValueRef(1, "device:b"), GraphWidget::AxisRight);
    check(!graph.otherCurveShown(1) && graph.otherValueSide(ValueRef(1, "device:b")) == GraphWidget::AxisAuto
            && notices.size() == 1,
          "its V chosen on the right beside A: not drawn, a notice");
    // the third axis is free: there it is drawn, in its own scale
    graph.setOtherValueAxis(ValueRef(1, "device:b"), GraphWidget::AxisThird);
    check(graph.otherCurveShown(1) && graph.otherCurveSide(1) == 2 && graph.thirdAxisTitle() == "[V]"
            && graph.otherValueSide(ValueRef(1, "device:b")) == GraphWidget::AxisThird
            && graph.sideTitleColor(2) == graph.otherValueColor(ValueRef(1, "device:b")),
          "its V chosen on the third axis: drawn there, the title in its colour, got " + graph.thirdAxisTitle());
    graph.setOtherValueAxis(ValueRef(1, "device:b"), GraphWidget::AxisLeft);
    check(graph.otherValueSide(ValueRef(1, "device:b")) == GraphWidget::AxisLeft
            && graph.otherValueColor(ValueRef(1, "device:b")).isValid(),
          "its V chosen on the left: drawn there, with its colour");
    // the main value off, nothing of this meter on the left: the other
    // meter's V there is the left axis' value - the overview draws it from
    // its store, the limit band is its meter's
    {
      QSignalSpy left(&graph, &GraphWidget::leftValueChanged);
      graph.setValueShown(0, false);
      QTest::qWait(50);
      check(graph.leftValue() == ValueRef(1, "device:b") && !left.isEmpty(),
            QString("main value off: the other meter's V on the left is the axis' value (%1)").arg(graph.leftValue().toString()));
      // its store began 2 s after the graph's: the window there 2 s earlier
      check(graph.minimap()->value() == 1 && graph.minimap()->column(1000) && graph.minimap()->column(1000)->any
              && graph.minimap()->viewStart() <= -1500,
            QString("the overview draws it from its store, on its time (window from %1 ms)").arg(graph.minimap()->viewStart()));
      int bands = 0;
      auto countBands = [&]
      {
        bands = 0;
        for (QGraphicsItem *item : graph.findChild<QChartView *>()->chart()->childItems())
          if (item->data(0).toString() == "limit" && item->isVisible() && !dynamic_cast<QGraphicsLineItem *>(item))
            ++bands;
        return bands;
      };
      graph.setLimits(ValueRef(1, "device:b"), 2.5, 3.5);
      check(countBands() == 1, "its limit band on the axis");
      graph.setLimits(ValueRef(1), 2.5, 3.5);
      check(countBands() == 0, "this meter's value 2: not the left axis' value, no band");
      graph.setLimits(-1);
      // the left axis free: the other meter's A chosen there takes it, its V gives way
      graph.setOtherValueAxis(ValueRef(0, "device:b"), GraphWidget::AxisLeft);
      check(graph.otherCurveShown(0) && !graph.otherCurveOnRight(0) && !graph.otherCurveShown(1)
              && graph.leftAxisTitle().endsWith("A]"),
            "main value off: another meter's A chosen on the left is drawn there, " + graph.leftAxisTitle());
      graph.setOtherValueAxis(ValueRef(0, "device:b"), GraphWidget::AxisAuto);
      check(graph.otherCurveOnRight(0) && graph.otherCurveShown(1) && !graph.otherCurveOnRight(1),
            "its A by itself again: on the right, its V on the left");
      graph.setValueShown(0, true);
      check(graph.leftValue() == ValueRef(0) && graph.minimap()->value() == 0, "main value back: its axis, its overview");
    }
    // one list for all meters: the graph's meter (its source, or none) and the others
    {
      graph.setBaseSource("device:w");
      QSignalSpy changed(&graph, &GraphWidget::valuesChanged);
      check(graph.values() == QList<ValueRef>{ ValueRef(0, "device:w"), ValueRef(0, "device:b"), ValueRef(1, "device:b") },
            "values: the graph's meter's main value with its source, then the other meter's");
      graph.setValues({ ValueRef(1, "device:b"), ValueRef(0) });
      check(graph.mainShown() && graph.otherValues() == QList<ValueRef>{ ValueRef(1, "device:b") }
              && graph.valueShown(ValueRef(0, "device:w")) && !graph.valueShown(ValueRef(0, "device:b"))
              && changed.size() == 1,
            "setValues: an empty source and its own source are the graph's meter");
      graph.setValueShown(ValueRef(0, "device:b"), true);
      check(graph.valueShown(ValueRef(0, "device:b")) && changed.size() == 2, "setValueShown: another meter's");
      check(graph.drawnColor(ValueRef(0, "device:w")) == graph.valueColor(0) && !graph.drawnColor(ValueRef(2, "device:w")).isValid(),
            "drawnColor: the main curve's, none for a value not drawn");
    }
    graph.setOtherValueShown(ValueRef(1, "device:b"), false);
    check(graph.otherValues() == QList<ValueRef>{ ValueRef(0, "device:b") }, "one hidden: one kept");
    graph.setOtherValueShown(ValueRef(1, "device:b"), true);
    open = false;
    graph.otherStoresChanged();
    check(!graph.otherCurveShown(0) && !graph.otherCurveShown(1) && graph.otherValues().size() == 2
            && !graph.rightAxis()->isVisible(),
          "closed: no curves, the values kept");

    // the export has its values after its name; the file imported shows
    // them as curves of their own, not among the values chosen, until Live
    open = true;
    graph.otherStoresChanged();
    const QString both = tmpDir.path() + "/othermeter.csv";
    check(graph.exportCsvFile(both, true) && readFile(both).startsWith(
            "timestamp;time (s);Sim W: value;Sim W: unit;Sim W: status;Sim B: value;Sim B: unit;Sim B: status;Sim B: value 2;"),
          "export: every meter's values after its name\n" + readFile(both).section('\n', 0, 2));
    GraphWidget viewed(nullptr, &cfg);
    viewed.resize(800, 500);
    viewed.show();
    viewed.setOtherStores([&](const QString &source) { return source == "device:b" ? &other : nullptr; },
                          [](const QString &) { return QStringLiteral("Sim B"); });
    viewed.setOtherValues({ ValueRef(0, "device:b") });
    check(viewed.importCsvFile(both) && viewed.otherCurveCount() == 3 && viewed.otherValues().size() == 1
            && !viewed.otherCurveShown(0),
          QString("import: its values as curves, %1; the meter open now not drawn").arg(viewed.otherCurveCount()));
    QTest::qWait(50);
    const QList<QPointF> fileAmps = viewed.otherCurvePoints(1);
    check(viewed.otherCurveShown(1) && viewed.legendText().contains("Sim B") && !fileAmps.isEmpty()
            && qAbs(fileAmps.first().x() - 2.0) < 0.15,
          QString("import: drawn from 2 s on, %1; %2").arg(fileAmps.isEmpty() ? -1 : fileAmps.first().x()).arg(viewed.legendText()));
    const QString again = tmpDir.path() + "/othermeter2.csv";
    check(viewed.exportCsvFile(again, true) && readFile(again) == readFile(both), "import, export: the same file");
    viewed.liveSLOT();
    check(viewed.otherCurveCount() == 1 && viewed.otherCurveShown(0), "Live: the file's other meter gone, the open one back");
  }

  // --- 7l. the meter's other values: a curve each in a colour of its own;
  //          the main unit on the left axis, the first other one on a right
  //          axis with its own scale, a third unit not drawn; a legend; a
  //          value can be hidden; phosphor tells them apart by dashes ---
  {
    Settings cfg("othervalues", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    graph.setUnit("V");
    Feed feed(graph);
    graph.liveSLOT();
    check(graph.legendText().isEmpty() && !graph.rightAxis()->isVisible(), "other values: none, no legend, no right axis");
    for (int i = 0; i < 50; ++i)
    {
      feed.other(1, 20 + i * 0.1, "°C", Quantity::Temperature);   // comes before the main value: dropped at first
      feed.value(1.0 + i * 0.01);
      feed.other(2, 0.5, "V", Quantity::Voltage, "Battery voltage");
      feed.other(3, 50, "Hz", Quantity::Frequency);
    }
    QTest::qWait(20);
    const RecordingStore *store = graph.store();
    check(store->extraCount() == 3 && graph.extraCurveCount() == 3, QString("other values: %1 curves").arg(graph.extraCurveCount()));
    // off at first: the graph shows the main value, as it always did
    check(!graph.extraCurveShown(0) && !graph.extraCurveShown(1) && !graph.extraCurveShown(2)
            && graph.legendText().isEmpty() && !graph.rightAxis()->isVisible(),
          "other values: recorded, not drawn until switched on");
    for (int id = 1; id <= 3; ++id)
      graph.setValueShown(id, true);
    int celsius = -1, volts = -1, hertz = -1;
    for (int i = 0; i < store->extraCount(); ++i)
      (store->extra(i).id == 1 ? celsius : store->extra(i).id == 2 ? volts : hertz) = i;
    check(graph.extraCurveShown(celsius) && graph.extraCurveOnRight(celsius) && graph.extraCurve(celsius)->count() > 10,
          QString("°C: on the right, %1 points").arg(graph.extraCurve(celsius)->count()));
    check(graph.rightAxis()->isVisible() && graph.rightAxisTitle() == "[°C]"
            && graph.rightAxis()->min() <= 20.1 && graph.rightAxis()->max() >= 24.9,
          QString("right axis [°C] %1..%2").arg(graph.rightAxis()->min()).arg(graph.rightAxis()->max()));
    check(graph.extraCurveShown(volts) && !graph.extraCurveOnRight(volts), "V: with the main value on the left");
    check(graph.extraCurveShown(hertz) && graph.extraCurveSide(hertz) == 2 && graph.thirdAxis()->isVisible()
            && graph.thirdAxisTitle() == "[Hz]" && graph.thirdAxis()->min() <= 50 && graph.thirdAxis()->max() >= 50,
          QString("Hz: a third unit, on the third axis %1").arg(graph.thirdAxisTitle()));
    // the right axes' titles in their curves' colours: which is whose
    check(graph.sideTitleColor(1) == graph.extraCurve(celsius)->pen().color()
            && graph.sideTitleColor(2) == graph.extraCurve(hertz)->pen().color(),
          QString("axis titles in the curves' colours: %1 %2").arg(graph.sideTitleColor(1).name(), graph.sideTitleColor(2).name()));
    const QString legend = graph.legendText();
    check(legend.contains("Temperature") && legend.contains("°C") && legend.contains("Hz"),
          "legend names the curves drawn: " + legend);
    check(legend.contains("Battery voltage (V)"), "the meter's own name of a value in the legend: " + legend);
    check(graph.extraCurve(celsius)->pen().color() != graph.extraCurve(volts)->pen().color(),
          "other values: a colour each");
    // a value that keeps coming adds to its curve
    const int before = graph.extraCurve(celsius)->count();
    for (int i = 0; i < 20; ++i)
    {
      feed.value(1.0);
      feed.other(1, 30, "°C", Quantity::Temperature);
    }
    check(graph.extraCurve(celsius)->count() > before && graph.rightAxis()->max() >= 30,
          QString("°C goes on: %1 -> %2 points, axis up to %3").arg(before).arg(graph.extraCurve(celsius)->count())
            .arg(graph.rightAxis()->max()));

    // the integration curve of value 2: its series' integral, on its axis
    {
      auto *intSeries = qobject_cast<QXYSeries *>(graph.findChild<QChartView *>()->chart()->series().at(2));
      graph.setIntegration(true, 1.0, 0.0, 0.0);
      graph.setIntegrationValue(1);
      const RecordingSeries &c = store->extra(celsius);
      check(intSeries->count() > 5 && intSeries->attachedAxes().contains(graph.rightAxis())
              && std::abs(intSeries->points().last().y() - c.last().integral) < 1e-6,
            QString("integral of value 2: %1 points, last %2, the store's %3")
              .arg(intSeries->count()).arg(intSeries->count() ? intSeries->points().last().y() : 0).arg(c.last().integral));
      const double before = c.last().integral;
      feed.value(1.0);
      feed.other(1, 30, "°C", Quantity::Temperature);
      check(store->extra(celsius).last().integral > before
              && std::abs(intSeries->points().last().y() - store->extra(celsius).last().integral) < 1e-6,
            "integral of value 2 goes on with its readings");
      graph.setIntegrationValue(0);
      check(intSeries->attachedAxes().contains(graph.findChild<QChartView *>()->chart()->axes(Qt::Vertical).first())
              && !intSeries->attachedAxes().contains(graph.rightAxis())
              && std::abs(intSeries->points().last().y() - store->series().last().integral) < 1e-6,
            "integral of value 1 again, on the left");
      graph.setIntegration(false, 1.0, 0.0, 0.0);
    }
    // value 2 integrated while it is hidden: the integral still on its axis
    {
      QChart *chart = graph.findChild<QChartView *>()->chart();
      auto *intSeries = qobject_cast<QXYSeries *>(chart->series().at(2));
      graph.setValueShown(1, false);
      graph.setIntegration(true, 1.0, 0.0, 0.0);
      graph.setIntegrationValue(1);
      check(intSeries->attachedAxes().contains(graph.rightAxis()) && graph.rightAxis()->isVisible()
              && graph.rightAxisTitle() == "[°C]" && graph.rightAxis()->max() >= 30,
            QString("integral of hidden value 2: on the right %1, axis %2 up to %3")
              .arg(intSeries->attachedAxes().contains(graph.rightAxis())).arg(graph.rightAxisTitle())
              .arg(graph.rightAxis()->max()));
      graph.setIntegrationValue(0);
      graph.setIntegration(false, 1.0, 0.0, 0.0);
      graph.setValueShown(1, true);
    }
    // the auto scale of the left axis: the other values drawn there count,
    // also after setScale(); hidden, they no longer do
    {
      auto *left = qobject_cast<QValueAxis *>(graph.findChild<QChartView *>()->chart()->axes(Qt::Vertical).first());
      graph.setScale(true, false, 0, 0);
      check(left->min() < 0.5, QString("auto scale with 0.5 V on the left: from %1").arg(left->min()));
      graph.setValueShown(2, false);
      check(left->min() > 0.5, QString("0.5 V hidden: the scale from %1").arg(left->min()));
      graph.setValueShown(2, true);
      check(left->min() < 0.5, QString("0.5 V shown again: from %1").arg(left->min()));
    }

    // a time cursor shows the other values drawn there
    graph.setTimeCursor(0, 2000);
    check(graph.timeCursorText().contains("°C") && graph.timeCursorText().contains("500 mV")
            && graph.timeCursorText().contains("50 Hz"),
          "time cursor with the other values: " + graph.timeCursorText());
    // a picture to look at: QTDMM_SHOT=<dir>
    if (qEnvironmentVariableIsSet("QTDMM_SHOT"))
    {
      QTest::qWait(50);
      graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/graph_other_values.png");
      graph.setColorVariant(GraphWidget::ScopeBlue);
      QTest::qWait(50);
      graph.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/graph_other_values_scope.png");
      graph.setColorVariant(GraphWidget::Neutral);
    }

    // hidden: °C goes, Hz gets the right axis
    graph.setValueShown(1, false);
    check(!graph.extraCurveShown(celsius) && graph.extraCurveShown(hertz) && graph.rightAxisTitle() == "[Hz]"
            && !graph.thirdAxis()->isVisible(),
          "°C hidden: Hz on the right axis, no third");
    check(graph.shownValues() == (QList<int>{ 2, 3 }), "shown values: 2 3");
    graph.setValueShown(1, true);
    check(graph.extraCurveShown(celsius) && graph.extraCurveSide(celsius) == 1 && graph.extraCurveShown(hertz)
            && graph.extraCurveSide(hertz) == 2,
          "°C shown again: right, Hz on the third axis");

    // a line style by the value's number, in every design: overlapping
    // curves are all seen, also in phosphor's one hue
    graph.setColorVariant(GraphWidget::PhosphorGreen);
    check(graph.extraCurve(celsius)->pen().style() != Qt::SolidLine, "phosphor: the other values dashed");
    graph.setColorVariant(GraphWidget::Neutral);
    check(graph.extraCurve(celsius)->pen().style() != Qt::SolidLine
            && graph.extraCurve(celsius)->pen().style() != graph.extraCurve(hertz)->pen().style(),
          "neutral: dashed too, a style each");

    // a new window starts with what the displays show
    graph.liveSLOT();
    bool fresh = true;
    for (int i = 0; i < graph.extraCurveCount(); ++i)
      fresh = fresh && (!graph.extraCurveShown(i) || graph.extraCurve(i)->count() <= 1);
    check(fresh && graph.rightAxis()->isVisible(), "Live anew: the other curves start again from what is shown");
    // a recording of a meter without other values: none
    graph.store()->clear();
    feed.value(1.0);
    check(!graph.rightAxis()->isVisible() && graph.legendText().isEmpty(), "cleared: no other curves, no legend");
  }

  // --- 7m. a recording that grows: the columns are counted anew only when
  //          they double, not with every few readings (the graph had built
  //          the whole curve anew once a column grew, ~7.7 ms a reading at
  //          2000 readings; R12) ---
  {
    Settings cfg("record-speed", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    // as the settings start: a solid line, no points, no integral
    graph.setLineStyle(GraphWidget::Solid, GraphWidget::NoPoint, GraphWidget::NoLine, GraphWidget::Circle);
    Feed feed(graph);
    graph.startSLOT();
    const int rebuilds = graph.rebuilds();
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < 2000; ++i)
      feed.value(1.0 + (i % 10) * 0.01);
    qInfo() << "record: 2000 readings in" << timer.elapsed() << "ms," << graph.rebuilds() - rebuilds << "rebuilds";
    check(graph.rebuilds() - rebuilds < 20,
          QString("record: 2000 readings built the curve anew %1 times").arg(graph.rebuilds() - rebuilds));
    // hidden points are left empty, and filled once they are switched on
    const QList<QAbstractSeries *> series = graph.findChild<QChartView *>()->chart()->series();
    auto *line = qobject_cast<QLineSeries *>(series.first());
    QScatterSeries *points = nullptr;
    for (QAbstractSeries *s : series)
      if (auto *scatter = qobject_cast<QScatterSeries *>(s); scatter && !points)
        points = scatter;
    check(points && points->count() == 0, "record: no points while they are hidden");
    graph.setLineStyle(GraphWidget::Solid, GraphWidget::Circle, GraphWidget::NoLine, GraphWidget::Circle);
    check(points && points->count() > 0 && points->count() == line->count(),
          QString("record: the points once switched on, %1 of %2").arg(points ? points->count() : -1)
            .arg(line->count()));
    timer.restart();
    for (int i = 0; i < 500; ++i)
      feed.value(1.0 + (i % 10) * 0.01);
    qInfo() << "record: 500 readings with points in" << timer.elapsed() << "ms";
    check(points && points->count() == line->count(), "record: the points go on with the line");
  }

  // --- 7m2. a long recording in a short window: a reading costs what the
  //           window has, not what the recording has (the chart got every
  //           point, ~8 ms a reading at 40000; R14-02) ---
  {
    Settings cfg("record-long", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    graph.setLineStyle(GraphWidget::Solid, GraphWidget::NoPoint, GraphWidget::NoLine, GraphWidget::Circle);
    graph.setGraphSize(60);
    Feed feed(graph);
    graph.startSLOT();
    QElapsedTimer timer;
    auto block = [&]
    {
      timer.restart();
      for (int i = 0; i < 1000; ++i)
        feed.value(1.0 + (i % 10) * 0.01);
      return timer.elapsed();
    };
    block();
    const qint64 early = block();
    for (int k = 0; k < 16; ++k)
      block();
    const qint64 late = block();
    auto *line = qobject_cast<QLineSeries *>(graph.findChild<QChartView *>()->chart()->series().first());
    qInfo() << "record: 1000 readings at 2000 in" << early << "ms, at 19000 in" << late << "ms; the chart has"
            << line->count() << "points";
    check(late < 3 * early + 50, QString("record: 1000 readings at 19000 took %1 ms, at 2000 %2 ms").arg(late).arg(early));
    check(line->count() < 5000, QString("record: the chart has the points around the window, %1").arg(line->count()));
  }

  // --- 7m3. the same with points and the integral's points: the scatter
  //           series had the whole recording, copied and laid out anew with
  //           each reading (50 ms a reading at 6000; R14-08) ---
  {
    Settings cfg("record-long-points", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    graph.setLineStyle(GraphWidget::Solid, GraphWidget::Circle, GraphWidget::Solid, GraphWidget::Circle);
    graph.setIntegration(true, 1, 0, 0);
    graph.setGraphSize(60);
    Feed feed(graph);
    graph.startSLOT();
    QElapsedTimer timer;
    auto block = [&]
    {
      timer.restart();
      for (int i = 0; i < 1000; ++i)
        feed.value(1.0 + (i % 10) * 0.01);
      return timer.elapsed();
    };
    block();
    const qint64 early = block();
    for (int k = 0; k < 6; ++k)
      block();
    const qint64 late = block();
    QList<QScatterSeries *> scatters;
    for (QAbstractSeries *s : graph.findChild<QChartView *>()->chart()->series())
      if (auto *scatter = qobject_cast<QScatterSeries *>(s))
        scatters << scatter;
    const int dataPts = scatters.value(0) ? scatters.value(0)->count() : -1;
    const int intPts = scatters.value(1) ? scatters.value(1)->count() : -1;
    qInfo() << "record with points: 1000 readings at 2000 in" << early << "ms, at 9000 in" << late
            << "ms; the chart has" << dataPts << "and" << intPts << "points";
    check(late < 3 * early + 50,
          QString("record with points: 1000 readings at 9000 took %1 ms, at 2000 %2 ms").arg(late).arg(early));
    check(dataPts > 0 && dataPts < 5000 && intPts > 0 && intPts < 5000,
          QString("record with points: the chart has the points around the window, %1 and %2").arg(dataPts).arg(intPts));
  }

  // --- 7l3. three values in three units (CO2, °C, %RH): at most two at a
  //          time, the main value can be hidden - the left axis takes the
  //          unit of the lowest value shown; its lines go with it ---
  {
    Settings cfg("threevalues", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    graph.setUnit("ppm");
    Feed feed(graph);
    graph.liveSLOT();
    graph.setReference(GraphWidget::RefValue, 420);
    for (int i = 0; i < 40; ++i)
    {
      feed.value(400 + i);
      feed.other(1, 22 + i * 0.01, "°C", Quantity::Temperature);
      feed.other(2, 45, "%", Quantity::RelativeHumidity);
    }
    QTest::qWait(20);
    graph.setValueShown(1, true);
    graph.setValueShown(2, true);
    int celsius = -1, percent = -1;
    for (int i = 0; i < graph.store()->extraCount(); ++i)
      (graph.store()->extra(i).id == 1 ? celsius : percent) = i;
    check(celsius >= 0 && percent >= 0 && graph.extraCurveShown(celsius) && graph.extraCurveSide(celsius) == 1
            && graph.extraCurveShown(percent) && graph.extraCurveSide(percent) == 2 && graph.thirdAxisTitle() == "[%]",
          "three units: ppm left, °C right, % on the third axis");
    graph.setValueShown(0, false);
    check(!graph.mainShown() && graph.leftUnit() == "°C" && graph.leftAxisTitle() == "[°C]",
          "main hidden: the left axis is °C, got " + graph.leftAxisTitle());
    check(graph.extraCurveShown(celsius) && !graph.extraCurveOnRight(celsius) && graph.extraCurveShown(percent)
            && graph.extraCurveOnRight(percent) && graph.rightAxisTitle() == "[%]",
          "main hidden: °C left, % right, got " + graph.rightAxisTitle());
    QValueAxis *left = nullptr;
    for (QAbstractAxis *a : graph.findChild<QChartView *>()->chart()->axes(Qt::Vertical))
      if (a != graph.rightAxis() && a != graph.thirdAxis())
        left = qobject_cast<QValueAxis *>(a);
    check(left && left->min() <= 22.0 && left->max() >= 22.39 && left->max() < 100,
          QString("main hidden: the left scale is the temperature's, %1..%2").arg(left ? left->min() : 0).arg(left ? left->max() : 0));
    check(!graph.legendText().contains("ppm") && graph.legendText().contains("°C"), "main hidden: not in the legend");
    check(!graph.referenceShown() && graph.referenceId() == 1, "main hidden: 420 is no temperature, no reference line");
    // the reference line, the band, the cursor levels and the cursor box
    // follow the value of the left axis
    graph.setReference(GraphWidget::RefMean);
    check(graph.referenceShown() && graph.referenceValue() > 22 && graph.referenceValue() < 22.4,
          QString("main hidden: the mean of the temperature, got %1").arg(graph.referenceValue()));
    graph.setEnvelopeVisible(true);
    check(!graph.envelopePath().isEmpty(), "main hidden: the band around the temperature");
    graph.setEnvelopeVisible(false);
    graph.setTimeCursor(0, 200);
    graph.setTimeCursor(1, 3000);
    {
      const QStringList lines = graph.timeCursorText().split('\n');
      check(lines.size() == 5 && lines[3].startsWith(QString::fromUtf8("Δ 0.28 °C  mean 22.")) && lines[3].endsWith("°C")
              && lines[4] == QString::fromUtf8("Δ 0 %  mean 45 %") && !graph.timeCursorText().contains("ppm"),
            "main hidden: a line per value, no ppm, no integral: " + lines.join(" | "));
      int levels = 0;
      for (QGraphicsItem *item : graph.findChild<QChartView *>()->chart()->childItems())
        levels += item->data(0).toString() == "cursor-level" && item->isVisible();
      check(levels == 2, QString("main hidden: the cursor levels on the temperature, got %1").arg(levels));
    }
    graph.clearTimeCursors();
    feed.value(600);   // the main value goes on: not on the left axis now
    check(left && left->max() < 100, "main hidden: its readings do not widen the scale");
    graph.setValueShown(0, true);
    check(graph.leftAxisTitle() == "[ppm]" && graph.extraCurveSide(celsius) == 1 && graph.extraCurveSide(percent) == 2
            && left && left->max() >= 600 && graph.referenceShown(),
          "main back: ppm left again, °C right, % third, the scale has its values, got " + graph.leftAxisTitle());
  }

  // --- 7l4. an axis chosen per value (the sidebar's value menu): it takes
  //          that axis for its unit, the other unit's curves there go off ---
  {
    Settings cfg("valueaxes", tmpDir.path());
    GraphWidget graph(nullptr, &cfg);
    graph.resize(800, 500);
    graph.show();
    QTest::qWait(50);
    graph.setUnit("V");
    Feed feed(graph);
    graph.liveSLOT();
    for (int i = 0; i < 20; ++i)
    {
      feed.value(12 + i * 0.01);
      feed.other(1, -2, "A", Quantity::Current);
      feed.other(2, 25, "°C", Quantity::Temperature);
      feed.other(3, 13.1, "V", Quantity::Voltage);
    }
    QTest::qWait(20);
    QSignalSpy notices(&graph, &GraphWidget::notice);
    check(graph.valueAxis(0) == GraphWidget::AxisLeft && graph.valueAxis(2) == GraphWidget::AxisAuto,
          "axes: the main value left, the others automatic");
    graph.setValueShown(1, true);
    graph.setValueShown(3, true);
    check(graph.valueDrawn(0) && graph.valueDrawn(1) && graph.valueDrawn(3) && graph.leftAxisTitle() == "[V]"
            && graph.rightAxisTitle() == "[A]",
          "axes: two voltages left, the current right, got " + graph.rightAxisTitle());
    check(graph.valueSide(1) == GraphWidget::AxisRight && graph.valueSide(3) == GraphWidget::AxisLeft
            && graph.valueSide(2) == GraphWidget::AxisAuto,
          "axes: the side each is drawn on, none for one off");
    // the temperature to the right: the current (automatic) moves to the
    // third axis, nothing goes off
    graph.setValueAxis(2, GraphWidget::AxisRight);
    graph.setValueShown(2, true);
    check(graph.valueDrawn(2) && graph.valueDrawn(1) && graph.valueSide(1) == GraphWidget::AxisThird
            && graph.rightAxisTitle() == QString::fromUtf8("[°C]") && graph.thirdAxisTitle() == "[A]"
            && graph.valueDrawn(0) && graph.valueDrawn(3) && notices.isEmpty(),
          "axes: °C right, the current on the third axis, got " + graph.rightAxisTitle() + " " + graph.thirdAxisTitle());
    // a fourth unit has no axis: chosen for the third, the current goes off
    feed.other(4, 0.5, "Hz", Quantity::Frequency);
    graph.setValueAxis(4, GraphWidget::AxisThird);
    graph.setValueShown(4, true);
    check(graph.valueDrawn(4) && graph.thirdAxisTitle() == "[Hz]" && !graph.valueDrawn(1) && !graph.shownValues().contains(1),
          "axes: Hz on the third axis, the current off, got " + graph.thirdAxisTitle());
    check(notices.size() == 1 && notices.first().first().toString().contains("third"),
          "axes: a notice of what went off, got " + (notices.isEmpty() ? QString() : notices.first().first().toString()));
    graph.setValueShown(4, false);
    // the current to the left: both voltages go off, the left axis is A
    graph.setValueAxis(1, GraphWidget::AxisLeft);
    graph.setValueShown(1, true);
    check(graph.valueDrawn(1) && !graph.mainShown() && !graph.valueDrawn(3) && graph.leftAxisTitle() == "[A]"
            && graph.valueDrawn(2) && graph.referenceId() == 1,
          "axes: A left, the voltages off, got " + graph.leftAxisTitle());
    // the overview draws the value of the left axis, the main one is off
    {
      const auto *strip = graph.findChild<GraphMinimap *>();
      const GraphMinimap::Column *c = strip ? strip->column(graph.store()->extra(0).last().t) : nullptr;
      check(graph.minimapValue() == 1 && strip && strip->value() == 1 && c && c->any && c->max < 0,
            QString("axes: the overview has the current, got value %1, max %2")
              .arg(strip ? strip->value() : -1).arg(c ? c->max : 0));
      feed.other(1, -3, "A", Quantity::Current);
      c = strip ? strip->column(graph.store()->extra(0).last().t) : nullptr;
      check(c && c->min <= -3, "axes: a new current reading joins the overview");
    }
    // the main value back: the current chose the left axis, so it goes off
    graph.setValueShown(0, true);
    check(graph.mainShown() && !graph.valueDrawn(1) && graph.leftAxisTitle() == "[V]" && graph.valueDrawn(2),
          "axes: the main value back, the current off, got " + graph.leftAxisTitle());
    // the second voltage to the right in the same unit: an axis of its own
    graph.setValueAxis(3, GraphWidget::AxisRight);
    graph.setValueShown(3, true);
    check(graph.valueDrawn(3) && graph.rightAxisTitle() == "[V]" && !graph.valueDrawn(2),
          "axes: the voltage right, the temperature off, got " + graph.rightAxisTitle());
    // automatic again: the voltage shares the left axis
    graph.setValueAxis(3, GraphWidget::AxisAuto);
    check(graph.valueDrawn(3) && graph.valueAxis(3) == GraphWidget::AxisAuto, "axes: automatic, on the left");
    // the left axis chosen in its unit: it joins the main value, nothing goes off
    const qsizetype before = notices.size();
    graph.setValueAxis(3, GraphWidget::AxisLeft);
    check(graph.valueSide(3) == GraphWidget::AxisLeft && graph.valueDrawn(0) && notices.size() == before,
          "axes: the same unit joins the axis");
    check(graph.valueColor(1) != graph.valueColor(0), "axes: a colour per curve");
  }

  // --- 8. EngNumberValidator: what engValue() writes, value() must read
  //         back. engValue() emits "µ" while value() used to recognise only
  //         "u", so micro thresholds silently lost their factor. ---
  {
    struct { double v; const char *text; } cases[] = {
      {1500.0,   "1.5k"},
      {0.0015,   "1.5m"},
      {1.5e-6,   "1.5µ"},
      {2.5e9,    "2.5G"},
      {42.0,     "42"},
    };
    for (const auto &c : cases)
    {
      QString written = EngNumberValidator::engValue(c.v);
      check(written == QString::fromUtf8(c.text),
            QString("engValue(%1) should be '%2', got '%3'").arg(c.v).arg(c.text).arg(written));
      check(qFuzzyCompare(EngNumberValidator::value(written) + 1.0, c.v + 1.0),
            QString("value(engValue(%1)) should round-trip, got %2")
              .arg(c.v).arg(EngNumberValidator::value(written)));
    }
    check(qFuzzyCompare(EngNumberValidator::value("1.5u"), 1.5e-6),
          "value() should accept the ASCII 'u' for micro");
  }

  // --- 9. the modes: Live follows the newest reading; recording shows
  //         "● REC" top left, View where the curve comes from; export while
  //         live saves the window up to now; a file loaded is viewed ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(10);
    graph.setGraphSize(10);        // a window of 10 s
    graph.setSampleLength(300);         // live keeps 30 s
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph, 500);
    QSignalSpy states(&graph, &GraphWidget::stateChanged);
    graph.liveSLOT();
    check(states.size() == 1 && states.first().first().toInt() == RecordingStore::Live, "modes: Live said");
    check(graph.stateText().isEmpty(), "modes: nothing top left while live");
    for (int i = 0; i < 80; ++i)        // 40 s
      feed.value(i);
    auto *x = qobject_cast<QValueAxis *>(graph.findChild<QChartView *>()->chart()->axes(Qt::Horizontal).first());
    const double newest = graph.store()->series().last().t / 1000.0;
    // Live is "All": the window grows to the 30 s kept, then moves on
    check(graph.windowSeconds() == 30, QString("live: All up to the length, %1 s").arg(graph.windowSeconds()));
    check(qAbs(x->max() - newest) < 0.01 && qAbs(x->max() - x->min() - 29) < 0.01,
          QString("live: the window %1..%2 s follows the newest at %3 s").arg(x->min()).arg(x->max()).arg(newest));
    check(graph.store()->origin() == graph.store()->duration() - 30000, "live: keeps the recording length");
    check(!graph.findChild<QScrollBar *>()->isEnabled(), "live: the scroll bar rests");

    const QString liveFile = tmpDir.filePath("live.csv");
    check(graph.exportCsvFile(liveFile, true), "live export: written");
    const std::optional<Recording> saved = RecordingFile::read(liveFile);
    check(saved && saved->values.size() >= 60 && saved->values.last() == 79,
          QString("live export: %1 values up to the newest").arg(saved ? saved->values.size() : -1));
    check(graph.store()->state() == RecordingStore::Live, "live export: still live");

    graph.setSampleLength(50);          // 5 s
    graph.startSLOT();
    check(graph.store()->count() == 1 && graph.findChild<QScrollBar *>()->isEnabled(), "record: cleared");
    for (int i = 0; i < 4; ++i)         // 2 s
      feed.value(i);
    graph.store()->poll();              // the clock of the label, every second
    check(graph.stateText() == QString::fromUtf8("● REC 0:02 / 0:05"),
          "record: top left " + graph.stateText());
    auto *label = graph.findChild<QLabel *>("ui_graphState");
    check(label && label->isVisible() == graph.isVisible() && label->text() == graph.stateText(), "record: the label shows it");
    graph.setSampleLength(0);
    check(graph.stateText() == QString::fromUtf8("● REC 0:02"), "record without a length: " + graph.stateText());
    graph.stopSLOT();
    const QString shown = QLocale().toString(Feed::wall(40000), QLocale::ShortFormat);
    check(graph.stateText() == QString::fromUtf8("Recording of %1 · 0:02").arg(shown),
          "view: top left " + graph.stateText());

    check(graph.importCsvFile(dataDir + "/new_larger.csv"), "view: import");
    check(graph.store()->state() == RecordingStore::View && graph.stateText().startsWith("new_larger.csv · "),
          "view: the file top left, got " + graph.stateText());
    graph.liveSLOT();
    check(graph.stateText().isEmpty() && graph.store()->count() <= 1, "live again: the file goes");
  }

  // --- 9b. the status bar's time runs on while live, also with a fixed
  //          window (only "All", whose window grows, said it anew) ---
  {
    GraphWidget graph(nullptr, &settings);
    QObject::connect(&graph, &GraphWidget::windowRequested, [&](int s) { graph.setGraphSize(s); });
    Feed feed(graph, 1000);
    graph.liveSLOT();
    for (QToolButton *b : graph.findChildren<QToolButton *>())
      if (b->text() == "1 min")
        b->click();
    QSignalSpy info(&graph, &GraphWidget::info);
    for (int i = 0; i < 70; ++i)
      feed.value(1.0);
    graph.store()->poll();   // the store's clock, every second
    check(!info.isEmpty() && info.last().first().toString().startsWith("1:10"),
          "live: the status bar says 1:10 with a window of 1 min, got "
            + (info.isEmpty() ? QString("nothing") : info.last().first().toString()));
  }

  // --- 9c. the FrameClock: Live runs on with the time between the readings
  //          (a Victron sends one a second), held or hidden it does not;
  //          a recording seen at its end runs on as well ---
  {
    GraphWidget graph(nullptr, &settings);
    graph.setSampleTime(10);
    graph.setGraphSize(10);
    graph.setSampleLength(100);         // live keeps 10 s
    graph.setMode(GraphWidget::Manual);
    Feed feed(graph, 1000);
    graph.show();
    graph.liveSLOT();
    for (int i = 0; i < 30; ++i)        // 30 s, one reading a second
      feed.value(i);
    FrameClock *clock = FrameClock::instance();
    check(clock->wanted(&graph) && clock->isRunning(), "clock: wanted while live and shown");
    clock->tickNow();                   // now: a second after the last reading
    const double before = graph.timeAxisStart();
    feed.now += 400;                    // 0.4 s later, no reading
    clock->tickNow();
    check(std::abs(graph.timeAxisStart() - before - 0.4) < 0.01,
          QString("clock: the window moved on by %1 s without a reading").arg(graph.timeAxisStart() - before));
    Q_EMIT graph.minimap()->moveRequested(graph.store()->origin());   // held by the overview
    const double held = graph.timeAxisStart();
    feed.now += 400;
    clock->tickNow();
    check(graph.livePaused() && graph.timeAxisStart() == held, "clock: held, the window stays");
    Q_EMIT graph.minimap()->endRequested();
    graph.hide();
    check(!clock->wanted(&graph), "clock: hidden, not wanted");
    graph.show();
    // a recording longer than the window, seen at its end
    graph.setSampleLength(0);
    graph.startSLOT();
    graph.setGraphSize(5);              // not "All": a window of 5 s
    for (int i = 0; i < 20; ++i)
      feed.value(i);
    clock->tickNow();
    const double recorded = graph.timeAxisStart();
    feed.now += 500;
    clock->tickNow();
    check(std::abs(graph.timeAxisStart() - recorded - 0.5) < 0.01,
          QString("clock: recording at its end runs on, moved %1 s").arg(graph.timeAxisStart() - recorded));
    graph.findChild<QScrollBar *>()->setValue(0);   // scrolled back
    const double back = graph.timeAxisStart();
    feed.now += 500;
    clock->tickNow();
    check(graph.timeAxisStart() == back, "clock: scrolled back, the window stays");
    graph.stopSLOT();
    check(!clock->wanted(&graph), "clock: viewing a recording, not wanted");
  }

  if (failed == 0)
    qInfo() << "All GraphWidget baseline tests passed.";
  else
    qWarning() << failed << "GraphWidget baseline test(s) failed.";

  return failed == 0 ? 0 : 1;
}
