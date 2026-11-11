//======================================================================
// File:		graphwidget_curves.cpp (from dmmgraph.cpp)
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:45:35 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================
//
// GraphWidget, part: building the curves: buckets, the integral, the envelope, the other values and the legend.
// The class is in graphwidget.h, its other parts in graphwidget*.cpp.

#include <QtGui>
#include <QtWidgets>
#include <QPen>
#include <cmath>
#include <climits>

#include "ui/views/graphlegend.h"
#include "ui/views/graphstatestrip.h"
#include "ui/views/graphwidget.h"
#include "ui/views/graphwidget_p.h"
#include "ui/views/graphminimap.h"

QPainterPath GraphWidget::envelopePath() const
{
  return m_envelope->isVisible() ? m_envelope->path() : QPainterPath();
}

// Columns of one pixel over the plot (and kEnvelopeColumns beyond each
// edge, so the band runs out of the plot as the curve does): the lowest
// and highest of the curve as drawn in each, its points and the lines
// between them - readings further apart than the band is wide would leave
// it a staircase. The curve has a point for every bucket, its minimum and
// maximum, the buckets themselves a column wide, so this reads a few
// points per column, not the readings. A gap (NaN) parts the band as it
// parts the curve. Each column gets the extremes of the columns within
// kEnvelopeColumns of it on its side of a gap.
void GraphWidget::updateEnvelope()
{
  updateDenseBands();
  updateCrossings();   // new readings, another window
  if (m_outlierMode != OutliersShown && !m_outlierCountPending)
  {
    m_outlierCountPending = true;
    QTimer::singleShot(250, this, [this]
    {
      m_outlierCountPending = false;
      countOutliers();
    });
  }
  const QPainterPath path = m_envelopeOn ? columnBand(referencePoints(), m_yAxis->min(), m_yAxis->max(),
                                                      kEnvelopeColumns)
                                         : QPainterPath();
  m_envelope->setPath(path);
  m_envelope->setVisible(!path.isEmpty());
}

QPainterPath GraphWidget::columnBand(const QList<QPointF> &pts, double y0, double y1, int k) const
{
  const QRectF plot = m_chart->plotArea();
  const double x0 = m_xAxis->min(), x1 = m_xAxis->max();
  if (pts.isEmpty() || plot.width() < 2 || plot.height() <= 0 || !(x1 > x0) || !(y1 > y0))
    return QPainterPath();
  // the columns counted from time 0, not from the left edge: Live moving
  // on by a part of a pixel leaves each point in its column, the band only
  // moves along (else it flickers, its columns cut anew every frame)
  const double perColumn = (x1 - x0) / plot.width();
  const double first = std::floor(x0 / perColumn) - k;
  const int columns = int(plot.width()) + 2 * k + 2;
  const double from = first * perColumn, to = from + columns * perColumn;
  struct Column
  {
    double lo = qInf(), hi = -qInf();
    int run = -1;   ///< which stretch between gaps; -1 no point
  };
  QList<Column> col(columns);
  auto take = [&](int c, double lo, double hi, int r)
  {
    Column &cc = col[c];
    cc.lo = qMin(cc.lo, lo);
    cc.hi = qMax(cc.hi, hi);
    cc.run = r;
  };
  auto columnOf = [&](double x) { return qBound(0, int((x - from) / perColumn), columns - 1); };
  // the point before the window too: its line runs into it
  auto it = std::lower_bound(pts.cbegin(), pts.cend(), from, [](const QPointF &p, double x) { return p.x() < x; });
  if (it != pts.cbegin())
    --it;
  int run = 0;
  const QPointF *prev = nullptr;
  for (; it != pts.cend(); ++it)
  {
    if (std::isnan(it->y()))
    {
      // a gap parts the curve and the band
      if (it->x() > to)
        break;
      run++;
      prev = nullptr;
      continue;
    }
    if (it->x() >= from)
    {
      const int c = columnOf(it->x());
      take(c, it->y(), it->y(), run);
      // the line from the point before: in each column it crosses, the
      // part of it there
      if (prev)
        for (int j = columnOf(prev->x()); j <= c; ++j)
        {
          const double xa = qMax(prev->x(), from + j * perColumn), xb = qMin(it->x(), from + (j + 1) * perColumn);
          const double dx = it->x() - prev->x();
          auto yAt = [&](double x) { return dx > 0 ? prev->y() + (it->y() - prev->y()) * (x - prev->x()) / dx : it->y(); };
          take(j, qMin(yAt(xa), yAt(xb)), qMax(yAt(xa), yAt(xb)), run);
        }
    }
    if (it->x() > to)
      break;
    prev = &*it;
  }
  auto yOf = [&](double v) { return plot.bottom() - (v - y0) / (y1 - y0) * plot.height(); };
  auto xOf = [&](int c) { return plot.left() + (first + c + 0.5) - x0 / perColumn; };
  QPainterPath path;
  QList<QPointF> upper, lower;
  auto flush = [&]
  {
    if (upper.size() > 1)
    {
      QPolygonF poly(upper);
      for (qsizetype i = lower.size() - 1; i >= 0; --i)
        poly << lower[i];
      path.addPolygon(poly);
      path.closeSubpath();
    }
    upper.clear();
    lower.clear();
  };
  int lastRun = -1;
  for (int c = 0; c < columns; ++c)
  {
    if (col[c].run < 0)
      continue;
    if (col[c].run != lastRun)
      flush();
    lastRun = col[c].run;
    double lo = col[c].lo, hi = col[c].hi;
    for (int j = qMax(0, c - k); j <= qMin(columns - 1, c + k); ++j)
      if (col[j].run == col[c].run)
      {
        lo = qMin(lo, col[j].lo);
        hi = qMax(hi, col[j].hi);
      }
    upper << QPointF(xOf(c), yOf(hi));
    lower << QPointF(xOf(c), yOf(lo));
  }
  flush();
  QPainterPath clip;
  clip.addRect(plot);
  return path.intersected(clip);
}


