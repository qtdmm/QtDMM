//======================================================================
// File:		graphwidget_overlays.cpp (from dmmgraph.cpp)
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
// GraphWidget, part: what lies over the curves: time cursors, the reference
// line, threshold lines, alarm marks and flags.
// The class is in graphwidget.h, its other parts in graphwidget*.cpp.

#include <QtGui>
#include <QtWidgets>
#include <QPen>
#include <cmath>
#include <limits>
#include <numeric>

#include "ui/views/graphwidget.h"
#include "ui/views/graphwidget_p.h"
#include "core/nicestep.h"
#include "ui/views/graphflags.h"
#include "core/siprefix.h"
#include "ui/dialogs/notedlg.h"
#include "ui/views/graphstatestrip.h"

QString GraphWidget::stateWords(quint32 flags, Quality quality)
{
  QStringList words;
  if (quality == Quality::Overload)
    words << QStringLiteral("OL");
  if (flags & SampleFlag::Hold)
    words << QStringLiteral("HOLD");
  if (flags & SampleFlag::Peak)
    words << QStringLiteral("PEAK");
  if (flags & SampleFlag::Min)
    words << QStringLiteral("MIN");
  if (flags & SampleFlag::Max)
    words << QStringLiteral("MAX");
  if (flags & SampleFlag::Relative)
    words << QStringLiteral("REL");
  return words.join(' ');
}

std::pair<QString, Qt::BrushStyle> GraphWidget::gapCause(const RawPoint &p)
{
  if (p.quality == Quality::Fault)
    return { tr("fault"), Qt::FDiagPattern };
  if (p.quality == Quality::NoValue)
    return { tr("no number"), Qt::Dense5Pattern };
  if (p.flags & SampleFlag::GapPause)
    return { tr("paused"), Qt::HorPattern };
  if (p.flags & SampleFlag::GapLost)
    return { tr("connection lost"), Qt::DiagCrossPattern };
  return { tr("no value"), Qt::BDiagPattern };
}

QList<GraphWidget::StateSpan> GraphWidget::stateSpans() const
{
  QList<StateSpan> out;
  if (!m_statesOn)
    return out;
  // the value on the left axis, as the hover line and the reference line
  const qint64 from = qRound64(m_xAxis->min() * 1000), to = qRound64(m_xAxis->max() * 1000);
  const RefSeries ref = refSeries(qMin(to, m_store->duration()));
  if (!ref.series || ref.series->isEmpty())
    return out;
  const RecordingSeries &s = *ref.series;
  const qint64 end = qMax(ref.store->duration(), s.last().t) + ref.shift;
  // the open stretch of each kind; a reading holds until the next one
  int open[GraphStateStrip::KindCount];
  std::fill(std::begin(open), std::end(open), -1);
  auto put = [&](int kind, const QString &label, qint64 t0, qint64 t1, Qt::BrushStyle pattern = Qt::SolidPattern)
  {
    if (open[kind] >= 0 && out[open[kind]].label == label && out[open[kind]].to >= t0)
      out[open[kind]].to = t1;
    else
    {
      open[kind] = int(out.size());
      out << StateSpan{ kind, label, t0, t1, pattern };
    }
  };
  for (int i = qMax(0, s.holding(from - ref.shift)); i < s.count(); ++i)
  {
    const qint64 t0 = s.at(i).t + ref.shift;
    if (t0 > to)
      break;
    const qint64 t1 = i + 1 < s.count() ? s.at(i + 1).t + ref.shift : qMax(t0, end);
    const quint32 flags = s.at(i).flags;
    if (s.at(i).quality == Quality::Overload)
      put(GraphStateStrip::Overload, QStringLiteral("OL"), t0, t1);
    else if (s.at(i).gap() && t1 > t0)
    {
      // a gap, by its cause
      const auto [word, pattern] = gapCause(s.at(i));
      put(GraphStateStrip::Gap, word, t0, t1, pattern);
    }
    if (flags & SampleFlag::Hold)
      put(GraphStateStrip::Hold, QStringLiteral("HOLD"), t0, t1);
    if (flags & (SampleFlag::Peak | SampleFlag::Min | SampleFlag::Max))
      put(GraphStateStrip::Extreme, flags & SampleFlag::Peak ? QStringLiteral("PEAK")
                                    : (flags & SampleFlag::Min) && (flags & SampleFlag::Max) ? QStringLiteral("MIN MAX")
                                    : flags & SampleFlag::Min ? QStringLiteral("MIN") : QStringLiteral("MAX"), t0, t1);
    if (flags & SampleFlag::Relative)
      put(GraphStateStrip::Relative, QStringLiteral("REL"), t0, t1);
  }
  return out;
}

QString GraphWidget::statesText() const
{
  QStringList parts;
  for (const StateSpan &s : stateSpans())
    parts << QString("%1 %2-%3").arg(s.label).arg(s.from / 1000.0, 0, 'f', 1).arg(s.to / 1000.0, 0, 'f', 1);
  return parts.join(" | ");
}

void GraphWidget::hideCrosshair()
{
  m_crosshairVLine->setVisible(false);
  m_crosshairHLine->setVisible(false);
  if (m_cursorLabel)
    m_cursorLabel->hide();
}

double GraphWidget::valueAt(const RecordingSeries *series, qint64 t, const RecordingStore *store) const
{
  const int i = series ? series->holding(t) : -1;
  if (i < 0 || t > (store ? store : m_store)->duration() || series->at(i).gap())
    return std::nan("");
  return series->at(i).value;
}

int GraphWidget::drawnExtra(int id, qint64 t, bool leftOnly) const
{
  // the series whose reading holds at t is the one with the latest reading
  // up to t: one that went on after another (moved to the end) has an older
  // one, its gap where the other began
  int best = -1;
  qint64 bestT = -1;
  for (int i = 0; i < m_store->extraCount() && i < int(m_extraCurves.size()); ++i)
  {
    const RecordingSeries &s = m_store->extra(i);
    const ExtraCurve &c = m_extraCurves[size_t(i)];
    const int j = s.id == id && c.shown && !(leftOnly && c.side > 0) ? s.holding(t) : -1;
    if (j >= 0 && s.at(j).t >= bestT)
    {
      best = i;
      bestT = s.at(j).t;
    }
  }
  return best;
}

