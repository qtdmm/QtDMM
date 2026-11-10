//======================================================================
// File:		graphwidget_axes.cpp (from dmmgraph.cpp)
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
// GraphWidget, part: the axes: time and value labels, the scales, the units and the right axis.
// The class is in graphwidget.h, its other parts in graphwidget*.cpp.

#include <QtGui>
#include <QtWidgets>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "ui/frameclock.h"
#include "ui/views/graphwidget.h"
#include "ui/views/graphwidget_p.h"
#include "core/nicestep.h"
#include "core/siprefix.h"

qint64 GraphWidget::windowStart() const
{
  // Live: the newest reading at the right edge once the window is full (the
  // axis ends a sample time short of the window, updateXAxisRange())
  if (m_store->state() == RecordingStore::Live)
  {
    // paused by the overview: where it was put, while the store keeps it
    const qint64 follow = followStart();
    return m_liveHold ? qBound(m_store->origin(), m_holdStart, follow) : follow;
  }
  // a recording seen at its end: to the millisecond, as Live, so it runs
  // on smoothly between the readings
  if (following())
    return followStart();
  // the scroll bar counts tenths of a second from what the store keeps
  return m_store->origin() + qint64(qMax(0, scrollbar->value())) * 100;
}

double GraphWidget::timeAxisStart() const
{
  return m_xAxis->min();
}

qint64 GraphWidget::followStart() const
{
  const qint64 window = qint64(qMax(1, m_windowSeconds)) * 1000;
  const qint64 shortOf = qMin<qint64>(m_store->sampleTime() * 100, window / 10);
  return qMax(m_store->origin(), m_store->duration() - window + shortOf);
}

bool GraphWidget::following() const
{
  if (m_store->state() == RecordingStore::Live)
    return !m_liveHold;
  // "All" shows the whole recording, its end too
  return m_store->state() == RecordingStore::Record
         && (m_followAll || (scrollbar->maximum() > 0 && scrollbar->value() >= scrollbar->maximum()));
}

void GraphWidget::updateClockWant()
{
  const bool moving = m_store->state() == RecordingStore::Live || m_store->state() == RecordingStore::Record;
  FrameClock::instance()->want(this, moving && isVisible());
}

void GraphWidget::frameTick()
{
  if (!following() || !isVisible())
    return;
  updateTimeAxis();
  updateMarkPositions();
  // the min/max band is drawn in pixels: along with the time axis
  if (m_envelopeOn)
    updateEnvelope();
}

void GraphWidget::updateXAxisRange()
{
  updateTimeAxis();
  updateReferenceLine();   // the window's mean, minimum or maximum
  updateEnvelope();
  scheduleStats();         // the window's statistics
}

void GraphWidget::updateTimeAxis()
{
  // one sample time short of the window, as it always was: no label on the
  // right edge
  double window = qMax(1, m_windowSeconds);
  double start = windowStart() / 1000.0, end = start + window - qMin(m_store->sampleTime() / 10.0, window / 10);
  if (stretching())
  {
    start = m_store->origin() / 1000.0;
    window = stretchedSeconds();
    end = start + window;
  }
  double div = timeStep((end - start) / 6);   // about 5 ticks
  if (divisions())
  {
    // up to 10 divisions of a whole time step over exactly the window:
    // 600 s are 10 x 1 min, 1 min 6 x 10 s. The axis is not moved onto a
    // step - that made 1 min 100 s wide and Live jump every 10 s; the grid
    // lines stay on whole steps and run with the readings
    div = timeStep(window / 10);
    end = start + window;
  }
  m_xAxis->setRange(start, end);
  updateLineSpans();
  // the ticks on whole steps, counted from the start of the recording
  m_xStep = div;
  m_xAxis->setTickType(QValueAxis::TicksDynamic);
  m_xAxis->setTickAnchor(0);
  m_xAxis->setTickInterval(div);
  updateXLabels();
  updateMinimapView();
}

