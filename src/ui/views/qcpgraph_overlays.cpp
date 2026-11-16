// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: what lies over the curves to read them - the crosshair on a
// reading, the time cursors A and B - and the statistics column.
#include "ui/views/qcpgraph.h"

#include "core/siprefix.h"
#include "recording/recordingstore.h"
#include "ui/views/graphstatspanel.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/qcp.h"

#include <QElapsedTimer>
#include <QLayout>
#include <cmath>

void QcpGraph::setupOverlays()
{
  for (int i = 0; i < 2; ++i)
  {
    m_cursorLine[i] = new QCPItemStraightLine(m_plot);
    m_cursorLevel[i] = new QCPItemStraightLine(m_plot);
    m_cursorFlag[i] = new QCPItemText(m_plot);
    m_cursorFlag[i]->setText(i ? QStringLiteral("B") : QStringLiteral("A"));
    // at its time, at the top of the plot
    m_cursorFlag[i]->position->setTypeX(QCPItemPosition::ptPlotCoords);
    m_cursorFlag[i]->position->setTypeY(QCPItemPosition::ptAxisRectRatio);
    m_cursorFlag[i]->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_cursorFlag[i]->setPadding(QMargins(3, 0, 3, 0));
    for (QCPAbstractItem *item : std::initializer_list<QCPAbstractItem *>{ m_cursorLine[i], m_cursorLevel[i], m_cursorFlag[i] })
    {
      item->setSelectable(false);
      item->setVisible(false);
    }
  }
  m_statsPanel = new GraphStatsPanel(this);
  m_statsPanel->hide();
  m_statsTimer.setSingleShot(true);
  m_statsTimer.setInterval(250);
  connect(&m_statsTimer, &QTimer::timeout, this, &QcpGraph::updateStats);
  connect(m_timeBar, &GraphTimeBar::statsToggled, this, &QcpGraph::setStatsVisible);
}

int QcpGraph::leftCurve() const
{
  for (int i = 0; i < m_curves.size(); ++i)
    if (m_curves.at(i).axis == 0)
      return i;
  return -1;
}

GraphStats::Curve QcpGraph::statsCurve(int curve) const
{
  GraphStats::Curve out;
  if (curve < 0 || curve >= m_curves.size())
    return out;
  const Curve &c = m_curves.at(curve);
  // with several meters their names say whose value it is
  bool several = false;
  for (const Curve &other : m_curves)
    several = several || other.value.source != c.value.source;
  out.name = c.label.isEmpty() ? c.meter : several && !c.meter.isEmpty() ? c.meter + " · " + c.label : c.label;
  out.unit = c.unit;
  out.colour = colorOf(curve);
  out.store = c.store;
  out.parts = seriesOf(c);
  out.shift = qRound64(c.shift * 1000);
  return out;
}

GraphStats::Input QcpGraph::statsInput() const
{
  GraphStats::Input input;
  for (int i = 0; i < m_curves.size(); ++i)
    if (m_curves.at(i).axis >= 0)
      input.curves << statsCurve(i);
  input.cursor[0] = m_cursorT[0];
  input.cursor[1] = m_cursorT[1];
  input.from = windowStart();
  input.to = input.from + qint64(m_windowSeconds) * 1000;
  input.end = m_store ? m_store->duration() : 0;
  return input;
}

// On the reading of the left axis' value that holds under the mouse, or
// the next one when that is nearer: the vertical line at the mouse, the
// horizontal one on the value. In a gap it says why there is no value.
QcpPlotBase::CrosshairAt QcpGraph::crosshairAt(double x, double y)
{
  CrosshairAt at{ x, y, false, QString() };
  const qint64 t = qRound64(x * 1000);
  const GraphStats::Curve curve = statsCurve(leftCurve());
  qint64 when = t;
  const RawPoint *reading = curve.readingNear(t, &when);
  // the time as the axis counts it; the clock time in the tooltip
  at.text = GraphStats::timeText(reading ? when : t);
  m_plot->setToolTip(m_store ? m_store->startDateTime().addMSecs(reading ? when : t).time().toString("HH:mm:ss.zzz") : QString());
  if (!reading)
    return at;
  if (reading->gap())
  {
    at.text += "   " + (reading->quality == Quality::Overload ? QStringLiteral("OL") : GraphStats::gapCause(*reading));
    return at;
  }
  at.y = reading->value / m_axes.first().factor;
  at.horizontal = true;
  QString prefix;
  const QString number = SiPrefix::format(reading->value, curve.unit, &prefix);
  at.text += "   " + QString("%1 %2").arg(number, SiPrefix::displayText(prefix + curve.unit));
  // what the meter said about it: a frozen or stored value, a difference
  if (const QString state = GraphStats::stateWords(reading->flags, Quality::Valid); !state.isEmpty())
    at.text += "   " + state;
  return at;
}

