// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/xyplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"
#include "ui/views/plotaxis.h"

#include <QCoreApplication>
#include <QVBoxLayout>

#include <cmath>

XYPlot::XYPlot(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(220, 180);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  m_plot = new QCustomPlot(this);
  layout->addWidget(m_plot);
  m_plot->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_plot, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    emit customContextMenuRequested(mapFromGlobal(m_plot->mapToGlobal(pos)));
  });

  m_plot->plotLayout()->insertRow(0);
  m_title = new QCPTextElement(m_plot, QString(), font());
  m_plot->plotLayout()->addElement(0, 0, m_title);
  for (QCPAxis *axis : { m_plot->xAxis, m_plot->yAxis })
  {
    axis->setNumberFormat(QStringLiteral("gb"));
    axis->setNumberPrecision(4);
  }
  m_graph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
  m_graph->setLineStyle(QCPGraph::lsNone);
  m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));
  m_graph->setAdaptiveSampling(true);

  m_noDataText = new QCPItemText(m_plot);
  m_noDataText->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_noDataText->position->setCoords(0.5, 0.5);
  m_noDataText->setPositionAlignment(Qt::AlignCenter);
  m_noDataText->setText(QCoreApplication::translate("XYPlot", "No XY data yet"));

  m_holdText = new QCPItemText(m_plot);
  m_holdText->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_holdText->position->setCoords(0.98, 0.02);
  m_holdText->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
  m_holdText->setText(QCoreApplication::translate("XYPlot", "HOLD"));
  m_holdText->setVisible(false);
  refreshPlot();
}

void XYPlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels, const QColor &data)
{
  const QBrush bg = background.style() == Qt::NoBrush ? QBrush(palette().base()) : background;
  m_plot->setBackground(bg);
  m_plot->axisRect()->setBackground(Qt::NoBrush);
  for (QCPAxis *axis : { m_plot->xAxis, m_plot->yAxis })
  {
    axis->setBasePen(QPen(grid));
    axis->setTickPen(QPen(grid));
    axis->setSubTickPen(QPen(grid));
    axis->setTickLabelColor(labels);
    axis->setLabelColor(labels);
    axis->grid()->setPen(QPen(grid, 1, Qt::DotLine));
    axis->grid()->setSubGridPen(QPen(grid, 1, Qt::DotLine));
  }
  m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(data), QBrush(data), 5));
  m_title->setTextColor(labels);
  m_noDataText->setColor(labels);
  m_holdText->setColor(labels);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void XYPlot::setLegend(const QString &legend)
{
  m_title->setText(legend);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
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
  const QPointF point(x, y);
  if (m_points.size() < kMaxPoints)
    m_points.append(point);
  else
  {
    m_points[m_pointHead] = point;
    m_pointHead = (m_pointHead + 1) % kMaxPoints;
  }
  if (!m_hold)
    refreshPlot();
}

void XYPlot::clear()
{
  m_points.clear();
  m_shown.clear();
  m_pointHead = 0;
  refreshPlot();
}

void XYPlot::setHold(bool hold)
{
  if (m_hold == hold)
    return;
  m_hold = hold;
  if (hold)
    m_shown = m_points;
  else
    m_shown.clear();
  refreshPlot();
}

void XYPlot::refreshPlot()
{
  const QVector<QPointF> &shown = m_hold ? m_shown : m_points;
  m_noDataText->setVisible(shown.isEmpty());
  m_holdText->setVisible(m_hold);
  if (shown.isEmpty())
  {
    m_graph->data()->clear();
    const PlotAxis::Range xAxis = PlotAxis::range(0, 0, m_xUnit);
    const PlotAxis::Range yAxis = PlotAxis::range(0, 0, m_yUnit);
    m_plot->xAxis->setRange(xAxis.minimum / xAxis.factor, xAxis.maximum / xAxis.factor);
    m_plot->yAxis->setRange(yAxis.minimum / yAxis.factor, yAxis.maximum / yAxis.factor);
    m_plot->xAxis->setLabel(xAxis.title);
    m_plot->yAxis->setLabel(yAxis.title);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    return;
  }

  double xmin = shown.first().x(), xmax = xmin;
  double ymin = shown.first().y(), ymax = ymin;
  for (const QPointF &point : std::as_const(shown))
  {
    xmin = qMin(xmin, point.x());
    xmax = qMax(xmax, point.x());
    ymin = qMin(ymin, point.y());
    ymax = qMax(ymax, point.y());
  }
  const PlotAxis::Range xAxis = PlotAxis::range(xmin, xmax, m_xUnit);
  const PlotAxis::Range yAxis = PlotAxis::range(ymin, ymax, m_yUnit);
  m_plot->xAxis->setRange(xAxis.minimum / xAxis.factor, xAxis.maximum / xAxis.factor);
  m_plot->yAxis->setRange(yAxis.minimum / yAxis.factor, yAxis.maximum / yAxis.factor);
  m_plot->xAxis->setLabel(xAxis.title);
  m_plot->yAxis->setLabel(yAxis.title);

  QVector<double> xValues;
  QVector<double> yValues;
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