bool GraphWidget::stretching() const
{
  return m_followAll && following() && m_store->duration() - m_store->origin() < qint64(m_windowSeconds) * 1000;
}

double GraphWidget::stretchedSeconds() const
{
  return qMax(10.0, (m_store->duration() - m_store->origin()) / 1000.0);
}

void GraphWidget::updateLineSpans()
{
  const double min = m_xAxis->min(), max = m_xAxis->max();
  // the newest values run on with the clock while the window follows them
  const double now = following() ? m_store->duration() / 1000.0 : qQNaN();
  m_dataLine->setSpan(min, max);
  m_dataLine->setHold(now);
  m_intLine->setSpan(min, max);
  for (ExtraCurve &c : m_extraCurves)
  {
    c.line->setSpan(min, max);
    c.line->setHold(now);
  }
  for (OtherCurve &c : m_otherCurves)
    c.line->setSpan(min, max);
}

double GraphWidget::timeStep(double v)
{
  if (!(v > 0) || !std::isfinite(v))
    return 1;
  static const double steps[] = { 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800,
                                  3600, 7200, 10800, 21600, 43200, 86400 };
  for (double s : steps)
    if (s >= v * (1 - 1e-12))
      return s;
  return niceStep(v / 86400) * 86400;   // days in 1-2-5
}

// Seconds, minutes or hours, as the step reads best; the title says which.
double GraphWidget::timeAxisUnit() const
{
  if (m_xStep >= 3600 && std::fmod(m_xStep, 3600) == 0)
    return 3600;
  return m_xStep >= 60 && std::fmod(m_xStep, 60) == 0 ? 60 : 1;
}

QString GraphWidget::axisTimeText(qint64 t) const
{
  const double unit = timeAxisUnit();
  return QString::number(t / 1000.0 / unit, 'f', 3) + QChar(0xa0) + (unit == 3600 ? tr("h") : unit == 60 ? tr("min") : tr("s"));
}

void GraphWidget::updateXLabels()
{
  if (!m_xAxis || m_xStep <= 0)
    return;
  const double unit = timeAxisUnit();
  const QString title = unit == 3600 ? tr("[h]") : unit == 60 ? tr("[min]") : tr("[sec]");
  if (m_xAxis->titleText() != title)
    m_xAxis->setTitleText(title);

  const QRectF plot = m_chart->plotArea();
  const double min = m_xAxis->min(), max = m_xAxis->max();
  QList<double> values;
  if (max > min && plot.width() > 0)
    for (double v = std::ceil(min / m_xStep - 1e-9) * m_xStep; v <= max + m_xStep * 1e-9 && values.size() < 200; v += m_xStep)
      values << v;
  while (m_xLabels.size() < values.size())
  {
    auto *item = new QGraphicsSimpleTextItem(m_chart);
    item->setZValue(m_yTitle->zValue());
    m_xLabels << item;
  }
  const QFont font = m_xAxis->labelsFont();
  // Qt's labels start below the axis line and its tick marks
  const double top = plot.bottom() + 6;
  for (int i = 0; i < m_xLabels.size(); ++i)
  {
    QGraphicsSimpleTextItem *item = m_xLabels[i];
    item->setVisible(i < values.size());
    if (i >= values.size())
      continue;
    const double v = values[i] / unit;
    item->setText(QString::number(std::abs(v) < 1e-9 ? 0.0 : v, 'g', 6));
    item->setFont(font);
    item->setBrush(m_xLabelColor.isValid() ? QBrush(m_xLabelColor) : m_xAxis->titleBrush());
    const double x = plot.left() + (values[i] - min) / (max - min) * plot.width();
    item->setPos(x - item->boundingRect().width() / 2, top);
  }
  updateTimeCursors();
}

