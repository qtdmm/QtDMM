// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: what lies over the curves to read them - the crosshair on a
// reading, the time cursors A and B, the reference lines with their
// crossings, the limit band, the start threshold - and the statistics column.
#include "ui/views/qcpgraph.h"

#include "core/siprefix.h"
#include "recording/recordingstore.h"
#include "ui/views/graphstatspanel.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/qcp.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLayout>
#include <cmath>

namespace
{
// A word the graph on QtCharts has translated already ("mean", "alarm"):
// not through tr(), so it is not extracted a second time.
QString word(const char *text)
{
  return QCoreApplication::translate("GraphWidget", text);
}

/// Horizontal lines at values of an axis with a label at the right, a band
/// between two values and the triangles of crossings: one item draws what
/// QcpGraph::updateLevels() gave it, at the place the axes have now.
class LevelLines : public QCPAbstractItem
{
public:
  struct Line
  {
    QCPAxis *axis = nullptr;
    double   value = 0;     ///< the axis' unit
    QPen     pen;
    QString  label;         ///< empty: none
  };
  struct Mark
  {
    double t = 0;           ///< time axis
    double value = 0;       ///< left axis
    bool   up = false;
  };

  explicit LevelLines(QCustomPlot *plot) : QCPAbstractItem(plot) {}
  double selectTest(const QPointF &, bool, QVariant *) const override { return -1; }
  void clear()
  {
    lines.clear();
    marks.clear();
    band = false;
  }

  QList<Line> lines;
  QList<Mark> marks;
  bool   band = false;
  double bandLow = 0;       ///< left axis' unit; infinite: to the edge
  double bandHigh = 0;
  QBrush bandBrush;
  QColor ink;               ///< labels and the triangles' outline
  QFont  font;

protected:
  void draw(QCPPainter *painter) override
  {
    const QRectF r = parentPlot()->axisRect()->rect();
    const QCPAxis *left = parentPlot()->yAxis;
    if (band)
    {
      const double top = std::isfinite(bandHigh) ? left->coordToPixel(bandHigh) : r.top();
      const double bottom = std::isfinite(bandLow) ? left->coordToPixel(bandLow) : r.bottom();
      if (bottom > top)
        painter->fillRect(QRectF(QPointF(r.left(), qMax(top, r.top())), QPointF(r.right(), qMin(bottom, r.bottom()))), bandBrush);
    }
    painter->setFont(font);
    const QFontMetricsF metrics(font);
    for (const Line &line : std::as_const(lines))
    {
      const double y = line.axis->coordToPixel(line.value);
      if (line.pen.style() != Qt::NoPen)
      {
        painter->setPen(line.pen);
        painter->drawLine(QLineF(r.left(), y, r.right(), y));
      }
      if (line.label.isEmpty())
        continue;
      // above the line, unless that leaves the plot
      const QSizeF size = metrics.size(Qt::TextSingleLine, line.label);
      const double top = y - size.height() - 1 >= r.top() ? y - size.height() - 1 : y + 1;
      painter->setPen(ink);
      painter->drawText(QRectF(r.right() - size.width() - 3, top, size.width(), size.height()), Qt::AlignCenter, line.label);
    }
    if (marks.isEmpty())
      return;
    // hollow, in the lettering's colour: seen on a curve of any colour
    painter->setAntialiasing(true);
    painter->setPen(QPen(ink, 1.5));
    painter->setBrush(ink.lightness() > 128 ? Qt::black : Qt::white);
    for (const Mark &mark : std::as_const(marks))
    {
      const double x = parentPlot()->xAxis->coordToPixel(mark.t), y = left->coordToPixel(mark.value);
      const double d = mark.up ? 1 : -1;   // the tip up when it rose (pixels grow down)
      painter->drawPolygon(QPolygonF({ QPointF(x - 5, y - d * 2), QPointF(x + 5, y - d * 2), QPointF(x, y - d * 11) }));
    }
  }
};
}