QGraphicsPathItem *GraphWidget::newBand()
{
  auto *band = new QGraphicsPathItem(m_chart);
  band->setZValue(4);   // as the min/max band: under the curves
  band->setVisible(false);
  return band;
}

void GraphWidget::updateDenseBands()
{
  for (int i = 0; i < int(m_extraCurves.size()); ++i)
  {
    ExtraCurve &c = m_extraCurves[size_t(i)];
    // the pen of its value; a curve beyond the store's values is not shown
    const int id = i < m_store->extraCount() ? m_store->extra(i).id : i + 1;
    updateDenseBand(c.band, c.line.get(), c.shown, c.points, c.side, extraPen(id));
  }
  for (int i = 0; i < int(m_otherCurves.size()); ++i)
  {
    OtherCurve &c = m_otherCurves[size_t(i)];
    updateDenseBand(c.band, c.line.get(), c.shown, c.points, c.side, otherPen(i));
  }
}

void GraphWidget::updateDenseBand(QGraphicsPathItem *band, GapLine *line, bool shown, const QList<QPointF> &points,
                                  int side, const QPen &pen)
{
  if (!band || !line)
    return;
  const double x0 = m_xAxis->min(), x1 = m_xAxis->max();
  const double width = m_chart->plotArea().width();
  bool dense = false;
  if (shown && width > 0)
  {
    // the points in the window: they are in time order
    auto from = std::lower_bound(points.cbegin(), points.cend(), x0, [](const QPointF &p, double x) { return p.x() < x; });
    auto to = std::upper_bound(from, points.cend(), x1, [](double x, const QPointF &p) { return x < p.x(); });
    dense = double(to - from) * kDensePixels > width;
  }
  QPainterPath path;
  if (dense)
  {
    const QValueAxis *axis = yAxisOf(side);
    path = columnBand(points, axis->min(), axis->max(), kDenseColumns);
  }
  if (!path.isEmpty())
  {
    QColor fill = pen.color();
    fill.setAlpha(64);
    band->setBrush(fill);
    band->setPen(pen);
    band->setPath(path);
  }
  band->setVisible(!path.isEmpty());
  line->setVisible(shown && path.isEmpty());
}
// ms per pixel column, rounded up to a power of two. Counted over the window
// or, while the recording does not fill it yet, over what is there: a short
// recording in a long window is drawn as it is. Readings further apart than a
// column are a bucket each. The power of two: a growing recording builds the
// curve anew only when the buckets double, not every few readings (as the
// overview, GraphMinimap); a bucket is then one to two columns wide.
qint64 GraphWidget::bucketSize() const
{
  const double plotWidth = m_chart->plotArea().width();
  const int columns = qMax(1, int(plotWidth > 0 ? plotWidth : width()));
  const qint64 shown = qMin<qint64>(qint64(m_windowSeconds) * 1000, m_store->duration() - m_store->origin());
  const qint64 need = (shown + columns - 1) / columns;
  qint64 ms = 1;
  while (ms < need)
    ms *= 2;
  return ms;
}

// The index of the first reading in the bucket of reading @p i. The buckets
// are columns of time counted from the start of the recording: while the
// window scrolls, a reading stays in its bucket and the drawn minima and
// maxima stay put instead of jittering with every new reading.
int GraphWidget::bucketStart(int i) const
{
  const RecordingSeries &series = m_store->series();
  const qint64 begin = series.at(i).t / m_bucket * m_bucket;
  return qMin(i, series.lowerBound(begin));
}