QString GraphWidget::yPrefix() const
{
  const QString base = leftUnit();
  // units that take no prefix: °C, %, ppm, none ...
  if (!SiPrefix::takesPrefix(base))
    return QString();
  // by the axis, not by the reading: a meter that changes range (V, mV, µV)
  // turned the whole axis to mV with labels like 1.6e+04; 350 mV on an axis
  // up to 0.4 V are still mV. Without readings yet the axis says nothing:
  // the meter's prefix
  if (m_mainShown && m_store->count() == 0 && !m_liveBase.isEmpty() && m_liveBase == base)
    return m_livePrefix;
  QString prefix;
  SiPrefix::scale(qMax(std::abs(m_yAxis->min()), std::abs(m_yAxis->max())), &prefix);
  return prefix;
}

void GraphWidget::updateYLabels()
{
  if (!m_yAxis)
    return;
  const QString prefix = yPrefix();
  if (prefix != m_yPrefix)
  {
    m_yPrefix = prefix;
    showUnit();
  }
  const double factor = SiPrefix::factor(prefix);
  const QRectF plot = m_chart->plotArea();
  const double min = m_yAxis->min(), max = m_yAxis->max();
  const int n = m_yAxis->tickCount();
  QList<double> values;
  if (max > min && plot.height() > 0 && n > 1)
    for (int i = 0; i < n; ++i)
      values << min + i * (max - min) / (n - 1);
  while (m_yLabels.size() < values.size())
  {
    auto *item = new QGraphicsSimpleTextItem(m_chart);
    item->setZValue(m_yTitle->zValue());
    m_yLabels << item;
  }
  const QFont font = m_yAxis->labelsFont();
  const double gap = QFontMetricsF(font).horizontalAdvance(' ') + 2;
  for (int i = 0; i < m_yLabels.size(); ++i)
  {
    QGraphicsSimpleTextItem *item = m_yLabels[i];
    item->setVisible(i < values.size());
    if (i >= values.size())
      continue;
    // as precise as Qt's invisible labels that keep the room ("%.4g"), and
    // no "-0"
    double v = values[i] / factor;
    if (std::abs(values[i]) < (max - min) * 1e-9)
      v = 0;
    item->setText(QString::number(v, 'g', 4));
    item->setFont(font);
    item->setBrush(m_xLabelColor.isValid() ? QBrush(m_xLabelColor) : m_yAxis->titleBrush());
    const QRectF r = item->boundingRect();
    const double y = plot.bottom() - (values[i] - min) / (max - min) * plot.height();
    item->setPos(qMax(1.0, plot.left() - r.width() - gap), y - r.height() / 2);
  }
  updateReferenceLine();
  updateEnvelope();
  updateTimeCursors();   // their levels
}

bool GraphWidget::divisions() const
{
  return m_variant == ScopeBlue || phosphor() || m_variant == ChartRecorder;
}

void GraphWidget::setYRange(double min, double max)
{
  // a range without width - no value yet (the auto scale starts at
  // +-1e40, or 0..0 with "include zero"), or one value that stays the same
  // (0 V on a shorted input) - draws no grid and no labels: show 0..1
  // while there is no value, else a tenth of the value around it (0: +-1)
  if (m_zoomed)   // a frame zoomed on (Shift + drag)
  {
    min = m_yZoom[0].first;
    max = m_yZoom[0].second;
  }
  const bool noWidth = !std::isfinite(min) || !std::isfinite(max) || !(max > min);
  if (noWidth && (m_store->count() == 0 || !std::isfinite(min) || !std::isfinite(max) || min > max))
  {
    min = 0;
    max = 1;
  }
  else if (max == min)
  {
    const double half = min == 0 ? 1.0 : std::abs(min) * 0.1;
    min -= half;
    max += half;
  }
  if (divisions() && max > min && std::isfinite(min) && std::isfinite(max))
  {
    // 8 divisions of a 1-2-5 step; the range grows to whole steps
    double div = niceStep((max - min) / 8);
    double first = std::floor(min / div + 1e-9) * div;
    while (first + 8 * div < max - div * 1e-9)
    {
      div = niceStep(div * 1.01);
      first = std::floor(min / div + 1e-9) * div;
    }
    m_yAxis->setRange(first, first + 8 * div);
  }
  else
    m_yAxis->setRange(min, max);
  updateCentreTicks();
  updateYLabels();
}

