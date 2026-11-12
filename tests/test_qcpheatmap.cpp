// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/heatmapplothost.h"
#include "ui/views/qcpheatmapplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"

#include <QApplication>
#include <QStackedWidget>

#include <cmath>

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  HeatmapPlotHost host;
  host.setCellCount(2, 2);
  host.setColorScheme(ColorMap::Ironbow);
  host.addPoint(0.1, 0.1, 1.0);
  host.addPoint(0.2, 0.2, 3.0);
  host.addPoint(0.9, 0.9, 2.0);

  auto *qcp = host.findChild<QCPHeatmapPlot *>();
  auto *plot = qcp ? qcp->findChild<QCustomPlot *>() : nullptr;
  auto *map = plot ? qobject_cast<QCPColorMap *>(plot->plottable(0)) : nullptr;
  if (!qcp || !plot || !map)
    return 1;
  if (!qFuzzyCompare(map->data()->cell(0, 0), 2.0))
    return 2;
  if (!qFuzzyCompare(map->data()->cell(1, 1), 2.0))
    return 3;
  if (map->gradient().nanColor() != ColorMap::noDataColor(ColorMap::Ironbow))
    return 4;

  host.setHold(true);
  host.addPoint(0.9, 0.1, 9.0);
  if (!std::isnan(map->data()->cell(1, 0)))
    return 5;

  host.setBackend(HeatmapPlotHost::QCustomPlot);
  auto *stack = host.findChild<QStackedWidget *>();
  if (!stack || stack->currentWidget() != qcp || !host.hold())
    return 6;
  host.resize(800, 500);
  host.show();
  app.processEvents();
  plot->replot();
  const QRect plotArea = plot->axisRect()->rect();
  const QRect colorScaleArea = map->colorScale()->axis()->axisRect()->rect();
  if (qAbs(plotArea.top() - colorScaleArea.top()) > 1 || qAbs(plotArea.bottom() - colorScaleArea.bottom()) > 1)
  {
    qWarning() << "heatmap/color-scale rects:" << plotArea << colorScaleArea;
    return 7;
  }
  host.setBackend(HeatmapPlotHost::Legacy);
  host.setBackend(HeatmapPlotHost::QCustomPlot);
  if (!qFuzzyCompare(map->data()->cell(0, 0), 2.0))
    return 8;

  host.setHold(false);
  if (!qFuzzyCompare(map->data()->cell(1, 0), 9.0))
    return 9;
  host.clear();
  if (!std::isnan(map->data()->cell(0, 0)))
    return 10;
  return 0;
}
