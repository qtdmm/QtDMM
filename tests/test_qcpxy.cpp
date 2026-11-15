// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
// The XY diagram on QCustomPlot: points in the axes' prefix, hold, clear,
// the newest kMaxPoints, the title, and the value crosshair under the mouse.
#include "ui/views/xyplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"
#include "testutil.h"

#include <QApplication>
#include <QMouseEvent>

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  XYPlot xy;
  xy.resize(600, 400);
  xy.show();
  xy.setAxisUnits(QStringLiteral("V"), QStringLiteral("A"));
  xy.setTitle(QStringLiteral("X: voltage     Y: current"));
  xy.addPoint(0.001, 0.002);
  xy.addPoint(0.002, 0.004);
  xy.addPoint(0.004, 0.008);
  xy.addPoint(qQNaN(), 1.0);

  auto *plot = xy.findChild<QCustomPlot *>();
  check(plot && plot->graphCount() == 1, "one graph in the plot");
  if (!plot || plot->graphCount() != 1)
    return 1;
  QCPGraph *graph = plot->graph(0);
  // drawn at the plots' pace, not at once
  const bool drawn = waitFor([&] { return graph->data()->size() == 3; }, 1000);
  check(drawn, QString("three points drawn, NaN left out: %1").arg(graph->data()->size()));
  check(graph->lineStyle() == QCPGraph::lsNone, "points, no line");
  check(qFuzzyCompare(graph->data()->at(0)->key, 1.0) && qFuzzyCompare(graph->data()->at(2)->key, 4.0),
        "the keys in mV");
  check(plot->xAxis->label() == "[mV]" && plot->yAxis->label() == "[mA]",
        "axes in mV and mA: " + plot->xAxis->label() + " " + plot->yAxis->label());
  check(xy.title() == "X: voltage     Y: current", "the title");

  // the crosshair: both values with their units, gone when the mouse leaves
  app.processEvents();
  plot->replot();
  QCPItemText *cross = nullptr;
  for (int i = 0; i < plot->itemCount(); ++i)
    if (auto *text = qobject_cast<QCPItemText *>(plot->item(i)); text && text->layer()->name() == "crosshair")
      cross = text;
  const QPoint middle = plot->axisRect()->rect().center();
  QMouseEvent move(QEvent::MouseMove, middle, plot->mapToGlobal(middle), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(plot, &move);
  const QString text = cross ? cross->text() : QString();
  check(cross && cross->visible() && text.contains(" mV") && text.contains(" mA"), "crosshair under the mouse: " + text);
  QEvent leave(QEvent::Leave);
  QCoreApplication::sendEvent(plot, &leave);
  check(cross && !cross->visible(), "crosshair gone when the mouse leaves");

  xy.setHold(true);
  xy.addPoint(0.003, 0.006);
  waitFor([] { return false; }, 100);
  check(xy.hold() && graph->data()->size() == 3, "hold: the picture stays");
  xy.setHold(false);
  check(graph->data()->size() == 4, "released: the point held back shows");
  xy.clear();
  check(graph->data()->size() == 0, "clear");

  for (int i = 0; i < 2005; ++i)
    xy.addPoint(i, i * 2.0);
  const bool newest = waitFor([&] { return graph->data()->size() == 2000; }, 1000);
  check(newest, QString("the newest 2000 points: %1").arg(graph->data()->size()));

  if (failed == 0)
    qInfo() << "All XY plot tests passed.";
  return failed == 0 ? 0 : 1;
}
