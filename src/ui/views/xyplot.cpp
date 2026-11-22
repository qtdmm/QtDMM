// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/xyplot.h"

#include "ui/views/qcp.h"

#include <cmath>

XYPlot::XYPlot(QWidget *parent) : QcpGraphCommon(parent, tr("No XY data yet"))
{
  for (QCPAxis *axis : { m_plot->xAxis, m_plot->yAxis })
  {
    axis->setNumberFormat(QStringLiteral("gb"));
    axis->setNumberPrecision(4);
  }
  m_graph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
  m_graph->setLineStyle(QCPGraph::lsNone);
  m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));
  m_graph->setAdaptiveSampling(true);
  refreshPlot();
}

void XYPlot::setTheme(const PlotTheme &theme)
{
  const QColor data = theme.resolved(palette()).data;
  m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(data), QBrush(data), 5));
  QcpGraphCommon::setTheme(theme);
}

void XYPlot::setAxisUnits(const QString &x, const QString &y)
{
  m_xUnit = x;
  m_yUnit = y;
  refreshPlot();
}

void XYPlot::addPoint(double x, double y)
{
  if (!std::isfinite(x) || !std::isfinite(y))
    return;
  m_points.append(QPointF(x, y));
  scheduleRefresh();
}

void XYPlot::holdChanged(bool hold)
{
  m_held = hold ? m_points.values() : QVector<QPointF>();
}

void XYPlot::clearSamples()
{
  m_points.clear();
  m_held.clear();
}

void XYPlot::refreshPlot()
{
  const QVector<QPointF> shown = hold() ? m_held : m_points.values();
  setNoDataVisible(shown.isEmpty());
  double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
  if (!shown.isEmpty())
  {
    xmin = xmax = shown.first().x();
    ymin = ymax = shown.first().y();
  }
  for (const QPointF &point : shown)
  {
    xmin = qMin(xmin, point.x());
    xmax = qMax(xmax, point.x());
    ymin = qMin(ymin, point.y());
    ymax = qMax(ymax, point.y());
  }
  const PlotAxis::Range xAxis = PlotAxis::range(xmin, xmax, m_xUnit);
  const PlotAxis::Range yAxis = PlotAxis::range(ymin, ymax, m_yUnit);
  setAxisRange(m_plot->xAxis, xAxis);
  setAxisRange(m_plot->yAxis, yAxis);

  QVector<double> xValues, yValues;
  xValues.reserve(shown.size());
  yValues.reserve(shown.size());
  for (const QPointF &point : shown)
  {
    xValues.append(point.x() / xAxis.factor);
    yValues.append(point.y() / yAxis.factor);
  }
  m_graph->setData(xValues, yValues);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}
