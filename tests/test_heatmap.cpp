// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
// The heatmap on QCustomPlot: cells averaged, the colour scheme's no-data
// colour, hold, the colour scale beside the plot, the cell's value in the
// crosshair, clear.
#include "ui/views/heatmapplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"
#include "testutil.h"

#include <QApplication>
#include <QMouseEvent>

#include <cmath>

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  HeatmapPlot heatmap;
  heatmap.resize(800, 500);
  heatmap.show();
  heatmap.setCellCount(2, 2);
  heatmap.setColorScheme(ColorMap::Ironbow);
  heatmap.setAxisUnits(QStringLiteral("V"), QStringLiteral("A"), QStringLiteral("°C"));
  heatmap.addPoint(0.1, 0.1, 1.0);
  heatmap.addPoint(0.2, 0.2, 3.0);
  heatmap.addPoint(0.9, 0.9, 2.0);

  auto *plot = heatmap.findChild<QCustomPlot *>();
  auto *map = plot ? qobject_cast<QCPColorMap *>(plot->plottable(0)) : nullptr;
  check(plot && map, "a colour map in the plot");
  if (!plot || !map)
    return 1;
  const bool drawn = waitFor([&] { return qFuzzyCompare(map->data()->cell(0, 0), 2.0); }, 1000);
  check(drawn, "cell (0,0): the mean of 1 and 3");
  check(qFuzzyCompare(map->data()->cell(1, 1), 2.0), "cell (1,1): 2");
  check(map->gradient().nanColor() == ColorMap::noDataColor(ColorMap::Ironbow), "empty cells in the no-data colour");

  heatmap.setHold(true);
  heatmap.addPoint(0.9, 0.1, 9.0);
  waitFor([] { return false; }, 100);
  check(std::isnan(map->data()->cell(1, 0)) && heatmap.hold(), "hold: the new sample waits");

  app.processEvents();
  plot->replot();
  const QRect plotArea = plot->axisRect()->rect();
  const QRect scaleArea = map->colorScale()->axis()->axisRect()->rect();
  check(qAbs(plotArea.top() - scaleArea.top()) <= 1 && qAbs(plotArea.bottom() - scaleArea.bottom()) <= 1,
        "the colour scale as high as the plot");

  // the crosshair: X, Y and the value of the cell under the mouse
  QCPItemText *cross = nullptr;
  for (int i = 0; i < plot->itemCount(); ++i)
    if (auto *text = qobject_cast<QCPItemText *>(plot->item(i)); text && text->layer()->name() == "crosshair")
      cross = text;
  const QPoint corner(plot->xAxis->coordToPixel(plot->xAxis->range().lower) + 5,
                      plot->yAxis->coordToPixel(plot->yAxis->range().lower) - 5);
  QMouseEvent move(QEvent::MouseMove, corner, plot->mapToGlobal(corner), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(plot, &move);
  const QString text = cross ? cross->text() : QString();
  check(cross && cross->visible() && text.contains("°C"), "crosshair with the cell's value: " + text);

  heatmap.setHold(false);
  check(qFuzzyCompare(map->data()->cell(1, 0), 9.0), "released: the sample shows");
  heatmap.clear();
  check(std::isnan(map->data()->cell(0, 0)), "clear");

  if (failed == 0)
    qInfo() << "All heatmap tests passed.";
  return failed == 0 ? 0 : 1;
}