// The points of the readings first..last: the reading itself, or with more
// than one the minimum and the maximum in time order, so a spike survives
// the thinning. The store keeps every reading; only the drawing is thinned.
// Readings without a value (NaN: overload, stale) do not count; a bucket of
// nothing else is one NaN point, the gap, and one that ends where the meter
// fell silent ends with it.
int GraphWidget::pointsOf(const RecordingSeries &series, int first, int last, bool integral, QList<QPointF> &out) const
{
  auto value = [&](int i)
  {
    const RawPoint &p = series.at(i);
    return integral ? m_integrationOffset + p.integral * m_integrationScale : p.value;
  };
  auto x = [&](int i) { return series.at(i).t / 1000.0; };
  // hidden outliers are passed over: the line runs from the reading before
  // to the one after them, no gap
  const bool hide = !integral && m_outlierMode != OutliersShown;
  auto hidden = [&](int i) { return hide && isOutlier(series, i); };
  bool gap = false;
  while (first <= last && (std::isnan(value(first)) || hidden(first)))
    gap = std::isnan(value(first++)) || gap;
  if (first > last)
  {
    if (!gap)
      return 0;   // only hidden readings
    out.append(QPointF(x(last), qQNaN()));
    return 1;
  }
  int lo = first, hi = first;
  double loValue = value(first), hiValue = loValue;
  for (int i = first + 1; i <= last; i++)
  {
    const double v = value(i);
    if (std::isnan(v) || hidden(i))
      continue;
    if (v < loValue)
    {
      loValue = v;
      lo = i;
    }
    if (v > hiValue)
    {
      hiValue = v;
      hi = i;
    }
  }
  // a bucket that ends where the meter fell silent: the gap goes on to the
  // next reading, in a later bucket - the line must not bridge it (a sensor
  // silent for minutes was drawn as a straight line whenever its last value
  // and the gap fell into one column). An overload comes at the meter's
  // pace and is as short as a reading.
  const bool gapAfter = std::isnan(value(last)) && series.at(last).quality == Quality::Stale;
  int n = 1;
  if (lo == hi)
    out.append(QPointF(x(lo), loValue));
  else
  {
    if (lo > hi)
    {
      qSwap(lo, hi);
      qSwap(loValue, hiValue);
    }
    out.append(QPointF(x(lo), loValue));
    out.append(QPointF(x(hi), hiValue));
    n = 2;
  }
  if (gapAfter)
  {
    out.append(QPointF(x(last), qQNaN()));
    n++;
  }
  return n;
}

void GraphWidget::rebuildSeries()
{
  m_rebuilds++;
  QList<QPointF> points;
  QList<QPointF> intPoints;
  const int count = m_store->count();
  m_bucket = bucketSize();
  points.reserve(2 * count + 2);
  intPoints.reserve(2 * count + 2);

  m_tailData = m_tailInt = 0;
  const RecordingSeries &series = m_store->series();
  for (int first = 0; first < count;)
  {
    // the readings of this bucket
    const qint64 end = (series.at(first).t / m_bucket + 1) * m_bucket;
    const int last = qMin(series.lowerBound(end), count) - 1;
    m_tailData = pointsOf(m_store->series(), first, last, false, points);
    if (m_intValueId == 0 && m_showIntegration)
      m_tailInt = pointsOf(m_store->series(), first, last, true, intPoints);
    first = last + 1;
  }

  m_dataLine->setMarkers(pointsShown() ? m_dataPoints : nullptr);
  m_dataLine->replace(points);
  m_curvePts = points;
  // the integral of another value: rebuildExtras()
  m_intLine->setMarkers(intPointsShown() ? m_intPoints : nullptr);
  m_intLine->replace(intPoints);
  rebuildExtras();   // the same buckets
}

// The newest reading went into the series: a new bucket adds its point, one
// that is still filling replaces the points it had.
void GraphWidget::appendToSeries()
{
  const int count = m_store->count();
  const int first = bucketStart(count - 1);
  const bool filling = first != count - 1;
  if (laterOutlier(m_store->series(), first))
  {
    rebuildSeries();
    return;
  }
  if (filling)
  {
    m_dataLine->removeLast(m_tailData);
    m_curvePts.resize(qMax<qsizetype>(0, m_curvePts.size() - m_tailData));
  }
  QList<QPointF> points;
  m_tailData = pointsOf(m_store->series(), first, count - 1, false, points);
  m_dataLine->append(points);
  m_curvePts.append(points);
  if (m_intValueId == 0 && m_showIntegration)   // else onExtraAppended()
    appendIntegral(m_store->series(), first, count - 1, filling);
}

// A spike is one only with the reading after it (and a reading's
// neighbours grow): the reading before the newest, drawn already in a bucket
// before @p first, is an outlier now - the curve is drawn anew. Rare: only
// when one turns up.
bool GraphWidget::laterOutlier(const RecordingSeries &series, int first) const
{
  const int before = series.count() - 2;
  return m_outlierMode != OutliersShown && before >= 0 && before < first && isOutlier(series, before);
}

// The integral's points of readings first..last of @p series, in place of
// the ones they had while the bucket was @p filling.
void GraphWidget::appendIntegral(const RecordingSeries &series, int first, int last, bool filling)
{
  if (filling)
    m_intLine->removeLast(m_tailInt);
  QList<QPointF> intPoints;
  m_tailInt = pointsOf(series, first, last, true, intPoints);
  m_intLine->append(intPoints);
}

