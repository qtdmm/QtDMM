// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
// The histogram on QCustomPlot: readings counted, a reading without a number
// keeps them (it is no change of unit), another unit starts afresh, and the
// crosshair gives the height of the bar under the mouse.
#include "ui/views/histogramplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"
#include "core/reading.h"
#include "testutil.h"

#include <QApplication>
#include <QMouseEvent>

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  HistogramPlot histogram;
  histogram.resize(600, 400);
  histogram.show();
  auto *plot = histogram.findChild<QCustomPlot *>();
  auto *bars = plot ? qobject_cast<QCPBars *>(plot->plottable(0)) : nullptr;
  check(plot && bars, "bars in the plot");
  if (!plot || !bars)
    return 1;
  auto counted = [bars]
  {
    double n = 0;
    for (const QCPBarsData &bar : *bars->data())
      n += bar.value;
    return int(n);
  };
  auto reading = [](double v, const QString &unit)
  {
    Reading r;
    r.value = v;
    r.baseUnit = unit;
    return r;
  };
  for (int i = 0; i < 100; ++i)
    histogram.addReading(reading(i % 10 == 0 ? 5.0 : 1.0 + i % 3, QStringLiteral("V")));
  check(waitFor([&] { return counted() == 100; }, 1000), QString("100 readings counted: %1").arg(counted()));

  // no number and no unit (a function the decoder does not know): kept (R19-02)
  Reading none = reading(qQNaN(), QString());
  none.quality = Quality::NoValue;
  histogram.addReading(none);
  waitFor([] { return false; }, 100);
  check(counted() == 100, QString("a reading without a number keeps them: %1").arg(counted()));
  // another unit: afresh
  histogram.addReading(reading(1.0, QStringLiteral("A")));
  check(waitFor([&] { return counted() == 1; }, 1000), QString("another unit starts afresh: %1").arg(counted()));

  // the crosshair: the bar's height, wherever the mouse is on it (R19-09)
  histogram.clear();
  for (int i = 0; i < 20; ++i)
    histogram.addReading(reading(2.0, QStringLiteral("V")));
  for (int i = 0; i < 4; ++i)
    histogram.addReading(reading(8.0, QStringLiteral("V")));
  check(waitFor([&] { return counted() == 24; }, 1000), "24 readings");
  QCPItemText *cross = nullptr;
  for (int i = 0; i < plot->itemCount(); ++i)
    if (auto *text = qobject_cast<QCPItemText *>(plot->item(i)); text && text->layer()->name() == "crosshair")
      cross = text;
  double tallest = 0, key = 0;
  for (const QCPBarsData &bar : *bars->data())
    if (bar.value > tallest)
    {
      tallest = bar.value;
      key = bar.key;
    }
  const QPoint half(int(plot->xAxis->coordToPixel(key)), int(plot->yAxis->coordToPixel(tallest / 2)));
  QMouseEvent move(QEvent::MouseMove, half, plot->mapToGlobal(half), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(plot, &move);
  const QString text = cross ? cross->text() : QString();
  check(cross && text.contains("20 readings"), "half way up a bar of 20: 20 readings, got " + text);

  // the System design leaves the colours to the platform (issue #603, Windows
  // dark mode): bars and grid from the palette, not black on dark grey
  QPalette dark = histogram.palette();
  dark.setColor(QPalette::Base, QColor(32, 32, 32));
  dark.setColor(QPalette::Text, QColor(230, 230, 230));
  dark.setColor(QPalette::Mid, QColor(90, 90, 90));
  dark.setColor(QPalette::Highlight, QColor(0, 120, 212));
  histogram.setPalette(dark);
  histogram.setThemeColors(Qt::NoBrush, QColor(), QColor(), QColor());
  check(bars->brush().color() == QColor(0, 120, 212), "System design: the bars in the highlight colour");
  check(plot->xAxis->grid()->pen().color() == QColor(90, 90, 90), "System design: the grid in the palette's mid");
  check(plot->xAxis->tickLabelColor() == QColor(230, 230, 230), "System design: the labels in the palette's text");
  if (qEnvironmentVariableIsSet("QTDMM_SHOT"))
  {
    plot->replot();
    histogram.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/histogram_system_dark.png");
  }

  if (failed == 0)
    qInfo() << "All histogram tests passed.";
  return failed == 0 ? 0 : 1;
}