void GraphWidget::updateCentreTicks()
{
  if (!phosphor())
  {
    m_centreTicks->setPath(QPainterPath());
    return;
  }
  // No cross in the middle: it said nothing and looked like a reference.
  // The horizontal line is the reference line while that is in view, the
  // vertical one the trigger of a pre-triggered recording; without them
  // the fine ticks sit on the plot's bottom and left edge.
  const QRectF pa = m_chart->plotArea();
  const double ref = referenceY();
  const bool horizontal = std::isfinite(ref);
  const double cy = horizontal ? ref : pa.bottom();
  bool vertical = false;
  double cx = pa.left();
  const double min = m_xAxis->min(), max = m_xAxis->max();
  for (const RecordingStore::Mark &m : m_store->marks())
    if (m.color == RecordingStore::kTriggerColor && max > min && m.t / 1000.0 >= min && m.t / 1000.0 <= max)
    {
      cx = pa.left() + (m.t / 1000.0 - min) / (max - min) * pa.width();
      vertical = true;
    }
  QPainterPath path;
  if (horizontal)
  {
    path.moveTo(pa.left(), cy);
    path.lineTo(pa.right(), cy);
  }
  if (vertical)
  {
    path.moveTo(cx, pa.top());
    path.lineTo(cx, pa.bottom());
  }
  // on a line both sides, on the edge inwards
  const double up = 3, down = horizontal ? 3 : 0, right = 3, left = vertical ? 3 : 0;
  const double dx = pa.width() / 50, dy = pa.height() / 40;   // 10 x 8 divisions, 5 ticks each
  for (int i = 0; i <= 50; ++i)
  {
    path.moveTo(pa.left() + i * dx, cy - up);
    path.lineTo(pa.left() + i * dx, cy + down);
  }
  for (int i = 0; i <= 40; ++i)
  {
    path.moveTo(cx - left, pa.top() + i * dy);
    path.lineTo(cx + right, pa.top() + i * dy);
  }
  m_centreTicks->setPath(path);
}

void GraphWidget::setUnit(const QString &unit)
{
  // Values arrive in SI base units (see DmmChannel), so the store keeps the
  // base unit; the prefix is the y labels' (yPrefix()).
  const SiPrefix::Split split = SiPrefix::split(unit);
  m_store->setUnit(split.baseUnit);
  m_liveBase = split.baseUnit;
  m_livePrefix = split.prefix == QLatin1String("u") ? QStringLiteral("µ") : split.prefix;
  showUnit();
  updateYLabels();
}

// The axis title: the unit of the recorded values (a recording keeps its
// unit when the meter changes to another one).
void GraphWidget::showUnit()
{
  const QString base = leftUnit();
  // as the right axis and the display write it: "[°C]", "[kΩ]"
  m_yTitle->setText(base.isEmpty() ? QString() : QString("[%1]").arg(SiPrefix::displayText(m_yPrefix + base)));
  // room above the plot for the title
  const int h = base.isEmpty() ? 0 : int(m_yTitle->boundingRect().height()
                                             + QFontMetricsF(m_yAxis->labelsFont()).height() / 2);
  // and at least as much as the time buttons need, so they sit above the plot
  const int bar = m_timeBar ? m_timeBar->sizeHint().height() : 0;
  m_chart->setMargins(QMargins(4, 4 + qMax(h, bar), 4, 4));
  placeYTitle();
}