void GraphWidget::setIntegrationValue(int id)
{
  id = qMax(0, id);
  if (id == m_intValueId)
    return;
  m_intValueId = id;
  // the curve on the main value's axis until its value says otherwise
  integralOnAxis(m_yAxis);
  rebuildSeries();
}

void GraphWidget::integralOnAxis(QAbstractAxis *axis)
{
  m_intLine->setAxes({ m_xAxis, axis });
  for (QAbstractAxis *a : m_intPoints->attachedAxes())
    if (a != m_xAxis && a != axis)
      m_intPoints->detachAxis(a);
  if (!m_intPoints->attachedAxes().contains(axis))
    m_intPoints->attachAxis(axis);
}

void GraphWidget::onAppended(bool shifted)
{
  const RawPoint &p = m_store->series().last();
  scheduleStats();   // a new reading: the statistics, at most 4 times a second
  if (m_stateStrip)
    m_stateStrip->update();   // and its state, when the window stands still

  // "All": the window grows once the recording fills it. The new window
  // rebuilds the series with this reading in it: then it must not be
  // appended again
  // A window much longer than what there is (Live starts with All, the
  // window from the settings) comes down to it.
  const int rebuilds = m_rebuilds;
  const qint64 recorded = m_store->duration() - m_store->origin();
  const qint64 window = qint64(m_windowSeconds) * 1000;
  if (m_followAll && (recorded >= window || window > 2 * qMax<qint64>(10000, recorded)))
    requestAll(true);
  const bool rebuilt = m_rebuilds != rebuilds;
  {
    // the scroll bar's own move of the axis would come once more below
    const QSignalBlocker block(scrollbar);
    updateScrollRange();
  }
  // a full store, Live or a recording seen at its end: the window moves on
  // with what it keeps
  if (m_store->origin() > 0 || m_store->state() == RecordingStore::Live || following())
  {
    updateXAxisRange();
    updateMarkPositions();
  }

  const bool resFlag = m_autoScale && m_mainShown && growScale(p.value, m_scaleMin, m_scaleMax);

  if (shifted || bucketSize() != m_bucket)
    rebuildSeries();
  else if (!rebuilt)
    appendToSeries();

  if (resFlag)
  {
    setYRange(m_scaleMin, m_scaleMax);
    updateThresholdLinePositions();
  }
  else if (m_referenceKind == RefMean || m_referenceKind == RefMin || m_referenceKind == RefMax)
    updateReferenceLine();
  if (m_cursorT[0] >= 0 || m_cursorT[1] >= 0)
    updateTimeCursors();
  updateEnvelope();
}

void GraphWidget::rebuildExtras()
{
  const int n = m_store->extraCount();
  // the units: the main value's on the left; on the right axes the units of
  // the lowest values in others (by number, not by which came first), their
  // newest series - a value with the axis chosen has the say there first,
  // in any unit. The integrated value has its axis also while it is hidden
  const QString main = leftUnit();
  QMap<int, QString> units;   // the values on an axis, by number: their newest unit
  for (int i = 0; i < n; ++i)
  {
    const RecordingSeries &s = m_store->extra(i);
    const bool onAxis = m_shownValues.contains(s.id) || (m_intValueId > 0 && s.id == m_intValueId);
    if (onAxis && !s.unit.isEmpty())
      units[s.id] = s.unit;
  }
  for (SideAxis &a : m_right)
    a.unit.clear();
  for (auto it = units.cbegin(); it != units.cend(); ++it)
    if (const int side = sideOf(valueAxis(it.key())); side > 0 && m_right[size_t(side - 1)].unit.isEmpty())
      m_right[size_t(side - 1)].unit = it.value();
  for (auto it = units.cbegin(); it != units.cend(); ++it)
  {
    if (valueAxis(it.key()) != AxisAuto || it.value() == main || it.value() == m_right[0].unit
        || it.value() == m_right[1].unit)
      continue;
    for (SideAxis &a : m_right)
      if (a.unit.isEmpty())
      {
        a.unit = it.value();
        break;
      }
  }

  while (int(m_extraCurves.size()) < n)
  {
    ExtraCurve c;
    c.series = new QLineSeries();
    m_chart->addSeries(c.series);
    c.series->attachAxis(m_xAxis);
    c.series->attachAxis(m_yAxis);
    c.line = std::make_unique<GapLine>(c.series);
    c.band = newBand();
    m_extraCurves.push_back(std::move(c));
  }

  resetSideScales();
  bool leftGrew = false;
  for (int i = 0; i < int(m_extraCurves.size()); ++i)
  {
    ExtraCurve &c = m_extraCurves[size_t(i)];
    c.line->clear();
    c.points.clear();
    c.tail = 0;
    if (i >= n)
    {
      c.shown = false;
      c.line->setVisible(false);
      continue;
    }
    const RecordingSeries &s = m_store->extra(i);
    // a series without a unit yet (only gaps) goes with the main one
    // the axis chosen, on Auto the first one that has its unit
    const ValueAxis axis = valueAxis(s.id);
    auto fits = [&](int side)
    {
      if (axis != AxisAuto && sideOf(axis) != side)
        return false;
      return side == 0 ? s.unit.isEmpty() || s.unit == main
                       : !s.unit.isEmpty() && s.unit == m_right[size_t(side - 1)].unit;
    };
    c.side = axis != AxisAuto ? sideOf(axis) : fits(0) ? 0 : fits(1) ? 1 : fits(2) ? 2 : 0;
    const bool fitted = fits(c.side);
    c.shown = m_shownValues.contains(s.id) && fitted;
    c.line->setAxes({ m_xAxis, yAxisOf(fitted ? c.side : 0) });
    c.line->setPen(extraPen(s.id));
    c.line->setVisible(c.shown);
    if (!c.shown)
    {
      // a right axis also scales a hidden value integrated there
      if (fitted && c.side > 0 && s.id == m_intValueId)
        for (int j = 0; j < s.count(); ++j)
          if (std::isfinite(s.at(j).value))
            growSide(c.side, s.at(j).value);
      continue;
    }
    QList<QPointF> points;
    const int count = s.count();
    for (int first = 0; first < count;)
    {
      const qint64 end = (s.at(first).t / m_bucket + 1) * m_bucket;
      const int last = qMin(s.lowerBound(end), count) - 1;
      c.tail = pointsOf(s, first, last, false, points);
      first = last + 1;
    }
    c.line->replace(points);
    c.points = points;
    for (const QPointF &p : std::as_const(points))
      if (std::isfinite(p.y()))
      {
        if (c.side > 0)
          growSide(c.side, p.y());
        else if (m_autoScale)
          leftGrew = growScale(p.y(), m_scaleMin, m_scaleMax) || leftGrew;
      }
  }
  // the other meters' values: the left axis in its unit, the right one if free
  leftGrew = rebuildOtherCurves() || leftGrew;
  if (leftGrew)
  {
    setYRange(m_scaleMin, m_scaleMax);
    updateThresholdLinePositions();
  }
  updateRightAxis();
  updateLegend();
  updateMinimapValue();
  if (m_intValueId != 0)
    rebuildOtherIntegral();
  updateDenseBands();
}

