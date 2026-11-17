// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: the time shown (window, "All", scrolling), the time axis and
// the left axis, the row above and the scrollbar below.
#include "ui/views/qcpgraph.h"

#include "recording/recordingstore.h"
#include "ui/graphmodel.h"
#include "ui/views/graphtimebar.h"
#include "core/siprefix.h"
#include "ui/views/qcp.h"

#include <QCoreApplication>
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
  // the value axes: the left one, the others on the right, outwards
  for (int i = 0; i < kMaxAxes; ++i)
  {
    Axis a;
    a.axis = i == 0 ? m_plot->yAxis : i == 1 ? m_plot->yAxis2 : m_plot->axisRect()->addAxis(QCPAxis::atRight);
    if (i > 0)
    {
      a.axis->setVisible(false);
      a.axis->setTickLabels(true);
      a.axis->grid()->setVisible(false);   // one grid, the left axis'
    }
    m_axes.append(a);
  }

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

double QcpGraph::shownSeconds() const
{
  if (!m_store || !m_followAll || !m_followEnd)
    return m_windowSeconds;
  const double recorded = (m_store->duration() - m_store->origin()) / 1000.0;
  return recorded < m_windowSeconds ? qMax(10.0, recorded) : m_windowSeconds;
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
  // each value axis: the frame's height in its scale, kept in the base unit
  for (Axis &a : m_axes)
  {
    a.zoomMin = a.axis->pixelToCoord(frame.bottom()) * a.factor;
    a.zoomMax = a.axis->pixelToCoord(frame.top()) * a.factor;
  }
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

// The axis chosen for a value if its unit fits there. On Auto the axis its
// unit got by Auto, else the first free one, else an axis chosen for that
// unit: a value put on an axis of its own stays alone there while there is
// room. A value whose unit finds no axis is not drawn. The values with a
// choice come first, so Auto fills what is left.
void QcpGraph::assignAxes()
{
  for (Axis &a : m_axes)
    a.unit.clear();
  QList<bool> chosenAxis(m_axes.size(), false);
  m_notice.clear();
  auto fits = [this](int axis, const QString &unit)
  { return axis >= 0 && axis < m_axes.size() && (m_axes.at(axis).unit.isEmpty() || m_axes.at(axis).unit == unit); };
  QStringList left;
  for (int pass = 0; pass < 2; ++pass)
    for (Curve &c : m_curves)
    {
      const int chosen = m_ownStore ? kAxisAuto : m_valueAxes.value(c.value, kAxisAuto);
      if ((chosen == kAxisAuto) != (pass == 1))
        continue;
      c.axis = -1;
      if (c.unit.isEmpty())
        continue;   // no reading yet
      if (chosen != kAxisAuto)
      {
        c.axis = fits(chosen, c.unit) ? chosen : -1;
        if (c.axis >= 0)
          chosenAxis[c.axis] = true;
      }
      else
      {
        for (int i = 0; i < m_axes.size() && c.axis < 0; ++i)
          if (m_axes.at(i).unit == c.unit && !chosenAxis.at(i))
            c.axis = i;
        for (int i = 0; i < m_axes.size() && c.axis < 0; ++i)
          if (m_axes.at(i).unit.isEmpty())
            c.axis = i;
        for (int i = 0; i < m_axes.size() && c.axis < 0; ++i)
          if (m_axes.at(i).unit == c.unit)
            c.axis = i;
      }
      if (c.axis >= 0)
        m_axes[c.axis].unit = c.unit;
      else if (const QString unit = SiPrefix::displayText(c.unit); !left.contains(unit))
        left << unit;
    }
  if (!left.isEmpty())
    m_notice = tr("%1: no free axis").arg(left.join(", "));
}

bool QcpGraph::valueSpan(int axis, double from, double to, double &lo, double &hi) const
{
  bool any = false;
  const auto byTime = [](const QPointF &p, double t) { return p.x() < t; };
  for (const Curve &c : m_curves)
  {
    if (c.axis != axis)
      continue;
    for (auto it = std::lower_bound(c.points.cbegin(), c.points.cend(), from, byTime);
         it != c.points.cend() && it->x() <= to; ++it)
    {
      if (std::isnan(it->y()))
        continue;
      lo = any ? qMin(lo, it->y()) : it->y();
      hi = any ? qMax(hi, it->y()) : it->y();
      any = true;
    }
  }
  return any;
}

// The time axis over the window; each value axis over the frame zoomed on,
// the fixed range of its unit or the values shown, in the unit with the SI
// prefix that suits them. An axis without a unit is not shown, but the left one.
void QcpGraph::updateAxes(double &from, double &to)
{
  from = windowStart() / 1000.0;
  to = from + shownSeconds();
  m_plot->xAxis->setRange(from, to);
  // a label is about 70 pixels wide: fewer ticks on a narrow plot
  // with divisions up to 10 of them
  m_plot->xAxis->ticker()->setTickCount(qBound(2, m_plot->axisRect()->width() / 110, m_look.divisions ? 10 : 6));

  bool anyValue = false;
  for (int i = 0; i < m_axes.size(); ++i)
  {
    Axis &a = m_axes[i];
    a.axis->setVisible(i == 0 || !a.unit.isEmpty());
    double lo = 0, hi = 0;
    const bool any = valueSpan(i, from, to, lo, hi);
    anyValue = anyValue || any;
    if (m_zoomed)
    {
      lo = a.zoomMin;
      hi = a.zoomMax;
    }
    else if (const auto fixed = m_fixedRanges.constFind(a.unit); !a.unit.isEmpty() && fixed != m_fixedRanges.cend())
    {
      lo = fixed->first;
      hi = fixed->second;
    }
    else
    {
      // a little room above and below, the curve does not touch the frame
      const double room = (hi - lo) * 0.05;
      lo -= room;
      hi += room;
    }
    PlotAxis::Range range = PlotAxis::range(lo, hi, a.unit);
    // a scope's or a recorder's graticule: 8 divisions of a whole step on
    // every axis, so one grid is right for all; not for a range that is given
    const bool given = m_zoomed || (!a.unit.isEmpty() && m_fixedRanges.contains(a.unit));
    const bool divided = m_look.divisions && !given && (any || !a.unit.isEmpty());
    if (divided)
    {
      double low = range.minimum / range.factor, high = range.maximum / range.factor, step = 1;
      divide(low, high, step);
      range.minimum = low * range.factor;
      range.maximum = high * range.factor;
      QSharedPointer<QCPAxisTickerFixed> ticker(new QCPAxisTickerFixed);
      ticker->setTickStep(step);
      ticker->setScaleStrategy(QCPAxisTickerFixed::ssNone);
      a.axis->setTicker(ticker);
    }
    else if (a.divided)
      a.axis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
    a.divided = divided;
    setAxisRange(a.axis, range);
    a.factor = range.factor;
  }
  setCrosshairEnabled(anyValue && m_lookSettings.crosshair);
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
  if (m_outliersHidden > 0)
  {
    const QString hidden = QCoreApplication::translate("GraphWidget", "%n outlier(s) hidden", nullptr, m_outliersHidden);
    state = state.isEmpty() ? hidden : state + QStringLiteral(" · ") + hidden;
  }
  if (!m_notice.isEmpty())
    state = state.isEmpty() ? m_notice : state + "   " + m_notice;
  m_timeBar->setStateText(state);
}

double QcpGraph::timeAxisStart() const
{
  return m_plot->xAxis->range().lower;
}

double QcpGraph::timeAxisEnd() const
{
  return m_plot->xAxis->range().upper;
}

int QcpGraph::axisCount() const
{
  int n = 1;
  for (int i = 1; i < m_axes.size(); ++i)
    n += !m_axes.at(i).unit.isEmpty();
  return n;
}

QString QcpGraph::axisTitle(int i) const
{
  return i >= 0 && i < m_axes.size() ? m_axes.at(i).axis->label() : QString();
}

double QcpGraph::axisMin(int i) const
{
  return i >= 0 && i < m_axes.size() ? m_axes.at(i).axis->range().lower : 0;
}

double QcpGraph::axisMax(int i) const
{
  return i >= 0 && i < m_axes.size() ? m_axes.at(i).axis->range().upper : 0;
}

QString QcpGraph::stateText() const
{
  return m_timeBar->stateText();
}

QRect QcpGraph::plotRect() const
{
  return m_plot->axisRect()->rect();
}
