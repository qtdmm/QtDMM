// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
// The heatmap: cells averaged, the colour scheme's no-data colour, hold,
// the colour scale beside the plot, clear.
#include "ui/views/heatmapplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"

#include <QApplication>

#include <cmath>

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  HeatmapPlot heatmap;
  heatmap.setCellCount(2, 2);
  heatmap.setColorScheme(ColorMap::Ironbow);
  heatmap.addPoint(0.1, 0.1, 1.0);
  heatmap.addPoint(0.2, 0.2, 3.0);
  heatmap.addPoint(0.9, 0.9, 2.0);

  auto *plot = heatmap.findChild<QCustomPlot *>();
  auto *map = plot ? qobject_cast<QCPColorMap *>(plot->plottable(0)) : nullptr;
  if (!plot || !map)
    return 1;
  if (!qFuzzyCompare(map->data()->cell(0, 0), 2.0))
    return 2;
  if (!qFuzzyCompare(map->data()->cell(1, 1), 2.0))
    return 3;
  if (map->gradient().nanColor() != ColorMap::noDataColor(ColorMap::Ironbow))
    return 4;

  heatmap.setHold(true);
  heatmap.addPoint(0.9, 0.1, 9.0);
  if (!std::isnan(map->data()->cell(1, 0)) || !heatmap.hold())
    return 5;

  heatmap.resize(800, 500);
  heatmap.show();
  app.processEvents();
  plot->replot();
  const QRect plotArea = plot->axisRect()->rect();
  const QRect colorScaleArea = map->colorScale()->axis()->axisRect()->rect();
  if (qAbs(plotArea.top() - colorScaleArea.top()) > 1 || qAbs(plotArea.bottom() - colorScaleArea.bottom()) > 1)
  {
    qWarning() << "heatmap/color-scale rects:" << plotArea << colorScaleArea;
    return 7;
  }
  if (!qFuzzyCompare(map->data()->cell(0, 0), 2.0))
    return 8;

  heatmap.setHold(false);
  if (!qFuzzyCompare(map->data()->cell(1, 0), 9.0))
    return 9;
  heatmap.clear();
  if (!std::isnan(map->data()->cell(0, 0)))
    return 10;
  return 0;
}