GraphWidget::RefSeries GraphWidget::refSeries(qint64 t) const
{
  RefSeries r;
  const int id = referenceId();
  if (id >= 0)
  {
    r.store = m_store;
    const int i = id > 0 ? drawnExtra(id, t, true) : -1;
    r.series = id == 0 ? &m_store->series() : i < 0 ? nullptr : &m_store->extra(i);
    return r;
  }
  // another meter's on the left: its series that holds at t, on its time
  const int other = referenceOther();
  if (other < 0)
    return r;
  const OtherCurve &c = m_otherCurves[size_t(other)];
  r.store = c.store;
  r.shift = m_store->offsetOf(*c.store);
  if (c.value.id == 0)
    r.series = &c.store->series();
  else
  {
    qint64 latest = -1;
    for (int j = 0; j < c.store->extraCount(); ++j)
    {
      const RecordingSeries &s = c.store->extra(j);
      const int k = s.id == c.value.id ? s.holding(t - r.shift) : -1;
      if (k >= 0 && s.at(k).t >= latest)
      {
        r.series = &s;
        latest = s.at(k).t;
      }
    }
  }
  return r;
}

void GraphWidget::setTimeCursor(int which, qint64 t)
{
  m_cursorT[which & 1] = t < 0 ? -1 : qBound(m_store->origin(), t, qMax(m_store->origin(), m_store->duration()));
  updateTimeCursors();
}

void GraphWidget::clearTimeCursors()
{
  m_cursorT[0] = m_cursorT[1] = -1;
  updateTimeCursors();
}

// "250 ms", "12.5 s", "1:05:09"
static QString spanText(qint64 ms)
{
  if (ms < 1000)
    return QString("%1 ms").arg(ms);
  if (ms < 60000)
    return QString("%1 s").arg(QString::number(ms / 1000.0, 'f', ms < 10000 ? 3 : 2));
  return durationText(qRound64(ms / 1000.0));
}

QString GraphWidget::withUnit(double v, const QString &unit)
{
  if (!std::isfinite(v))
    return QStringLiteral("--");
  QString prefix;
  const double x = SiPrefix::scale(v, unit, &prefix);
  return QString("%1 %2%3").arg(QString::number(x, 'g', 4), prefix, unit).trimmed();
}

QString GraphWidget::ColumnScale::text(double v) const
{
  if (!std::isfinite(v))
    return QStringLiteral("--");
  return (QString::number(v * factor, 'f', decimals) + ' ' + unit).trimmed();
}

GraphWidget::ColumnScale GraphWidget::columnScale(const QList<double> &values, const QList<double> &fine, const QString &unit)
{
  ColumnScale c;
  c.unit = unit;
  double top = 0;
  for (double v : values + fine)
    if (std::isfinite(v))
      top = qMax(top, std::abs(v));
  if (top <= 0)
    return c;   // nothing but zeros: "0 V"
  QString prefix;
  const double scaled = std::abs(SiPrefix::scale(top, unit, &prefix));
  c.factor = scaled / top;
  c.unit = prefix + unit;
  c.decimals = qBound(0, 3 - int(std::floor(std::log10(scaled))), 6);
  double low = 0;
  for (double v : fine)
    if (std::isfinite(v) && v != 0 && (low == 0 || std::abs(v) < low))
      low = std::abs(v);
  if (low > 0)
    c.decimals = qMin(6, qMax(c.decimals, 1 - int(std::floor(std::log10(low * c.factor)))));
  return c;
}

QList<std::pair<int, const RecordingSeries *>> GraphWidget::drawnSeries(qint64 t) const
{
  // the main one first; of each value the series that holds at the time
  QList<std::pair<int, const RecordingSeries *>> out;
  if (m_mainShown)
    out << std::pair{ 0, &m_store->series() };
  for (int id : shownValues())
    if (const int i = drawnExtra(id, t, false); i >= 0)
      out << std::pair{ id, &m_store->extra(i) };
  return out;
}

QList<GraphWidget::StatCurve> GraphWidget::statCurves(qint64 t) const
{
  QList<StatCurve> out;
  for (const auto &[id, s] : drawnSeries(t))
    out << StatCurve{ id, s, m_store, 0, curveName(*s, s == &m_store->series() ? m_store->unit() : s->unit), seriesUnit(s),
                      id == 0 ? dataColor() : extraPen(s->id).color() };
  for (int i = 0; i < int(m_otherCurves.size()); ++i)
  {
    const OtherCurve &c = m_otherCurves[size_t(i)];
    if (!c.shown || !c.store)
      continue;
    // its series that holds at t, on its store's time
    const qint64 shift = m_store->offsetOf(*c.store);
    const RecordingSeries *series = c.value.id == 0 ? &c.store->series() : nullptr;
    qint64 latest = -1;
    for (int j = 0; c.value.id != 0 && j < c.store->extraCount(); ++j)
    {
      const RecordingSeries &s = c.store->extra(j);
      const int k = s.id == c.value.id ? s.holding(t - shift) : -1;
      if (k >= 0 && s.at(k).t >= latest)
      {
        series = &s;
        latest = s.at(k).t;
      }
    }
    if (series)
      out << StatCurve{ -1, series, c.store, shift, c.name, c.value.id == 0 ? c.unit : SiPrefix::displayText(c.unit), otherColor(i) };
  }
  return out;
}

QString GraphWidget::seriesUnit(const RecordingSeries *s) const
{
  return s == &m_store->series() ? m_store->unit() : SiPrefix::displayText(s->unit);
}

void GraphWidget::updateTimeCursors()
{
  if (!m_cursorLine[0])
    return;
  const QRectF plot = m_chart->plotArea();
  const double min = m_xAxis->min(), max = m_xAxis->max();
  const double yMin = m_yAxis->min(), yMax = m_yAxis->max();
  for (int i = 0; i < 2; ++i)
  {
    const double x = m_cursorT[i] / 1000.0;
    const bool shown = m_cursorT[i] >= 0 && max > min && x >= min && x <= max;
    m_cursorLine[i]->setVisible(shown);
    m_cursorName[i]->setVisible(false);   // placed below, with the other flag
    // the level: only for a value inside the scale (none in a gap)
    const RefSeries r = shown ? refSeries(m_cursorT[i]) : RefSeries();
    const double v = r.series ? valueAt(r.series, m_cursorT[i] - r.shift, r.store) : qQNaN();
    const bool level = std::isfinite(v) && yMax > yMin && v >= yMin && v <= yMax;
    m_cursorLevel[i]->setVisible(level);
    if (level)
    {
      const double py = plot.bottom() - (v - yMin) / (yMax - yMin) * plot.height();
      m_cursorLevel[i]->setLine(plot.left(), py, plot.right(), py);
    }
    if (!shown)
      continue;
    const double px = plot.left() + (x - min) / (max - min) * plot.width();
    m_cursorLine[i]->setLine(px, plot.top(), px, plot.bottom());
  }
  updateFlags();
  updateViewButtons();   // Remove time cursors: greyed without
  scheduleStats();
}

