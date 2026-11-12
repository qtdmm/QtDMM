// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/histogramplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"
#include "ui/views/plotaxis.h"

#include <QCoreApplication>
#include <QVBoxLayout>

#include <cmath>

HistogramPlot::HistogramPlot(QWidget *parent) : QWidget(parent)
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
  m_plot->xAxis->setNumberFormat(QStringLiteral("gb"));
  m_plot->xAxis->setNumberPrecision(4);
  m_plot->yAxis->setNumberFormat(QStringLiteral("gb"));
  m_plot->yAxis->setNumberPrecision(3);
  m_bars = new QCPBars(m_plot->xAxis, m_plot->yAxis);
  m_bars->setAntialiased(false);

  m_noDataText = new QCPItemText(m_plot);
  m_noDataText->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_noDataText->position->setCoords(0.5, 0.5);
  m_noDataText->setPositionAlignment(Qt::AlignCenter);
  m_noDataText->setText(QCoreApplication::translate("HistogramPlot", "No readings yet"));
  m_holdText = new QCPItemText(m_plot);
  m_holdText->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_holdText->position->setCoords(0.98, 0.02);
  m_holdText->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
  m_holdText->setText(QCoreApplication::translate("HistogramPlot", "HOLD"));
  refreshPlot();
}

void HistogramPlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels,
                                   const QColor &data)
{
  m_background = background;
  m_grid = grid;
  m_labels = labels;
  m_data = data;
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
  m_bars->setPen(QPen(data.darker(115)));
  m_bars->setBrush(data);
  m_title->setTextColor(labels);
  m_noDataText->setColor(labels);
  m_holdText->setColor(labels);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
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
  if (reading.baseUnit != m_unit)
  {
    clear();
    m_unit = reading.baseUnit;
  }
  if (reading.noNumber() || reading.overload || reading.hold || !std::isfinite(reading.value))
    return;
  if (m_values.size() < kMaxValues)
    m_values.append(reading.value);
  else
  {
    m_values[m_valueHead] = reading.value;
    m_valueHead = (m_valueHead + 1) % kMaxValues;
  }
  if (!m_hold)
    refreshPlot();
}

void HistogramPlot::clear()
{
  m_values.clear();
  m_shown.clear();
  m_valueHead = 0;
  refreshPlot();
}

void HistogramPlot::setHold(bool hold)
{
  if (m_hold == hold)
    return;
  m_hold = hold;
  if (hold)
    m_shown = m_values;
  else
    m_shown.clear();
  refreshPlot();
}

void HistogramPlot::setCaption(const QString &caption)
{
  if (m_caption == caption)
    return;
  m_caption = caption;
  m_title->setText(caption);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void HistogramPlot::setNotice(const QString &notice)
{
  if (m_notice == notice)
    return;
  m_notice = notice;
  m_noDataText->setText(notice.isEmpty() ? tr("No readings yet") : notice);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void HistogramPlot::refreshPlot()
{
  const QVector<double> &shown = m_hold ? m_shown : m_values;
  m_noDataText->setVisible(shown.isEmpty());
  m_holdText->setVisible(m_hold);
  if (shown.isEmpty())
  {
    const PlotAxis::Range axis = PlotAxis::range(0, 0, m_unit);
    m_bars->setData(QVector<double>(), QVector<double>());
    m_plot->xAxis->setRange(axis.minimum / axis.factor, axis.maximum / axis.factor);
    m_plot->yAxis->setRange(0, 1);
    m_plot->xAxis->setLabel(axis.title);
    m_plot->yAxis->setLabel(tr("Count"));
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    return;
  }
  double lo = shown.first(), hi = lo;
  for (double value : shown)
  {
    lo = qMin(lo, value);
    hi = qMax(hi, value);
  }
  const PlotAxis::Range axis = PlotAxis::range(lo, hi, m_unit);
  const int bins = qBound(5, int(std::ceil(std::sqrt(shown.size()))), 40);
  QVector<double> keys(bins), counts(bins, 0);
  const double step = (axis.maximum - axis.minimum) / bins;
  for (int bin = 0; bin < bins; ++bin)
    keys[bin] = axis.minimum / axis.factor + (bin + 0.5) * step / axis.factor;
  for (double value : shown)
  {
    const int bin = qBound(0, int((value - axis.minimum) / (axis.maximum - axis.minimum) * bins), bins - 1);
    ++counts[bin];
  }
  m_bars->setWidth(step / axis.factor * 0.94);
  m_bars->setData(keys, counts);
  m_plot->xAxis->setRange(axis.minimum / axis.factor, axis.maximum / axis.factor);
  m_plot->yAxis->setRange(0, qMax(1.0, *std::max_element(counts.cbegin(), counts.cend()) * 1.12));
  m_plot->xAxis->setLabel(axis.title);
  m_plot->yAxis->setLabel(tr("Count"));
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}
