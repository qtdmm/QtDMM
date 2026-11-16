// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: the time axis and the left axis.
#include "ui/views/qcpgraph.h"

#include "recording/recordingstore.h"
#include "ui/views/qcp.h"

#include <cmath>

void QcpGraph::setupAxes()
{
  // the time since the start of the recording
  QSharedPointer<QCPAxisTickerTime> time(new QCPAxisTickerTime);
  time->setTimeFormat(QStringLiteral("%h:%m:%s"));
  m_plot->xAxis->setTicker(time);
}

qint64 QcpGraph::windowStart() const
{
  if (!m_store)
    return 0;
  return qMax(m_store->origin(), m_store->duration() - qint64(m_windowSeconds) * 1000);
}

bool QcpGraph::valueSpan(double from, double to, double &lo, double &hi) const
{
  bool any = false;
  const auto byTime = [](const QPointF &p, double t) { return p.x() < t; };
  for (auto it = std::lower_bound(m_points.cbegin(), m_points.cend(), from, byTime);
       it != m_points.cend() && it->x() <= to; ++it)
  {
    if (std::isnan(it->y()))
      continue;
    lo = any ? qMin(lo, it->y()) : it->y();
    hi = any ? qMax(hi, it->y()) : it->y();
    any = true;
  }
  return any;
}

// The time axis over the window up to the newest reading, the left axis
// over the values shown, in the unit with the SI prefix that suits them.
void QcpGraph::updateAxes(double &from, double &to, double &factor)
{
  from = windowStart() / 1000.0;
  to = from + m_windowSeconds;
  m_plot->xAxis->setRange(from, to);

  double lo = 0, hi = 0;
  const bool any = valueSpan(from, to, lo, hi);
  // a little room above and below, the curve does not touch the frame
  const double room = (hi - lo) * 0.05;
  lo -= room;
  hi += room;
  const PlotAxis::Range range = PlotAxis::range(lo, hi, m_store && any ? m_store->series().unit : QString());
  setAxisRange(m_plot->yAxis, range);
  factor = range.factor;
  setCrosshairEnabled(any);
}

QString QcpGraph::crosshairText(double x, double y) const
{
  const int seconds = qMax(0, int(x));
  const QString time = QString("%1:%2:%3").arg(seconds / 3600).arg(seconds / 60 % 60, 2, 10, QChar('0'))
                       .arg(seconds % 60, 2, 10, QChar('0'));
  return QString("%1   %2").arg(time, axisValueText(m_plot->yAxis, y));
}

double QcpGraph::timeAxisStart() const
{
  return m_plot->xAxis->range().lower;
}

double QcpGraph::timeAxisEnd() const
{
  return m_plot->xAxis->range().upper;
}

QString QcpGraph::leftAxisTitle() const
{
  return m_plot->yAxis->label();
}