double GraphWidget::referenceY() const
{
  const double min = m_yAxis->min(), max = m_yAxis->max();
  const QRectF plot = m_chart->plotArea();
  if (m_referenceKind == RefOff || leftValue().id < 0 || !(m_reference > min && m_reference < max) || plot.height() <= 0)
    return std::nan("");
  return plot.bottom() - (m_reference - min) / (max - min) * plot.height();
}

bool GraphWidget::referenceShown() const
{
  return std::isfinite(referenceY());
}

RecordingStore::Stats GraphWidget::windowStats() const
{
  // the reference's series at the end of the window
  const qint64 from = qRound64(m_xAxis->min() * 1000), to = qRound64(m_xAxis->max() * 1000);
  const RefSeries r = refSeries(qMin(to, m_store->duration()));
  return r.series ? r.store->statsOf(*r.series, from - r.shift, to - r.shift, outlierSkip(*r.series)) : RecordingStore::Stats();
}

void GraphWidget::setReference(ReferenceKind kind, double value)
{
  if (kind == RefValue && value == 0)
    kind = RefZero;
  const bool changed = kind != m_referenceKind || ((kind == RefValue || kind == RefAlarm) && value != m_reference);
  m_referenceKind = kind;
  if (kind == RefZero)
    m_reference = 0;
  else if (kind == RefValue || kind == RefAlarm)
    m_reference = value;
  updateReferenceLine();
  updateViewButtons();
  if (changed)
    Q_EMIT referenceChanged();
}

QString GraphWidget::referenceSetting() const
{
  switch (m_referenceKind)
  {
    case RefMean:  return QStringLiteral("mean");
    case RefMin:   return QStringLiteral("min");
    case RefMax:   return QStringLiteral("max");
    case RefAlarm: return QStringLiteral("alarm ") + QString::number(m_reference, 'g', 12);
    case RefValue: return QString::number(m_reference, 'g', 12);
    case RefOff:   return QStringLiteral("off");
    case RefZero:  break;
  }
  return QString();
}

void GraphWidget::setReferenceSetting(const QString &text)
{
  const QString t = text.trimmed();
  bool ok = false;
  if (t == QLatin1String("mean"))
    setReference(RefMean);
  else if (t == QLatin1String("min"))
    setReference(RefMin);
  else if (t == QLatin1String("max"))
    setReference(RefMax);
  else if (t == QLatin1String("off"))
    setReference(RefOff);
  else if (const double v = t.section(' ', 1).toDouble(&ok); t.startsWith(QLatin1String("alarm ")) && ok)
    setReference(RefAlarm, v);
  else if (const double v = t.toDouble(&ok); ok && std::isfinite(v))
    setReference(RefValue, v);
  else
    setReference(RefZero);
}

void GraphWidget::setAlarmThresholds(const QList<double> &values)
{
  m_alarmThresholds = values;
  if (m_referenceKind == RefAlarm && !values.contains(m_reference))
    m_referenceKind = RefValue;
  updateReferenceLine();
}

void GraphWidget::setLimits(const ValueRef &value, double low, double high)
{
  m_limitValue = value;
  m_limitLow = low;
  m_limitHigh = high;
  updateLimitBand();
}

void GraphWidget::updateLimitBand()
{
  const double min = m_yAxis->min(), max = m_yAxis->max();
  const QRectF plot = m_chart->plotArea();
  const ValueRef ref = leftValue();
  // another value on the left axis: its limit band, if it has one
  if (ref != m_lastReference)
  {
    m_lastReference = ref;
    Q_EMIT leftValueChanged(ref);
  }
  const bool on = m_limitValue.id >= 0 && m_limitValue == m_lastReference && max > min && plot.height() > 0;
  auto sceneY = [&](double v) { return plot.bottom() - (v - min) / (max - min) * plot.height(); };
  const double bounds[2] = { m_limitLow, m_limitHigh };
  for (int i = 0; i < 2; ++i)
  {
    const double v = bounds[i];
    const bool shown = on && std::isfinite(v) && v > min && v < max;
    m_limitLines[i]->setVisible(shown);
    if (shown)
      m_limitLines[i]->setLine(plot.left(), sceneY(v), plot.right(), sceneY(v));
  }
  // an open side reaches the edge; nothing of the band in the scale: none
  const double lo = std::isfinite(m_limitLow) ? m_limitLow : -std::numeric_limits<double>::infinity();
  const double hi = std::isfinite(m_limitHigh) ? m_limitHigh : std::numeric_limits<double>::infinity();
  const double top = std::min(hi, max), bottom = std::max(lo, min);
  m_limitBand->setVisible(on && top > bottom);
  if (m_limitBand->isVisible())
    m_limitBand->setRect(QRectF(QPointF(plot.left(), sceneY(top)), QPointF(plot.right(), sceneY(bottom))));
}

void GraphWidget::dragReference(double y)
{
  const QRectF plot = m_chart->plotArea();
  const double min = m_yAxis->min(), max = m_yAxis->max();
  if (!(max > min) || plot.height() <= 0)
    return;
  auto sceneY = [&](double v) { return plot.bottom() - (v - min) / (max - min) * plot.height(); };
  QList<std::pair<ReferenceKind, double>> snaps { { RefZero, 0.0 } };
  const RecordingStore::Stats st = windowStats();
  if (st.count > 0)
    snaps << std::pair{ RefMean, st.mean } << std::pair{ RefMin, st.min } << std::pair{ RefMax, st.max };
  // the alarms drawn are the main value's
  for (double a : referenceId() == 0 ? m_alarmThresholds : QList<double>())
    snaps << std::pair{ RefAlarm, a };
  if (m_limitLines[0]->isVisible())
    snaps << std::pair{ RefValue, m_limitLow };
  if (m_limitLines[1]->isVisible())
    snaps << std::pair{ RefValue, m_limitHigh };
  for (const auto &[kind, v] : snaps)
    if (std::abs(sceneY(v) - y) <= 6)
    {
      setReference(kind, v);
      return;
    }
  setReference(RefValue, roundedValueAt(y));
}

