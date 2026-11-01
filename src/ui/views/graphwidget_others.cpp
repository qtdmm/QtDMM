// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GraphWidget: the curves of values of other meters (setOtherValues()),
// each from that meter's store.

#include "ui/views/graphwidget.h"
#include "ui/views/graphwidget_p.h"

#include <algorithm>
#include <cmath>

#include "recording/recordingfile.h"

void GraphWidget::setOtherStores(std::function<RecordingStore *(const QString &source)> store,
                                 std::function<QString(const QString &source)> name)
{
  m_otherStore = std::move(store);
  m_otherName = std::move(name);
  otherStoresChanged();
}

namespace
{
// the source of another meter's value in a file viewed: "file:2"
const QLatin1String kFileSource("file:");
}

void GraphWidget::setOtherValues(const QList<ValueRef> &values)
{
  if (values == otherValues())
    return;
  makeOtherCurves(values + m_fileValues);
}

void GraphWidget::makeOtherCurves(const QList<ValueRef> &values)
{
  for (OtherCurve &c : m_otherCurves)
  {
    c.line.reset();
    m_chart->removeSeries(c.series);
    delete c.series;
  }
  m_otherCurves.clear();
  for (const ValueRef &value : values)
  {
    OtherCurve c;
    c.value = value;
    c.series = new QLineSeries();
    m_chart->addSeries(c.series);
    c.series->attachAxis(m_xAxis);
    c.series->attachAxis(m_yAxis);
    c.line = std::make_unique<GapLine>(c.series);
    m_otherCurves.push_back(std::move(c));
  }
  otherStoresChanged();
}

QList<ValueRef> GraphWidget::otherValues() const
{
  QList<ValueRef> values;
  for (const OtherCurve &c : m_otherCurves)
    if (!c.value.source.startsWith(kFileSource))
      values << c.value;
  return values;
}

RecordingStore *GraphWidget::otherStore(const QString &source) const
{
  if (source.startsWith(kFileSource))
    return m_fileStores.value(source.mid(kFileSource.size()).toInt(), nullptr);
  // a file viewed: its values only, not those of the meters open now
  return m_otherStore && m_sourceFile.isEmpty() ? m_otherStore(source) : nullptr;
}

QString GraphWidget::otherName(const QString &source) const
{
  if (source.startsWith(kFileSource))
    return m_fileDevices.value(source.mid(kFileSource.size()).toInt());
  return m_otherName ? m_otherName(source) : QString();
}

void GraphWidget::setFileStores(const Recording &rec)
{
  dropFileStores();
  const QStringList devices = RecordingFile::devices(rec);
  for (int i = 0; i < devices.size(); ++i)
  {
    auto *store = new RecordingStore(this);
    store->load(RecordingFile::devicePart(rec, devices[i]));
    const QString source = kFileSource + QString::number(i);
    if (store->count() > 0)
      m_fileValues << ValueRef(0, source);
    for (int j = 0; j < store->extraCount(); ++j)
      if (!m_fileValues.contains(ValueRef(store->extra(j).id, source)))
        m_fileValues << ValueRef(store->extra(j).id, source);
    m_fileStores << store;
    m_fileDevices << devices[i];
  }
  makeOtherCurves(otherValues() + m_fileValues);
}

void GraphWidget::dropFileStores()
{
  if (m_fileStores.isEmpty())
    return;
  m_fileValues.clear();
  makeOtherCurves(otherValues());
  qDeleteAll(m_fileStores);
  m_fileStores.clear();
  m_fileDevices.clear();
}

void GraphWidget::setOtherValueShown(const ValueRef &value, bool shown)
{
  QList<ValueRef> values = otherValues();
  if (shown == values.contains(value))
    return;
  if (shown)
    values << value;
  else
    values.removeAll(value);
  setOtherValues(values);
}

void GraphWidget::setOtherValueAxis(const ValueRef &value, ValueAxis axis)
{
  if (axis == otherValueAxis(value))
    return;
  if (axis == AxisAuto)
    m_otherAxes.remove(value);
  else
    m_otherAxes.insert(value, axis);
  refreshValues();
  for (const OtherCurve &c : m_otherCurves)
    if (c.value == value && c.store && !c.shown)
      Q_EMIT notice(tr("%1: its unit %2 has no room on that axis").arg(c.name, c.unit));
}

GraphWidget::ValueAxis GraphWidget::otherValueSide(const ValueRef &value) const
{
  for (const OtherCurve &c : m_otherCurves)
    if (c.value == value && c.shown)
      return axisOfSide(c.side);
  return AxisAuto;
}

QColor GraphWidget::otherValueColor(const ValueRef &value) const
{
  for (size_t i = 0; i < m_otherCurves.size(); ++i)
    if (m_otherCurves[i].value == value && m_otherCurves[i].shown)
      return otherColor(int(i));
  return QColor();
}

