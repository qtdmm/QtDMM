// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GraphWidget: the values, their stores, the thinned points and the curves.
#include "ui/views/graphwidget.h"

#include "recording/recordingstore.h"
#include "recording/seriesdecimator.h"
#include "ui/views/graphband.h"
#include "ui/views/graphlegend.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/graphtext.h"   // curveName()
#include "ui/views/qcp.h"

#include <cmath>

#include <QResizeEvent>
#include <QSet>

GraphWidget::GraphWidget(QWidget *parent) : QcpPlotBase(parent)
{
  setMinimumSize(220, 180);
  setupAxes();
  setupInput();
  setupOverview();   // the bands under the curves
  setupOverlays();
  setupMarks();
  setupTheme();
  refreshPlot();
}

void GraphWidget::setSources(std::function<RecordingStore *(const QString &source)> storeOf,
                          std::function<QString(const QString &source)> nameOf)
{
  m_storeOf = std::move(storeOf);
  m_nameOf = std::move(nameOf);
  makeCurves();
}

void GraphWidget::sourcesChanged()
{
  makeCurves();
}

void GraphWidget::setValues(const QList<ValueRef> &values)
{
  if (values == m_values)
    return;
  m_values = values;
  makeCurves();
}

void GraphWidget::setValueAxes(const QHash<ValueRef, int> &axes)
{
  if (axes == m_valueAxes)
    return;
  m_valueAxes = axes;
  storeChanged();
}

void GraphWidget::setFixedRanges(const QMap<QString, QPair<double, double>> &ranges)
{
  if (ranges == m_fixedRanges)
    return;
  m_fixedRanges = ranges;
  redraw();
}

void GraphWidget::setStore(RecordingStore *store)
{
  if (store == m_ownStore)
    return;
  m_ownStore = store;
  makeCurves();
}

void GraphWidget::setSourceName(const QString &name)
{
  m_sourceName = name;
  updateTimeBar();
}

// One curve per value whose meter is open, or the main value of the store
// given; the stores are connected once each.
void GraphWidget::makeCurves()
{
  for (const Curve &c : std::as_const(m_curves))
    m_plot->removeGraph(c.graph);
  m_curves.clear();
  for (const QPointer<RecordingStore> &store : std::as_const(m_watched))
    if (store)
      disconnect(store, nullptr, this, nullptr);
  m_watched.clear();

  auto add = [this](const ValueRef &value, RecordingStore *store, const QString &meter)
  {
    Curve c;
    c.value = value;
    c.store = store;
    c.meter = meter;
    c.graph = m_plot->addGraph(m_plot->xAxis, m_axes.first().axis);
    // the points are thinned already, minimum and maximum of a column
    c.graph->setAdaptiveSampling(false);
    m_curves.append(c);
  };
  if (m_ownStore)
    add(ValueRef(0), m_ownStore, QString());
  else if (m_storeOf)
    for (const ValueRef &v : std::as_const(m_values))
      if (RecordingStore *store = m_storeOf(v.source))
        add(v, store, m_nameOf ? m_nameOf(v.source) : QString());

  RecordingStore *base = m_curves.isEmpty() ? nullptr : m_curves.first().store.data();
  if (base != m_store)
  {
    // another recording gives the time: at its end, no frame of the one before
    m_store = base;
    m_followEnd = true;
    m_zoomed = false;
  }
  for (const Curve &c : std::as_const(m_curves))
  {
    RecordingStore *store = c.store;
    if (m_watched.contains(store))
      continue;
    m_watched.append(store);
    connect(store, &RecordingStore::appended, this, [this, store](bool shifted) { mainAppended(store, shifted); });
    connect(store, &RecordingStore::extraAppended, this, [this, store](int index) { extraAppended(store, index); });
    connect(store, &RecordingStore::extrasChanged, this, &GraphWidget::storeChanged);
    connect(store, &RecordingStore::cleared, this, &GraphWidget::storeChanged);
    connect(store, &RecordingStore::loaded, this, &GraphWidget::storeChanged);
    connect(store, &RecordingStore::stateChanged, this, &GraphWidget::storeChanged);
    // also while the picture is held: a mark is named by its place among them
    connect(store, &RecordingStore::marksChanged, this, [this]
    {
      updateMarks();
      m_plot->replot(QCustomPlot::rpQueuedReplot);
    });
    connect(store, &QObject::destroyed, this, &GraphWidget::storeGone);
  }
  // the time goes on without readings: the window follows
  if (m_store)
    connect(m_store, &RecordingStore::progressChanged, this, &GraphWidget::scheduleRefresh);
  storeChanged();
}