// right-aligned with the axis line, just above the plot area
void GraphWidget::placeYTitle()
{
  const QRectF plot = m_chart->plotArea();
  const QRectF text = m_yTitle->boundingRect();
  // the top tick label is centred on the plot's top edge: stay above it
  const double labelHalf = QFontMetricsF(m_yAxis->labelsFont()).height() / 2;
  m_yTitle->setPos(qMax(2.0, plot.left() - text.width() - 4), plot.top() - labelHalf - text.height() - 1);
  // the right one left-aligned with its axis line; the third one, the
  // outermost axis, at the right edge, the right one ending before it
  if (!m_right[0].title)
    return;
  const double width = m_chart->size().width();
  QGraphicsSimpleTextItem *right = m_right[0].title, *third = m_right[1].title;
  double end = width - 2;
  if (third->isVisible())
  {
    third->setPos(width - third->boundingRect().width() - 2, plot.top() - labelHalf - third->boundingRect().height() - 1);
    end = third->x() - 6;
  }
  right->setPos(qMin(plot.right() + 4, end - right->boundingRect().width()),
                plot.top() - labelHalf - right->boundingRect().height() - 1);
}

void GraphWidget::setScale(bool autoScale, bool includeZero, double min, double max)
{
  m_autoScale = autoScale;
  m_includeZero = includeZero;

  if (!autoScale)
  {
    m_scaleMin = min;
    m_scaleMax = max;

  }
  else
  {
    if (m_includeZero)
      m_scaleMin = m_scaleMax = 0;
    else
    {
      m_scaleMin =  1e40;
      m_scaleMax = -1e40;
    }

    for (int i = 0; m_mainShown && i < m_store->count(); i++)
      growScale(m_store->series().at(i).value, m_scaleMin, m_scaleMax);
    // the other values drawn on the left axis
    for (int i = 0; i < m_store->extraCount() && i < int(m_extraCurves.size()); ++i)
    {
      const ExtraCurve &c = m_extraCurves[size_t(i)];
      if (!c.shown || c.side != 0)
        continue;
      const RecordingSeries &x = m_store->extra(i);
      for (int j = 0; j < x.count(); ++j)
        if (std::isfinite(x.at(j).value))
          growScale(x.at(j).value, m_scaleMin, m_scaleMax);
    }
    for (const OtherCurve &c : m_otherCurves)
      for (const QPointF &p : c.points)
        if (c.shown && c.side == 0 && std::isfinite(p.y()))
          growScale(p.y(), m_scaleMin, m_scaleMax);
  }

  setYRange(m_scaleMin, m_scaleMax);
  updateThresholdLinePositions();
}

bool GraphWidget::growSide(int side, double v)
{
  SideAxis &a = m_right[size_t(side - 1)];
  return growScale(v, a.scaleMin, a.scaleMax);
}

void GraphWidget::resetSideScales()
{
  for (SideAxis &a : m_right)
  {
    a.scaleMin = 1e40;
    a.scaleMax = -1e40;
  }
}

QString GraphWidget::sideTitle(int side) const
{
  const SideAxis &a = m_right[size_t(side - 1)];
  return a.title->isVisible() ? a.title->text() : QString();
}

void GraphWidget::updateRightAxis()
{
  for (int k = 0; k < int(m_right.size()); ++k)
  {
    SideAxis &a = m_right[size_t(k)];
    const bool on = !a.unit.isEmpty();
    a.axis->setVisible(on);
    a.title->setVisible(on);
    if (!on)
      continue;
    double min = a.scaleMin, max = a.scaleMax;
    if (m_zoomed)
    {
      min = m_yZoom[size_t(k + 1)].first;
      max = m_yZoom[size_t(k + 1)].second;
    }
    else if (const auto fixed = m_fixedRanges.constFind(a.unit); fixed != m_fixedRanges.cend())
    {
      // fixed: at least this range, the ticks round as ever
      min = fixed->first;
      max = fixed->second;
    }
    else if (m_includeZero)
    {
      min = qMin(min, 0.0);
      max = qMax(max, 0.0);
    }
    if (!std::isfinite(min) || !std::isfinite(max) || !(max >= min) || min > 1e39)
    {
      min = 0;
      max = 1;
    }
    else if (max == min)
    {
      const double half = min == 0 ? 1.0 : std::abs(min) * 0.1;
      min -= half;
      max += half;
    }
    // its ticks on the left axis' grid lines: the same count, a round step;
    // a fixed range also takes steps of 1.5, 3, 4, 6, 8 (0 .. 100 % in 8
    // divisions: 0 .. 120 in 15)
    const int ticks = qMax(2, m_yAxis->tickCount());
    double from = 0, s = 1;
    if (!m_zoomed && m_fixedRanges.contains(a.unit))
      std::tie(from, s) = fixedRangeGrid(min, max, ticks - 1);
    else
    {
      s = niceStep((max - min) / (ticks - 1));
      double top = std::floor(min / s + 1e-9) * s + (ticks - 1) * s;
      while (top < max - s * 1e-9)
      {
        s = niceStep(s * 1.01);
        top = std::floor(min / s + 1e-9) * s + (ticks - 1) * s;
      }
      from = std::floor(min / s + 1e-9) * s;
    }
    a.axis->setTickCount(ticks);
    a.axis->setRange(from, from + (ticks - 1) * s);
    a.title->setText(QString("[%1]").arg(SiPrefix::displayText(a.unit)));
    // in the colour of its first curve: which axis is whose, with three
    a.title->setBrush(sideColor(k + 1));
  }
  placeYTitle();
  placeTimeBar();
  if (m_intValueId > 0)
    updateThresholdLinePositions();
}