// The integration curve of another value than the main one: its series in
// turn (a gap between them), on the axis the value is on.
void GraphWidget::rebuildOtherIntegral()
{
  QList<QPointF> points;
  QAbstractAxis *axis = m_yAxis;
  m_tailInt = 0;
  for (int i = 0; i < m_store->extraCount(); ++i)
  {
    const RecordingSeries &s = m_store->extra(i);
    if (s.id != m_intValueId || s.isEmpty())
      continue;
    if (i < int(m_extraCurves.size()) && m_extraCurves[size_t(i)].side > 0)
      axis = yAxisOf(m_extraCurves[size_t(i)].side);
    if (!m_showIntegration)
      continue;
    if (!points.isEmpty())
      points.append(QPointF(s.first().t / 1000.0, qQNaN()));
    for (int first = 0; first < s.count();)
    {
      const qint64 end = (s.at(first).t / m_bucket + 1) * m_bucket;
      const int last = qMin(s.lowerBound(end), s.count()) - 1;
      m_tailInt = pointsOf(s, first, last, true, points);
      first = last + 1;
    }
  }
  integralOnAxis(axis);
  m_intLine->setMarkers(intPointsShown() ? m_intPoints : nullptr);
  m_intLine->replace(points);
  updateThresholdLinePositions();   // the integration threshold on that axis
}

void GraphWidget::onExtraAppended(int i)
{
  if (i >= int(m_extraCurves.size()))
  {
    rebuildExtras();
    return;
  }
  if (bucketSize() != m_bucket)
  {
    rebuildSeries();   // sets the bucket, then rebuildExtras()
    return;
  }
  ExtraCurve &c = m_extraCurves[size_t(i)];
  const RecordingSeries &s = m_store->extra(i);
  const int count = s.count();
  // as appendToSeries(): a new bucket adds its points, one still filling
  // replaces them
  const qint64 begin = s.at(count - 1).t / m_bucket * m_bucket;
  const int first = qMin(count - 1, s.lowerBound(begin));
  // the integration curve of this value
  if (s.id == m_intValueId && m_showIntegration)
    appendIntegral(s, first, count - 1, first != count - 1);
  if (!c.shown)
  {
    const double v = s.last().value;
    if (c.side > 0 && s.id == m_intValueId && std::isfinite(v) && growSide(c.side, v))
      updateRightAxis();
    return;
  }
  if (laterOutlier(s, first))
  {
    rebuildExtras();
    return;
  }
  if (first != count - 1)
  {
    c.line->removeLast(c.tail);
    c.points.resize(qMax<qsizetype>(0, c.points.size() - c.tail));
  }
  QList<QPointF> points;
  c.tail = pointsOf(s, first, count - 1, false, points);
  c.line->append(points);
  c.points.append(points);
  const double v = s.last().value;
  bool grew = false;
  if (std::isfinite(v) && c.side > 0)
  {
    if (growSide(c.side, v))
      updateRightAxis();
  }
  else if (std::isfinite(v) && m_autoScale && growScale(v, m_scaleMin, m_scaleMax))
  {
    setYRange(m_scaleMin, m_scaleMax);
    updateThresholdLinePositions();
    grew = true;   // the reference line and the band with it
  }
  // what follows the value of the left axis, as onAppended() for the main one
  if (s.id == referenceId() && c.side == 0 && !grew)
  {
    if (m_referenceKind == RefMean || m_referenceKind == RefMin || m_referenceKind == RefMax)
      updateReferenceLine();
    updateEnvelope();
  }
  else
    updateDenseBand(c.band, c.line.get(), c.shown, c.points, c.side, extraPen(s.id));
  if (m_cursorT[0] >= 0 || m_cursorT[1] >= 0)
    updateTimeCursors();
}

