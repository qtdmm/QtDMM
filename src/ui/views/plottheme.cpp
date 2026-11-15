// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/plottheme.h"

#include "3rdparty/qcustomplot/qcustomplot.h"

void PlotTheme::applyTo(QCustomPlot *plot, const QList<QCPAxis *> &axes) const
{
  if (!plot)
    return;
  plot->setBackground(background.style() == Qt::NoBrush ? plot->palette().base() : background);
  plot->axisRect()->setBackground(Qt::NoBrush);
  const QList<QCPAxis *> targets = axes.isEmpty() ? QList<QCPAxis *>{ plot->xAxis, plot->yAxis } : axes;
  for (QCPAxis *axis : targets)
  {
    if (!axis)
      continue;
    axis->setBasePen(QPen(grid));
    axis->setTickPen(QPen(grid));
    axis->setSubTickPen(QPen(grid));
    axis->setTickLabelColor(labels);
    axis->setLabelColor(labels);
    axis->grid()->setPen(QPen(grid, 1, Qt::DotLine));
    axis->grid()->setSubGridPen(QPen(grid, 1, Qt::DotLine));
  }
}
