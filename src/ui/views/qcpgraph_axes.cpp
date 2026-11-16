// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: the time shown (window, "All", scrolling), the time axis and
// the left axis, the row above and the scrollbar below.
#include "ui/views/qcpgraph.h"

#include "recording/recordingstore.h"
#include "ui/graphmodel.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/qcp.h"

#include <QScrollBar>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>

// "0:01:20"
static QString timeText(qint64 ms)
{
  const qint64 seconds = qMax<qint64>(0, ms / 1000);
  return QString("%1:%2:%3").arg(seconds / 3600).arg(seconds / 60 % 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
}

void QcpGraph::setupAxes()
{
  // the time since the start of the recording
  QSharedPointer<QCPAxisTickerTime> time(new QCPAxisTickerTime);
  time->setTimeFormat(QStringLiteral("%h:%m:%s"));
  m_plot->xAxis->setTicker(time);

  // above the plot the state and the time buttons, below it the scrollbar
  auto *box = qobject_cast<QVBoxLayout *>(layout());
  m_timeBar = new GraphTimeBar(this);
  m_timeBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  box->insertWidget(0, m_timeBar);
  box->setStretchFactor(m_plot, 1);
  connect(m_timeBar, &GraphTimeBar::windowChosen, this, [this](int seconds)
  {
    if (seconds == 0)
      setFollowAll(true);
    else
    {
      setWindowSeconds(seconds);
      followEnd();
    }
  });
  m_scrollBar = new QScrollBar(Qt::Horizontal, this);
  m_scrollBar->setFocusPolicy(Qt::NoFocus);
  box->addWidget(m_scrollBar);
  // in tenths of a second over what the store keeps
  connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int value)
  {
    if (m_store)
      showFrom(m_store->origin() + qint64(value) * 100);
  });
}

void QcpGraph::setWindowSeconds(int seconds)
{
  seconds = qMax(1, seconds);
  if (seconds == m_windowSeconds)
    return;
  // another window (a time button, a zoom, the bench) ends "All"
  if (m_followAll)
  {
    m_followAll = false;
    Q_EMIT followAllChanged(false);
  }
  applyWindow(seconds);
  redraw();
}

void QcpGraph::applyWindow(int seconds)
{
  seconds = qMax(1, seconds);
  if (seconds == m_windowSeconds)
    return;
  m_windowSeconds = seconds;
  Q_EMIT windowChanged(seconds);
}

void QcpGraph::setFollowAll(bool on)
{
  if (on == m_followAll)
    return;
  m_followAll = on;
  if (on)
  {
    m_followEnd = true;
    fitAll(false);
  }
  Q_EMIT followAllChanged(on);
  redraw();
}

void QcpGraph::fitAll(bool grow)
{
  if (!m_store)
    return;
  const double recorded = (m_store->duration() - m_store->origin()) / 1000.0;
  // growing by a quarter at a time: the window does not change with every reading
  if (grow && recorded <= m_windowSeconds)
    return;
  const qint64 limit = m_store->lengthLimit();
  applyWindow(GraphModel::allTarget(recorded, grow, limit > 0 ? int((limit + 999) / 1000) : 0));
}

qint64 QcpGraph::lastStart() const
{
  if (!m_store)
    return 0;
  return qMax(m_store->origin(), m_store->duration() - qint64(m_windowSeconds) * 1000);
}

qint64 QcpGraph::windowStart() const
{
  if (!m_store)
    return 0;
  if (m_followEnd)
    return lastStart();
  return qBound(m_store->origin(), m_start, lastStart());
}

void QcpGraph::showFrom(qint64 start)
{
  if (!m_store)
    return;
  // at the end it follows again: the scrollbar pushed to the right, a drag past the newest
  m_followEnd = start >= lastStart();
  m_start = qMax(m_store->origin(), start);
  redraw();
}

void QcpGraph::followEnd()
{
  if (m_followEnd)
    return;
  m_followEnd = true;
  redraw();
}

void QcpGraph::zoomBy(double factor)
{
  if (factor <= 0)
    return;
  // around the middle of what is shown; following, the end stays the newest
  const qint64 middle = windowStart() + qint64(m_windowSeconds) * 500;
  const bool atEnd = m_followEnd;
  setWindowSeconds(qBound(1, qRound(m_windowSeconds / factor), 99999 * 60));
  if (!atEnd)
    showFrom(middle - qint64(m_windowSeconds) * 500);
}

void QcpGraph::zoomFit()
{
  if (!m_store)
    return;
  if (m_followAll)
  {
    m_followAll = false;
    Q_EMIT followAllChanged(false);
  }
  fitAll(false);
  m_followEnd = true;
  redraw();
}