// Only what the graph holds itself: whoever gives the stores may be in the
// middle of taking this one away.
void GraphWidget::storeGone(QObject *store)
{
  for (int i = int(m_curves.size()) - 1; i >= 0; --i)
    if (!m_curves.at(i).store || m_curves.at(i).store.data() == store)
    {
      m_plot->removeGraph(m_curves.at(i).graph);
      m_curves.removeAt(i);
    }
  m_watched.removeIf([store](const QPointer<RecordingStore> &s) { return !s || s.data() == store; });
  RecordingStore *base = m_curves.isEmpty() ? nullptr : m_curves.first().store.data();
  if (base != m_store)
  {
    m_store = base;
    m_followEnd = true;
    m_zoomed = false;
    if (base)
      connect(base, &RecordingStore::progressChanged, this, &GraphWidget::scheduleRefresh, Qt::UniqueConnection);
  }
  storeChanged();
}

QList<const RecordingSeries *> GraphWidget::seriesOf(const Curve &curve) const
{
  QList<const RecordingSeries *> parts;
  if (!curve.store)
    return parts;
  if (curve.value.id == 0)
    parts << &curve.store->series();
  else
    for (int i = 0; i < curve.store->extraCount(); ++i)
      if (curve.store->extra(i).id == curve.value.id)
        parts << &curve.store->extra(i);
  return parts;
}

void GraphWidget::setHold(bool hold)
{
  if (hold == this->hold())
    return;
  cancelRefresh();
  QcpPlotBase::setHold(hold);
  // what came meanwhile shows when it is released
  if (!hold)
    refreshPlot();
}

void GraphWidget::clear()
{
  cancelRefresh();
  m_dirty = true;
  refreshPlot();
}

void GraphWidget::refreshNow()
{
  cancelRefresh();
  refreshPlot();
}

void GraphWidget::resizeEvent(QResizeEvent *event)
{
  QcpPlotBase::resizeEvent(event);
  layoutStats();
  scheduleRefresh();   // another width: other buckets
}

qint64 GraphWidget::bucketNow() const
{
  if (!m_store)
    return 1;
  const qint64 shown = qMin<qint64>(qint64(m_windowSeconds) * 1000, m_store->duration() - m_store->origin());
  const int width = m_plot->axisRect()->width();
  return SeriesDecimator::bucketSize(shown, width > 0 ? width : this->width());
}

void GraphWidget::rebuildPoints()
{
  m_dirty = false;
  m_bucket = bucketNow();
  for (Curve &c : m_curves)
    rebuildCurve(c);
  assignAxes();
  for (const Curve &c : std::as_const(m_curves))
  {
    c.graph->setVisible(c.axis >= 0);
    c.graph->setValueAxis(m_axes.at(qMax(0, c.axis)).axis);
  }
  styleCurves();
  updateLegend();
}

// The series of the value one after the other (a second display that went
// from Hz to % began a new one), a gap between them, on the graph's time.
void GraphWidget::rebuildCurve(Curve &curve)
{
  curve.points.clear();
  curve.tail = 0;
  curve.newest = nullptr;
  curve.unit.clear();
  curve.label.clear();
  if (!curve.store || !m_store)
    return;
  const QList<const RecordingSeries *> parts = seriesOf(curve);
  if (parts.isEmpty())
    return;
  curve.newest = parts.last();
  // the newest series names the unit; a loaded file's main value has only the store's
  for (const RecordingSeries *s : parts)
    if (!s->unit.isEmpty())
      curve.unit = s->unit;
  if (curve.unit.isEmpty() && curve.value.id == 0)
    curve.unit = curve.store->unit();
  curve.label = curveName(*curve.newest, curve.unit);
  curve.shift = curve.store == m_store ? 0 : m_store->offsetOf(*curve.store) / 1000.0;
  QList<QPointF> points;
  for (const RecordingSeries *s : parts)
  {
    if (s->isEmpty())
      continue;
    if (!curve.points.isEmpty())
      curve.points.append(QPointF(s->first().t / 1000.0 + curve.shift, qQNaN()));
    points.clear();
    curve.tail = SeriesDecimator::append(*s, 0, s->count() - 1, m_bucket, points, decimation(curve, *s));
    for (const QPointF &p : std::as_const(points))
      curve.points.append(QPointF(p.x() + curve.shift, p.y()));
  }
}