QString QcpGraph::crosshairTextAt(const QPoint &pos)
{
  return crosshairAt(m_plot->xAxis->pixelToCoord(pos.x()), m_plot->yAxis->pixelToCoord(pos.y())).text;
}

void QcpGraph::setTimeCursor(int which, qint64 t)
{
  which &= 1;
  if (t >= 0 && m_store)
    t = qBound(m_store->origin(), t, qMax(m_store->origin(), m_store->duration()));
  else
    t = -1;
  if (t == m_cursorT[which])
    return;
  m_cursorT[which] = t;
  updateCursors();
  m_plot->replot(QCustomPlot::rpQueuedReplot);
  updateStats();
  Q_EMIT cursorsChanged();
}

void QcpGraph::clearTimeCursors()
{
  if (m_cursorT[0] < 0 && m_cursorT[1] < 0)
    return;
  m_cursorT[0] = m_cursorT[1] = -1;
  updateCursors();
  m_plot->replot(QCustomPlot::rpQueuedReplot);
  updateStats();
  Q_EMIT cursorsChanged();
}

// Each cursor a line at its time with its letter, and the level of the left
// axis' value there - none in a gap.
void QcpGraph::updateCursors()
{
  const GraphStats::Curve curve = statsCurve(leftCurve());
  for (int i = 0; i < 2; ++i)
  {
    const bool shown = m_cursorT[i] >= 0;
    const double x = m_cursorT[i] / 1000.0;
    m_cursorLine[i]->setVisible(shown);
    m_cursorFlag[i]->setVisible(shown);
    const double v = shown ? curve.valueAt(m_cursorT[i]) : qQNaN();
    m_cursorLevel[i]->setVisible(std::isfinite(v));
    if (!shown)
      continue;
    m_cursorLine[i]->point1->setCoords(x, 0);
    m_cursorLine[i]->point2->setCoords(x, 1);
    m_cursorFlag[i]->position->setCoords(x, 0);
    if (std::isfinite(v))
    {
      const double y = v / m_axes.first().factor;
      m_cursorLevel[i]->point1->setCoords(0, y);
      m_cursorLevel[i]->point2->setCoords(1, y);
    }
  }
}

int QcpGraph::cursorAt(const QPoint &pos) const
{
  if (!m_plot->axisRect()->rect().contains(pos))
    return -1;
  for (int i = 1; i >= 0; --i)
    if (m_cursorT[i] >= 0 && std::abs(m_plot->xAxis->coordToPixel(m_cursorT[i] / 1000.0) - pos.x()) <= 4)
      return i;
  return -1;
}

double QcpGraph::cursorLevel(int which) const
{
  const QCPItemStraightLine *level = m_cursorLevel[which & 1];
  return level->visible() ? level->point1->coords().y() : qQNaN();
}

int QcpGraph::cursorPixel(int which) const
{
  const qint64 t = m_cursorT[which & 1];
  return t < 0 ? -1 : qRound(m_plot->xAxis->coordToPixel(t / 1000.0));
}

void QcpGraph::setStatsVisible(bool on)
{
  if (on == m_statsOn)
    return;
  m_statsOn = on;
  m_timeBar->setStatsChecked(on);
  layoutStats();
  updateStats();
  Q_EMIT statsVisibleChanged(on);
}

void QcpGraph::scheduleStats()
{
  if (m_statsOn && !m_statsTimer.isActive())
    m_statsTimer.start();
}

void QcpGraph::updateStats()
{
  if (!m_statsOn)
    return;
  QElapsedTimer cost;
  cost.start();
  const GraphStats::Input input = statsInput();
  m_statsPanel->setInput(input);
  // the column follows its widest row: the names cut for the new width
  const int before = m_statsPanel->width();
  layoutStats();
  if (m_statsPanel->width() != before)
    m_statsPanel->setInput(input);
  // the cursors' clock times, which the column gives as the axis counts
  QStringList clock;
  for (int i = 0; i < 2; ++i)
    if (m_cursorT[i] >= 0 && m_store)
      clock << QString("%1  %2").arg(i ? "B" : "A", m_store->startDateTime().addMSecs(m_cursorT[i]).time().toString("HH:mm:ss.zzz"));
  m_statsPanel->setClockText(clock.join('\n'));
  // over millions of readings they take a while (about 50 ms per million
  // and curve): then less often, a tenth of the time at most
  m_statsTimer.setInterval(qMax(250, int(cost.elapsed()) * 10));
}

void QcpGraph::layoutStats()
{
  const int side = m_statsOn ? m_statsPanel->wantedWidth(width() / 2) : 0;
  const bool left = m_lookSettings.statsLeft;
  layout()->setContentsMargins(left ? side : 0, 0, left ? 0 : side, 0);
  m_statsPanel->setGeometry(left ? 0 : width() - side, 0, side, height());
  m_statsPanel->setVisible(m_statsOn);
}

QString QcpGraph::statsText() const
{
  return GraphStats::text(statsInput());
}
