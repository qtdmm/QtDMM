// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/xyplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"

#include <QApplication>

#include <cmath>

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  XYPlot xy;
  xy.setAxisUnits(QStringLiteral("V"), QStringLiteral("A"));
  xy.setLegend(QStringLiteral("X: voltage     Y: current"));
  xy.addPoint(0.001, 0.002);
  xy.addPoint(0.002, 0.004);
  xy.addPoint(0.004, 0.008);
  xy.addPoint(qQNaN(), 1.0);

  auto *plot = xy.findChild<QCustomPlot *>();
  if (!plot || plot->graphCount() != 1)
    return 1;
  auto *graph = plot->graph(0);
  if (graph->data()->size() != 3 || graph->lineStyle() != QCPGraph::lsNone)
    return 2;
  if (!qFuzzyCompare(graph->data()->at(0)->key, 1.0) || !qFuzzyCompare(graph->data()->at(2)->key, 4.0))
    return 3;
  if (plot->xAxis->label() != QStringLiteral("[mV]") || plot->yAxis->label() != QStringLiteral("[mA]"))
    return 4;

  xy.setHold(true);
  xy.addPoint(0.003, 0.006);
  if (!xy.hold() || graph->data()->size() != 3)
    return 5;
  xy.setHold(false);
  if (graph->data()->size() != 4)
    return 6;
  xy.clear();
  if (graph->data()->size() != 0)
    return 7;

  for (int i = 0; i < 2005; ++i)
    xy.addPoint(i, i * 2.0);
  if (graph->data()->size() != 2000)
    return 8;
  return 0;
}