void GraphWidget::updateReferenceLine()
{
  if (!m_referenceLine)
    return;
  // the limit band moves with the same axis
  updateLimitBand();
  // the window's statistics follow the readings and the window
  if (m_referenceKind == RefMean || m_referenceKind == RefMin || m_referenceKind == RefMax)
  {
    const RecordingStore::Stats st = windowStats();
    m_reference = m_referenceKind == RefMean ? st.mean : m_referenceKind == RefMin ? st.min : st.max;
  }
  const double y = referenceY();
  const bool shown = std::isfinite(y);
  // phosphor draws it with its centre axis
  m_referenceLine->setVisible(shown && !phosphor());
  m_referenceLabel->setVisible(shown);
  updateCentreTicks();
  if (!shown)
  {
    updateExtraReferences();
    return;
  }
  const QRectF plot = m_chart->plotArea();
  m_referenceLine->setLine(plot.left(), y, plot.right(), y);
  QString what;
  switch (m_referenceKind)
  {
    case RefMean:  what = tr("mean") + ' '; break;
    case RefMin:   what = tr("min") + ' '; break;
    case RefMax:   what = tr("max") + ' '; break;
    case RefAlarm: what = tr("alarm") + ' '; break;
    default: break;
  }
  m_referenceText = what + referenceText(m_reference);
  placeReferenceLabel(m_referenceLabel, y, m_referenceText);
  updateExtraReferences();
}

// as the y labels: 4 digits with the prefix that suits the value
QString GraphWidget::referenceText(double value) const
{
  QString prefix;
  const QString base = leftUnit();
  const double v = SiPrefix::scale(value, base, &prefix);
  return base.isEmpty() ? QString::number(v, 'g', 4) : QString("%1 %2%3").arg(QString::number(v, 'g', 4), prefix, base);
}

void GraphWidget::placeReferenceLabel(QGraphicsSimpleTextItem *label, double y, const QString &text)
{
  const QRectF plot = m_chart->plotArea();
  label->setText(text);
  label->setFont(m_yAxis->labelsFont());
  const QRectF r = label->boundingRect();
  // above the line, unless that leaves the plot
  const double top = y - r.height() - 1 >= plot.top() ? y - r.height() - 1 : y + 1;
  label->setPos(plot.right() - r.width() - 3, top);
}

void GraphWidget::setExtraReferences(const QList<double> &values)
{
  QList<double> v;
  for (double x : values)
    if (std::isfinite(x) && v.size() < kMaxExtraReferences)
      v << x;
  if (v == m_extraRefs)
    return;
  m_extraRefs = v;
  updateExtraReferences();
  Q_EMIT referenceChanged();
}

// The added lines as the reference line, dashed apart from it; then the
// crossings of all of them
void GraphWidget::updateExtraReferences()
{
  while (m_extraRefItems.size() < size_t(m_extraRefs.size()))
  {
    ExtraRefItem item;
    item.line = new QGraphicsLineItem(m_chart);
    item.line->setZValue(5);
    item.label = new QGraphicsSimpleTextItem(m_chart);
    item.label->setZValue(5);
    m_extraRefItems.push_back(item);
  }
  const QRectF plot = m_chart->plotArea();
  const double min = m_yAxis->min(), max = m_yAxis->max();
  QPen pen = m_referenceLine->pen();
  pen.setStyle(Qt::DashLine);
  for (size_t i = 0; i < m_extraRefItems.size(); ++i)
  {
    const ExtraRefItem &item = m_extraRefItems[i];
    const bool shown = int(i) < m_extraRefs.size() && leftValue().id >= 0 && plot.height() > 0
                       && m_extraRefs[int(i)] > min && m_extraRefs[int(i)] < max;
    item.line->setVisible(shown);
    item.label->setVisible(shown);
    if (!shown)
      continue;
    const double y = plot.bottom() - (m_extraRefs[int(i)] - min) / (max - min) * plot.height();
    item.line->setPen(pen);
    item.line->setLine(plot.left(), y, plot.right(), y);
    item.label->setBrush(m_referenceLabel->brush());
    placeReferenceLabel(item.label, y, referenceText(m_extraRefs[int(i)]));
  }
  updateCrossings();
}

QList<std::pair<double, bool>> GraphWidget::crossingsOf(double value) const
{
  QList<std::pair<double, bool>> out;
  const QList<QPointF> pts = referencePoints();
  const double x0 = m_xAxis->min(), x1 = m_xAxis->max();
  const double width = m_chart->plotArea().width();
  if (pts.size() < 2 || !(x1 > x0) || width <= 0)
    return out;
  const double merge = 3 * (x1 - x0) / width;   // 3 pixels
  auto it = std::lower_bound(pts.cbegin(), pts.cend(), x0, [](const QPointF &p, double x) { return p.x() < x; });
  if (it != pts.cbegin())
    --it;
  for (auto next = it + 1; next != pts.cend() && it->x() <= x1; it = next++)
  {
    const double a = it->y(), b = next->y();
    if (!std::isfinite(a) || !std::isfinite(b) || (a > value) == (b > value))
      continue;
    // where the line between them meets the value
    const double t = it->x() + (next->x() - it->x()) * (value - a) / (b - a);
    if (t < x0 || t > x1)
      continue;
    if (!out.isEmpty() && t - out.last().first < merge)
      out.last() = { t, b > a };
    else
      out << std::pair{ t, b > a };
  }
  return out;
}

int GraphWidget::crossingCount(int line) const
{
  return line + 1 < m_crossCounts.size() ? m_crossCounts[line + 1] : 0;
}

