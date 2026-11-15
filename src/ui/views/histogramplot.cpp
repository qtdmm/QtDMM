// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/histogramplot.h"

#include "3rdparty/qcustomplot/qcustomplot.h"

#include <algorithm>
#include <cmath>

HistogramPlot::HistogramPlot(QWidget *parent) : QcpGraphCommon(parent, tr("No readings yet"))
{
  m_plot->xAxis->setNumberFormat(QStringLiteral("gb"));
  m_plot->xAxis->setNumberPrecision(4);
  m_plot->yAxis->setNumberFormat(QStringLiteral("gb"));
  m_plot->yAxis->setNumberPrecision(3);
  m_bars = new QCPBars(m_plot->xAxis, m_plot->yAxis);
  m_bars->setAntialiased(false);
  refreshPlot();
}

void HistogramPlot::setTheme(const PlotTheme &theme)
{
  m_bars->setPen(QPen(theme.data.darker(115)));
  m_bars->setBrush(theme.data);
  QcpGraphCommon::setTheme(theme);
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
  m_values.append(reading.value);
  scheduleRefresh();
}

void HistogramPlot::setNotice(const QString &notice)
{
  setNoDataText(notice.isEmpty() ? tr("No readings yet") : notice);
}

void HistogramPlot::holdChanged(bool hold)
{
  m_held = hold ? m_values.values() : QVector<double>();
}

void HistogramPlot::clearSamples()
{
  m_values.clear();
  m_held.clear();
}

void HistogramPlot::refreshPlot()
{
  const QVector<double> shown = hold() ? m_held : m_values.values();
  setNoDataVisible(shown.isEmpty());
  m_plot->yAxis->setLabel(tr("Count"));
  if (shown.isEmpty())
  {
    m_bars->setData(QVector<double>(), QVector<double>());
    setAxisRange(m_plot->xAxis, PlotAxis::range(0, 0, m_unit));
    m_plot->yAxis->setRange(0, 1);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    return;
  }
  const auto [lo, hi] = std::minmax_element(shown.cbegin(), shown.cend());
  const PlotAxis::Range axis = PlotAxis::range(*lo, *hi, m_unit);
  const int bins = qBound(5, int(std::ceil(std::sqrt(shown.size()))), 40);
  QVector<double> keys(bins), counts(bins, 0);
  const double step = (axis.maximum - axis.minimum) / bins;
  for (int bin = 0; bin < bins; ++bin)
    keys[bin] = (axis.minimum + (bin + 0.5) * step) / axis.factor;
  for (double value : shown)
    ++counts[qBound(0, int((value - axis.minimum) / (axis.maximum - axis.minimum) * bins), bins - 1)];
  m_bars->setWidth(step / axis.factor * 0.94);
  m_bars->setData(keys, counts);
  setAxisRange(m_plot->xAxis, axis);
  m_plot->yAxis->setRange(0, qMax(1.0, *std::max_element(counts.cbegin(), counts.cend()) * 1.12));
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

QString HistogramPlot::crosshairText(double x, double y) const
{
  return QString("%1   %2").arg(axisValueText(m_plot->xAxis, x), tr("%1 readings").arg(qRound(y)));
}