// A style for each value, by its number, so curves on top of each other are
// all seen and a value keeps its look when another is hidden: value 2
// dashed, 3 dotted, 4 dash-dot; a little thinner than the main value, drawn
// on top of it.
Qt::PenStyle GraphWidget::valueStyle(int id)
{
  switch (id)
  {
    case 0:  return Qt::SolidLine;
    case 1:  return Qt::DashLine;
    case 2:  return Qt::DotLine;
    default: return Qt::DashDotLine;
  }
}

QPen GraphWidget::extraPen(int id) const
{
  return QPen(extraColor(id), qMax(1.0, lineWidth() * 0.75), valueStyle(id), Qt::RoundCap, Qt::RoundJoin);
}

QString GraphWidget::legendText() const
{
  return m_legend && m_legend->isVisible() ? m_legend->text() : QString();
}

// A name of this meter's value in the legend's forms: as it is in the
// first three (no vendor, no meter to leave out), then its unit alone.
static QStringList nameForms(const QString &name, const QString &unit)
{
  const QString u = SiPrefix::displayText(unit);
  return { name, name, name, u.isEmpty() ? name : u };
}

void GraphWidget::updateLegend()
{
  if (!m_legend)
    return;
  // one entry per value, its newest series; only with more than the main one
  QList<GraphLegend::Entry> entries;
  QSet<int> seen;
  for (int i = m_store->extraCount() - 1; i >= 0; --i)
  {
    const RecordingSeries &s = m_store->extra(i);
    if (seen.contains(s.id) || i >= int(m_extraCurves.size()) || !m_extraCurves[size_t(i)].shown)
      continue;
    seen.insert(s.id);
    entries.prepend({ extraPen(s.id), nameForms(curveName(s, s.unit), s.unit) });
  }
  for (int i = 0; i < int(m_otherCurves.size()); ++i)
    if (const OtherCurve &c = m_otherCurves[size_t(i)]; c.shown)
    {
      // another meter's: without the vendor, without the meter, the unit alone
      QString model = c.meter;
      for (const QString &vendor : std::as_const(m_vendors))
        if (c.meter.startsWith(vendor + ' ', Qt::CaseInsensitive) && c.meter.size() - vendor.size() - 1 < model.size())
          model = c.meter.mid(vendor.size() + 1);
      const QString label = c.label.isEmpty() ? c.meter : c.label;
      const QStringList own = nameForms(label, c.unit);
      entries.append({ otherPen(i), { c.name, c.label.isEmpty() || model.isEmpty() ? c.name : model + " · " + c.label,
                                      label, own.last() } });
    }
  // the main value alone needs none; without it, the axis does not say which
  if (entries.isEmpty() && m_mainShown)
  {
    m_legend->hide();
    return;
  }
  if (m_mainShown)
    entries.prepend({ QPen(dataColor(), lineWidth(), penStyle(m_lineMode)),
                      nameForms(curveName(m_store->series(), m_store->unit()), m_store->unit()) });
  if (entries.isEmpty())
  {
    m_legend->hide();
    return;
  }
  const QColor ink = m_xLabelColor.isValid() ? m_xLabelColor : palette().color(QPalette::WindowText);
  // top left inside the plot, below the state ("LIVE", "REC"); as wide as
  // the plot (a graph not shown has no width to go by)
  const QPoint topLeft = m_chartView->mapFromScene(m_chart->plotArea().topLeft());
  m_legend->setEntries(entries, ink, isVisible() ? qMax(1, int(m_chart->plotArea().width()) - 12) : 0);
  const int below = m_stateLabel && m_stateLabel->isVisible() ? m_stateLabel->height() : 0;
  m_legend->move(topLeft.x() + 6, topLeft.y() + 4 + below);
  m_legend->show();
  m_legend->raise();
}

void GraphWidget::setValueShown(int id, bool shown)
{
  if (id == 0)
  {
    // back on the left: the others there move over (automatic), those
    // with the left axis chosen in another unit go off
    if (shown == m_mainShown)
      return;
    if (shown)
      makeRoom(0, AxisLeft, true);
    m_mainShown = shown;
  }
  else if (shown == m_shownValues.contains(id))
    return;
  else if (shown)
  {
    if (valueAxis(id) != AxisAuto)
      makeRoom(id, valueAxis(id));
    m_shownValues.insert(id);
  }
  else
    m_shownValues.remove(id);
  refreshValues();
}

