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

QString GraphWidget::timeCursorText() const
{
  auto withUnit = [](double v, const QString &unit)
  {
    if (!std::isfinite(v))
      return QStringLiteral("--");
    QString prefix;
    const double x = SiPrefix::scale(v, unit, &prefix);
    return QString("%1 %2%3").arg(QString::number(x, 'g', 4), prefix, unit).trimmed();
  };
  // the values drawn, the main one first: of each the series that holds at
  // the time
  auto drawn = [&](qint64 t)
  {
    QList<std::pair<int, const RecordingSeries *>> out;
    if (m_mainShown)
      out << std::pair{ 0, &m_store->series() };
    for (int id : shownValues())
      if (const int i = drawnExtra(id, t, false); i >= 0)
        out << std::pair{ id, &m_store->extra(i) };
    return out;
  };
  auto unitOf = [&](const RecordingSeries *s) { return s == &m_store->series() ? m_store->unit() : SiPrefix::displayText(s->unit); };
  QStringList lines;
  for (int i = 0; i < 2; ++i)
    if (m_cursorT[i] >= 0)
    {
      QString line = QString("%1  %2").arg(i ? "B" : "A", m_store->startDateTime().addMSecs(m_cursorT[i]).time().toString("HH:mm:ss.zzz"));
      for (const auto &[id, s] : drawn(m_cursorT[i]))
        line += QStringLiteral("  ") + withUnit(valueAt(s, m_cursorT[i]), unitOf(s));
      lines << line;
    }
  if (m_cursorT[0] < 0 || m_cursorT[1] < 0)
    return lines.join('\n');
  lines << QString("%1 %2").arg(QChar(0x0394) + QStringLiteral("t"), spanText(std::abs(m_cursorT[1] - m_cursorT[0])));
  // a line per value, its series the one at the later cursor
  for (const auto &[id, s] : drawn(qMax(m_cursorT[0], m_cursorT[1])))
  {
    const QString base = unitOf(s);
    const RecordingStore::Stats st = m_store->statsOf(*s, m_cursorT[0], m_cursorT[1]);
    QString line = QString("%1 %2  ").arg(QChar(0x0394), withUnit(valueAt(s, m_cursorT[1]) - valueAt(s, m_cursorT[0]), base))
                   + tr("mean %1").arg(withUnit(st.mean, base));
    // the integral: in hours for a current or a power (Ah, Wh, VAh), x s
    // for the value of the integration curve, else none
    const bool hours = base == QLatin1String("A") || base == QLatin1String("W") || base == QLatin1String("VA");
    if (hours || (m_showIntegration && id == m_intValueId))
    {
      const QString integral = hours ? withUnit(st.integral / 3600, base + "h")
                                     : withUnit(st.integral, base.isEmpty() ? QStringLiteral("s") : base + QStringLiteral("·s"));
      line += QString("  %1 %2").arg(QChar(0x222b), st.count ? integral : QStringLiteral("--"));
    }
    lines << line;
  }
  return lines.join('\n');
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
  if (!m_cursorInfo)
    return;
  const QString text = timeCursorText();
  m_cursorInfo->setVisible(!text.isEmpty());
  if (text.isEmpty())
    return;
  m_cursorInfo->setText(text);
  m_cursorInfo->adjustSize();
  // top right in the plot, below the time buttons
  const QPoint topRight = m_chartView->mapFromScene(plot.topRight());
  m_cursorInfo->move(topRight.x() - m_cursorInfo->width() - 6, topRight.y() + 4);
  m_cursorInfo->raise();
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
  return r.series ? r.store->statsOf(*r.series, from - r.shift, to - r.shift) : RecordingStore::Stats();
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
  const double step = niceStep((max - min) / 100);
  const double v = min + (plot.bottom() - y) / plot.height() * (max - min);
  setReference(RefValue, std::round(v / step) * step);
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
    return;
  const QRectF plot = m_chart->plotArea();
  m_referenceLine->setLine(plot.left(), y, plot.right(), y);
  // as the y labels: 4 digits with the prefix that suits the value
  QString prefix;
  const QString base = leftUnit();
  const double v = SiPrefix::scale(m_reference, base, &prefix);
  QString what;
  switch (m_referenceKind)
  {
    case RefMean:  what = tr("mean") + ' '; break;
    case RefMin:   what = tr("min") + ' '; break;
    case RefMax:   what = tr("max") + ' '; break;
    case RefAlarm: what = tr("alarm") + ' '; break;
    default: break;
  }
  m_referenceLabel->setText(what + (base.isEmpty() ? QString::number(v, 'g', 4)
                                                   : QString("%1 %2%3").arg(QString::number(v, 'g', 4), prefix, base)));
  m_referenceLabel->setFont(m_yAxis->labelsFont());
  const QRectF r = m_referenceLabel->boundingRect();
  // above the line, unless that leaves the plot
  const double top = y - r.height() - 1 >= plot.top() ? y - r.height() - 1 : y + 1;
  m_referenceLabel->setPos(plot.right() - r.width() - 3, top);
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
    line->setToolTip(m.name);
    m_marks << line;
    auto *flag = new GraphFlagItem(m_chart);
    flag->setColor(QColor::fromRgba(m.color));
    flag->setToolTip(m.name);
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
