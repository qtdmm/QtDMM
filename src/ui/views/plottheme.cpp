// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/plottheme.h"

#include "ui/views/qcp.h"

PlotTheme PlotTheme::resolved(const QPalette &pal) const
{
  PlotTheme t = *this;
  if (t.background.style() == Qt::NoBrush)
    t.background = pal.base();
  if (!t.grid.isValid())
    t.grid = pal.color(QPalette::Mid);
  if (!t.labels.isValid())
    t.labels = pal.color(QPalette::Text);
  if (!t.data.isValid())
    t.data = pal.color(QPalette::Highlight);
  return t;
}

void PlotTheme::applyTo(QCustomPlot *plot, const QList<QCPAxis *> &axes) const
{
  if (!plot)
    return;
  const PlotTheme t = resolved(plot->palette());
  const QColor &grid = t.grid;
  const QColor &labels = t.labels;
  plot->setBackground(t.background);
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
    // QCP's own is a light solid line: a white stroke at 0 on a dark design
    axis->grid()->setZeroLinePen(QPen(grid, 1, Qt::DotLine));
  }
}
