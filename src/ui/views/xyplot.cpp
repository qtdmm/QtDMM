// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/xyplot.h"

#include "core/siprefix.h"

#include <QPainter>

#include <cmath>

namespace
{
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

XYPlot::XYPlot(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(220, 180);
}

void XYPlot::setCaption(const QString &caption)
{
  m_caption = caption;
  update();
}

void XYPlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels, const QColor &data)
{
  m_background = background;
  m_grid = grid;
  m_labels = labels;
  m_data = data;
  update();
}

void XYPlot::setLegend(const QString &legend)
{
  m_legend = legend;
  update();
}

void XYPlot::setAxisUnits(const QString &x, const QString &y)
{
  m_xUnit = x;
  m_yUnit = y;
  update();
}

void XYPlot::addPoint(double x, double y)
{
  if (!std::isfinite(x) || !std::isfinite(y))
    return;
  m_points.append(QPointF(x, y));
  if (m_points.size() > 2000)
    m_points.remove(0, m_points.size() - 2000);
  update();
}

void XYPlot::clear()
{
  m_points.clear();
  m_shown.clear();
  update();
}

void XYPlot::setHold(bool hold)
{
  if (m_hold == hold)
    return;
  m_hold = hold;
  if (m_hold)
    m_shown = m_points;
  else
    m_shown.clear();
  update();
}

void XYPlot::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), m_background.style() == Qt::NoBrush ? QBrush(palette().base()) : m_background);
  const QRectF area = QRectF(rect()).adjusted(48, 48, -20, -42);
  const QFontMetrics fm(font());
  const QColor grid = m_grid.isValid() ? m_grid : palette().mid().color();
  const QColor labels = m_labels.isValid() ? m_labels : palette().text().color();
  const QColor data = m_data.isValid() ? m_data : palette().highlight().color();
  auto drawHold = [&]
  {
    if (m_hold)
    {
      p.setPen(labels);
      p.drawText(QRectF(area.right() - 58, area.top(), 54, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("HOLD"));
    }
  };
  p.setPen(grid);
  for (int i = 0; i <= 5; ++i)
  {
    const qreal x = area.left() + area.width() * i / 5;
    const qreal y = area.top() + area.height() * i / 5;
    p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
  }
  p.setPen(labels);
  const QString legend = m_legend.isEmpty() ? m_caption : m_legend;
  p.drawText(QRectF(6, 2, width() - 12, fm.height()), Qt::AlignCenter,
             fm.elidedText(legend, Qt::ElideRight, width() - 12));
  const QVector<QPointF> &shown = m_hold ? m_shown : m_points;
  if (shown.isEmpty())
  {
    p.setPen(labels);
    p.drawText(QRectF(area.left(), area.bottom() + 8, area.width(), 24), Qt::AlignCenter,
               unitScale(m_xUnit, 0).title);
    p.save();
    p.translate(12, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height() / 2, -12, area.height(), 24), Qt::AlignCenter,
               unitScale(m_yUnit, 0).title);
    p.restore();
    p.drawText(area, Qt::AlignCenter, tr("No XY data yet"));
    drawHold();
    return;
  }
  double xmin = shown.first().x(), xmax = xmin, ymin = shown.first().y(), ymax = ymin;
  for (const QPointF &pt : std::as_const(shown))
  {
    xmin = qMin(xmin, pt.x()); xmax = qMax(xmax, pt.x());
    ymin = qMin(ymin, pt.y()); ymax = qMax(ymax, pt.y());
  }
  const UnitScale xUnit = unitScale(m_xUnit, qMax(std::abs(xmin), std::abs(xmax)));
  const UnitScale yUnit = unitScale(m_yUnit, qMax(std::abs(ymin), std::abs(ymax)));
  if (xmin == xmax) { xmin -= 0.5; xmax += 0.5; }
  if (ymin == ymax) { ymin -= 0.5; ymax += 0.5; }
  p.setPen(labels);
  p.drawText(QRectF(area.left(), area.bottom() + 8, area.width(), 24), Qt::AlignCenter, xUnit.title);
  p.save();
  p.translate(12, area.center().y());
  p.rotate(-90);
  p.drawText(QRectF(-area.height() / 2, -12, area.height(), 24), Qt::AlignCenter, yUnit.title);
  p.restore();
  p.drawText(QRectF(area.left() - 32, area.bottom() - 12, 30, 18), Qt::AlignRight | Qt::AlignVCenter,
             QString::number(ymin / yUnit.factor, 'g', 4));
  p.drawText(QRectF(area.left() - 32, area.top() - 6, 30, 18), Qt::AlignRight | Qt::AlignVCenter,
             QString::number(ymax / yUnit.factor, 'g', 4));
  p.drawText(QRectF(area.left(), area.bottom() + 25, 60, 16), Qt::AlignLeft,
             QString::number(xmin / xUnit.factor, 'g', 4));
  p.drawText(QRectF(area.right() - 60, area.bottom() + 25, 60, 16), Qt::AlignRight,
             QString::number(xmax / xUnit.factor, 'g', 4));
  p.setPen(Qt::NoPen);
  p.setBrush(data);
  for (const QPointF &pt : std::as_const(shown))
    p.drawEllipse(QPointF(area.left() + (pt.x() - xmin) / (xmax - xmin) * area.width(),
                         area.bottom() - (pt.y() - ymin) / (ymax - ymin) * area.height()), 2.5, 2.5);
  drawHold();
}