void QcpGraph::setupOverlays()
{
  // band and lines over the grid and under the curves; labels and crossings over them
  auto *levels = new LevelLines(m_plot);
  levels->setLayer(QStringLiteral("grid"));
  levels->setSelectable(false);
  m_levels = levels;
  auto *marks = new LevelLines(m_plot);
  marks->setLayer(QStringLiteral("axes"));
  marks->setSelectable(false);
  m_levelMarks = marks;
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

ValueRef QcpGraph::leftValue() const
{
  const int curve = leftCurve();
  return curve < 0 || m_ownStore ? ValueRef(-1) : m_curves.at(curve).value;
}

void QcpGraph::setReference(const GraphReference::Line &line)
{
  if (line == m_reference)
    return;
  m_reference = line;
  m_followedAge.invalidate();
  redraw();
  Q_EMIT referenceChanged();
}

void QcpGraph::setReferenceLines(const QList<double> &values)
{
  const QList<double> lines = GraphReference::lines(values);
  if (lines == m_referenceLines)
    return;
  m_referenceLines = lines;
  redraw();
  Q_EMIT referenceChanged();
}

void QcpGraph::setCrossingsVisible(bool on)
{
  if (on == m_crossingsOn)
    return;
  m_crossingsOn = on;
  redraw();
  Q_EMIT referenceChanged();
}

void QcpGraph::setAlarmThresholds(const QList<double> &values)
{
  m_alarmThresholds = values;
  if (m_reference.kind == GraphReference::Alarm && !values.contains(m_reference.value))
    setReference(GraphReference::line(GraphReference::Value, m_reference.value));
}

void QcpGraph::setLimits(const ValueRef &value, double low, double high)
{
  m_limitValue = value;
  m_limitLow = low;
  m_limitHigh = high;
  redraw();
}

// As the axis' numbers: four digits with the prefix that suits the value
QString QcpGraph::levelText(double value) const
{
  const int curve = leftCurve();
  const QString base = curve < 0 ? QString() : m_curves.at(curve).unit;
  QString prefix;
  const double v = SiPrefix::scale(value, base, &prefix);
  return base.isEmpty() ? QString::number(v, 'g', 4) : QString("%1 %2").arg(QString::number(v, 'g', 4), SiPrefix::displayText(prefix + base));
}

// Over millions of readings the statistics of the window take a while:
// then not with every picture, a tenth of the time at most
double QcpGraph::followedValue()
{
  if (m_followedAge.isValid() && m_followedAge.elapsed() < m_followedCost * 10)
    return m_followed;
  QElapsedTimer cost;
  cost.start();
  m_followed = qQNaN();
  const GraphStats::Curve curve = statsCurve(leftCurve());
  const qint64 from = windowStart(), to = from + qint64(m_windowSeconds) * 1000;
  // the series at the end of the time shown
  if (const RecordingSeries *series = curve.seriesAt(qMin(to, m_store ? m_store->duration() : to)); series && curve.store)
  {
    const RecordingStore::Stats st = curve.store->statsOf(*series, from - curve.shift, to - curve.shift);
    if (st.count > 0)
      m_followed = m_reference.kind == GraphReference::Mean ? st.mean : m_reference.kind == GraphReference::Min ? st.min : st.max;
  }
  m_followedCost = cost.elapsed();
  m_followedAge.start();
  return m_followed;
}

void QcpGraph::updateLevels()
{
  // another value on the left: its alarms and its limit band
  if (const ValueRef left = leftValue(); left != m_lastLeft)
  {
    m_lastLeft = left;
    Q_EMIT leftValueChanged();
  }
  auto *levels = static_cast<LevelLines *>(m_levels);
  auto *marks = static_cast<LevelLines *>(m_levelMarks);
  levels->clear();
  marks->clear();
  // labels and crossings in the lettering's colour, as the axes' numbers
  marks->ink = m_look.theme.labels.isValid() ? m_look.theme.labels : palette().color(QPalette::Text);
  marks->font = m_plot->yAxis->tickLabelFont();
  m_levelsDrawn.clear();
  m_bandDrawn = false;
  m_triggerLevel = qQNaN();
  const int curve = leftCurve();
  const Axis &axis = m_axes.first();
  const double lo = axis.axis->range().lower * axis.factor, hi = axis.axis->range().upper * axis.factor;
  const double from = m_plot->xAxis->range().lower, to = m_plot->xAxis->range().upper;
  const double width = m_plot->axisRect()->width();
  if (curve >= 0 && hi > lo)
  {
    // the reference line, then the added ones, dashed apart from it
    const QColor labels = marks->ink;
    const QPen pen(QColor(labels.red(), labels.green(), labels.blue(), 150), 1);
    QPen dashed = pen;
    dashed.setStyle(Qt::DashLine);
    auto add = [&](int line, double value, const QString &what, bool counted)
    {
      if (!(value > lo && value < hi))
        return;
      Level level{ line, value, what + levelText(value), 0 };
      if (counted && m_crossingsOn && width > 0)
      {
        const QList<GraphReference::Crossing> found
          = GraphReference::crossings(m_curves.at(curve).points, from, to, 3 * (to - from) / width, value);
        for (const GraphReference::Crossing &c : found)
          marks->marks << LevelLines::Mark{ c.t, value / axis.factor, c.up };
        level.crossings = int(found.size());
        if (level.crossings > 0)
          level.label += QString(" · %1×").arg(level.crossings);
      }
      levels->lines << LevelLines::Line{ axis.axis, value / axis.factor, line < 0 ? pen : dashed, QString() };
      marks->lines << LevelLines::Line{ axis.axis, value / axis.factor, QPen(Qt::NoPen), level.label };
      m_levelsDrawn << level;
    };
    using namespace GraphReference;
    if (m_reference.kind != Off)
    {
      const double value = m_reference.follows() ? followedValue() : m_reference.value;
      const QString what = m_reference.kind == Mean ? word("mean") : m_reference.kind == Min ? word("min")
                           : m_reference.kind == Max ? word("max") : m_reference.kind == Alarm ? word("alarm") : QString();
      // not counted on the window's mean, minimum or maximum: they follow
      // the curve, a noisy one crosses its mean at every other reading
      add(-1, value, what.isEmpty() ? what : what + ' ', !m_reference.follows());
    }
    for (int i = 0; i < m_referenceLines.size(); ++i)
      add(i, m_referenceLines.at(i), QString(), true);
    // the limit test's band
    if (m_limitValue.id >= 0 && m_limitValue == leftValue())
    {
      const QPen green(QColor(0x3c, 0xb0, 0x4a), 1, Qt::DashLine);
      for (double bound : { m_limitLow, m_limitHigh })
        if (std::isfinite(bound) && bound > lo && bound < hi)
          levels->lines << LevelLines::Line{ axis.axis, bound / axis.factor, green, QString() };
      const double low = std::isfinite(m_limitLow) ? m_limitLow : -qInf(), high = std::isfinite(m_limitHigh) ? m_limitHigh : qInf();
      // nothing of the band in the scale: none
      if (qMin(high, hi) > qMax(low, lo))
      {
        levels->band = m_bandDrawn = true;
        levels->bandLow = low / axis.factor;
        levels->bandHigh = high / axis.factor;
        levels->bandBrush = QColor(0x3c, 0xb0, 0x4a, 28);
      }
    }
  }
  // the start threshold of a recording waiting for it, on the axis of the value it compares
  for (const Curve &c : std::as_const(m_curves))
  {
    if (m_ownStore || c.axis < 0 || !c.store)
      continue;
    const RecordingStore::StartMode mode = c.store->startMode();
    const ValueRef trigger = c.store->triggerValue();
    if ((mode != RecordingStore::Raising && mode != RecordingStore::Falling) || trigger.id != c.value.id
        || !(trigger.source.isEmpty() || trigger.source == c.value.source))
      continue;
    m_triggerLevel = mode == RecordingStore::Raising ? c.store->raisingThreshold() : c.store->fallingThreshold();
    const Axis &on = m_axes.at(c.axis);
    levels->lines << LevelLines::Line{ on.axis, m_triggerLevel / on.factor, QPen(m_look.trigger, 1, m_look.oneHue ? Qt::DashLine : Qt::SolidLine), QString() };
    break;
  }
}

double QcpGraph::referenceValue(int line) const
{
  for (const Level &level : m_levelsDrawn)
    if (level.line == line)
      return level.value;
  return qQNaN();
}

QString QcpGraph::referenceLabel(int line) const
{
  for (const Level &level : m_levelsDrawn)
    if (level.line == line)
      return level.label;
  return QString();
}

int QcpGraph::crossingCount(int line) const
{
  for (const Level &level : m_levelsDrawn)
    if (level.line == line)
      return level.crossings;
  return 0;
}

int QcpGraph::referencePixel(int line) const
{
  const double value = referenceValue(line);
  return std::isfinite(value) ? qRound(m_plot->yAxis->coordToPixel(value / m_axes.first().factor)) : -1;
}

bool QcpGraph::limitBand(double *low, double *high) const
{
  if (m_bandDrawn && low)
    *low = std::isfinite(m_limitLow) ? m_limitLow : -qInf();
  if (m_bandDrawn && high)
    *high = std::isfinite(m_limitHigh) ? m_limitHigh : qInf();
  return m_bandDrawn;
}

int QcpGraph::levelAt(const QPoint &pos) const
{
  if (!m_plot->axisRect()->rect().contains(pos))
    return -2;
  for (const Level &level : m_levelsDrawn)
    if (std::abs(referencePixel(level.line) - pos.y()) <= 4)
      return level.line;
  return -2;
}

double QcpGraph::roundedValueAt(const QPoint &globalPos) const
{
  const Axis &axis = m_axes.first();
  const QRect rect = m_plot->axisRect()->rect();
  const QPoint pos = m_plot->mapFromGlobal(globalPos);
  const double y = rect.contains(pos) ? pos.y() : rect.center().y();
  return GraphReference::rounded(axis.axis->pixelToCoord(y) * axis.factor, axis.axis->range().size() * axis.factor);
}

// The reference line snaps to 0, to the mean, minimum and maximum of the
// time shown, to the alarm thresholds and to the limits drawn
void QcpGraph::dragLevel(int line, double y)
{
  const Axis &axis = m_axes.first();
  const double span = axis.axis->range().size() * axis.factor, height = m_plot->axisRect()->height();
  if (!(span > 0) || height <= 0)
    return;
  const double value = axis.axis->pixelToCoord(y) * axis.factor;
  if (line >= 0)
  {
    QList<double> lines = m_referenceLines;
    if (line < lines.size())
      lines[line] = GraphReference::rounded(value, span);
    setReferenceLines(lines);
    return;
  }
  using namespace GraphReference;
  QList<Snap> snaps{ Snap{ Zero, 0 } };
  const GraphStats::Curve curve = statsCurve(leftCurve());
  const qint64 from = windowStart(), to = from + qint64(m_windowSeconds) * 1000;
  if (const RecordingSeries *series = curve.seriesAt(qMin(to, m_store ? m_store->duration() : to)); series && curve.store)
    if (const RecordingStore::Stats st = curve.store->statsOf(*series, from - curve.shift, to - curve.shift); st.count > 0)
      snaps << Snap{ Mean, st.mean } << Snap{ Min, st.min } << Snap{ Max, st.max };
  if (leftValue().id >= 0)
    for (double a : std::as_const(m_alarmThresholds))
      snaps << Snap{ Alarm, a };
  if (m_bandDrawn)
    for (double bound : { m_limitLow, m_limitHigh })
      if (std::isfinite(bound))
        snaps << Snap{ Value, bound };
  setReference(dragged(value, span, span / height, snaps));
}
