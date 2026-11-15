// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/heatmapplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"

#include <cmath>

HeatmapPlot::HeatmapPlot(QWidget *parent) : QcpGraphCommon(parent, tr("No heatmap data yet"))
{
  setMinimumSize(260, 180);   // room for the colour scale
  m_plot->plotLayout()->insertColumn(1);
  m_map = new QCPColorMap(m_plot->xAxis, m_plot->yAxis);
  m_map->setInterpolate(false);
  m_map->setTightBoundary(false);

  m_scale = new QCPColorScale(m_plot);
  m_scale->setMinimumMargins(QMargins(0, 0, 0, 0));
  m_scale->setType(QCPAxis::atRight);
  m_plot->plotLayout()->addElement(1, 1, m_scale);
  m_map->setColorScale(m_scale);
  // the scale as high as the plot
  auto *margins = new QCPMarginGroup(m_plot);
  m_plot->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, margins);
  m_scale->axis()->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, margins);

  applyGradient();
  refreshPlot();
}

void HeatmapPlot::setTheme(const PlotTheme &theme)
{
  QcpGraphCommon::setTheme(theme);
  theme.applyTo(m_plot, { m_scale->axis() });
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void HeatmapPlot::setAxisUnits(const QString &x, const QString &y, const QString &z)
{
  m_xUnit = x;
  m_yUnit = y;
  m_zUnit = z;
  refreshPlot();
}

void HeatmapPlot::setColorScheme(ColorMap::Scheme scheme)
{
  if (m_scheme == scheme)
    return;
  m_scheme = scheme;
  applyGradient();
  refreshPlot();
}

void HeatmapPlot::setCellCount(int nx, int ny)
{
  m_nx = qBound(2, nx, kMaxGridDimension);
  m_ny = qBound(2, ny, kMaxGridDimension);
  refreshPlot();
}

void HeatmapPlot::addPoint(double x, double y, double z)
{
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    return;
  m_samples.append({ x, y, z });
  scheduleRefresh();
}

void HeatmapPlot::holdChanged(bool hold)
{
  m_held = hold ? m_samples.values() : QVector<Sample>();
}

void HeatmapPlot::clearSamples()
{
  m_samples.clear();
  m_held.clear();
}

void HeatmapPlot::applyGradient()
{
  QCPColorGradient gradient;
  if (m_scheme == ColorMap::Ironbow)
  {
    // the thermal camera's steps; spread evenly they would lose the purple
    gradient.setColorStopAt(0.00, QColor(0, 0, 0));
    gradient.setColorStopAt(0.17, QColor(32, 0, 120));
    gradient.setColorStopAt(0.38, QColor(140, 20, 140));
    gradient.setColorStopAt(0.55, QColor(210, 50, 60));
    gradient.setColorStopAt(0.72, QColor(250, 130, 10));
    gradient.setColorStopAt(0.88, QColor(255, 220, 60));
    gradient.setColorStopAt(1.00, QColor(255, 255, 230));
  }
  else
    for (int i = 0; i <= 16; ++i)
      gradient.setColorStopAt(i / 16.0, ColorMap::color(m_scheme, i / 16.0));
  gradient.setLevelCount(256);
  gradient.setNanHandling(QCPColorGradient::nhNanColor);
  gradient.setNanColor(ColorMap::noDataColor(m_scheme));
  m_map->setGradient(gradient);
  m_scale->setGradient(gradient);
}

void HeatmapPlot::refreshPlot()
{
  const QVector<Sample> shown = hold() ? m_held : m_samples.values();
  setNoDataVisible(shown.isEmpty());
  double xmin = 0, xmax = 0, ymin = 0, ymax = 0, zmin = 0, zmax = 0;
  if (!shown.isEmpty())
  {
    xmin = xmax = shown.first().x;
    ymin = ymax = shown.first().y;
    zmin = zmax = shown.first().z;
  }
  for (const Sample &sample : shown)
  {
    xmin = qMin(xmin, sample.x);
    xmax = qMax(xmax, sample.x);
    ymin = qMin(ymin, sample.y);
    ymax = qMax(ymax, sample.y);
    zmin = qMin(zmin, sample.z);
    zmax = qMax(zmax, sample.z);
  }
  const PlotAxis::Range xAxis = PlotAxis::range(xmin, xmax, m_xUnit);
  const PlotAxis::Range yAxis = PlotAxis::range(ymin, ymax, m_yUnit);
  const PlotAxis::Range zAxis = PlotAxis::range(zmin, zmax, m_zUnit);
  setAxisRange(m_plot->xAxis, xAxis);
  setAxisRange(m_plot->yAxis, yAxis);
  setAxisRange(m_scale->axis(), zAxis);
  m_scale->setLabel(zAxis.title);
  // the cells fill the axes: QCP's range is from the centre of the first
  // cell to that of the last
  auto centres = [](const QCPRange &r, int n)
  {
    const double half = r.size() / n / 2;
    return QCPRange(r.lower + half, r.upper - half);
  };
  m_map->data()->setSize(m_nx, m_ny);
  m_map->data()->setRange(centres(m_plot->xAxis->range(), m_nx), centres(m_plot->yAxis->range(), m_ny));
  m_map->setDataRange(m_scale->axis()->range());

  // each cell the mean of its samples, NaN (the no-data colour) without any
  const int cells = m_nx * m_ny;
  QVector<double> sums(cells, 0.0);
  QVector<int> counts(cells, 0);
  for (const Sample &sample : shown)
  {
    // the cell it is drawn in and the crosshair reads
    int ix = 0, iy = 0;
    m_map->data()->coordToCell(sample.x / xAxis.factor, sample.y / yAxis.factor, &ix, &iy);   // the axes' prefix
    ix = qBound(0, ix, m_nx - 1);
    iy = qBound(0, iy, m_ny - 1);
    sums[iy * m_nx + ix] += sample.z / zAxis.factor;
    ++counts[iy * m_nx + ix];
  }
  for (int y = 0; y < m_ny; ++y)
    for (int x = 0; x < m_nx; ++x)
    {
      const int i = y * m_nx + x;
      m_map->data()->setCell(x, y, counts[i] ? sums[i] / counts[i] : qQNaN());
    }
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

// X and Y, and the colour's value of the cell under the mouse
QString HeatmapPlot::crosshairText(double x, double y) const
{
  int cx = 0, cy = 0;
  m_map->data()->coordToCell(x, y, &cx, &cy);   // the outer half of an edge cell: one past it
  const double z = m_map->data()->cell(qBound(0, cx, m_nx - 1), qBound(0, cy, m_ny - 1));
  const QString text = QcpGraphCommon::crosshairText(x, y);
  return std::isfinite(z) ? text + QStringLiteral("   ") + axisValueText(m_scale->axis(), z) : text;
}