// A small triangle at each crossing, pointing the way the curve went, and
// the count in the line's label
void GraphWidget::updateCrossings()
{
  if (!m_crossMarks)
    return;
  m_crossCounts.clear();
  QPainterPath marks;
  const QRectF plot = m_chart->plotArea();
  const double x0 = m_xAxis->min(), x1 = m_xAxis->max();
  auto lineOf = [&](bool shown, double value, QGraphicsSimpleTextItem *label, const QString &text)
  {
    QList<std::pair<double, bool>> found;
    if (shown && m_crossingsOn)
      found = crossingsOf(value);
    m_crossCounts << int(found.size());
    if (!shown)
      return;
    const double y = label == m_referenceLabel ? referenceY() : plot.bottom() - (value - m_yAxis->min())
                                                                   / (m_yAxis->max() - m_yAxis->min()) * plot.height();
    placeReferenceLabel(label, y, found.isEmpty() ? text : QString("%1 · %2×").arg(text).arg(found.size()));
    for (const auto &[t, up] : found)
    {
      const double x = plot.left() + (t - x0) / (x1 - x0) * plot.width();
      const double d = up ? 1 : -1;   // the tip up when it rose (scene y grows down)
      QPolygonF tri({ QPointF(x - 5, y - d * 2), QPointF(x + 5, y - d * 2), QPointF(x, y - d * 11) });
      marks.addPolygon(tri);
      marks.closeSubpath();
    }
  };
  // not on the window's mean, minimum or maximum: they follow the curve,
  // a noisy one crosses its mean at every other reading
  const bool follows = m_referenceKind == RefMean || m_referenceKind == RefMin || m_referenceKind == RefMax;
  lineOf(m_referenceLabel->isVisible() && !follows, m_reference, m_referenceLabel, m_referenceText);
  for (int i = 0; i < m_extraRefs.size() && i < int(m_extraRefItems.size()); ++i)
    lineOf(m_extraRefItems[size_t(i)].label->isVisible(), m_extraRefs[i], m_extraRefItems[size_t(i)].label,
           referenceText(m_extraRefs[i]));
  // hollow, in the lettering's colour: seen on a curve of any colour
  const QColor ink = m_referenceLabel->brush().color();
  m_crossMarks->setBrush(ink.lightness() > 128 ? Qt::black : Qt::white);
  m_crossMarks->setPen(QPen(ink, 1.5));
  m_crossMarks->setPath(marks);
  m_crossMarks->setVisible(!marks.isEmpty());
}

void GraphWidget::setCrossingsVisible(bool on)
{
  m_crossingsOn = on;
  updateCrossings();
}

double GraphWidget::roundedValueAt(double y) const
{
  const QRectF plot = m_chart->plotArea();
  const double min = m_yAxis->min(), max = m_yAxis->max();
  const double step = niceStep((max - min) / 100);
  const double v = min + (plot.bottom() - y) / plot.height() * (max - min);
  return std::round(v / step) * step;
}

void GraphWidget::updateThresholdLinesVisibility()
{
  // the trigger compares the main value: on its axis, hidden with it
  // on the main value's axis: a trigger on another value has no line there
  m_triggerLine->setVisible((mode() == Raising || mode() == Falling) && m_mainShown && m_store->triggerValue().id == 0);
  m_integrationLine->setVisible(m_showIntegration && (m_intValueId != 0 || m_mainShown));

  updateThresholdLinePositions();
}

void GraphWidget::updateThresholdLinePositions()
{
  QRectF plot = m_chart->plotArea();

  // the trigger compares the main value; the integration threshold is on
  // the axis of the value integrated
  auto positionLine = [&](QGraphicsLineItem *item, double value, QLineSeries *on)
  {
    if (!item->isVisible())
      return;
    double y = m_chart->mapToPosition(QPointF(m_xAxis->min(), value), on).y();
    item->setLine(plot.left(), y, plot.right(), y);
  };

  positionLine(m_triggerLine, mode() == Raising ? m_store->raisingThreshold() : m_store->fallingThreshold(),
               m_dataSeries);
  positionLine(m_integrationLine, m_store->integrationThreshold(), m_intLine->first());
  updateMarkPositions();
}

// The flags of the time cursors and of the marks share the row of the
// time labels: laid out together, they never cover each other. A mark's
// flag carries its number and, as room allows, its name.
void GraphWidget::updateFlags()
{
  if (!m_cursorName[0])
    return;
  const QRectF plot = m_chart->plotArea();
  const QFont font = m_xAxis->labelsFont();
  const QFontMetricsF fm(font);
  QList<GraphFlags::Flag> flags;
  QList<GraphFlagItem *> items;
  for (int i = 0; i < 2; ++i)
  {
    m_cursorName[i]->setVisible(false);
    if (m_cursorLine[i]->isVisible())
    {
      flags << GraphFlags::Flag{ m_cursorLine[i]->line().x1(), m_cursorName[i]->text(), {}, {}, {} };
      items << m_cursorName[i];
    }
  }
  const QList<RecordingStore::Mark> marks = m_store->marks();
  // "Alarm Peak" says "Peak" on the flag: every mark is an alarm's, the
  // word would take the room (the tooltip keeps it)
  const QString alarmWord = QCoreApplication::translate("Alarm", "Alarm") + ' ';
  for (int i = 0; i < m_markFlags.size() && i < m_marks.size() && i < marks.size(); ++i)
  {
    m_markFlags[i]->setVisible(false);
    if (m_marks[i]->isVisible())
    {
      QString note = marks[i].name;
      if (note.startsWith(alarmWord, Qt::CaseInsensitive) && note.size() > alarmWord.size())
        note = note.mid(alarmWord.size());
      flags << GraphFlags::Flag{ m_marks[i]->line().x1(), QString::number(i + 1), note, {}, {} };
      items << m_markFlags[i];
    }
  }
  QList<int> order(flags.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return flags[a].x < flags[b].x; });
  QList<GraphFlags::Flag> sorted;
  for (int i : order)
    sorted << flags[i];
  GraphFlags::layout(sorted, plot, fm.height() + 2, [&](const QString &t) { return fm.horizontalAdvance(t); });
  for (qsizetype j = 0; j < sorted.size(); ++j)
  {
    GraphFlagItem *flag = items[order[j]];
    flag->setFont(font);
    flag->setText(sorted[j].text);
    flag->setPlace(sorted[j].rect, sorted[j].x);
    flag->setVisible(!sorted[j].rect.isEmpty());
  }
}

void GraphWidget::addMark(const QColor &color, const QString &name)
{
  m_store->addMark(color.rgba(), name);
}