void GraphWidget::otherStoresChanged()
{
  attachOtherStores();
  refreshValues();   // the axes may change: rebuildExtras(), then rebuildOtherCurves()
}

void GraphWidget::attachOtherStores()
{
  QSet<RecordingStore *> stores;
  for (OtherCurve &c : m_otherCurves)
  {
    c.store = otherStore(c.value.source);
    if (c.store && c.store != m_store)
      stores.insert(c.store);
  }
  for (const QPointer<RecordingStore> &old : std::as_const(m_otherConnected))
    if (old)
      old->disconnect(this);
  m_otherConnected.clear();
  // their readings at the drawing pace, not one rebuild each
  auto later = [this]
  {
    if (!m_otherTimer.isActive())
      m_otherTimer.start();
  };
  for (RecordingStore *store : std::as_const(stores))
  {
    connect(store, &RecordingStore::appended, this, later);
    connect(store, &RecordingStore::extraAppended, this, later);
    connect(store, &RecordingStore::extrasChanged, this, later);
    connect(store, &RecordingStore::cleared, this, later);
    m_otherConnected << store;
  }
}

bool GraphWidget::rebuildOtherCurves()
{
  // nothing of the graph's meter on the left: the axis is free, the first
  // value chosen there gives its unit, else the first one there by itself
  const bool free = referenceId() < 0;
  QString left = free ? QString() : leftUnit();
  QList<int> order;
  for (int i = 0; i < int(m_otherCurves.size()); ++i)
    order << i;
  if (free)
    std::stable_sort(order.begin(), order.end(), [this](int a, int b)
    {
      return (otherValueAxis(m_otherCurves[size_t(a)].value) == AxisLeft)
             > (otherValueAxis(m_otherCurves[size_t(b)].value) == AxisLeft);
    });
  bool leftGrew = false;
  for (int i : std::as_const(order))
  {
    OtherCurve &c = m_otherCurves[size_t(i)];
    c.points.clear();
    c.line->clear();
    c.shown = false;
    const RecordingStore *store = c.store;
    if (!store)
    {
      c.line->setVisible(false);
      continue;
    }
    // its series of the value, oldest first; the newest names the unit
    QList<const RecordingSeries *> parts;
    if (c.value.id == 0)
      parts << &store->series();
    else
      for (int j = 0; j < store->extraCount(); ++j)
        if (store->extra(j).id == c.value.id)
          parts << &store->extra(j);
    c.unit = c.value.id == 0 ? store->unit() : QString();
    for (const RecordingSeries *s : std::as_const(parts))
      if (c.value.id != 0 && !s->unit.isEmpty())
        c.unit = s->unit;
    c.name = parts.isEmpty() ? QString() : curveName(*parts.last(), c.unit);
    const QString meter = otherName(c.value.source);
    if (!meter.isEmpty())
      c.name = c.name.isEmpty() ? meter : meter + " · " + c.name;
    // its axis: the one chosen if its unit fits there, else the left one
    // in its unit, else the right one, else the third one when free
    const ValueAxis chosen = otherValueAxis(c.value);
    auto fits = [&](int side)
    {
      if (side == 0)
        return c.unit.isEmpty() || c.unit == left || (free && left.isEmpty());
      const QString &unit = m_right[size_t(side - 1)].unit;
      return !c.unit.isEmpty() && (unit.isEmpty() || unit == c.unit);
    };
    if (chosen != AxisAuto ? fits(sideOf(chosen)) : fits(0) || fits(1) || fits(2))
      c.side = chosen != AxisAuto ? sideOf(chosen) : fits(0) ? 0 : fits(1) ? 1 : 2;
    else
    {
      c.line->setVisible(false);
      continue;
    }
    if (c.side > 0)
      m_right[size_t(c.side - 1)].unit = c.unit;
    else if (left.isEmpty())
      left = c.unit;
    c.shown = true;
    // its time on this store's clock
    const double shift = m_store->offsetOf(*store) / 1000.0;
    QList<QPointF> points;
    for (const RecordingSeries *s : std::as_const(parts))
    {
      if (s->isEmpty())
        continue;
      if (!points.isEmpty())
        points << QPointF(s->first().t / 1000.0, qQNaN());
      for (int first = 0; first < s->count();)
      {
        const qint64 end = (s->at(first).t / m_bucket + 1) * m_bucket;
        const int last = qMin(s->lowerBound(end), s->count()) - 1;
        pointsOf(*s, first, last, false, points);
        first = last + 1;
      }
    }
    for (QPointF &p : points)
    {
      p.rx() += shift;
      if (!std::isfinite(p.y()))
        continue;
      if (c.side > 0)
        growSide(c.side, p.y());
      else if (m_autoScale)
        leftGrew = growScale(p.y(), m_scaleMin, m_scaleMax) || leftGrew;
    }
    c.line->setAxes({ m_xAxis, yAxisOf(c.side) });
    c.line->setPen(otherPen(i));
    c.line->setVisible(true);
    c.line->replace(points);
    c.line->setSpan(m_xAxis->min(), m_xAxis->max());
    c.points = points;
  }
  return leftGrew;
}

