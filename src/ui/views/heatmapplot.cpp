// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/heatmapplot.h"

#include <QPainter>
#include <QMenu>
#include <QActionGroup>
#include <QContextMenuEvent>

#include <cmath>

namespace { constexpr int kMaxSamples = 5000; }

HeatmapPlot::HeatmapPlot(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(260, 180);
}

void HeatmapPlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels)
{ m_background = background; m_grid = grid; m_labels = labels; update(); }

void HeatmapPlot::setAxisCaptions(const QString &x, const QString &y, const QString &z)
{ m_xCaption = x; m_yCaption = y; m_zCaption = z; update(); }

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
  m_samples.append({x, y, z});
  if (m_samples.size() > kMaxSamples)
    m_samples.remove(0, m_samples.size() - kMaxSamples);
  update();
}

void HeatmapPlot::clear() { m_samples.clear(); update(); }

void HeatmapPlot::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.fillRect(rect(), m_background.style() == Qt::NoBrush ? QBrush(palette().base()) : m_background);

  const QColor grid   = m_grid.isValid()   ? m_grid   : palette().mid().color();
  const QColor labels = m_labels.isValid() ? m_labels : palette().text().color();

  // Plot area leaves room on the right for the color bar.
  const QRectF area = QRectF(rect()).adjusted(48, 48, -78, -42);
  const QRectF bar(area.right() + 16, area.top(), 14, area.height());

  p.setPen(labels);
  const QFontMetrics fm(font());
  const QString legend = m_legend;
  p.drawText(QRectF(6, 2, width() - 12, fm.height()), Qt::AlignCenter,
             fm.elidedText(legend, Qt::ElideRight, width() - 12));
  p.drawText(QRectF(area.left(), area.bottom() + 8, area.width(), 24), Qt::AlignCenter, m_xCaption);
  p.save();
  p.translate(12, area.center().y());
  p.rotate(-90);
  p.drawText(QRectF(-area.height() / 2, -12, area.height(), 24), Qt::AlignCenter, m_yCaption);
  p.restore();

  if (m_samples.isEmpty())
  {
    p.drawText(area, Qt::AlignCenter, tr("No heatmap data yet"));
    p.setPen(grid);
    p.drawRect(area);
    return;
  }

  double xmin = m_samples.first().x, xmax = xmin;
  double ymin = m_samples.first().y, ymax = ymin;
  double zmin = m_samples.first().z, zmax = zmin;
  for (const Sample &s : std::as_const(m_samples))
  {
    xmin = qMin(xmin, s.x); xmax = qMax(xmax, s.x);
    ymin = qMin(ymin, s.y); ymax = qMax(ymax, s.y);
    zmin = qMin(zmin, s.z); zmax = qMax(zmax, s.z);
  }
  if (xmin == xmax) { xmin -= 0.5; xmax += 0.5; }
  if (ymin == ymax) { ymin -= 0.5; ymax += 0.5; }
  if (zmin == zmax) { zmin -= 0.5; zmax += 0.5; }

  // Bin samples into cells and average per cell.
  QVector<Cell> cells(m_nx * m_ny);
  for (const Sample &s : std::as_const(m_samples))
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

  // Color bar
  for (int i = 0; i < int(bar.height()); ++i)
  {
    const double t = 1.0 - double(i) / qMax(1.0, bar.height() - 1);
    p.setPen(ColorMap::color(m_scheme, t));
    p.drawLine(QPointF(bar.left(), bar.top() + i), QPointF(bar.right(), bar.top() + i));
  }
  p.setPen(grid);
  p.drawRect(bar);

  // Labels
  p.setPen(labels);
  p.drawText(QRectF(area.left() - 32, area.bottom() - 12, 30, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(ymin, 'g', 4));
  p.drawText(QRectF(area.left() - 32, area.top() - 6, 30, 18),     Qt::AlignRight | Qt::AlignVCenter, QString::number(ymax, 'g', 4));
  p.drawText(QRectF(area.left(), area.bottom() + 25, 60, 16),      Qt::AlignLeft,  QString::number(xmin, 'g', 4));
  p.drawText(QRectF(area.right() - 60, area.bottom() + 25, 60, 16), Qt::AlignRight, QString::number(xmax, 'g', 4));
  p.drawText(QRectF(bar.right() + 4, bar.top() - 6, 56, 18),       Qt::AlignLeft | Qt::AlignVCenter, QString::number(zmax, 'g', 4));
  p.drawText(QRectF(bar.right() + 4, bar.bottom() - 12, 56, 18),   Qt::AlignLeft | Qt::AlignVCenter, QString::number(zmin, 'g', 4));
  p.drawText(QRectF(bar.left() - 20, bar.bottom() + 6, 90, 16),    Qt::AlignLeft, m_zCaption);
}

// in HeatmapPlot: setContextMenuPolicy(Qt::DefaultContextMenu);
void HeatmapPlot::contextMenuEvent(QContextMenuEvent *e)
{
  QMenu menu(this);
  auto *group = new QActionGroup(&menu);
  for (ColorMap::Scheme s : ColorMap::all())
  {
    QAction *a = menu.addAction(ColorMap::name(s));
    a->setCheckable(true);
    a->setChecked(s == m_scheme);
    group->addAction(a);
    connect(a, &QAction::triggered, this, [this, s] { setColorScheme(s); emit colorSchemeChanged(s); });
  }
  menu.exec(e->globalPos());
}

void HeatmapPlot::setColorScheme(ColorMap::Scheme scheme)
{
  if (scheme == m_scheme)
    return;
  m_scheme = scheme;
  update();
}