SeriesDecimator::Options GraphWidget::decimation(const Curve &curve, const RecordingSeries &series) const
{
  SeriesDecimator::Options options;
  if (m_outliers.active())
    options.hidden = [rule = m_outliers, unit = curve.unit, &series](int i) { return GraphOutliers::isOutlier(rule, series, i, unit); };
  return options;
}

void GraphWidget::appendTail(Curve &curve, const RecordingSeries &series)
{
  const int last = series.count() - 1;
  const int first = SeriesDecimator::bucketStart(series, last, m_bucket);
  // A spike is one only with the reading after it: the reading before the
  // newest, drawn already in an earlier bucket, is an outlier now - the
  // curve anew with the next picture. Rare: only when one turns up.
  if (m_outliers.active() && last - 1 >= 0 && last - 1 < first && GraphOutliers::isOutlier(m_outliers, series, last - 1, curve.unit))
  {
    m_dirty = true;
    return;
  }
  // the bucket was there before this reading: its points go for the new ones
  if (first < last)
    curve.points.remove(curve.points.size() - curve.tail, curve.tail);
  QList<QPointF> points;
  curve.tail = SeriesDecimator::pointsOf(series, first, last, points, decimation(curve, series));
  for (const QPointF &p : std::as_const(points))
    curve.points.append(QPointF(p.x() + curve.shift, p.y()));
}

void GraphWidget::mainAppended(RecordingStore *store, bool shifted)
{
  // older readings went, or all is to be built anyway: with the next picture
  if (!shifted && !m_dirty)
    for (Curve &c : m_curves)
    {
      if (c.store != store || c.value.id != 0)
        continue;
      // the first reading names the unit, and with it the axis
      if (c.unit.isEmpty() || c.newest != &store->series())
        m_dirty = true;
      else
        appendTail(c, store->series());
    }
  else
    m_dirty = true;
  scheduleRefresh();
}

void GraphWidget::extraAppended(RecordingStore *store, int index)
{
  if (!m_dirty && index >= 0 && index < store->extraCount())
  {
    const RecordingSeries &series = store->extra(index);
    for (Curve &c : m_curves)
    {
      if (c.store != store || c.value.id != series.id || c.value.id == 0)
        continue;
      // a series that began, or one without a unit yet: all anew
      if (c.newest != &series || c.unit.isEmpty() || c.unit != series.unit)
        m_dirty = true;
      else
        appendTail(c, series);
    }
  }
  scheduleRefresh();
}

void GraphWidget::storeChanged()
{
  m_dirty = true;
  scheduleRefresh();
}

