// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/histogramplot.h"

#include "core/siprefix.h"

#include <QPainter>

#include <cmath>

HistogramPlot::HistogramPlot(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(220, 180);
}

void HistogramPlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels,
                                   const QColor &data)
{
  m_background = background;
  m_grid = grid;
  m_labels = labels;
  m_data = data;
  update();
}

void HistogramPlot::setValueId(int id)
{
  if (m_valueId == id)
    return;
  m_valueId = id;
  clear();
}

void HistogramPlot::addReading(const Reading &reading)
{
  if (reading.id != m_valueId)
    return;
  if (reading.noNumber() || reading.overload || reading.hold || !std::isfinite(reading.value))
    return;
  m_values.append(reading.value);
  if (m_values.size() > 1000)
    m_values.remove(0, m_values.size() - 1000);
  m_unit = reading.baseUnit;
  update();
}

void HistogramPlot::clear()
{
  m_values.clear();
  update();
}

void HistogramPlot::setCaption(const QString &caption)
{
  if (m_caption == caption)
    return;
  m_caption = caption;
  update();
}

void HistogramPlot::setNotice(const QString &notice)
{
  if (m_notice == notice)
    return;
  m_notice = notice;
  update();
}

void HistogramPlot::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), m_background.style() == Qt::NoBrush ? QBrush(palette().base()) : m_background);
  const QColor grid = m_grid.isValid() ? m_grid : palette().mid().color();
  const QColor labels = m_labels.isValid() ? m_labels : palette().text().color();
  const QColor data = m_data.isValid() ? m_data : palette().highlight().color();
  const QFontMetrics fm(font());
  p.setPen(labels);
  p.drawText(QRect(6, 2, width() - 12, fm.height()), Qt::AlignCenter,
             fm.elidedText(m_caption, Qt::ElideRight, width() - 12));
  const QRect area = rect().adjusted(48, fm.height() + 14, -16, -fm.height() * 2 - 14);
  p.setPen(grid);
  for (int i = 0; i <= 4; ++i)
  {
    const int y = area.bottom() - area.height() * i / 4;
    p.drawLine(area.left(), y, area.right(), y);
  }
  if (m_values.isEmpty())
  {
    p.setPen(labels);
    p.drawText(area, Qt::AlignCenter, m_notice.isEmpty() ? tr("No readings yet") : m_notice);
    return;
  }
  double lo = m_values.first(), hi = lo;
  for (double value : std::as_const(m_values))
  {
    lo = qMin(lo, value);
    hi = qMax(hi, value);
  }
  if (lo == hi)
  {
    const double pad = qMax(std::abs(lo) * 0.05, 0.5);
    lo -= pad;
    hi += pad;
  }
  QString prefix;
  SiPrefix::scale(qMax(std::abs(lo), std::abs(hi)), m_unit, &prefix);
  const double factor = SiPrefix::factor(prefix);
  const int bins = qBound(5, int(std::ceil(std::sqrt(m_values.size()))), 40);
  QVector<int> counts(bins, 0);
  int maxCount = 1;
  for (double value : std::as_const(m_values))
  {
    const int bin = qBound(0, int((value - lo) / (hi - lo) * bins), bins - 1);
    maxCount = qMax(maxCount, ++counts[bin]);
  }
  p.setPen(labels);
  p.drawText(QRect(area.left() - 42, area.top() - fm.height() / 2, 38, fm.height()), Qt::AlignRight,
             QString::number(maxCount));
  p.drawText(QRect(area.left() - 42, area.bottom() - fm.height() / 2, 38, fm.height()), Qt::AlignRight, "0");
  p.drawText(QRect(area.left(), area.bottom() + 4, 90, fm.height()), Qt::AlignLeft,
             QString::number(lo / factor, 'g', 4));
  p.drawText(QRect(area.right() - 90, area.bottom() + 4, 90, fm.height()), Qt::AlignRight,
             QString::number(hi / factor, 'g', 4));
  p.drawText(QRect(area.left(), area.bottom() + fm.height() + 4, area.width(), fm.height()), Qt::AlignCenter,
             m_unit.isEmpty() ? tr("Value") : SiPrefix::displayText(prefix + m_unit));
  const double slot = double(area.width()) / bins;
  p.setPen(Qt::NoPen);
  p.setBrush(data);
  for (int i = 0; i < bins; ++i)
  {
    const double height = double(counts[i]) / maxCount * area.height();
    p.drawRect(QRectF(area.left() + i * slot + 1, area.bottom() - height, qMax(1.0, slot - 2), height));
  }
}