void QcpGraph::zoomToRect(const QRect &rect)
{
  const QRect plot = m_plot->axisRect()->rect();
  const QRect frame = rect.normalized().intersected(plot);
  if (!m_store || frame.width() < 1 || frame.height() < 1)
    return;
  if (!m_zoomed)
    m_zoomBefore = { windowStart(), m_windowSeconds, m_followAll, m_followEnd };
  // the left axis: the frame's height in its scale, kept in the base unit
  m_yZoomMin = m_plot->yAxis->pixelToCoord(frame.bottom()) * m_factor;
  m_yZoomMax = m_plot->yAxis->pixelToCoord(frame.top()) * m_factor;
  m_zoomed = true;
  const double t0 = m_plot->xAxis->pixelToCoord(frame.left());
  const double t1 = m_plot->xAxis->pixelToCoord(frame.right());
  setWindowSeconds(qMax(1, qRound(t1 - t0)));
  showFrom(qRound64(t0 * 1000));
}

void QcpGraph::endZoom(bool restoreWindow)
{
  if (!m_zoomed)
    return;
  m_zoomed = false;
  if (restoreWindow)
  {
    if (m_zoomBefore.all)
      setFollowAll(true);
    else
    {
      setWindowSeconds(m_zoomBefore.seconds);
      if (m_zoomBefore.atEnd)
        followEnd();
      else
        showFrom(m_zoomBefore.start);
    }
  }
  redraw();
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

// The time axis over the window, the left axis over the values shown (or
// the frame zoomed on), in the unit with the SI prefix that suits them.
void QcpGraph::updateAxes(double &from, double &to, double &factor)
{
  from = windowStart() / 1000.0;
  to = from + m_windowSeconds;
  m_plot->xAxis->setRange(from, to);
  // a label is about 70 pixels wide: fewer ticks on a narrow plot
  m_plot->xAxis->ticker()->setTickCount(qBound(2, m_plot->axisRect()->width() / 110, 6));

  double lo = 0, hi = 0;
  const bool any = valueSpan(from, to, lo, hi);
  if (m_zoomed)
  {
    lo = m_yZoomMin;
    hi = m_yZoomMax;
  }
  else
  {
    // a little room above and below, the curve does not touch the frame
    const double room = (hi - lo) * 0.05;
    lo -= room;
    hi += room;
  }
  // the series' unit; a loaded file has only the store's
  QString unit;
  if (m_store && (any || m_zoomed))
    unit = m_store->series().unit.isEmpty() ? m_store->unit() : m_store->series().unit;
  const PlotAxis::Range range = PlotAxis::range(lo, hi, unit);
  setAxisRange(m_plot->yAxis, range);
  factor = m_factor = range.factor;
  setCrosshairEnabled(any);
}

void QcpGraph::updateScrollBar()
{
  const QSignalBlocker quiet(m_scrollBar);
  const qint64 origin = m_store ? m_store->origin() : 0;
  m_scrollBar->setRange(0, int((lastStart() - origin) / 100));
  m_scrollBar->setSingleStep(qMax(1, m_windowSeconds));
  m_scrollBar->setPageStep(m_windowSeconds * 10);
  m_scrollBar->setValue(int((windowStart() - origin) / 100));
}

void QcpGraph::updateTimeBar()
{
  m_timeBar->setWindow(m_windowSeconds, m_followAll);
  QString state;
  if (m_store)
  {
    const QString length = timeText(m_store->duration());
    if (m_store->state() == RecordingStore::Live)
      state = m_followEnd ? tr("Live") : tr("Live · held");
    else if (m_store->state() == RecordingStore::Record)
      state = tr("Recording · %1").arg(length);
    else if (!m_sourceName.isEmpty())
      state = tr("%1 · %2").arg(m_sourceName, length);
    else if (m_store->count() > 0)
      state = tr("Stopped · %1").arg(length);
  }
  m_timeBar->setStateText(state);
}

QString QcpGraph::crosshairText(double x, double y) const
{
  return QString("%1   %2").arg(timeText(qint64(x * 1000)), axisValueText(m_plot->yAxis, y));
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

double QcpGraph::leftAxisMin() const
{
  return m_plot->yAxis->range().lower;
}

double QcpGraph::leftAxisMax() const
{
  return m_plot->yAxis->range().upper;
}

QString QcpGraph::stateText() const
{
  return m_timeBar->stateText();
}

QRect QcpGraph::plotRect() const
{
  return m_plot->axisRect()->rect();
}