void GraphWidget::refreshPlot()
{
  if (m_followAll)
    fitAll(true);   // before the buckets: they follow the window
  if (m_dirty || bucketNow() != m_bucket)
    rebuildPoints();
  double from = 0, to = 0;
  updateAxes(from, to);
  drawCurves(from, to);
  updateOverview();
  updateLevels();
  updateCursors();
  scheduleStats();
  scheduleOutlierCount();
  updateScrollBar();
  updateTimeBar();
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void GraphWidget::redraw()
{
  if (hold())
    return;
  cancelRefresh();
  refreshPlot();
}

// The points of the time shown and one on either side, so the lines run to
// the edges; the rest of the recording is not handed to the plot.
void GraphWidget::drawCurves(double from, double to)
{
  const auto byTime = [](const QPointF &p, double t) { return p.x() < t; };
  QVector<double> x, y;
  const double width = m_plot->axisRect()->width();
  for (Curve &c : m_curves)
  {
    x.clear();
    y.clear();
    c.banded = false;
    if (c.axis >= 0)
    {
      // more readings than columns: a band says the same and costs less than
      // the zigzag (updateOverview()). The readings, not the points thinned
      // to the columns: those are never many more than the columns.
      int readings = 0;
      for (const RecordingSeries *s : seriesOf(c))
        readings += s->lowerBound(qint64((to - c.shift) * 1000)) - s->lowerBound(qint64((from - c.shift) * 1000));
      c.banded = width > 0 && readings > width * GraphBand::kDensePoints
                 && turnsIn(c, from, to) * GraphBand::kTurnColumns > width;
    }
    if (c.axis >= 0 && !c.banded)
    {
      auto begin = std::lower_bound(c.points.cbegin(), c.points.cend(), from, byTime);
      auto end = std::lower_bound(begin, c.points.cend(), to, byTime);
      if (begin != c.points.cbegin())
        --begin;
      if (end != c.points.cend())
        ++end;
      const double factor = m_axes.at(c.axis).factor;
      x.reserve(end - begin);
      y.reserve(end - begin);
      for (auto it = begin; it != end; ++it)
      {
        x.append(it->x());
        y.append(it->y() / factor);
      }
    }
    styleDensity(c, int(x.size()));
    c.graph->setData(x, y, true);
  }
}

// One entry per curve drawn; one curve alone needs none. With several meters
// their names come first and go when it gets narrow.
void GraphWidget::updateLegend()
{
  QList<GraphLegend::Entry> entries;
  QSet<QString> sources;
  for (const Curve &c : std::as_const(m_curves))
    sources.insert(c.value.source);
  if (m_curves.size() > 1)
    for (int i = 0; i < m_curves.size(); ++i)
    {
      const Curve &c = m_curves.at(i);
      if (c.axis < 0)
        continue;
      const QString unit = SiPrefix::displayText(c.unit);
      const QString label = c.label.isEmpty() ? c.meter : c.label;
      const QString full = sources.size() > 1 && !c.meter.isEmpty() && !c.label.isEmpty() ? c.meter + " · " + c.label : label;
      entries.append({ curvePen(i), { full, label, label, unit.isEmpty() ? label : unit } });
    }
  m_timeBar->setLegend(entries, m_look.theme.labels);
}

QList<QPointF> GraphWidget::curvePoints(int i) const
{
  return m_curves.value(i).points;
}

QList<QPointF> GraphWidget::drawnPoints(int i) const
{
  QList<QPointF> points;
  if (i < 0 || i >= m_curves.size())
    return points;
  const QCPGraph *graph = m_curves.at(i).graph;
  for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it)
    points.append(QPointF(it->key, it->value));
  return points;
}

int GraphWidget::curveAxis(int i) const
{
  return i >= 0 && i < m_curves.size() ? m_curves.at(i).axis : -1;
}


QString GraphWidget::legendText() const
{
  return m_timeBar->legendText();
}

// The turns of the curve in the time shown, of at least kTurnPixels on its
// axis: a zigzag has one every few columns, a smooth curve two a period.
int GraphWidget::turnsIn(const Curve &curve, double from, double to) const
{
  const Axis &axis = m_axes.at(curve.axis);
  const double span = axis.axis->range().size() * axis.factor;
  const double height = m_plot->axisRect()->height();
  if (!(span > 0) || height <= 0)
    return 0;
  const double perUnit = height / span;   // pixels per base unit
  const auto byTime = [](const QPointF &p, double t) { return p.x() < t; };
  auto it = std::lower_bound(curve.points.cbegin(), curve.points.cend(), from, byTime);
  const auto end = std::lower_bound(it, curve.points.cend(), to, byTime);
  int turns = 0, direction = 0;
  double extreme = qQNaN();
  for (; it != end; ++it)
  {
    const double y = it->y() * perUnit;
    if (std::isnan(y))
    {
      extreme = qQNaN();   // a gap: a new start after it
      direction = 0;
      continue;
    }
    if (std::isnan(extreme))
      extreme = y;
    else if (direction == 0)
    {
      if (std::abs(y - extreme) >= GraphBand::kTurnPixels)
      {
        direction = y > extreme ? 1 : -1;
        extreme = y;
      }
    }
    else if ((y - extreme) * direction > 0)
      extreme = y;   // on in the same direction
    else if (std::abs(y - extreme) >= GraphBand::kTurnPixels)
    {
      ++turns;
      direction = -direction;
      extreme = y;
    }
  }
  return turns;
}