void GraphWidget::setFixedRange(const QString &unit, double min, double max)
{
  if (unit.isEmpty() || !std::isfinite(min) || !std::isfinite(max))
    return;
  m_fixedRanges[unit] = { qMin(min, max), qMax(min, max) };
  updateRightAxis();
  updateThresholdLinePositions();
}

void GraphWidget::clearFixedRange(const QString &unit)
{
  if (m_fixedRanges.remove(unit) == 0)
    return;
  updateRightAxis();
  updateThresholdLinePositions();
}

QString GraphWidget::fixedRangesSetting() const
{
  QJsonObject o;
  for (auto it = m_fixedRanges.cbegin(); it != m_fixedRanges.cend(); ++it)
    o.insert(it.key(), QJsonArray{ it->first, it->second });
  return o.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void GraphWidget::setFixedRangesSetting(const QString &text)
{
  m_fixedRanges.clear();
  const QJsonObject o = QJsonDocument::fromJson(text.toUtf8()).object();
  for (auto it = o.begin(); it != o.end(); ++it)
  {
    const QJsonArray r = it.value().toArray();
    if (r.size() == 2 && r[0].isDouble() && r[1].isDouble())
      m_fixedRanges[it.key()] = { qMin(r[0].toDouble(), r[1].toDouble()), qMax(r[0].toDouble(), r[1].toDouble()) };
  }
  updateRightAxis();
  updateThresholdLinePositions();
}

int GraphWidget::axisSideAt(const QPoint &pos) const
{
  const QPointF scene = m_chartView->mapToScene(pos);
  const QRectF plot = m_chart->plotArea();
  if (scene.y() < plot.top() - 30 || scene.y() > plot.bottom() + 4)
    return -1;
  if (scene.x() < plot.left())
    return 0;
  if (scene.x() <= plot.right() || m_right[0].unit.isEmpty())
    return -1;
  if (m_right[1].unit.isEmpty())
    return 1;
  // the third axis is the outermost: its tick labels begin furthest
  // right (its title is often narrower than they are)
  double thirdLeft = m_right[1].title->x();
  double labelsLeft = -1;
  for (QGraphicsItem *item : m_chart->scene()->items())
  {
    const auto *label = dynamic_cast<const QGraphicsTextItem *>(item);
    if (!label || !label->isVisible())
      continue;
    const QRectF r = label->sceneBoundingRect();
    if (r.left() > plot.right() && r.center().y() >= plot.top() - 2 && r.center().y() <= plot.bottom() + 2)
      labelsLeft = qMax(labelsLeft, r.left());
  }
  if (labelsLeft > 0)
    thirdLeft = qMin(thirdLeft, labelsLeft);
  return scene.x() >= thirdLeft - 3 ? 2 : 1;
}
