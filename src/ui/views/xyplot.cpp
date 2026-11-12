// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/xyplot.h"

#include "ui/views/plotaxis.h"

#include <QPainter>

#include <cmath>

XYPlot::XYPlot(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(220, 180);
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

  const QColor grid = m_grid.isValid() ? m_grid : palette().mid().color();
  const QColor labels = m_labels.isValid() ? m_labels : palette().text().color();
  const QColor data = m_data.isValid() ? m_data : palette().highlight().color();
  const QFontMetrics fm(font());
  p.setPen(labels);
  p.drawText(QRectF(6, 2, width() - 12, fm.height()), Qt::AlignCenter,
             fm.elidedText(m_legend, Qt::ElideRight, width() - 12));

  const QVector<QPointF> &shown = m_hold ? m_shown : m_points;
  PlotAxis::Range xAxis = PlotAxis::range(0, 0, m_xUnit);
  PlotAxis::Range yAxis = PlotAxis::range(0, 0, m_yUnit);
  QString xMinimum, xMaximum, yMinimum, yMaximum;
  int yLabelWidth = 0;
  int xLabelWidth = 60;
  if (!shown.isEmpty())
  {
    double xmin = shown.first().x(), xmax = xmin, ymin = shown.first().y(), ymax = ymin;
    for (const QPointF &point : std::as_const(shown))
    {
      xmin = qMin(xmin, point.x());
      xmax = qMax(xmax, point.x());
      ymin = qMin(ymin, point.y());
      ymax = qMax(ymax, point.y());
    }
    xAxis = PlotAxis::range(xmin, xmax, m_xUnit);
    yAxis = PlotAxis::range(ymin, ymax, m_yUnit);
    xMinimum = QString::number(xAxis.minimum / xAxis.factor, 'g', 4);
    xMaximum = QString::number(xAxis.maximum / xAxis.factor, 'g', 4);
    yMinimum = QString::number(yAxis.minimum / yAxis.factor, 'g', 4);
    yMaximum = QString::number(yAxis.maximum / yAxis.factor, 'g', 4);
    yLabelWidth = qMax(fm.horizontalAdvance(yMinimum), fm.horizontalAdvance(yMaximum));
    xLabelWidth = qMax(fm.horizontalAdvance(xMinimum), fm.horizontalAdvance(xMaximum));
  }

  const qreal leftMargin = qMax(48, yLabelWidth + 20);
  const QRectF area = QRectF(rect()).adjusted(leftMargin, 48, -20, -42);
  auto drawHold = [&]
  {
    if (m_hold)
    {
      p.setPen(labels);
      p.drawText(QRectF(area.right() - 58, area.top(), 54, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                 tr("HOLD"));
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
  p.drawText(QRectF(area.left(), area.bottom() + 8, area.width(), 24), Qt::AlignCenter, xAxis.title);
  p.save();
  p.translate(12, area.center().y());
  p.rotate(-90);
  p.drawText(QRectF(-area.height() / 2, -12, area.height(), 24), Qt::AlignCenter, yAxis.title);
  p.restore();

  if (shown.isEmpty())
  {
    p.drawText(area, Qt::AlignCenter, tr("No XY data yet"));
    drawHold();
    return;
  }

  p.drawText(QRectF(area.left() - yLabelWidth - 4, area.bottom() - 12, yLabelWidth, 18),
             Qt::AlignRight | Qt::AlignVCenter, yMinimum);
  p.drawText(QRectF(area.left() - yLabelWidth - 4, area.top() - 6, yLabelWidth, 18),
             Qt::AlignRight | Qt::AlignVCenter, yMaximum);
  p.drawText(QRectF(area.left(), area.bottom() + 25, xLabelWidth, 16), Qt::AlignLeft, xMinimum);
  p.drawText(QRectF(area.right() - xLabelWidth, area.bottom() + 25, xLabelWidth, 16), Qt::AlignRight, xMaximum);

  p.setPen(Qt::NoPen);
  p.setBrush(data);
  for (const QPointF &point : std::as_const(shown))
  {
    p.drawEllipse(QPointF(area.left() + (point.x() - xAxis.minimum) / (xAxis.maximum - xAxis.minimum) * area.width(),
                         area.bottom() - (point.y() - yAxis.minimum) / (yAxis.maximum - yAxis.minimum) * area.height()),
                  2.5, 2.5);
  }
  drawHold();
}