void GraphWidget::onOtherReadings()
{
  if (rebuildOtherCurves())
  {
    setYRange(m_scaleMin, m_scaleMax);
    updateThresholdLinePositions();
  }
  updateRightAxis();
  updateLegend();
}

QPen GraphWidget::otherPen(int i) const
{
  return QPen(otherColor(i), qMax(1.0, m_lineWidth * 0.75), valueStyle(m_otherCurves[size_t(i)].value.id), Qt::RoundCap,
              Qt::RoundJoin);
}

// Colours of their own, apart from the graph meter's values: another
// meter, another hue; the line style still says which of its values.
QColor GraphWidget::otherColor(int i) const
{
  switch (m_variant)
  {
    case ScopeBlue:     return QColor(QList<const char *>{ "#5ce1e6", "#ff7eb6", "#e8e8e8", "#c6ff5c" }[i % 4]);
    case PhosphorGreen:
    case PhosphorAmber: return extraColor(i + 1);
    case ChartRecorder: return QColor(QList<const char *>{ "#1565c0", "#ad1457", "#00838f", "#6d4c41" }[i % 4]);
    default: break;
  }
  const bool dark = m_xLabelColor.isValid() && m_xLabelColor.lightness() > 128;
  static const char *light[] = { "#2f6fde", "#c2185b", "#00897b", "#8a6d3b" };
  static const char *darkSteps[] = { "#5b8def", "#e0568f", "#26a69a", "#b39060" };
  return QColor(dark ? darkSteps[i % 4] : light[i % 4]);
}

QList<ValueRef> GraphWidget::values() const
{
  QList<ValueRef> out;
  if (m_mainShown)
    out << ValueRef(0, m_baseSource);
  for (int id : shownValues())
    out << ValueRef(id, m_baseSource);
  return out + otherValues();
}

void GraphWidget::setValues(const QList<ValueRef> &values)
{
  if (values == this->values())
    return;
  QList<ValueRef> others;
  QSet<int> own;
  for (const ValueRef &v : values)
    if (isBase(v))
      own.insert(v.id);
    else if (!others.contains(resolved(v)))
      others << resolved(v);
  setOtherValues(others);
  for (int id = 1; id < kMaxValues; ++id)
    setValueShown(id, own.contains(id));
  setValueShown(0, own.contains(0));
  Q_EMIT valuesChanged();
}

bool GraphWidget::valueShown(const ValueRef &value) const
{
  if (!isBase(value))
    return otherValues().contains(resolved(value));
  return value.id == 0 ? m_mainShown : m_shownValues.contains(value.id);
}

void GraphWidget::setValueShown(const ValueRef &value, bool shown)
{
  if (shown == valueShown(value))
    return;
  if (isBase(value))
    setValueShown(value.id, shown);
  else
    setOtherValueShown(resolved(value), shown);
  Q_EMIT valuesChanged();
}

GraphWidget::ValueAxis GraphWidget::valueAxis(const ValueRef &value) const
{
  return isBase(value) ? valueAxis(value.id) : otherValueAxis(resolved(value));
}

void GraphWidget::setValueAxis(const ValueRef &value, ValueAxis axis)
{
  if (isBase(value))
    setValueAxis(value.id, axis);
  else
    setOtherValueAxis(resolved(value), axis);
}

GraphWidget::ValueAxis GraphWidget::valueSide(const ValueRef &value) const
{
  return isBase(value) ? valueSide(value.id) : otherValueSide(resolved(value));
}

QColor GraphWidget::drawnColor(const ValueRef &value) const
{
  if (!isBase(value))
    return otherValueColor(resolved(value));
  return valueDrawn(value.id) ? valueColor(value.id) : QColor();
}

void GraphWidget::setBase(RecordingStore *store, const QString &source)
{
  if (!store || (store == m_store && source == m_baseSource))
    return;
  const QList<ValueRef> values = this->values();
  QHash<ValueRef, ValueAxis> axes;
  // the main value's left axis is no choice: another meter's it is free
  for (const ValueRef &v : values)
    if (!(isBase(v) && v.id == 0))
      axes.insert(v, valueAxis(v));
  // all off, then on again with the new meter's split
  makeOtherCurves(m_fileValues);
  m_mainShown = false;
  m_shownValues.clear();
  m_valueAxes.clear();
  m_otherAxes.clear();
  m_baseSource = source;
  setStore(store);
  for (auto it = axes.cbegin(); it != axes.cend(); ++it)
    if (it.value() != AxisAuto)
      setValueAxis(it.key(), it.value());
  setValues(values);
}