namespace
{
// "14:03:12 (0:42 into the recording)"
QString markWhen(const RecordingStore *store, qint64 t)
{
  return GraphWidget::tr("%1 (%2 into the recording)")
    .arg(store->startDateTime().addMSecs(t).toString("HH:mm:ss"), durationText(t / 1000));
}
}

int GraphWidget::askNote(qint64 t)
{
  if (t < 0)
    t = m_store->markTime();
  NoteDlg dlg(tr("At %1").arg(markWhen(m_store, t)), this);
  if (dlg.exec() != QDialog::Accepted || dlg.title().isEmpty())
    return -1;
  const int index = addNote(t, dlg.title(), dlg.text());
  Q_EMIT noteAdded(m_store->startDateTime().addMSecs(t), dlg.title());
  return index;
}

int GraphWidget::addNote(qint64 t, const QString &title, const QString &note)
{
  return m_store->addNote(t, title, note);
}

bool GraphWidget::editMark(int index)
{
  const QList<RecordingStore::Mark> marks = m_store->marks();
  if (index < 0 || index >= marks.size())
    return false;
  NoteDlg dlg(tr("Mark %1 at %2").arg(index + 1).arg(markWhen(m_store, marks[index].t)), this);
  dlg.setTitle(marks[index].name);
  dlg.setText(marks[index].note);
  if (dlg.exec() != QDialog::Accepted || dlg.title().isEmpty())
    return false;
  m_store->setMarkText(index, dlg.title(), dlg.text());
  return true;
}

int GraphWidget::markAt(const QPoint &viewPos) const
{
  const QPointF scenePos = m_chartView->mapToScene(viewPos);
  for (int i = 0; i < m_markFlags.size(); ++i)
    if (m_markFlags[i]->isVisible() && m_markFlags[i]->mapRectToScene(m_markFlags[i]->tagRect()).contains(scenePos))
      return i;
  return -1;
}

bool GraphWidget::markDraggable(int index) const
{
  const QList<RecordingStore::Mark> marks = m_store->marks();
  // alarm and trigger marks tell when something happened: they stay
  return index >= 0 && index < marks.size() && marks[index].kind == RecordingStore::MarkKind::Note;
}

bool GraphWidget::dragMark(const QPoint &pos)
{
  if (!markDraggable(m_pressedMark))
    return false;
  if (!m_markDragging)
  {
    if (qAbs(pos.x() - m_pressPos.x()) <= 4)
      return false;
    m_markDragging = true;
    m_markDragFrom = m_store->marks().at(m_pressedMark).t;
    QToolTip::hideText();
  }
  const QPointF scenePos = m_chartView->mapToScene(pos);
  const RecordingSeries &series = m_store->series();
  const qint64 end = qMax(m_store->duration(), series.isEmpty() ? qint64(0) : series.last().t);
  const qint64 t = qBound(qint64(0), qRound64(m_chart->mapToValue(scenePos, m_dataSeries).x() * 1000), end);
  m_pressedMark = m_store->setMarkTime(m_pressedMark, t);
  m_cursorLabel->setText(markWhen(m_store, t));
  placeCursorLabel();
  return true;
}

void GraphWidget::endMarkDrag(bool cancel)
{
  if (m_markDragging && m_pressedMark >= 0 && m_pressedMark < m_store->marks().size())
  {
    if (cancel)
      m_store->setMarkTime(m_pressedMark, m_markDragFrom);
    else
    {
      const RecordingStore::Mark &m = m_store->marks().at(m_pressedMark);
      if (m.t != m_markDragFrom)
        Q_EMIT noteMoved(m_store->startDateTime().addMSecs(m_markDragFrom), m_store->startDateTime().addMSecs(m.t),
                         m.name);
    }
    m_cursorLabel->hide();
  }
  m_markDragging = false;
  m_pressedMark = -1;
}

void GraphWidget::showMarkNote(int index, const QPoint &globalPos)
{
  const QList<RecordingStore::Mark> marks = m_store->marks();
  if (index < 0 || index >= marks.size())
    return;
  const RecordingStore::Mark &m = marks[index];
  QString html = QString("<b>%1 · %2</b><br>%3").arg(index + 1).arg(m.name.toHtmlEscaped(), markWhen(m_store, m.t));
  if (!m.note.isEmpty())
    html += "<p>" + m.note.toHtmlEscaped().replace('\n', "<br>") + "</p>";
  QToolTip::showText(globalPos, html, m_chartView);
}

void GraphWidget::syncMarks()
{
  qDeleteAll(m_marks);
  m_marks.clear();
  qDeleteAll(m_markFlags);
  m_markFlags.clear();
  for (const RecordingStore::Mark &m : m_store->marks())
  {
    auto *line = new QGraphicsLineItem(m_chart);
    line->setPen(QPen(QColor::fromRgba(m.color), 2, Qt::DashLine));
    line->setZValue(999);
    const QString tip = m.note.isEmpty() ? m.name : m.name + '\n' + m.note;
    line->setToolTip(tip);
    m_marks << line;
    auto *flag = new GraphFlagItem(m_chart);
    flag->setColor(QColor::fromRgba(m.color));
    flag->setToolTip(tip);
    flag->setZValue(999);
    flag->setData(0, QStringLiteral("mark-flag"));
    flag->setVisible(false);
    m_markFlags << flag;
  }
  updateMarkPositions();
  updateCentreTicks();   // phosphor: the trigger's mark is the vertical centre
}

void GraphWidget::updateMarkPositions()
{
  const QRectF plot = m_chart->plotArea();
  const QList<RecordingStore::Mark> marks = m_store->marks();
  for (int i = 0; i < m_marks.size() && i < marks.size(); i++)
  {
    QGraphicsLineItem *line = m_marks[i];
    const double x = marks[i].t / 1000.0;
    const bool inView = x >= m_xAxis->min() && x <= m_xAxis->max();
    line->setVisible(inView);
    if (!inView)
      continue;
    const double px = m_chart->mapToPosition(QPointF(x, m_yAxis->min()), m_dataSeries).x();
    line->setLine(px, plot.top(), px, plot.bottom());
  }
  updateFlags();
}

void GraphWidget::setThresholds(double falling, double raising)
{
  m_store->setThresholds(falling, raising);

  updateThresholdLinesVisibility();
}

void GraphWidget::scheduleStats()
{
  if (m_statsOn && !m_statsTimer.isActive())
    m_statsTimer.start();
}

