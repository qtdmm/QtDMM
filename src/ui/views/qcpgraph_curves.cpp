// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: the store, the thinned points and the curve.
#include "ui/views/qcpgraph.h"

#include "recording/recordingstore.h"
#include "recording/seriesdecimator.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/qcp.h"

#include <QResizeEvent>

QcpGraph::QcpGraph(QWidget *parent) : QcpPlotBase(parent)
{
  setMinimumSize(220, 180);
  setupAxes();
  setupCurve();
  setupInput();
  refreshPlot();
}

void QcpGraph::setupCurve()
{
  m_curve = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
  // the points are thinned already, minimum and maximum of a column
  m_curve->setAdaptiveSampling(false);
}

void QcpGraph::setStore(RecordingStore *store)
{
  if (store == m_store)
    return;
  if (m_store)
    disconnect(m_store, nullptr, this, nullptr);
  m_store = store;
  if (store)
  {
    connect(store, &RecordingStore::appended, this, &QcpGraph::appendPoint);
    connect(store, &RecordingStore::cleared, this, &QcpGraph::storeChanged);
    connect(store, &RecordingStore::loaded, this, &QcpGraph::storeChanged);
    connect(store, &RecordingStore::stateChanged, this, &QcpGraph::storeChanged);
    // the time goes on without readings: the window follows
    connect(store, &RecordingStore::progressChanged, this, &QcpGraph::scheduleRefresh);
    connect(store, &QObject::destroyed, this, &QcpGraph::storeChanged);
  }
  // another recording: at its end, no frame of the one before
  m_followEnd = true;
  m_zoomed = false;
  storeChanged();
}

void QcpGraph::setSourceName(const QString &name)
{
  m_sourceName = name;
  updateTimeBar();
}

void QcpGraph::setTheme(const PlotTheme &theme)
{
  m_curve->setPen(QPen(theme.data.isValid() ? theme.data : theme.labels, 1));
  // the rows above and below the plot on its background
  QPalette p = palette();
  p.setBrush(QPalette::Window, theme.background.style() == Qt::NoBrush ? p.base() : theme.background);
  setPalette(p);
  setAutoFillBackground(true);
  m_timeBar->setTextColor(theme.labels);
  QcpPlotBase::setTheme(theme);
}

void QcpGraph::setHold(bool hold)
{
  if (hold == this->hold())
    return;
  cancelRefresh();
  QcpPlotBase::setHold(hold);
  // what came meanwhile shows when it is released
  if (!hold)
    refreshPlot();
}

void QcpGraph::clear()
{
  cancelRefresh();
  m_dirty = true;
  refreshPlot();
}

void QcpGraph::refreshNow()
{
  cancelRefresh();
  refreshPlot();
}

void QcpGraph::resizeEvent(QResizeEvent *event)
{
  QcpPlotBase::resizeEvent(event);
  scheduleRefresh();   // another width: other buckets
}

qint64 QcpGraph::bucketNow() const
{
  if (!m_store)
    return 1;
  const qint64 shown = qMin<qint64>(qint64(m_windowSeconds) * 1000, m_store->duration() - m_store->origin());
  const int width = m_plot->axisRect()->width();
  return SeriesDecimator::bucketSize(shown, width > 0 ? width : this->width());
}

void QcpGraph::rebuildPoints()
{
  m_points.clear();
  m_tail = 0;
  m_dirty = false;
  m_bucket = bucketNow();
  if (!m_store || m_store->count() == 0)
    return;
  const RecordingSeries &series = m_store->series();
  m_points.reserve(2 * series.count() + 2);
  m_tail = SeriesDecimator::append(series, 0, series.count() - 1, m_bucket, m_points);
}

void QcpGraph::appendPoint(bool shifted)
{
  // older readings went, or all is to be built anyway: with the next picture
  if (shifted || m_dirty)
  {
    m_dirty = true;
    scheduleRefresh();
    return;
  }
  const RecordingSeries &series = m_store->series();
  const int last = series.count() - 1;
  const int first = SeriesDecimator::bucketStart(series, last, m_bucket);
  // the bucket was there before this reading: its points go for the new ones
  if (first < last)
    m_points.remove(m_points.size() - m_tail, m_tail);
  m_tail = SeriesDecimator::pointsOf(series, first, last, m_points);
  scheduleRefresh();
}

void QcpGraph::storeChanged()
{
  m_dirty = true;
  scheduleRefresh();
}

void QcpGraph::refreshPlot()
{
  if (m_followAll)
    fitAll(true);   // before the buckets: they follow the window
  if (m_dirty || bucketNow() != m_bucket)
    rebuildPoints();
  double from = 0, to = 0, factor = 1;
  updateAxes(from, to, factor);
  drawCurve(from, to, factor);
  updateScrollBar();
  updateTimeBar();
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QcpGraph::redraw()
{
  if (hold())
    return;
  cancelRefresh();
  refreshPlot();
}

// The points of the time shown and one on either side, so the line runs to
// the edges; the rest of the recording is not handed to the plot.
void QcpGraph::drawCurve(double from, double to, double factor)
{
  const auto byTime = [](const QPointF &p, double t) { return p.x() < t; };
  auto begin = std::lower_bound(m_points.cbegin(), m_points.cend(), from, byTime);
  auto end = std::lower_bound(begin, m_points.cend(), to, byTime);
  if (begin != m_points.cbegin())
    --begin;
  if (end != m_points.cend())
    ++end;
  QVector<double> x, y;
  x.reserve(end - begin);
  y.reserve(end - begin);
  for (auto it = begin; it != end; ++it)
  {
    x.append(it->x());
    y.append(it->y() / factor);
  }
  m_curve->setData(x, y, true);
}

QList<QPointF> QcpGraph::drawnPoints() const
{
  QList<QPointF> points;
  for (auto it = m_curve->data()->constBegin(); it != m_curve->data()->constEnd(); ++it)
    points.append(QPointF(it->key, it->value));
  return points;
}
