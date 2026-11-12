// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/heatmapplot.h"

#include "ui/views/plotaxis.h"

#include <QPainter>

#include <cmath>

namespace
{
constexpr int kMaxSamples = 5000;
constexpr int kMaxGridDimension = 256;
}

HeatmapPlot::HeatmapPlot(QWidget *parent) : HeatmapPlotBackend(parent)
{
  setMinimumSize(260, 180);
}

void HeatmapPlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels)
{
  m_background = background;
  m_grid = grid;
  m_labels = labels;
  update();
}

void HeatmapPlot::setAxisUnits(const QString &x, const QString &y, const QString &z)
{
  m_xUnit = x;
  m_yUnit = y;
  m_zUnit = z;
  update();
}

void HeatmapPlot::setLegend(const QString &legend)
{
  m_legend = legend;
  update();
}

void HeatmapPlot::setCellCount(int nx, int ny)
{
  m_nx = qBound(2, nx, kMaxGridDimension);
  m_ny = qBound(2, ny, kMaxGridDimension);
  update();
}

void HeatmapPlot::addPoint(double x, double y, double z)
{
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    return;
  // Binning is order-independent, so overwrite the oldest slot instead of shifting all samples.
  if (m_samples.size() < kMaxSamples)
    m_samples.append({x, y, z});
  else
  {
    m_samples[m_sampleHead] = {x, y, z};
    m_sampleHead = (m_sampleHead + 1) % kMaxSamples;
  }
  update();
}

void HeatmapPlot::clear()
{
  m_samples.clear();
  m_sampleHead = 0;
  m_shown.clear();
  update();
}

void HeatmapPlot::setHold(bool hold)
{
  if (m_hold == hold)
    return;
  m_hold = hold;
  if (m_hold)
    m_shown = m_samples;
  else
    m_shown.clear();
  update();
}