void GraphWidget::setValueAxis(int id, ValueAxis axis)
{
  if (id <= 0 || axis == valueAxis(id))
    return;
  if (axis == AxisAuto)
    m_valueAxes.remove(id);
  else
  {
    if (m_shownValues.contains(id))
      makeRoom(id, axis);
    m_valueAxes[id] = axis;
  }
  refreshValues();
}

QString GraphWidget::valueUnitOf(int id) const
{
  if (id <= 0)
    return m_store->unit();
  for (int i = m_store->extraCount() - 1; i >= 0; --i)
    if (m_store->extra(i).id == id && !m_store->extra(i).unit.isEmpty())
      return m_store->extra(i).unit;
  return QString();
}

bool GraphWidget::valueDrawn(int id) const
{
  if (id <= 0)
    return m_mainShown;
  for (int i = 0; i < m_store->extraCount() && i < int(m_extraCurves.size()); ++i)
    if (m_store->extra(i).id == id && m_extraCurves[size_t(i)].shown)
      return true;
  return false;
}

int GraphWidget::minimapValue() const
{
  const int left = referenceId();
  if (left >= 0)
    return left;
  int lowest = -1;
  for (int i = 0; i < m_store->extraCount() && i < int(m_extraCurves.size()); ++i)
    if (m_extraCurves[size_t(i)].shown && (lowest < 0 || m_store->extra(i).id < lowest))
      lowest = m_store->extra(i).id;
  return qMax(0, lowest);
}

void GraphWidget::updateMinimapValue()
{
  if (!m_minimap)
    return;
  // another meter's value on the left axis: from its store, on its time
  if (const int other = referenceId() < 0 ? referenceOther() : -1; other >= 0)
  {
    const OtherCurve &c = m_otherCurves[size_t(other)];
    m_minimapShift = m_store->offsetOf(*c.store);
    m_minimap->setStore(c.store);
    m_minimap->setValue(c.value.id);
    m_minimap->setCurveColor(otherColor(other));
  }
  else
  {
    const int id = minimapValue();
    m_minimapShift = 0;
    m_minimap->setStore(m_store);
    m_minimap->setValue(id);
    m_minimap->setCurveColor(valueColor(id));
  }
  updateMinimapView();
}

GraphWidget::ValueAxis GraphWidget::valueSide(int id) const
{
  if (id <= 0)
    return m_mainShown ? AxisLeft : AxisAuto;
  for (int i = m_store->extraCount() - 1; i >= 0; --i)
    if (i < int(m_extraCurves.size()) && m_store->extra(i).id == id && m_extraCurves[size_t(i)].shown)
      return axisOfSide(m_extraCurves[size_t(i)].side);
  return AxisAuto;
}

QColor GraphWidget::sideColor(int side) const
{
  int lowest = INT_MAX;
  for (int i = 0; m_store && i < m_store->extraCount() && i < int(m_extraCurves.size()); ++i)
    if (m_extraCurves[size_t(i)].shown && m_extraCurves[size_t(i)].side == side)
      lowest = qMin(lowest, m_store->extra(i).id);
  if (lowest != INT_MAX)
    return valueColor(lowest);
  for (int i = 0; i < int(m_otherCurves.size()); ++i)
    if (m_otherCurves[size_t(i)].shown && m_otherCurves[size_t(i)].side == side)
      return otherColor(i);
  return m_stateColor.isValid() ? m_stateColor : m_xAxis->labelsBrush().color();
}

void GraphWidget::makeRoom(int id, ValueAxis axis, bool chosenOnly)
{
  const QString unit = valueUnitOf(id);
  const int side = sideOf(axis);
  const QString taken = side == 0 ? leftUnit() : m_right[size_t(side - 1)].unit;
  if (unit.isEmpty() || taken.isEmpty() || taken == unit)
    return;
  // a curve on Auto pushed off a right axis moves to the other one when that
  // is free (or freed by this value) or has its unit already
  const int other = side == 1 ? 2 : 1;
  bool otherTaken = false;
  for (int i = 0; side > 0 && i < m_store->extraCount() && i < int(m_extraCurves.size()); ++i)
    if (m_store->extra(i).id != id && m_extraCurves[size_t(i)].shown && m_extraCurves[size_t(i)].side == other)
      otherTaken = true;
  for (const OtherCurve &c : m_otherCurves)
    otherTaken = otherTaken || (side > 0 && c.shown && c.side == other);
  const QString otherUnit = m_right[size_t(other - 1)].unit;
  // the values drawn on that axis now, in the other unit
  QStringList names;
  auto off = [&](int value, const QString &name)
  {
    names << name;
    if (value == 0)
      m_mainShown = false;
    else
      m_shownValues.remove(value);
  };
  if (axis == AxisLeft && m_mainShown && id != 0)
    off(0, curveName(m_store->series(), m_store->unit()));
  QSet<int> done;
  for (int i = 0; i < m_store->extraCount() && i < int(m_extraCurves.size()); ++i)
  {
    const RecordingSeries &s = m_store->extra(i);
    const ExtraCurve &c = m_extraCurves[size_t(i)];
    if (s.id == id || done.contains(s.id) || !c.shown || c.side != side || s.unit == unit
        || (chosenOnly && valueAxis(s.id) != axis))
      continue;
    done.insert(s.id);
    if (side > 0 && valueAxis(s.id) == AxisAuto && (!otherTaken || otherUnit == s.unit))
    {
      otherTaken = true;   // it takes the other axis
      continue;
    }
    off(s.id, curveName(s, s.unit));
  }
  static const char *const sides[] = { QT_TR_NOOP("left"), QT_TR_NOOP("right"), QT_TR_NOOP("third") };
  if (!names.isEmpty())
    Q_EMIT notice(tr("%1 off: the %2 axis shows %3 now")
                    .arg(names.join(QStringLiteral(", ")), tr(sides[side]), SiPrefix::displayText(unit)));
}

