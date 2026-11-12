// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/heatmapplot.h"

#include "core/siprefix.h"

#include <QPainter>

#include <cmath>

namespace
{
constexpr int kMaxSamples = 5000;

struct UnitScale
{
  double factor = 1.0;
  QString title;
};

UnitScale unitScale(const QString &baseUnit, double magnitude)
{
  if (baseUnit.isEmpty())
    return {};

  QString prefix;
  SiPrefix::scale(magnitude, baseUnit, &prefix);
  return { SiPrefix::factor(prefix), QString("[%1]").arg(SiPrefix::displayText(prefix + baseUnit)) };
}
}

HeatmapPlot::HeatmapPlot(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(260, 180);
}

void HeatmapPlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels)
{ m_background = background; m_grid = grid; m_labels = labels; update(); }

void HeatmapPlot::setAxisUnits(const QString &x, const QString &y, const QString &z)
{ m_xUnit = x; m_yUnit = y; m_zUnit = z; update(); }

void HeatmapPlot::setLegend(const QString &legend) { m_legend = legend; update(); }

void HeatmapPlot::setCellCount(int nx, int ny)
{
  m_nx = qMax(2, nx);
  m_ny = qMax(2, ny);
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

  const QColor grid   = m_grid.isValid()   ? m_grid   : palette().mid().color();
  const QColor labels = m_labels.isValid() ? m_labels : palette().text().color();

  // Plot area leaves room on the right for the color bar.
  const QRectF area = QRectF(rect()).adjusted(48, 48, -78, -42);
  const QFontMetrics fm(font());
  auto drawHold = [&]
  {
    if (m_hold)
    {
      p.setPen(labels);
      p.drawText(QRectF(area.right() - 58, area.top(), 54, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("HOLD"));
    }
  };
  const QRectF bar(area.right() + 16, area.top(), 14, area.height());
  const QColor noData = ColorMap::noDataColor(m_scheme);
  p.fillRect(area, noData);

  p.setPen(labels);
  const QString legend = m_legend;
  const QString noDataText = tr("No data");
  const int noDataKeyWidth = 10 + 4 + fm.horizontalAdvance(noDataText);
  const int legendWidth = qMax(0, width() - 20 - noDataKeyWidth);
  p.drawText(QRectF(6, 2, legendWidth, fm.height()), Qt::AlignCenter,
             fm.elidedText(legend, Qt::ElideRight, legendWidth));
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

  const QVector<Sample> &shown = m_hold ? m_shown : m_samples;
  if (shown.isEmpty())
  {
    drawAxisUnits(unitScale(m_xUnit, 0).title, unitScale(m_yUnit, 0).title);
    p.setPen(labels);
    p.drawText(QRectF(bar.left() - 20, bar.bottom() + 6, 90, 16), Qt::AlignLeft,
               unitScale(m_zUnit, 0).title);
    p.drawText(area, Qt::AlignCenter, tr("No heatmap data yet"));
    p.setPen(grid);
    p.drawRect(area);
    drawHold();
    return;
  }

  double xmin = shown.first().x, xmax = xmin;
  double ymin = shown.first().y, ymax = ymin;
  double zmin = shown.first().z, zmax = zmin;
  for (const Sample &s : std::as_const(shown))
  {
    xmin = qMin(xmin, s.x); xmax = qMax(xmax, s.x);
    ymin = qMin(ymin, s.y); ymax = qMax(ymax, s.y);
    zmin = qMin(zmin, s.z); zmax = qMax(zmax, s.z);
  }
  const double xMagnitude = qMax(std::abs(xmin), std::abs(xmax));
  const double yMagnitude = qMax(std::abs(ymin), std::abs(ymax));
  const double zMagnitude = qMax(std::abs(zmin), std::abs(zmax));
  const UnitScale xUnit = unitScale(m_xUnit, xMagnitude);
  const UnitScale yUnit = unitScale(m_yUnit, yMagnitude);
  const UnitScale zUnit = unitScale(m_zUnit, zMagnitude);

  if (xmin == xmax) { xmin -= 0.5; xmax += 0.5; }
  if (ymin == ymax) { ymin -= 0.5; ymax += 0.5; }
  if (zmin == zmax) { zmin -= 0.5; zmax += 0.5; }

  drawAxisUnits(xUnit.title, yUnit.title);

  // Bin samples into cells and average per cell.
  QVector<Cell> cells(m_nx * m_ny);
  for (const Sample &s : std::as_const(shown))
  {
    const int ix = qBound(0, int((s.x - xmin) / (xmax - xmin) * m_nx), m_nx - 1);
    const int iy = qBound(0, int((s.y - ymin) / (ymax - ymin) * m_ny), m_ny - 1);
    Cell &c = cells[iy * m_nx + ix];
    c.sum += s.z;
    ++c.n;
  }

  const qreal cw = area.width() / m_nx;
  const qreal ch = area.height() / m_ny;
  p.setPen(Qt::NoPen);
  for (int iy = 0; iy < m_ny; ++iy)
  {
    for (int ix = 0; ix < m_nx; ++ix)
    {
      const Cell &c = cells[iy * m_nx + ix];
      if (c.n == 0)
        continue;                                   // empty cells stay background
      const double t = (c.sum / c.n - zmin) / (zmax - zmin);
      p.setBrush(ColorMap::color(m_scheme, t));
      // y grows upwards; +1 px overlap avoids hairline gaps
      p.drawRect(QRectF(area.left() + ix * cw, area.bottom() - (iy + 1) * ch, cw + 1, ch + 1));
    }
  }

  // Frame
  p.setBrush(Qt::NoBrush);
  p.setPen(grid);
  p.drawRect(area);

  // Cache the vertical color strip; its pixels only change with size or scheme.
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

  // Labels
  p.setPen(labels);
  p.drawText(QRectF(area.left() - 32, area.bottom() - 12, 30, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(ymin / yUnit.factor, 'g', 4));
  p.drawText(QRectF(area.left() - 32, area.top() - 6, 30, 18),     Qt::AlignRight | Qt::AlignVCenter, QString::number(ymax / yUnit.factor, 'g', 4));
  p.drawText(QRectF(area.left(), area.bottom() + 25, 60, 16),      Qt::AlignLeft,  QString::number(xmin / xUnit.factor, 'g', 4));
  p.drawText(QRectF(area.right() - 60, area.bottom() + 25, 60, 16), Qt::AlignRight, QString::number(xmax / xUnit.factor, 'g', 4));
  p.drawText(QRectF(bar.right() + 4, bar.top() - 6, 56, 18),       Qt::AlignLeft | Qt::AlignVCenter, QString::number(zmax / zUnit.factor, 'g', 4));
  p.drawText(QRectF(bar.right() + 4, bar.bottom() - 12, 56, 18),   Qt::AlignLeft | Qt::AlignVCenter, QString::number(zmin / zUnit.factor, 'g', 4));
  p.drawText(QRectF(bar.left() - 20, bar.bottom() + 6, 90, 16),    Qt::AlignLeft, zUnit.title);
  drawHold();
}

void HeatmapPlot::setColorScheme(ColorMap::Scheme scheme)
{
  if (scheme == m_scheme)
    return;
  m_scheme = scheme;
  update();
}