void GraphWidget::updateStats()
{
  if (!m_statsLabel || !m_statsOn)
    return;
  m_statsLabel->setText(statsHtml());
  // the column follows its widest row, in steps so it does not twitch
  if (const int side = statsWidth(); m_statsPanel && side != m_statsPanel->width())
  {
    layoutParts();
    m_statsLabel->setText(statsHtml());   // the names cut for the new width
  }
  // the cursors' clock times, which the panel gives as the axis counts
  QStringList clock;
  for (int i = 0; i < 2; ++i)
    if (m_cursorT[i] >= 0)
      clock << QString("%1  %2").arg(i ? "B" : "A", m_store->startDateTime().addMSecs(m_cursorT[i]).time().toString("HH:mm:ss.zzz"));
  m_statsLabel->setToolTip(clock.join('\n'));
}

QString GraphWidget::statsText() const
{
  QTextDocument doc;
  doc.setHtml(statsHtml());
  return doc.toPlainText().replace(QChar(0xa0), ' ');   // the cells' fixed spaces
}

QString GraphWidget::statsHtml() const
{
  // nothing breaks inside a cell: a unit stays with its number
  auto solid = [](const QString &text) { return text.toHtmlEscaped().replace(' ', "&nbsp;"); };
  // a value's name in its curve's colour, cut to the column's width
  const int room = m_statsPanel && m_statsPanel->width() > 0 ? qMax(60, m_statsPanel->width() - 46) : 0;
  auto fitted = [this, room](const QString &name)
  {
    return room > 0 ? m_statsLabel->fontMetrics().elidedText(name, Qt::ElideRight, room) : name;
  };
  auto head = [&](const StatCurve &c)
  {
    return QString("<tr><td colspan=\"2\"><span style=\"color:%1\">&#9632;</span>&nbsp;%2</td></tr>")
      .arg(c.colour.name(), solid(fitted(c.name)));
  };
  // a curve's value at a time of this graph, its statistics between two
  auto at = [this](const StatCurve &c, qint64 t) { return valueAt(c.series, t - c.shift, c.store); };
  auto between = [this](const StatCurve &c, qint64 from, qint64 to)
  {
    return c.store->statsOf(*c.series, from - c.shift, to - c.shift, outlierSkip(*c.series));
  };
  // a row: the number in full, its name cut to what the column leaves; the
  // widest row says how wide the column wants to be (layoutParts())
  const QFontMetrics fm = m_statsLabel ? m_statsLabel->fontMetrics() : fontMetrics();
  const int frame = 2 * 6 + 4 + style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  int want = 0;
  auto row = [&](QString name, const QString &value)
  {
    const int number = fm.horizontalAdvance(value) + 12, indent = fm.horizontalAdvance(QStringLiteral("  "));
    want = qMax(want, indent + fm.horizontalAdvance(name) + number + frame);
    if (room > 0)
      name = fm.elidedText(name, Qt::ElideRight, qMax(30, m_statsPanel->width() - frame - indent - number));
    return QString("<tr><td>&nbsp;&nbsp;%1</td><td align=\"right\">%2</td></tr>").arg(solid(name), solid(value));
  };
  auto table = [](const QString &rows) { return "<table cellspacing=\"0\" cellpadding=\"1\" width=\"100%\">" + rows + "</table>"; };
  auto title = [](const QString &text) { return QString("<p style=\"margin-top:8px\"><b>%1</b></p>").arg(text.toHtmlEscaped()); };
  auto statRows = [&](const RecordingStore::Stats &st, const ColumnScale &c)
  {
    return row(tr("mean"), c.text(st.mean)) + row(QString(QChar(0x03c3)), c.text(st.sigma))
           + row(tr("min"), c.text(st.min)) + row(tr("max"), c.text(st.max));
  };

  QString html;
  // the cursors
  html += title(tr("Cursors"));
  if (m_cursorT[0] < 0 && m_cursorT[1] < 0)
    html += QString("<p>%1</p>").arg(tr("Click: cursor A, Shift+click: B").toHtmlEscaped());
  QString rows;
  for (int i = 0; i < 2; ++i)
    if (m_cursorT[i] >= 0)
    {
      // the time as the axis counts it (the clock time: the panel's tooltip)
      rows += QString("<tr><td colspan=\"2\"><b>%1</b>&nbsp;%2</td></tr>").arg(i ? "B" : "A", solid(axisTimeText(m_cursorT[i])));
      for (const StatCurve &c : statCurves(m_cursorT[i]))
      {
        // A and B of a value on one scale
        QList<double> both;
        for (qint64 t : m_cursorT)
          if (t >= 0)
            both << at(c, t);
        rows += row(c.name, columnScale(both, {}, c.unit).text(at(c, m_cursorT[i])));
      }
    }
  html += table(rows);

  // from A to B
  if (m_cursorT[0] >= 0 && m_cursorT[1] >= 0)
  {
    html += title(QString("A %1 B   %2 %3").arg(QChar(0x2013), QChar(0x0394) + QStringLiteral("t"),
                                                spanText(std::abs(m_cursorT[1] - m_cursorT[0]))));
    rows.clear();
    for (const StatCurve &curve : statCurves(qMax(m_cursorT[0], m_cursorT[1])))
    {
      const int id = curve.id;
      const QString base = curve.unit;
      const RecordingStore::Stats st = between(curve, m_cursorT[0], m_cursorT[1]);
      const double delta = at(curve, m_cursorT[1]) - at(curve, m_cursorT[0]);
      const ColumnScale c = st.count ? columnScale({ delta, st.mean, st.min, st.max }, { st.sigma }, base)
                                     : columnScale({ delta }, {}, base);
      rows += head(curve);
      rows += row(QString(QChar(0x0394)), c.text(delta));
      rows += statRows(st, c);
      rows += row(tr("readings"), QString::number(st.count));
      // the integral: in hours for a current or a power, x s for the
      // value of the integration curve
      const bool hours = base == QLatin1String("A") || base == QLatin1String("W") || base == QLatin1String("VA");
      if (hours || (m_showIntegration && id == m_intValueId))
        rows += row(QString(QChar(0x222b)), !st.count ? QStringLiteral("--")
                                            : hours ? withUnit(st.integral / 3600, base + "h")
                                                    : withUnit(st.integral, base.isEmpty() ? QStringLiteral("s") : base + QStringLiteral("·s")));
    }
    html += table(rows);
  }

  // the window shown
  const qint64 from = windowStart();
  const qint64 to = from + qint64(m_windowSeconds) * 1000;
  html += title(tr("Shown") + QStringLiteral("   ") + spanText(to - from)
                + (m_outliersHidden > 0 ? QStringLiteral("   ") + tr("without %n outlier(s)", nullptr, m_outliersHidden)
                                        : QString()));
  rows.clear();
  for (const StatCurve &curve : statCurves(qMin(to, m_store->duration())))
  {
    const QString base = curve.unit;
    const RecordingStore::Stats st = between(curve, from, to);
    const ColumnScale c = st.count ? columnScale({ st.mean, st.min, st.max }, { st.sigma, st.max - st.min }, base)
                                   : ColumnScale();
    rows += head(curve);
    rows += statRows(st, c);
    rows += row(tr("peak-peak"), st.count ? c.text(st.max - st.min) : QStringLiteral("--"));
    rows += row(tr("readings"), QString::number(st.count));
  }
  html += table(rows);
  m_statsWant = want;
  return html;
}