void HeatmapPlot::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.fillRect(rect(), m_background.style() == Qt::NoBrush ? QBrush(palette().base()) : m_background);

  const QColor grid = m_grid.isValid() ? m_grid : palette().mid().color();
  const QColor labels = m_labels.isValid() ? m_labels : palette().text().color();
  const QFontMetrics fm(font());
  const QVector<Sample> &shown = m_hold ? m_shown : m_samples;
  PlotAxis::Range xAxis = PlotAxis::range(0, 0, m_xUnit);
  PlotAxis::Range yAxis = PlotAxis::range(0, 0, m_yUnit);
  PlotAxis::Range zAxis = PlotAxis::range(0, 0, m_zUnit);
  QString xMinimum, xMaximum, yMinimum, yMaximum, zMinimum, zMaximum;
  int yLabelWidth = 0;
  int zLabelWidth = 0;
  int xLabelWidth = 60;
  if (!shown.isEmpty())
  {
    double xmin = shown.first().x, xmax = xmin;
    double ymin = shown.first().y, ymax = ymin;
    double zmin = shown.first().z, zmax = zmin;
    for (const Sample &sample : std::as_const(shown))
    {
      xmin = qMin(xmin, sample.x);
      xmax = qMax(xmax, sample.x);
      ymin = qMin(ymin, sample.y);
      ymax = qMax(ymax, sample.y);
      zmin = qMin(zmin, sample.z);
      zmax = qMax(zmax, sample.z);
    }
    xAxis = PlotAxis::range(xmin, xmax, m_xUnit);
    yAxis = PlotAxis::range(ymin, ymax, m_yUnit);
    zAxis = PlotAxis::range(zmin, zmax, m_zUnit);
    xMinimum = QString::number(xAxis.minimum / xAxis.factor, 'g', 4);
    xMaximum = QString::number(xAxis.maximum / xAxis.factor, 'g', 4);
    yMinimum = QString::number(yAxis.minimum / yAxis.factor, 'g', 4);
    yMaximum = QString::number(yAxis.maximum / yAxis.factor, 'g', 4);
    zMinimum = QString::number(zAxis.minimum / zAxis.factor, 'g', 4);
    zMaximum = QString::number(zAxis.maximum / zAxis.factor, 'g', 4);
    yLabelWidth = qMax(fm.horizontalAdvance(yMinimum), fm.horizontalAdvance(yMaximum));
    zLabelWidth = qMax(fm.horizontalAdvance(zMinimum), fm.horizontalAdvance(zMaximum));
    xLabelWidth = qMax(fm.horizontalAdvance(xMinimum), fm.horizontalAdvance(xMaximum));
  }

  const qreal leftMargin = qMax(48, yLabelWidth + 20);
  const qreal rightMargin = qMax(92, 40 + zLabelWidth);
  const QRectF area = QRectF(rect()).adjusted(leftMargin, 48, -rightMargin, -42);
  const QRectF bar(area.right() + 16, area.top(), 14, area.height());
  const QColor noData = ColorMap::noDataColor(m_scheme);
  p.fillRect(area, noData);

  p.setPen(labels);
  const QString noDataText = tr("No data");
  const int noDataKeyWidth = 10 + 4 + fm.horizontalAdvance(noDataText);
  const int legendWidth = qMax(0, width() - 20 - noDataKeyWidth);
  p.drawText(QRectF(6, 2, legendWidth, fm.height()), Qt::AlignCenter,
             fm.elidedText(m_legend, Qt::ElideRight, legendWidth));
  const QRectF noDataSwatch(width() - 6 - noDataKeyWidth, 3 + (fm.height() - 10) / 2, 10, 10);
  p.fillRect(noDataSwatch, noData);
  p.setPen(grid);
  p.setBrush(Qt::NoBrush);
  p.drawRect(noDataSwatch);
  p.setPen(labels);
  p.drawText(QRectF(noDataSwatch.right() + 4, 2, fm.horizontalAdvance(noDataText), fm.height()),
             Qt::AlignLeft | Qt::AlignVCenter, noDataText);

  auto drawAxisUnits = [&](const QString &x, const QString &y)
  {
    p.setPen(labels);
    p.drawText(QRectF(area.left(), area.bottom() + 8, area.width(), 24), Qt::AlignCenter, x);
    p.save();
    p.translate(12, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height() / 2, -12, area.height(), 24), Qt::AlignCenter, y);
    p.restore();
  };
  auto drawHold = [&]
  {
    if (m_hold)
    {
      p.setPen(labels);
      p.drawText(QRectF(area.right() - 58, area.top(), 54, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                 tr("HOLD"));
    }
  };

  if (shown.isEmpty())
  {
    drawAxisUnits(xAxis.title, yAxis.title);
    p.setPen(labels);
    p.drawText(QRectF(bar.left() - 20, bar.bottom() + 6, 90, 16), Qt::AlignLeft, zAxis.title);
    p.drawText(area, Qt::AlignCenter, tr("No heatmap data yet"));
    p.setPen(grid);
    p.drawRect(area);
    drawHold();
    return;
  }

  drawAxisUnits(xAxis.title, yAxis.title);
  QVector<Cell> cells(m_nx * m_ny);
  for (const Sample &sample : std::as_const(shown))
  {
    const int ix = qBound(0, int((sample.x - xAxis.minimum) / (xAxis.maximum - xAxis.minimum) * m_nx), m_nx - 1);
    const int iy = qBound(0, int((sample.y - yAxis.minimum) / (yAxis.maximum - yAxis.minimum) * m_ny), m_ny - 1);
    Cell &cell = cells[iy * m_nx + ix];
    cell.sum += sample.z;
    ++cell.n;
  }

  const qreal cellWidth = area.width() / m_nx;
  const qreal cellHeight = area.height() / m_ny;
  p.setPen(Qt::NoPen);
  for (int iy = 0; iy < m_ny; ++iy)
  {
    for (int ix = 0; ix < m_nx; ++ix)
    {
      const Cell &cell = cells[iy * m_nx + ix];
      if (cell.n == 0)
        continue;                                   // empty cells stay background
      const double t = (cell.sum / cell.n - zAxis.minimum) / (zAxis.maximum - zAxis.minimum);
      p.setBrush(ColorMap::color(m_scheme, t));
      // y grows upwards; +1 px overlap avoids hairline gaps
      p.drawRect(QRectF(area.left() + ix * cellWidth, area.bottom() - (iy + 1) * cellHeight,
                        cellWidth + 1, cellHeight + 1));
    }
  }

  p.setBrush(Qt::NoBrush);
  p.setPen(grid);
  p.drawRect(area);

  const int colorBarHeight = qMax(1, qRound(bar.height()));
  if (m_colorBar.size() != QSize(1, colorBarHeight) || m_colorBarScheme != m_scheme)
  {
    m_colorBar = QImage(1, colorBarHeight, QImage::Format_RGB32);
    for (int i = 0; i < colorBarHeight; ++i)
    {
      const double t = 1.0 - double(i) / qMax(1, colorBarHeight - 1);
      m_colorBar.setPixelColor(0, i, ColorMap::color(m_scheme, t));
    }
    m_colorBarScheme = m_scheme;
  }
  p.drawImage(bar, m_colorBar);
  p.setPen(grid);
  p.drawRect(bar);

  p.setPen(labels);
  p.drawText(QRectF(area.left() - yLabelWidth - 4, area.bottom() - 12, yLabelWidth, 18),
             Qt::AlignRight | Qt::AlignVCenter, yMinimum);
  p.drawText(QRectF(area.left() - yLabelWidth - 4, area.top() - 6, yLabelWidth, 18),
             Qt::AlignRight | Qt::AlignVCenter, yMaximum);
  p.drawText(QRectF(area.left(), area.bottom() + 25, xLabelWidth, 16), Qt::AlignLeft, xMinimum);
  p.drawText(QRectF(area.right() - xLabelWidth, area.bottom() + 25, xLabelWidth, 16), Qt::AlignRight, xMaximum);
  p.drawText(QRectF(bar.right() + 4, bar.top() - 6, zLabelWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, zMaximum);
  p.drawText(QRectF(bar.right() + 4, bar.bottom() - 12, zLabelWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, zMinimum);
  p.drawText(QRectF(bar.left() - 20, bar.bottom() + 6, 90, 16), Qt::AlignLeft, zAxis.title);
  drawHold();
}

void HeatmapPlot::setColorScheme(ColorMap::Scheme scheme)
{
  if (scheme == m_scheme)
    return;
  m_scheme = scheme;
  update();
}
