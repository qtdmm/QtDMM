// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
// QtDMM's own fixes in QCustomPlot 2.1.1 (src/3rdparty/qcustomplot, the
// lines marked "QtDMM:"): the colour map's cells, the time tickers' sub
// ticks for a huge step, the scatter sampling for a huge pixel span. The
// colour map's checks fail without the fixes; the other two were undefined
// behaviour (a double out of int's range made int), which a release build
// on x86 happens to survive: there they only check the results stay right.
#include "ui/views/qcp.h"
#include "testutil.h"

#include <QApplication>
#include <cmath>

#ifndef QTDMM_NEOQCP
namespace
{
struct TimeTicker : QCPAxisTickerTime
{
  using QCPAxisTickerTime::getSubTickCount;
};
struct DateTimeTicker : QCPAxisTickerDateTime
{
  using QCPAxisTickerDateTime::getSubTickCount;
};
struct Graph : QCPGraph
{
  using QCPGraph::QCPGraph;
  using QCPGraph::getOptimizedScatterData;
};
}

static void testColorMap()
{
  QCPColorMapData map(3, 2, QCPRange(0, 10), QCPRange(0, 1));
  check(map.cell(2, 1) == 0 && map.dataBounds().lower == 0, "a new map: its cells 0");
  map.fill(2.5);
  check(map.cell(0, 0) == 2.5 && map.cell(2, 1) == 2.5, QString("fill(2.5) sets 2.5, not a repeated byte: %1").arg(map.cell(0, 0)));
  map.setSize(-1, 4);
  check(map.keySize() == 3 && map.valueSize() == 2 && map.cell(1, 1) == 2.5, "a negative size: the map stays as it was");
  map.setSize(100000, 100000);
  check(map.keySize() == 3 && map.valueSize() == 2, "more cells than an int counts: the map stays as it was");

  QCPColorMapData one(1, 4, QCPRange(2, 6), QCPRange(0, 3));
  double key = 0, value = 0;
  one.cellToCoord(0, 3, &key, &value);
  check(key == 4 && value == 3, QString("a single column lies in the middle of its range: %1 %2").arg(key).arg(value));
  QCPColorMapData copy(one);
  check(copy.keySize() == 1 && copy.valueSize() == 4 && copy.cell(0, 2) == 0, "a copy");
}

static void testTickers()
{
  TimeTicker time;
  DateTimeTicker date;
  check(time.getSubTickCount(3600) == 3 && date.getSubTickCount(86400 * 30.4375) == 3, "the hand chosen sub ticks stay");
  // qRound() of these is out of int's range (Qt 6.10 asserts on it)
  const int t = time.getSubTickCount(1e12), d = date.getSubTickCount(1e15);
  check(t >= 0 && d >= 0 && time.getSubTickCount(qInf()) >= 0, "a huge step: the default sub ticks");
}

static void testScatterSampling(QCustomPlot &plot)
{
  auto *graph = new Graph(plot.xAxis, plot.yAxis);
  graph->setLineStyle(QCPGraph::lsNone);
  graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));
  for (int i = 0; i < 1000; ++i)
    graph->addData(i, std::sin(i * 0.1));
  // zoomed in so far that the points' span is far more pixels than an int counts
  plot.xAxis->setRange(500, 500 + 1e-9);
  plot.yAxis->setRange(-1, 1);
  QVector<QCPGraphData> sampled, all;
  graph->setAdaptiveSampling(true);
  graph->getOptimizedScatterData(&sampled, graph->data()->constBegin(), graph->data()->constEnd());
  graph->setAdaptiveSampling(false);
  graph->getOptimizedScatterData(&all, graph->data()->constBegin(), graph->data()->constEnd());
  check(sampled.size() == all.size(), QString("a huge pixel span: no sampling (%1 points, %2 without)").arg(sampled.size()).arg(all.size()));
}
#endif

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
#ifndef QTDMM_NEOQCP
  QCustomPlot plot;
  plot.resize(600, 400);
  testColorMap();
  testTickers();
  testScatterSampling(plot);
#endif
  return failed ? 1 : 0;
}