void GraphWidget::refreshValues()
{
  // the view afresh: the left unit may be another one now
  resetSideScales();
  updateSeriesAppearance();   // the curves, then rebuildExtras()
  showUnit();
  if (m_autoScale)
    setScale(true, m_includeZero, m_scaleMin, m_scaleMax);
  updateYLabels();
  updateThresholdLinesVisibility();
  updateReferenceLine();
  updateEnvelope();
  updateTimeCursors();
}

int GraphWidget::referenceId() const
{
  if (m_mainShown)
    return 0;
  // the lowest with the left axis chosen, else the lowest left to the graph;
  // those on the right are not the left axis'
  int lowest = -1;
  bool fixed = false;
  for (int i = 0; i < m_store->extraCount(); ++i)
  {
    const RecordingSeries &s = m_store->extra(i);
    const ValueAxis axis = valueAxis(s.id);
    if (!m_shownValues.contains(s.id) || s.unit.isEmpty() || sideOf(axis) > 0 || (fixed && axis != AxisLeft))
      continue;
    if (lowest < 0 || s.id < lowest || (axis == AxisLeft && !fixed))
    {
      lowest = s.id;
      fixed = axis == AxisLeft;
    }
  }
  return lowest;
}

ValueRef GraphWidget::leftValue() const
{
  if (const int id = referenceId(); id >= 0)
    return ValueRef(id, m_baseSource);
  const int other = referenceOther();
  return other >= 0 ? m_otherCurves[size_t(other)].value : ValueRef(-1);
}

int GraphWidget::referenceOther() const
{
  if (referenceId() >= 0)
    return -1;
  for (int i = 0; i < int(m_otherCurves.size()); ++i)
    if (m_otherCurves[size_t(i)].shown && m_otherCurves[size_t(i)].side == 0 && m_otherCurves[size_t(i)].store)
      return i;
  return -1;
}

QString GraphWidget::leftUnit() const
{
  // the newest series of the value has the say; nothing of this meter on
  // the left: another meter's value there
  const int id = referenceId();
  if (const int other = id < 0 ? referenceOther() : -1; other >= 0 && !m_otherCurves[size_t(other)].unit.isEmpty())
    return m_otherCurves[size_t(other)].unit;
  for (int i = m_store->extraCount() - 1; id > 0 && i >= 0; --i)
    if (m_store->extra(i).id == id && !m_store->extra(i).unit.isEmpty())
      return m_store->extra(i).unit;
  return m_store->unit();
}

QList<QPointF> GraphWidget::referencePoints() const
{
  const int id = referenceId();
  if (id == 0)
    return m_curvePts;
  if (const int other = id < 0 ? referenceOther() : -1; other >= 0)
    return m_otherCurves[size_t(other)].points;
  // the curves of the value on the left (Hz, then % again: two), in time,
  // a gap between them
  QList<const QList<QPointF> *> parts;
  for (int i = 0; id > 0 && i < m_store->extraCount() && i < int(m_extraCurves.size()); ++i)
  {
    const ExtraCurve &c = m_extraCurves[size_t(i)];
    if (m_store->extra(i).id == id && c.shown && c.side == 0 && !c.points.isEmpty())
      parts << &c.points;
  }
  if (parts.size() == 1)
    return *parts.first();
  std::sort(parts.begin(), parts.end(), [](auto a, auto b) { return a->first().x() < b->first().x(); });
  QList<QPointF> out;
  for (const QList<QPointF> *p : std::as_const(parts))
  {
    if (!out.isEmpty())
      out << QPointF(p->first().x(), qQNaN());
    out += *p;
  }
  return out;
}

QList<int> GraphWidget::shownValues() const
{
  QList<int> ids(m_shownValues.cbegin(), m_shownValues.cend());
  std::sort(ids.begin(), ids.end());
  return ids;
}