// --- outliers: hidden in the drawing and the statistics, kept recorded ---

std::function<bool(int)> GraphWidget::outlierSkip(const RecordingSeries &series) const
{
  if (m_outlierMode == OutliersShown)
    return nullptr;
  return [this, &series](int i) { return isOutlier(series, i); };
}

bool GraphWidget::isOutlier(const RecordingSeries &s, int i) const
{
  if (m_outlierMode == OutliersShown || i < 0 || i >= s.count())
    return false;
  const double v = s.at(i).value;
  if (!std::isfinite(v))
    return false;
  // a jump or limits are in a unit: other units' curves stay as they are
  // (the main series learns its unit with a known quantity: else the store's)
  const QString unit = !s.unit.isEmpty() || &s != &m_store->series() ? s.unit : m_store->unit();
  if (m_outlierMode != OutliersSigma && unit != m_outlierUnit)
    return false;
  switch (m_outlierMode)
  {
    case OutliersLimits:
      return v < m_outlierA || v > m_outlierB;
    case OutliersJump:
    {
      // a spike: away from the reading before and the one after, the same
      // way; a step to another level is away from one side only
      int before = i - 1, after = i + 1;
      while (before >= 0 && !std::isfinite(s.at(before).value))
        before--;
      while (after < s.count() && !std::isfinite(s.at(after).value))
        after++;
      if (before < 0 || after >= s.count())
        return false;
      const double up = v - s.at(before).value, down = v - s.at(after).value;
      return std::abs(up) > m_outlierA && std::abs(down) > m_outlierA && (up > 0) == (down > 0);
    }
    case OutliersSigma:
    {
      // its neighbours, without it: the mean and the spread there. Not the
      // window's - a drifting value would hide its ends, and Live would
      // draw every curve anew at each reading
      double sum = 0, sum2 = 0;
      int n = 0;
      const int from = qMax(0, i - kOutlierNeighbours), to = qMin(s.count() - 1, i + kOutlierNeighbours);
      const double ref = v;   // differences, not large values squared
      for (int j = from; j <= to; ++j)
        if (const double w = s.at(j).value; j != i && std::isfinite(w))
        {
          sum += w - ref;
          sum2 += (w - ref) * (w - ref);
          n++;
        }
      if (n < 10)
        return false;
      const double mean = sum / n;
      const double sigma = std::sqrt(qMax(0.0, sum2 / n - mean * mean));
      // mean is the neighbours' mean less v: |v - their mean| = |mean|
      return std::abs(mean) > m_outlierA * sigma && std::abs(mean) > 1e-12 * qMax(1.0, std::abs(v));
    }
    case OutliersShown:
      break;
  }
  return false;
}

void GraphWidget::setOutliers(OutlierMode mode, double a, double b, const QString &unit)
{
  m_outlierMode = mode;
  m_outlierA = mode == OutliersShown ? 3 : a;
  m_outlierB = b;
  m_outlierUnit = mode == OutliersJump || mode == OutliersLimits ? unit : QString();
  rebuildSeries();   // every curve, the band, the crossings
  countOutliers();
  scheduleStats();
}

QString GraphWidget::outlierSetting() const
{
  const QString unit = m_outlierUnit.isEmpty() ? QString() : ' ' + m_outlierUnit;
  switch (m_outlierMode)
  {
    case OutliersSigma:  return QString("sigma %1").arg(m_outlierA, 0, 'g', 12);
    case OutliersJump:   return QString("jump %1").arg(m_outlierA, 0, 'g', 12) + unit;
    case OutliersLimits: return QString("limits %1 %2").arg(m_outlierA, 0, 'g', 12).arg(m_outlierB, 0, 'g', 12) + unit;
    case OutliersShown:  break;
  }
  return QString();
}

void GraphWidget::setOutlierSetting(const QString &text)
{
  const QStringList p = text.split(' ', Qt::SkipEmptyParts);
  bool okA = false, okB = false;
  const double a = p.value(1).toDouble(&okA), b = p.value(2).toDouble(&okB);
  if (p.value(0) == QLatin1String("sigma") && okA && a > 0)
    setOutliers(OutliersSigma, a);
  else if (p.value(0) == QLatin1String("jump") && okA && a > 0)
    setOutliers(OutliersJump, a, 0, p.value(2));
  else if (p.value(0) == QLatin1String("limits") && okA && okB && b > a)
    setOutliers(OutliersLimits, a, b, p.value(3));
  else if (m_outlierMode != OutliersShown)
    setOutliers(OutliersShown);
}

void GraphWidget::countOutliers()
{
  int n = 0;
  if (m_outlierMode != OutliersShown)
  {
    const qint64 from = qRound64(m_xAxis->min() * 1000), to = qRound64(m_xAxis->max() * 1000);
    for (const StatCurve &c : statCurves(qMin(to, m_store->duration())))
      for (int i = qMax(0, c.series->lowerBound(from - c.shift)), end = c.series->lowerBound(to - c.shift + 1);
           i < end && i < c.series->count(); ++i)
        n += isOutlier(*c.series, i) ? 1 : 0;
  }
  if (n != m_outliersHidden)
  {
    m_outliersHidden = n;
    updateStateLabel();
    scheduleStats();
  }
}
