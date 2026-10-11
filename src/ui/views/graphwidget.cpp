//======================================================================
// File:		dmmgraph.cpp
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:45:35 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
// as published by the Free Software Foundation.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================

#include <QtGui>
#include <QtWidgets>
#include <QPen>
#include <QRegularExpression>
#include <cmath>
#include <QSvgGenerator>
#include <QPdfWriter>

#include "ui/views/graphwidget.h"

#include <cmath>
#include "core/siprefix.h"
#include "recording/recordingfile.h"
#include "core/settings.h"
#include "ui/engnumbervalidator.h"


// "1:05:09", "5:09", "2 d 1:05:09"
static QString durationText(qint64 seconds)
{
  const qint64 d = seconds / 86400, h = seconds / 3600 % 24, m = seconds / 60 % 60, sec = seconds % 60;
  QString text = h || d ? QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'))
                        : QString("%1:%2").arg(m).arg(sec, 2, 10, QChar('0'));
  return d ? QString("%1 d %2").arg(d).arg(text) : text;
}

GraphWidget::GraphWidget(QWidget *parent): GraphWidget(parent, Q_NULLPTR)
{
}
GraphWidget::GraphWidget(QWidget *parent, Settings *settings) :
  QWidget(parent),
  m_scaleMin(0),
  m_scaleMax(0),
  m_autoScale(true),
  m_connected(false),
  m_mouseDown(false),
  m_mousePan(false),
  m_cursorMode(NoCursor),
  m_lineWidth(2),
  m_intLineWidth(2),
  m_alertUnsaved(true),
  m_crosshair(true),
  m_pointMode(Circle),
  m_intPointMode(Square),
  m_lineMode(Solid),
  m_intLineMode(NoLine),
  m_integrationScale(1.0),
  m_integrationOffset(0.0),
  m_showIntegration(false),
  m_includeZero(false)
{
  m_cfg = settings;
  m_store = new RecordingStore(this);

  scrollbar = new QScrollBar(Qt::Horizontal, this);
  scrollbar->setGeometry(0, height() - 16, width(), 16);
  scrollbar->setTracking(true);
  scrollbar->setCursor(Qt::ArrowCursor);

  connect(scrollbar, &QScrollBar::valueChanged, this, [this](int) { updateXAxisRange(); updateMarkPositions(); });

  emitInfo();

  m_chart = new QChart();
  m_chart->legend()->hide();
  m_chart->setMargins(QMargins(4, 4, 4, 4));

  m_dataSeries = new QLineSeries();
  m_chart->addSeries(m_dataSeries);

  m_dataPoints = new QScatterSeries();
  m_chart->addSeries(m_dataPoints);

  m_intSeries = new QLineSeries();
  m_chart->addSeries(m_intSeries);

  m_intPoints = new QScatterSeries();
  m_chart->addSeries(m_intPoints);

  m_xAxis = new QValueAxis();
  m_xAxis->setTitleText(tr("[sec]"));
  // updateXLabels() writes the x labels; Qt's own only keep the room for
  // them - from the start, not only once colours are set
  m_xAxis->setLabelsBrush(Qt::transparent);
  m_chart->addAxis(m_xAxis, Qt::AlignBottom);
  m_dataSeries->attachAxis(m_xAxis);
  m_dataPoints->attachAxis(m_xAxis);
  m_intSeries->attachAxis(m_xAxis);
  m_intPoints->attachAxis(m_xAxis);

  m_yAxis = new QValueAxis();
  m_chart->addAxis(m_yAxis, Qt::AlignLeft);
  // the same for the y labels (updateYLabels())
  m_yAxis->setLabelsBrush(Qt::transparent);
  m_yAxis->setLabelFormat("%.4g");
  m_defaultAxisLine = m_yAxis->linePenColor();
  m_yTitle = new QGraphicsSimpleTextItem(m_chart);
  m_yTitle->setFont(m_xAxis->titleFont());
  m_yTitle->setBrush(m_xAxis->titleBrush());
  connect(m_chart, &QChart::plotAreaChanged, this, [this](const QRectF &)
  {
    placeYTitle();
    updateCentreTicks();
    updateXLabels();
    updateYLabels();
    updateStateLabel();
  });
  m_centreTicks = new QGraphicsPathItem(m_chart);
  m_centreTicks->setZValue(5);   // above the grid, below the curves' markers
  m_dataSeries->attachAxis(m_yAxis);
  m_dataPoints->attachAxis(m_yAxis);
  m_intSeries->attachAxis(m_yAxis);
  m_intPoints->attachAxis(m_yAxis);
  m_dataLine = std::make_unique<GapLine>(m_dataSeries);
  m_intLine = std::make_unique<GapLine>(m_intSeries);

  updateSeriesAppearance();

  // Cursor crosshair + draggable threshold lines, overlaid directly on the
  // chart's graphics scene, above the series (see setZValue below).
  m_crosshairVLine   = new QGraphicsLineItem(m_chart);
  m_crosshairHLine   = new QGraphicsLineItem(m_chart);
  m_triggerLine      = new QGraphicsLineItem(m_chart);
  m_integrationLine  = new QGraphicsLineItem(m_chart);

  for (QGraphicsLineItem *item : {m_crosshairVLine, m_crosshairHLine, m_triggerLine, m_integrationLine})
  {
    item->setZValue(1000);
    item->setVisible(false);
  }

  m_chartView = new QChartView(m_chart, this);
  m_chartView->setRenderHint(QPainter::Antialiasing);
  // the graph background fills the widget edge to edge, without Qt Charts'
  // inset and rounded corners, so the scrollbar below lines up with it
  m_chart->setBackgroundRoundness(0);
  m_chart->layout()->setContentsMargins(0, 0, 0, 0);
  m_chartView->setFrameShape(QFrame::NoFrame);
  m_chartView->viewport()->setMouseTracking(true);
  m_chartView->viewport()->installEventFilter(this);
  // keyboard zoom/pan once the graph has been clicked
  m_chartView->setFocusPolicy(Qt::ClickFocus);
  m_chartView->installEventFilter(this);

  m_popup = new QMenu(this);
  connect(m_popup, SIGNAL(triggered(QAction *)), this, SLOT(popupSLOT(QAction *)));

  // time buttons top right, as in the UI demo: the visible window at a click
  m_timeBar = new QWidget(m_chartView);
  auto *bar = new QHBoxLayout(m_timeBar);
  bar->setContentsMargins(0, 0, 0, 0);
  bar->setSpacing(2);
  const QList<QPair<QString, int>> steps = { { tr("All"), 0 }, { tr("1 min"), 60 }, { tr("5 min"), 300 },
                                             { tr("30 min"), 1800 } };
  for (const auto &step : steps)
  {
    auto *b = new QToolButton(m_timeBar);
    b->setText(step.first);
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::ArrowCursor);
    b->setProperty("seconds", step.second);
    b->setToolTip(step.second == 0 ? tr("Show the whole recording, growing with it")
                                   : tr("Show the last %1").arg(step.first));
    connect(b, &QToolButton::clicked, this, [this, b] { timeButtonClicked(b->property("seconds").toInt()); });
    bar->addWidget(b);
    m_timeButtons.append(b);
  }
  updateTimeButtons();

  m_cursorLabel = new QLabel(m_chartView);
  m_cursorLabel->setAttribute(Qt::WA_TransparentForMouseEvents);   // no Leave for the view
  m_cursorLabel->hide();

  m_stateLabel = new QLabel(m_chartView);
  m_stateLabel->setObjectName("ui_graphState");
  m_stateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_stateLabel->hide();

  connectStore();
}

void GraphWidget::connectStore()
{
  connect(m_store, &RecordingStore::appended, this, &GraphWidget::onAppended);
  connect(m_store, &RecordingStore::cleared, this, &GraphWidget::onCleared);
  connect(m_store, &RecordingStore::marksChanged, this, &GraphWidget::syncMarks);
  connect(m_store, &RecordingStore::progressChanged, this, &GraphWidget::emitInfo);
  connect(m_store, &RecordingStore::runningChanged, this, &GraphWidget::running);
  connect(m_store, &RecordingStore::alert, this, [] { QApplication::beep(); });
  // the axis names the recording's unit, which a new one may change
  connect(m_store, &RecordingStore::cleared, this, &GraphWidget::showUnit);
  connect(m_store, &RecordingStore::loaded, this, &GraphWidget::showUnit);
  connect(m_store, &RecordingStore::stateChanged, this, &GraphWidget::onStateChanged);
  // the time of a recording, and what a load or a clear left to view
  connect(m_store, &RecordingStore::progressChanged, this, &GraphWidget::updateStateLabel);
  connect(m_store, &RecordingStore::loaded, this, &GraphWidget::updateStateLabel);
  connect(m_store, &RecordingStore::cleared, this, &GraphWidget::updateStateLabel);
}

void GraphWidget::setStore(RecordingStore *store)
{
  if (!store || store == m_store)
    return;
  m_store->disconnect(this);
  m_store = store;
  connectStore();
  // the view shows the new store as it is
  onCleared();
  showUnit();
  rebuildSeries();
  syncMarks();
  onStateChanged();
}

void GraphWidget::timeButtonClicked(int seconds)
{
  m_followAll = (seconds == 0);
  if (m_followAll)
    requestAll(false);
  else
    Q_EMIT windowRequested(seconds);   // back through the settings, setGraphSize()
  updateTimeButtons();   // also when nothing changes: undo the click's own toggle
}

int GraphWidget::allTarget(bool grow) const
{
  const double recorded = (m_store->duration() - m_store->origin()) / 1000.0;
  // growing by a quarter at a time: the window does not change with every sample
  int target = int(std::ceil(grow ? recorded * 1.25 : recorded));
  target = qMax(target, 10);
  // whole minutes from one minute on: the settings keep seconds only up to
  // 99999, an odd value above would be cut and asked for again with every sample
  if (target > 60)
    target = (target + 59) / 60 * 60;
  const int limit = lengthLimit();
  if (limit > 0)
    target = qMin(target, limit);
  return target;
}

void GraphWidget::requestAll(bool grow)
{
  const int target = allTarget(grow);
  if (target != m_windowSeconds)
    applyWindow(target);
}

void GraphWidget::leaveAll()
{
  if (!m_followAll)
    return;
  m_followAll = false;
  if (m_windowSeconds != m_savedWindow)
    Q_EMIT windowRequested(m_windowSeconds);
}

void GraphWidget::updateTimeButtons()
{
  for (QToolButton *b : m_timeButtons)
  {
    const int seconds = b->property("seconds").toInt();
    // a window longer than the recording is no choice
    const int limit = lengthLimit();
    b->setVisible(seconds == 0 || limit <= 0 || seconds <= limit);
    b->setChecked(seconds == 0 ? m_followAll : !m_followAll && seconds == m_windowSeconds);
  }
  placeTimeBar();
}

void GraphWidget::placeTimeBar()
{
  if (!m_timeBar)
    return;
  m_timeBar->adjustSize();
  m_timeBar->move(m_chartView->width() - m_timeBar->width() - 6, 3);
  m_timeBar->raise();
}

void GraphWidget::hideCrosshair()
{
  m_crosshairVLine->setVisible(false);
  m_crosshairHLine->setVisible(false);
  if (m_cursorLabel)
    m_cursorLabel->hide();
}

GraphWidget::~GraphWidget()
{
}

void GraphWidget::print(QPrinter *prt, const QString &title, const QString &comment)
{
  if (!title.isEmpty())
    prt->setDocName(title);
  else
    prt->setDocName(tr("QtDMM: %1").arg(QDateTime::currentDateTime().toString()));

  prt->setCreator("QtDMM: (c) 2001 Matthias Toussaint");
  prt->setPrintProgram("QtDMM: (c) 2001 Matthias Toussaint");

  QPainter p;
  p.begin(prt);

  int w = prt->width();
  int h = prt->height();

  p.setFont(QFont("Helvetica", 16));

  QRect tRect = p.boundingRect(0, 0, w, h, Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap, title);

  p.drawText(tRect, Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap, title);

  p.setFont(QFont("Helvetica", 10));

  QFontMetrics fm = p.fontMetrics();
  int maxWidth = qMax(fm.horizontalAdvance(tr("Sampling start:")),
                      fm.horizontalAdvance(tr("Sampling resolution:")));
  int tHeight = fm.height();

  p.drawText(0, tRect.height() + 10, maxWidth, tHeight, Qt::AlignLeft | Qt::AlignVCenter,
             tr("Sampling start:"));
  p.drawText(maxWidth + 10, tRect.height() + 10, w - maxWidth - 10, tHeight, Qt::AlignLeft | Qt::AlignVCenter,
             m_store->startDateTime().toString());

  p.drawText(0, tRect.height() + 10 + tHeight, maxWidth, tHeight, Qt::AlignLeft | Qt::AlignVCenter,
             tr("Sampling resolution:"));
  p.drawText(maxWidth + 10, tRect.height() + 10 + tHeight,
             w - maxWidth - 10, tHeight, Qt::AlignLeft | Qt::AlignVCenter,
             tr("%1 Seconds").arg(m_store->sampleTime() / 10.0));

  //p.setFont( QFont( "Helvetica", 10 ));

  QRect cRect = p.boundingRect(0, 0, w, h, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, comment);

  p.drawText(0, tRect.height() + 20 + 2 * tHeight, w, cRect.height(),
             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, comment);

  h -= tRect.height() + 30 + 2 * tHeight + cRect.height();

  QRectF chartRect(0, tRect.height() + 30 + 2 * tHeight + cRect.height(), w, h);
  m_chartView->render(&p, chartRect);

  p.end();
}

void GraphWidget::resizeEvent(QResizeEvent *)
{
  m_chartView->setGeometry(0, 0, width(), height() - 16);
  scrollbar->setGeometry(0, height() - 16, width(), 16);
  placeTimeBar();

  // Simpler than tracking/restoring hover state across a resize: just hide it,
  // the next mouse move will reposition it correctly.
  hideCrosshair();

  updateThresholdLinePositions();
  // the thinning follows the plot's width
  if (bucketSize() != m_bucket)
    rebuildSeries();
}

// ms per pixel column. Counted over the window or, while the recording does
// not fill it yet, over what is there: a short recording in a long window is
// drawn as it is. Readings further apart than a column are a bucket each.
qint64 GraphWidget::bucketSize() const
{
  const double plotWidth = m_chart->plotArea().width();
  const int columns = qMax(1, int(plotWidth > 0 ? plotWidth : width()));
  const qint64 shown = qMin<qint64>(qint64(m_windowSeconds) * 1000, m_store->duration() - m_store->origin());
  return qMax<qint64>(1, (shown + columns - 1) / columns);
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
int GraphWidget::bucketPoints(int first, int last, bool integral, QList<QPointF> &out) const
{
  const RecordingSeries &series = m_store->series();
  auto value = [&](int i)
  {
    const RawPoint &p = series.at(i);
    return integral ? m_integrationOffset + p.integral * m_integrationScale : p.value;
  };
  auto x = [&](int i) { return series.at(i).t / 1000.0; };
  while (first <= last && std::isnan(value(first)))
    first++;
  if (first > last)
  {
    out.append(QPointF(x(last), qQNaN()));
    return 1;
  }
  int lo = first, hi = first;
  double loValue = value(first), hiValue = loValue;
  for (int i = first + 1; i <= last; i++)
  {
    const double v = value(i);
    if (std::isnan(v))
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
    m_tailData = bucketPoints(first, last, false, points);
    m_tailInt = bucketPoints(first, last, true, intPoints);
    first = last + 1;
  }

  m_dataLine->replace(points);
  m_dataPoints->replace(withoutGaps(points));
  m_intLine->replace(intPoints);
  m_intPoints->replace(withoutGaps(intPoints));
  m_tailDataPts = finiteTail(points, m_tailData);
  m_tailIntPts = finiteTail(intPoints, m_tailInt);
}

// The newest reading went into the series: a new bucket adds its point, one
// that is still filling replaces the points it had.
void GraphWidget::appendToSeries()
{
  const int count = m_store->count();
  const int first = bucketStart(count - 1);
  if (first != count - 1)
  {
    m_dataLine->removeLast(m_tailData);
    m_dataPoints->removePoints(m_dataPoints->count() - m_tailDataPts, m_tailDataPts);
    m_intLine->removeLast(m_tailInt);
    m_intPoints->removePoints(m_intPoints->count() - m_tailIntPts, m_tailIntPts);
  }
  QList<QPointF> points, intPoints;
  m_tailData = bucketPoints(first, count - 1, false, points);
  m_tailInt = bucketPoints(first, count - 1, true, intPoints);
  m_dataLine->append(points);
  m_intLine->append(intPoints);
  const QList<QPointF> dataPts = withoutGaps(points), intPts = withoutGaps(intPoints);
  m_dataPoints->append(dataPts);
  m_intPoints->append(intPts);
  m_tailDataPts = int(dataPts.size());
  m_tailIntPts = int(intPts.size());
}

QList<QPointF> GraphWidget::withoutGaps(const QList<QPointF> &points)
{
  QList<QPointF> out;
  out.reserve(points.size());
  for (const QPointF &p : points)
    if (!std::isnan(p.y()))
      out.append(p);
  return out;
}

// how many of the last @p tail points have a value
int GraphWidget::finiteTail(const QList<QPointF> &points, int tail)
{
  int n = 0;
  for (qsizetype i = qMax<qsizetype>(0, points.size() - tail); i < points.size(); ++i)
    if (!std::isnan(points[i].y()))
      n++;
  return n;
}

qint64 GraphWidget::windowStart() const
{
  // Live: the newest reading at the right edge once the window is full (the
  // axis ends a sample time short of the window, updateXAxisRange())
  if (m_store->state() == RecordingStore::Live)
  {
    const qint64 window = qint64(qMax(1, m_windowSeconds)) * 1000;
    const qint64 shortOf = qMin<qint64>(m_store->sampleTime() * 100, window / 10);
    return qMax(m_store->origin(), m_store->duration() - window + shortOf);
  }
  // the scroll bar counts tenths of a second from what the store keeps
  return m_store->origin() + qint64(qMax(0, scrollbar->value())) * 100;
}

void GraphWidget::updateXAxisRange()
{
  // one sample time short of the window, as it always was: no label on the
  // right edge
  const double window = qMax(1, m_windowSeconds);
  double start = windowStart() / 1000.0, end = start + window - qMin(m_store->sampleTime() / 10.0, window / 10);
  double div = timeStep((end - start) / 6);   // about 5 ticks
  if (divisions() && end > start)
  {
    // 10 divisions of a time step that cover the window, starting on a
    // step: 600 s are 10 x 1 min
    div = timeStep((end - start) / 10);
    double first = std::floor(start / div) * div;
    while (first + 10 * div < end - div * 1e-9)
    {
      div = timeStep(div * 1.01);
      first = std::floor(start / div) * div;
    }
    start = first;
    end = first + 10 * div;
  }
  m_xAxis->setRange(start, end);
  // the ticks on whole steps, counted from the start of the recording
  m_xStep = div;
  m_xAxis->setTickType(QValueAxis::TicksDynamic);
  m_xAxis->setTickAnchor(0);
  m_xAxis->setTickInterval(div);
  updateXLabels();
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
void GraphWidget::updateXLabels()
{
  if (!m_xAxis || m_xStep <= 0)
    return;
  double unit = 1;
  QString title = tr("[sec]");
  if (m_xStep >= 3600 && std::fmod(m_xStep, 3600) == 0)
  {
    unit = 3600;
    title = tr("[h]");
  }
  else if (m_xStep >= 60 && std::fmod(m_xStep, 60) == 0)
  {
    unit = 60;
    title = tr("[min]");
  }
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
}

QString GraphWidget::yPrefix() const
{
  const QString base = m_store->unit();
  // units that take no prefix: °C, %, none
  if (base.isEmpty() || base.startsWith(QChar(0x00b0)) || base == QLatin1String("%"))
    return QString();
  // by the axis, not by the reading: a meter that changes range (V, mV, µV)
  // turned the whole axis to mV with labels like 1.6e+04; 350 mV on an axis
  // up to 0.4 V are still mV. Without readings yet the axis says nothing:
  // the meter's prefix
  if (m_store->count() == 0 && !m_liveBase.isEmpty() && m_liveBase == base)
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
}

double GraphWidget::niceStep(double v)
{
  if (!(v > 0) || !std::isfinite(v))
    return 1;
  double decade = std::pow(10.0, std::floor(std::log10(v)));
  for (double m : { 1.0, 2.0, 5.0, 10.0 })
    if (m * decade >= v * (1 - 1e-12))
      return m * decade;
  return 10 * decade;
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
  const QRectF pa = m_chart->plotArea();
  const double cx = pa.center().x(), cy = pa.center().y();
  QPainterPath path;
  path.moveTo(pa.left(), cy);
  path.lineTo(pa.right(), cy);
  path.moveTo(cx, pa.top());
  path.lineTo(cx, pa.bottom());
  const double dx = pa.width() / 50, dy = pa.height() / 40;   // 10 x 8 divisions, 5 ticks each
  for (int i = 0; i <= 50; ++i)
  {
    path.moveTo(pa.left() + i * dx, cy - 3);
    path.lineTo(pa.left() + i * dx, cy + 3);
  }
  for (int i = 0; i <= 40; ++i)
  {
    path.moveTo(cx - 3, pa.top() + i * dy);
    path.lineTo(cx + 3, pa.top() + i * dy);
  }
  m_centreTicks->setPath(path);
}

void GraphWidget::updateSeriesAppearance()
{
  m_dataLine->setPen(QPen(dataColor(), m_lineWidth, penStyle(m_lineMode), Qt::RoundCap, Qt::RoundJoin));
  m_dataLine->setVisible(m_lineMode != NoLine);

  QScatterSeries::MarkerShape shape = QScatterSeries::MarkerShapeCircle;
  int size = 7;

  switch (m_pointMode)
  {
    case NoPoint:                                                                     break;
    case Circle:      case LargeCircle:  shape = QScatterSeries::MarkerShapeCircle;    break;
    case Square:      case LargeSquare:  shape = QScatterSeries::MarkerShapeRectangle; break;
    // Qt Charts has no "X"/cross marker; approximate both Diamond and X with a
    // rotated square, the closest built-in shape available.
    case Diamond:     case LargeDiamond:
    case X:           case LargeX:       shape = QScatterSeries::MarkerShapeRotatedRectangle; break;
  }
  if (m_pointMode == LargeCircle || m_pointMode == LargeSquare ||
      m_pointMode == LargeDiamond || m_pointMode == LargeX)
    size = 11;

  m_dataPoints->setMarkerShape(shape);
  m_dataPoints->setMarkerSize(size);
  m_dataPoints->setColor(dataColor());
  // Qt Charts rims the markers in white: dense points turn into a white band
  m_dataPoints->setBorderColor(dataColor());
  m_dataPoints->setVisible(m_pointMode != NoPoint);

  // phosphor: one colour, the integration curve dashed when it is a solid line
  const Qt::PenStyle intStyle = phosphor() && m_intLineMode == Solid ? Qt::DashLine : penStyle(m_intLineMode);
  m_intLine->setPen(QPen(intColor(), m_intLineWidth, intStyle, Qt::RoundCap, Qt::RoundJoin));
  m_intLine->setVisible(m_showIntegration && m_intLineMode != NoLine);

  QScatterSeries::MarkerShape intShape = QScatterSeries::MarkerShapeCircle;
  int intSize = 7;

  switch (m_intPointMode)
  {
    case NoPoint:                                                                        break;
    case Circle:      case LargeCircle:  intShape = QScatterSeries::MarkerShapeCircle;    break;
    case Square:      case LargeSquare:  intShape = QScatterSeries::MarkerShapeRectangle; break;
    case Diamond:     case LargeDiamond:
    case X:           case LargeX:       intShape = QScatterSeries::MarkerShapeRotatedRectangle; break;
  }
  if (m_intPointMode == LargeCircle || m_intPointMode == LargeSquare ||
      m_intPointMode == LargeDiamond || m_intPointMode == LargeX)
    intSize = 11;

  m_intPoints->setMarkerShape(intShape);
  m_intPoints->setMarkerSize(intSize);
  m_intPoints->setColor(intColor());
  m_intPoints->setBorderColor(intColor());
  m_intPoints->setVisible(m_showIntegration && m_intPointMode != NoPoint);
}

void GraphWidget::updateThresholdLinesVisibility()
{
  m_triggerLine->setVisible(mode() == Raising || mode() == Falling);
  m_integrationLine->setVisible(m_showIntegration);

  updateThresholdLinePositions();
}

void GraphWidget::updateThresholdLinePositions()
{
  QRectF plot = m_chart->plotArea();

  auto positionLine = [&](QGraphicsLineItem *item, double value)
  {
    if (!item->isVisible())
      return;
    double y = m_chart->mapToPosition(QPointF(m_xAxis->min(), value), m_dataSeries).y();
    item->setLine(plot.left(), y, plot.right(), y);
  };

  positionLine(m_triggerLine, mode() == Raising ? m_store->raisingThreshold() : m_store->fallingThreshold());
  positionLine(m_integrationLine, m_store->integrationThreshold());
  updateMarkPositions();
}

void GraphWidget::addMark(const QColor &color, const QString &name)
{
  m_store->addMark(color.rgba(), name);
}

void GraphWidget::syncMarks()
{
  qDeleteAll(m_marks);
  m_marks.clear();
  for (const RecordingStore::Mark &m : m_store->marks())
  {
    auto *line = new QGraphicsLineItem(m_chart);
    line->setPen(QPen(QColor::fromRgba(m.color), 2, Qt::DashLine));
    line->setZValue(999);
    line->setToolTip(m.name);
    m_marks << line;
  }
  updateMarkPositions();
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
}

int GraphWidget::lengthLimit() const
{
  const qint64 ms = m_store->lengthLimit();
  return ms > 0 ? int((ms + 999) / 1000) : 0;
}

void GraphWidget::updateScrollRange()
{
  // in tenths of a second, over what the store keeps; a recording seen at
  // its end stays at the end
  const bool atEnd = scrollbar->value() >= scrollbar->maximum();
  const qint64 kept = m_store->duration() - m_store->origin();
  scrollbar->setMaximum(int(qMax<qint64>(0, (kept - qint64(m_windowSeconds) * 1000) / 100)));
  if (atEnd && m_store->state() == RecordingStore::Record)
    scrollbar->setValue(scrollbar->maximum());
}

void GraphWidget::setGraphSize(int size)
{
  size = qMax(1, size);
  // another window (the settings page, a zoom, a time button) ends "All";
  // the same one again (OK in the dialog) keeps it with its window
  if (m_savedWindow > 0 && size != m_savedWindow)
    m_followAll = false;
  m_savedWindow = size;
  applyWindow(m_followAll ? m_windowSeconds : size);
}

void GraphWidget::applyWindow(int seconds)
{
  m_windowSeconds = qMax(1, seconds);

  scrollbar->setMinimum(0);
  scrollbar->setSingleStep(qMax(1, m_windowSeconds));
  scrollbar->setPageStep(m_windowSeconds * 10);
  updateScrollRange();

  m_store->setLiveWidth(m_windowSeconds);

  emitInfo();

  rebuildSeries();
  updateXAxisRange();
  updateThresholdLinePositions();
  updateTimeButtons();
}

void GraphWidget::setSampleLength(int v)
{
  m_store->setSampleLength(v);
  // the longest time button follows the length
  updateTimeButtons();
}

void GraphWidget::setSampleTime(int v)
{
  // the grid of the export: the graph shows every reading anyway
  m_store->setSampleTime(v);
}

void GraphWidget::startSLOT()
{
  m_store->start();
}

void GraphWidget::stopSLOT()
{
  m_store->stop();
}

void GraphWidget::liveSLOT()
{
  m_store->live();
}

void GraphWidget::onStateChanged()
{
  const bool live = m_store->state() == RecordingStore::Live;
  // a file viewed is gone with Live or a recording
  if (m_store->state() != RecordingStore::View)
    m_sourceFile.clear();
  // Live follows the newest reading: nothing to scroll; it starts with
  // "All", so the graph fills with what there is instead of a needle at the
  // left of a long window
  scrollbar->setEnabled(!live);
  if (live)
  {
    m_followAll = true;
    requestAll(false);
  }
  updateScrollRange();
  updateXAxisRange();
  updateTimeButtons();
  updateMarkPositions();
  updateStateLabel();
  emitInfo();
  m_chartView->viewport()->update();
  Q_EMIT stateChanged(int(m_store->state()));
}

QString GraphWidget::stateText() const
{
  switch (m_store->state())
  {
    case RecordingStore::Live:
      return QString();
    case RecordingStore::Record:
    {
      const qint64 seconds = m_store->duration() / 1000;
      const int left = m_store->remainingLength();
      if (left <= 0)
        return tr("● REC %1").arg(durationText(seconds));
      return tr("● REC %1 / %2").arg(durationText(seconds), durationText((m_store->duration() / 100 + left) / 10));
    }
    case RecordingStore::View:
      break;
  }
  if (m_store->count() == 0)
    return QString();
  const QString length = durationText((m_store->duration() - m_store->origin()) / 1000);
  if (!m_sourceFile.isEmpty())
    return tr("%1 · %2").arg(m_sourceFile, length);
  return tr("Recording of %1 · %2").arg(QLocale().toString(m_store->startDateTime(), QLocale::ShortFormat), length);
}

void GraphWidget::updateStateLabel()
{
  if (!m_stateLabel)
    return;
  const QString text = stateText();
  if (text.isEmpty())
  {
    m_stateLabel->hide();
    return;
  }
  // a recording in red; what is viewed in the lettering colour, dimmed
  const QColor grey = m_stateColor.isValid() ? m_stateColor : palette().color(QPalette::WindowText);
  const QColor color = m_store->isRunning() ? QColor(0xd3, 0x2f, 0x2f)
                                            : QColor(grey.red(), grey.green(), grey.blue(), 170);
  m_stateLabel->setStyleSheet(QString("QLabel { color: %1; background: transparent; font-weight: %2; }")
                                .arg(color.name(QColor::HexArgb), m_store->isRunning() ? "bold" : "normal"));
  m_stateLabel->setText(text);
  m_stateLabel->adjustSize();
  // top left inside the plot: the line above it is the crosshair's
  const QPoint topLeft = m_chartView->mapFromScene(m_chart->plotArea().topLeft());
  m_stateLabel->move(topLeft.x() + 6, topLeft.y() + 4);
  m_stateLabel->show();
  m_stateLabel->raise();
}

void GraphWidget::onAppended(bool shifted)
{
  const RawPoint &p = m_store->series().last();

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
  updateScrollRange();
  // a full store, or Live: the window moves on with what it keeps
  if (m_store->origin() > 0 || m_store->state() == RecordingStore::Live)
  {
    updateXAxisRange();
    updateMarkPositions();
  }

  const bool resFlag = m_autoScale && computeMinMax(p.value);

  if (shifted || bucketSize() != m_bucket)
    rebuildSeries();
  else if (!rebuilt)
    appendToSeries();

  if (resFlag)
  {
    setYRange(m_scaleMin, m_scaleMax);
    updateThresholdLinePositions();
  }
}

void GraphWidget::setUnit(const QString &unit)
{
  // Values arrive in SI base units (see DmmResponse), so the store keeps the
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
  const QString base = m_store->unit();
  m_yTitle->setText(base.isEmpty() ? QString() : QString("[%1%2]").arg(m_yPrefix, base));
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
}

void GraphWidget::clearSLOT()
{
  m_store->clear();
}

void GraphWidget::onCleared()
{
  if (m_autoScale)
  {
    if (m_includeZero)
      m_scaleMin = m_scaleMax = 0;
    else
    {
      m_scaleMin =  1e40;
      m_scaleMax = -1e40;
    }
  }

  m_dataLine->clear();
  m_dataPoints->clear();
  m_intLine->clear();
  m_intPoints->clear();
  m_tailData = m_tailInt = m_tailDataPts = m_tailIntPts = 0;
  // Qt Charts does not repaint a series emptied: without this the old curve
  // stood after Record until the next click
  m_chartView->viewport()->update();
}

void GraphWidget::emitInfo()
{
  // recorded / the recording length - left until it - state
  // Live: what the window holds of what it keeps
  const bool live = m_store->state() == RecordingStore::Live;
  QString txt = durationText((m_store->duration() - (live ? m_store->origin() : 0)) / 1000);
  const qint64 limit = live ? m_store->liveWindow() : m_store->lengthLimit();
  if (limit > 0 && m_store->state() != RecordingStore::View)
    txt += " / " + durationText(limit / 1000);
  if (m_store->remainingLength() > 0)
    txt += " - " + tr("%1 left").arg(durationText(m_store->remainingLength() / 10));
  switch (m_store->state())
  {
    case RecordingStore::Live:
      txt += " - " + tr("Live");
      break;
    case RecordingStore::Record:
      txt += " - " + tr("Recording");
      break;
    case RecordingStore::View:
      txt += " - " + tr("Stopped");
      break;
  }
  Q_EMIT info(txt);
}

// m_chartView is a child widget covering the whole graph area, so mouse/wheel
// events over it are delivered to its viewport, not to GraphWidget's own
// mousePressEvent/etc. overrides. An event filter on the viewport is the
// standard way to intercept them while keeping all interaction state/logic on
// GraphWidget itself (it already owns everything these handlers need).
bool GraphWidget::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_chartView && event->type() == QEvent::KeyPress)
    return handleChartKey(static_cast<QKeyEvent *>(event)) || QWidget::eventFilter(watched, event);

  if (watched == m_chartView->viewport())
  {
    switch (event->type())
    {
      case QEvent::MouseButtonPress:
        handleChartMousePress(static_cast<QMouseEvent *>(event));
        return true;
      case QEvent::MouseMove:
        handleChartMouseMove(static_cast<QMouseEvent *>(event));
        return true;
      case QEvent::MouseButtonRelease:
        handleChartMouseRelease(static_cast<QMouseEvent *>(event));
        return true;
      case QEvent::Wheel:
        handleChartWheel(static_cast<QWheelEvent *>(event));
        return true;
      case QEvent::Leave:
        hideCrosshair();
        break;
      default:
        break;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void GraphWidget::handleChartMousePress(QMouseEvent *ev)
{
  QPoint pos(qRound(ev->position().x()), qRound(ev->position().y()));
  QPoint globalPos(qRound(ev->globalPosition().x()), qRound(ev->globalPosition().y()));

  if (ev->button() == Qt::LeftButton)
  {
    m_mouseDown = true;
    m_mousePan = false;
  }
  else if (ev->button() == Qt::MiddleButton)
  {
    m_mouseDown = false;
    m_mousePan = true;
    m_mpos = pos;
  }
  else if (ev->button() == Qt::RightButton)
  {
    m_popup->clear();

    if (m_store->isRunning())
    {
      QAction *action = new QAction(tr("Stop recording"), m_popup);
      action->setProperty("ID", IDStopRecorder);
      m_popup->addAction(action);
      //m_popup->insertItem( tr("Stop recorder"), IDStopRecorder );
    }
    else
    {
      QAction *action = new QAction(tr("Start recording"), m_popup);
      action->setProperty("ID", IDStartRecorder);
      m_popup->addAction(action);
      //m_popup->insertItem( tr("Start recorder"), IDStartRecorder );
    }
    if (m_store->state() == RecordingStore::View)
    {
      QAction *action = new QAction(tr("Live"), m_popup);
      action->setProperty("ID", IDLive);
      m_popup->addAction(action);
    }
    QAction *action = new QAction(tr("Clear graph"), m_popup);
    action->setProperty("ID", IDClearGraph);
    m_popup->addAction(action);
    //m_popup->insertItem( tr("Clear graph"), IDClearGraph );
    m_popup->addSeparator();

    action = new QAction(tr("Configure..."), m_popup);
    action->setProperty("ID", IDConfigure);
    m_popup->addAction(action);
    //m_popup->insertItem( tr("Configure..."), IDConfigure );
    action = new QAction(tr("Copy image"), m_popup);
    action->setProperty("ID", IDCopyImage);
    m_popup->addAction(action);
    action = new QAction(tr("Export image..."), m_popup);
    action->setProperty("ID", IDExportImage);
    m_popup->addAction(action);

    // this graph's colours: the default from the settings page, or one of
    // the variants for this graph only
    QMenu *colours = m_popup->addMenu(tr("Graph &colours"));
    auto variant = [this, colours](const QString &text, int v)
    {
      QAction *a = colours->addAction(text);
      a->setProperty("ID", IDColorVariant);
      a->setProperty("variant", v);
      a->setCheckable(true);
      a->setChecked(m_variantOverride == v);
    };
    variant(tr("&Default: %1").arg(variantTitle(m_defaultVariant)), -1);
    colours->addSeparator();
    for (ColorVariant v : { Neutral, ScopeBlue, PhosphorGreen, PhosphorAmber, ChartRecorder, Custom })
      variant(variantTitle(v), v);

    if (!m_store->isRunning())
    {
      m_popup->addSeparator();
      QAction *action = new QAction(tr("Export data..."), m_popup);
      action->setProperty("ID", IDExportData);
      m_popup->addAction(action);
      //m_popup->insertItem( tr("Export data..."), IDExportData );
      action = new QAction(tr("Import data..."), m_popup);
      action->setProperty("ID", IDImportData);
      m_popup->addAction(action);
      //m_popup->insertItem( tr("Import data..."), IDImportData );
    }

    m_popup->popup(globalPos);
  }
}

void GraphWidget::handleChartMouseMove(QMouseEvent *ev)
{
  QPoint pos(qRound(ev->position().x()), qRound(ev->position().y()));

  if (m_mousePan)
  {
    QRectF plot = m_chart->plotArea();
    double range = m_xAxis->max() - m_xAxis->min();
    if (plot.width() <= 0 || range <= 0)
      return;

    double pixelsPerSecond = plot.width() / range;
    double dxSeconds = (m_mpos.x() - pos.x()) / pixelsPerSecond;
    double dxTenths = dxSeconds * 10;

    if (fabs(dxTenths) >= 1)
    {
      int sv = qMax(0, scrollbar->value());
      scrollbar->setValue(qBound(0, sv + qRound(dxTenths), scrollbar->maximum()));
      m_mpos = pos;
    }
    return;
  }

  if (m_mouseDown && m_cursorMode != NoCursor)
  {
    QPointF scenePos = m_chartView->mapToScene(pos);
    double value = m_chart->mapToValue(scenePos, m_dataSeries).y();

    switch (m_cursorMode)
    {
      case Trigger:
        if (mode() == Raising) m_store->setRaisingThreshold(value);
        else                   m_store->setFallingThreshold(value);
        break;
      case Integration:
        m_store->setIntegrationThreshold(value);
        break;
      case NoCursor:
        break;
    }

    updateThresholdLinePositions();
    Q_EMIT thresholdChanged(m_cursorMode, value);
    return;
  }

  // Pure hover: hit-test the draggable threshold lines (trigger, then
  // integration - first match wins when lines overlap) and,
  // failing that, drive the crosshair.
  if (!m_mouseDown && !m_mousePan)
  {
    QPointF scenePos = m_chartView->mapToScene(pos);
    const int tolerance = 3;

    auto near = [&](QGraphicsLineItem *item)
    {
      return item->isVisible() && fabs(scenePos.y() - item->line().y1()) < tolerance;
    };

    if (near(m_triggerLine))
      m_cursorMode = Trigger;
    else if (near(m_integrationLine))
      m_cursorMode = Integration;
    else
      m_cursorMode = NoCursor;

    m_chartView->viewport()->setCursor(m_cursorMode == NoCursor ? Qt::ArrowCursor : Qt::SplitVCursor);

    if (m_cursorMode != NoCursor || !m_crosshair)
    {
      hideCrosshair();
      return;
    }

    QRectF plot = m_chart->plotArea();
    double x = qBound(plot.left(), scenePos.x(), plot.right());

    m_crosshairVLine->setLine(x, plot.top(), x, plot.bottom());
    m_crosshairVLine->setVisible(true);

    double xValue = m_chart->mapToValue(QPointF(x, scenePos.y()), m_dataSeries).x();
    const qint64 t = qRound64(xValue * 1000);
    const RecordingSeries &series = m_store->series();
    // the reading whose value holds there; the next one if it is nearer
    int idx = series.holding(t);
    if (idx + 1 < series.count() && (idx < 0 || (!series.at(idx).gap() && !series.at(idx + 1).gap()
                                                 && series.at(idx + 1).t - t < t - series.at(idx).t)))
      idx++;
    const bool inside = idx >= 0 && idx < series.count() && t <= qMax(m_store->duration(), series.last().t);

    QString text = m_store->startDateTime().addMSecs(inside ? series.at(idx).t : t).time().toString("HH:mm:ss.zzz");

    if (inside && series.at(idx).gap())
    {
      // a gap: say why there is no value
      m_crosshairHLine->setVisible(false);
      text += "   " + (series.at(idx).quality == Quality::Overload ? QStringLiteral("OL") : tr("no value"));
    }
    else if (inside)
    {
      double val = series.at(idx).value;
      QPointF scenePoint = m_chart->mapToPosition(QPointF(series.at(idx).t / 1000.0, val), m_dataSeries);
      m_crosshairHLine->setLine(plot.left(), scenePoint.y(), plot.right(), scenePoint.y());
      m_crosshairHLine->setVisible(true);
      QString unit;
      text += "   " + QString("%1 %2").arg(formatEngineeringValue(val, &unit)).arg(unit);
    }
    else
      m_crosshairHLine->setVisible(false);

    // fixed above the plot, left-aligned so only its end moves with the text
    m_cursorLabel->setText(text);
    m_cursorLabel->adjustSize();
    const QPoint topLeft = m_chartView->mapFromScene(plot.topLeft());
    m_cursorLabel->move(topLeft.x() + 4, 3);
    m_cursorLabel->show();
    m_cursorLabel->raise();
  }
}

void GraphWidget::handleChartMouseRelease(QMouseEvent *)
{
  m_mouseDown = false;
  m_mousePan = false;
}

void GraphWidget::handleChartWheel(QWheelEvent *ev)
{
  leaveAll();
  if (ev->angleDelta().x() < 0 || ev->angleDelta().y() < 0)
    Q_EMIT zoomOut(1.1);
  else
    Q_EMIT zoomIn(1.1);
}

QString GraphWidget::formatEngineeringValue(double value, QString *unit) const
{
  QString prefix;
  const QString text = SiPrefix::format(value, &prefix);
  if (unit)
    *unit = prefix + m_store->unit();
  return text;
}

bool GraphWidget::exportDataSLOT()
{
  QDir path;
  QFileInfo fileInfo(m_cfg->getString("QtDMM/LastUsesPath"));
  QStringList validSuffixes = { "csv", "xlsx", "ods" };
  QString fnSuffix = validSuffixes.contains(fileInfo.suffix()) ? fileInfo.suffix() : "csv";
  QString fn = fileInfo.baseName().isEmpty() ? "untitled." + fnSuffix : fileInfo.absolutePath() + "/untitled." + fnSuffix;
  const QString csvFilter = tr("CSV (*.csv)"), xlsxFilter = tr("Excel (*.xlsx)"), odsFilter = tr("OpenDocument (*.ods)");
  // every reading at its time instead of the sample time's grid
  const QString rawFilter = tr("CSV, every reading (*.csv)");
  QString filter = fnSuffix == "xlsx" ? xlsxFilter : fnSuffix == "ods" ? odsFilter : csvFilter;
  fn = QFileDialog::getSaveFileName(this, tr("Export data"), fn,
                                    csvFilter + ";;" + xlsxFilter + ";;" + odsFilter + ";;" + rawFilter, &filter);

  if (fn.isNull())
    return false;
  // the chosen filter decides when no suffix was typed
  if (QFileInfo(fn).suffix().isEmpty())
    fn += filter == xlsxFilter ? ".xlsx" : filter == odsFilter ? ".ods" : ".csv";

  return exportCsvFile(fn, filter == rawFilter);
}

bool GraphWidget::exportCsvFile(const QString &fileName, bool raw)
{
  if (m_store->count() <= 0)
    return false;

  m_cfg->setString("QtDMM/LastUsesPath", QDir().absoluteFilePath(fileName));

  QString err;
  if (!m_store->write(fileName, &err, raw))
  {
    Q_EMIT error(err);
    return false;
  }
  return true;
}


void GraphWidget::importDataSLOT()
{
  // unsaved data: InstanceWidget::importSLOT() asks first
  QString fn = QFileDialog::getOpenFileName(this, tr("Import data"), m_cfg->getString("QtDMM/LastUsesPath", tr("CSV (*.csv);;All files (*)")));

  if (!fn.isNull())
    importCsvFile(fn);
}

bool GraphWidget::importCsvFile(const QString &fileName)
{
  m_cfg->setString("QtDMM/LastUsesPath", QDir().absoluteFilePath(fileName));

  QString err;
  const std::optional<Recording> rec = RecordingFile::read(fileName, &err);
  if (!rec)
  {
    Q_EMIT error(err);
    return false;
  }

  setUnit(rec->unit);
  m_store->setSampleTime(rec->sampleTimeTenths);
  const int cnt = int(rec->values.size());
  if (cnt > 1)
    Q_EMIT sampleTime(m_store->sampleTime());

  m_scaleMin =  1e40;
  m_scaleMax = -1e40;

  m_sourceFile = QFileInfo(fileName).fileName();
  m_store->load(*rec);
  updateStateLabel();

  setScale(true, true, 0, 0);
  // the graph shows the import by itself
  rebuildSeries();
  updateScrollRange();
  scrollbar->setValue(0);
  updateXAxisRange();
  updateTimeButtons();

  Q_EMIT error(fileName);
  update();
  return true;
}


void GraphWidget::setThresholds(double falling, double raising)
{
  m_store->setThresholds(falling, raising);

  updateThresholdLinesVisibility();
}

void GraphWidget::setMode(GraphWidget::SampleMode mode)
{
  m_store->setStartMode(RecordingStore::StartMode(mode));

  updateThresholdLinesVisibility();
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


    for (int i = 0; i < m_store->count(); i++)
      computeMinMax(m_store->series().at(i).value);


  }

  setYRange(m_scaleMin, m_scaleMax);
  updateThresholdLinePositions();
}

bool GraphWidget::computeMinMax(double val)
{
  bool ret = false;

  if (val > m_scaleMax * 0.95)
  {
    if (val > 0)
      m_scaleMax = val * 1.2;
    else
      m_scaleMax = val / 1.2;

    ret = true;
  }
  if (val < m_scaleMin * 0.95)
  {
    if (val > 0)
      m_scaleMin = val / 1.2;
    else
      m_scaleMin = val * 1.2;

    ret = true;
  }

  return ret;
}

void GraphWidget::setColors(const QColor &bg, const QColor &grid,
                         const QColor &data, const QColor &cursor,
                         const QColor &start,
                         const QColor &integration, const QColor &intThreshold)
{
  m_bgColor           = bg;
  m_gridColor         = grid;
  m_dataColor         = data;
  m_cursorColor       = cursor;
  m_startColor        = start;
  m_intColor          = integration;
  m_intThresholdColor = intThreshold;

  applyThemeColors();
  updateSeriesAppearance();
}

void GraphWidget::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels,
                              const QColor &data)
{
  m_themeBackground = background;
  m_themeGrid = grid;
  m_themeLabels = labels;
  m_themeData = data;
  applyThemeColors();
  updateSeriesAppearance();
}

// The curve colour: a colour chosen in the settings stays in every
// variant; the default blue is replaced by the variant's (or the design's).
QColor GraphWidget::dataColor() const
{
  if (m_variant == Custom || m_dataColor != QColor(Qt::blue))
    return m_dataColor;
  switch (m_variant)
  {
    case ScopeBlue:     return QColor("#ffe14d");
    case PhosphorGreen: return QColor::fromHsv(135, 190, 255);
    case PhosphorAmber: return QColor::fromHsv(38, 230, 255);
    case ChartRecorder: return QColor("#1f4e9c");
    default:            return m_themeData.isValid() ? m_themeData : m_dataColor;
  }
}

// The same for the integration curve (default dark blue); phosphor keeps
// one colour and tells the curves apart by brightness and dashes.
QColor GraphWidget::intColor() const
{
  if (m_variant == Custom || m_intColor != QColor(Qt::darkBlue))
    return m_intColor;
  switch (m_variant)
  {
    case ScopeBlue:     return QColor("#4de8ff");
    case PhosphorGreen: return QColor::fromHsv(135, 190, 190);
    case PhosphorAmber: return QColor::fromHsv(38, 230, 190);
    case ChartRecorder: return QColor("#c0282d");
    default:            return m_themeData.isValid() ? m_themeData.lighter(140) : m_intColor;
  }
}

void GraphWidget::setColorVariant(ColorVariant defaultVariant, int override)
{
  m_defaultVariant = defaultVariant;
  m_variantOverride = override;
  m_variant = override >= 0 ? static_cast<ColorVariant>(override) : defaultVariant;
  applyThemeColors();
  updateSeriesAppearance();
}

QString GraphWidget::variantTitle(ColorVariant variant)
{
  switch (variant)
  {
    case ScopeBlue:     return tr("Scope blue");
    case PhosphorGreen: return tr("Phosphor green");
    case PhosphorAmber: return tr("Phosphor amber");
    case ChartRecorder: return tr("Chart recorder");
    case Custom:        return tr("Custom");
    default:            return tr("Neutral");
  }
}

QString GraphWidget::variantName(ColorVariant variant)
{
  switch (variant)
  {
    case ScopeBlue:     return "scope";
    case PhosphorGreen: return "phosphor-green";
    case PhosphorAmber: return "phosphor-amber";
    case ChartRecorder: return "recorder";
    case Custom:        return "custom";
    default:            return "neutral";
  }
}

GraphWidget::ColorVariant GraphWidget::variantFromName(const QString &name)
{
  for (ColorVariant v : { ScopeBlue, PhosphorGreen, PhosphorAmber, ChartRecorder, Custom })
    if (name == variantName(v))
      return v;
  return Neutral;
}

// Background, grid and lettering. Neutral follows the window design (on
// System the palette), Custom takes the settings page, the others bring
// their own; the scope-like ones also switch to 10 x 8 divisions.
void GraphWidget::applyThemeColors()
{
  QBrush background, plot(Qt::NoBrush);
  QPen grid, minor, line;
  QColor labels;
  int minors = 0;
  auto vgrad = [](const QColor &top, const QColor &bottom)
  {
    QLinearGradient g(0, 0, 0, 1);
    g.setCoordinateMode(QGradient::ObjectBoundingMode);
    g.setColorAt(0, top);
    g.setColorAt(1, bottom);
    return QBrush(g);
  };
  switch (m_variant)
  {
    case ScopeBlue:
      background = QColor("#0b1633");
      plot = vgrad(QColor("#16306a"), QColor("#081338"));
      grid = QPen(QColor(90, 130, 200, 150), 1, Qt::DashLine);
      line = QPen(QColor("#5a78b0"));
      labels = QColor("#b9c9e8");
      break;
    case PhosphorGreen:
    case PhosphorAmber:
    {
      const bool green = m_variant == PhosphorGreen;
      background = green ? QColor("#0a0d0a") : QColor("#0d0b08");
      plot = green ? QColor("#060d08") : QColor("#0e0a04");
      grid = QPen(green ? QColor(60, 150, 85, 110) : QColor(190, 120, 30, 110), 1);
      line = grid;
      labels = green ? QColor("#5fbf78") : QColor("#d99a2b");
      m_centreTicks->setPen(QPen(green ? QColor(80, 190, 110, 150) : QColor(220, 150, 50, 150), 1));
      break;
    }
    case ChartRecorder:
      background = QColor("#f4eedb");
      plot = QColor("#fbf6e6");
      grid = QPen(QColor(214, 140, 130, 190), 1);
      minor = QPen(QColor(226, 176, 166, 110), 0.6);
      line = QPen(QColor(190, 120, 110));
      labels = QColor("#5a4636");
      minors = 4;   // millimetre-ish paper
      break;
    case Custom:
      background = m_bgColor;
      grid = QPen(m_gridColor);
      line = QPen(m_defaultAxisLine);
      // lettering that reads on the chosen background
      labels = m_bgColor.lightness() < 128 ? QColor(220, 220, 220) : QColor(30, 30, 30);
      break;
    case Neutral:
      if (m_themeBackground.style() != Qt::NoBrush)
      {
        background = m_themeBackground;
        grid = QPen(m_themeGrid);
        labels = m_themeLabels;
      }
      else
      {
        // the System design: the palette, as the table and the dialogs
        const QPalette pal = palette();
        background = pal.base();
        grid = QPen(pal.color(QPalette::Mid));
        labels = pal.color(QPalette::Text);
      }
      line = QPen(labels);
      break;
  }
  m_chart->setBackgroundBrush(background);
  // the scrollbar in the graph's colours: a dark graph gets no light strip
  {
    QPalette pal = scrollbar->palette();
    for (QPalette::ColorRole role : { QPalette::Window, QPalette::Base, QPalette::Button })
      pal.setBrush(role, background);
    pal.setColor(QPalette::WindowText, labels);
    pal.setColor(QPalette::ButtonText, labels);
    scrollbar->setPalette(pal);
    scrollbar->setAutoFillBackground(true);
  }
  // the time buttons in the lettering colour, the chosen one framed
  if (m_timeBar)
    m_timeBar->setStyleSheet(QString("QToolButton { color: %1; border: 1px solid transparent; border-radius: 2px;"
                                     " padding: 0px 4px; background: transparent; }"
                                     " QToolButton:checked { border-color: %1; }"
                                     " QToolButton:hover { border-color: %2; }")
                               .arg(labels.name(), QColor(labels.red(), labels.green(), labels.blue(), 110).name(QColor::HexArgb)));
  if (m_cursorLabel)
    m_cursorLabel->setStyleSheet(QString("QLabel { color: %1; background: transparent; }").arg(labels.name()));
  m_stateColor = labels;
  updateStateLabel();
  m_chart->setPlotAreaBackgroundBrush(plot);
  m_chart->setPlotAreaBackgroundVisible(plot.style() != Qt::NoBrush);
  for (QValueAxis *axis : { m_xAxis, m_yAxis })
  {
    axis->setGridLinePen(grid);
    axis->setMinorTickCount(minors);
    axis->setMinorGridLineVisible(minors > 0);
    if (minors > 0)
      axis->setMinorGridLinePen(minor);
    axis->setLabelsBrush(labels);
    axis->setTitleBrush(labels);
    axis->setLinePen(line);
  }
  // 10 x 8 divisions for the scope-like variants, Qt's default otherwise;
  // their round steps need no forced decimals. The x ticks are time steps
  // (updateXAxisRange()), labelled by updateXLabels(): Qt's own x labels
  // only keep the room for them.
  m_yAxis->setTickCount(divisions() ? 9 : 5);
  m_xAxis->setLabelFormat("%.4g");
  m_yAxis->setLabelFormat("%.4g");   // invisible, only keeps the room
  m_xLabelColor = labels;
  m_xAxis->setLabelsBrush(Qt::transparent);
  m_yAxis->setLabelsBrush(Qt::transparent);
  updateYLabels();
  m_yTitle->setBrush(labels);

  // cursor and threshold lines: a colour chosen in the settings stays,
  // the defaults follow the variant (a black cursor on phosphor is lost)
  const bool own = m_variant == Custom;
  auto pick = [own](const QColor &setting, Qt::GlobalColor def, const QColor &variant)
  {
    return own || setting != QColor(def) || !variant.isValid() ? setting : variant;
  };
  QColor start;
  switch (m_variant)
  {
    case ScopeBlue:     start = QColor("#ff5fd2"); break;
    case PhosphorGreen: start = QColor::fromHsv(135, 190, 160); break;
    case PhosphorAmber: start = QColor::fromHsv(38, 230, 160); break;
    case ChartRecorder: start = QColor("#2e7d32"); break;
    default: break;
  }
  const Qt::PenStyle lineStyle = phosphor() ? Qt::DashLine : Qt::SolidLine;
  const QColor cursor = pick(m_cursorColor, Qt::black, labels);
  m_crosshairVLine->setPen(QPen(cursor));
  m_crosshairHLine->setPen(QPen(cursor));
  m_triggerLine->setPen(QPen(pick(m_startColor, Qt::magenta, start), 1, lineStyle));
  // dotted: by default it takes the integration curve's colour and must not
  // pass for a curve (phosphor dashes the curve)
  m_integrationLine->setPen(QPen(pick(m_intThresholdColor, Qt::darkBlue, intColor()), 1, Qt::DotLine));
  updateXAxisRange();
  setYRange(m_scaleMin, m_scaleMax);
  updateThresholdLinePositions();
}

void GraphWidget::setLineStyle(int lineMode, int pointMode, int intLineMode, int intPointMode)
{
  m_lineMode = static_cast<LineMode>(lineMode);
  m_pointMode = static_cast<PointMode>(pointMode);
  m_intLineMode = static_cast<LineMode>(intLineMode);
  m_intPointMode = static_cast<PointMode>(intPointMode);

  updateSeriesAppearance();
}

void GraphWidget::setLine(int d, int i)
{
  m_lineWidth    = d;
  m_intLineWidth = i;

  updateSeriesAppearance();
}

Qt::PenStyle GraphWidget::penStyle(LineMode mode)
{
  switch (mode)
  {
    case NoLine:
      return Qt::NoPen;
    case Solid:
      return Qt::SolidLine;
    case Dot:
      return Qt::DotLine;
  }
  return Qt::SolidLine;
}

bool GraphWidget::migrateIntegralScale(Settings *cfg)
{
  if (!cfg || cfg->getBool("Graph/int-scale-per-second"))
    return false;
  // the sample time as RecorderPrefs keeps it: a count and its unit (0.1 s,
  // s, min, h, days)
  static const double unitSeconds[] = { 0.1, 1, 60, 3600, 86400 };
  const int unit = qBound(0, cfg->getInt("Sample/rate-unit", 1), 4);
  const double seconds = qMax(1, cfg->getInt("Sample/rate", 1)) * unitSeconds[unit];
  bool changed = false;
  if (seconds != 1.0)
  {
    const double scale = EngNumberValidator::value(cfg->getString("Graph/int-scale", "1.0"));
    cfg->setString("Graph/int-scale", QString::number(scale / seconds, 'g', 12));
    changed = true;
  }
  cfg->setBool("Graph/int-scale-per-second", true);
  cfg->save();
  return changed;
}

void GraphWidget::setIntegration(bool showInt, double sc, double th, double off)
{
  m_showIntegration = showInt;
  m_integrationScale = sc;
  m_store->setIntegrationThreshold(th);
  m_integrationOffset = off;

  rebuildSeries();
  updateSeriesAppearance();
  updateThresholdLinesVisibility();
}


void GraphWidget::popupSLOT(QAction *action)
{
  switch (action->property("ID").toInt())
  {
    case IDConnect:
      Q_EMIT connectDMM(true);
      break;
    case IDDisconnect:
      Q_EMIT connectDMM(false);
      break;
    case IDStopRecorder:
      Q_EMIT recordRequested(false);
      break;
    case IDStartRecorder:
      Q_EMIT recordRequested(true);
      break;
    case IDLive:
      Q_EMIT liveRequested();
      break;
    case IDClearGraph:
      Q_EMIT clearRequested();
      break;
    case IDConfigure:
      Q_EMIT configure();
      break;
    case IDExportData:
      Q_EMIT exportData();
      break;
    case IDImportData:
      Q_EMIT importData();
      break;
    case IDCopyImage:
      copyImageSLOT();
      break;
    case IDExportImage:
      exportImageSLOT();
      break;
    case IDColorVariant:
      setColorVariant(m_defaultVariant, action->property("variant").toInt());
      Q_EMIT colorVariantChanged(m_variantOverride);
      break;
  }
}

bool GraphWidget::handleChartKey(QKeyEvent *ev)
{
  const bool shift = ev->modifiers() & Qt::ShiftModifier;

  switch (ev->key())
  {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      zoomInSLOT();
      return true;
    case Qt::Key_Minus:
      zoomOutSLOT();
      return true;
    case Qt::Key_0:
      zoomFitSLOT();
      return true;
    case Qt::Key_Left:
      pan(shift ? -0.5 : -0.1);
      return true;
    case Qt::Key_Right:
      pan(shift ? 0.5 : 0.1);
      return true;
    case Qt::Key_PageUp:
      pan(-1.0);
      return true;
    case Qt::Key_PageDown:
      pan(1.0);
      return true;
    case Qt::Key_Home:
      scrollToStart();
      return true;
    case Qt::Key_End:
      scrollToEnd();
      return true;
    default:
      return false;
  }
}

void GraphWidget::pan(double fraction)
{
  int step = qMax(1, qRound(m_windowSeconds * 10 * fabs(fraction)));
  if (fraction < 0)
    step = -step;
  scrollbar->setValue(qBound(0, scrollbar->value() + step, scrollbar->maximum()));
}

void GraphWidget::scrollToStart()
{
  scrollbar->setValue(0);
}

void GraphWidget::scrollToEnd()
{
  scrollbar->setValue(scrollbar->maximum());
}

void GraphWidget::copyImageSLOT()
{
  QGuiApplication::clipboard()->setPixmap(m_chartView->grab());
}

bool GraphWidget::exportImageSLOT()
{
  QFileInfo fileInfo(m_cfg->getString("QtDMM/LastImagePath"));
  const QStringList suffixes = { "svg", "pdf", "png", "jpg" };
  const QString suffix = suffixes.contains(fileInfo.suffix().toLower()) ? fileInfo.suffix().toLower() : "svg";
  const QString name = fileInfo.baseName().isEmpty() ? QString("untitled")
                                                     : fileInfo.absolutePath() + "/" + fileInfo.baseName();
  const QString svgFilter = tr("Scalable vector graphics (*.svg)"), pdfFilter = tr("PDF (*.pdf)"),
                pngFilter = tr("PNG image (*.png)"), jpgFilter = tr("JPEG image (*.jpg)");
  QString filter = suffix == "pdf" ? pdfFilter : suffix == "png" ? pngFilter
                 : suffix == "jpg" ? jpgFilter : svgFilter;
  QString fn = QFileDialog::getSaveFileName(this, tr("Export image"), name + "." + suffix,
                                            svgFilter + ";;" + pdfFilter + ";;" + pngFilter + ";;" + jpgFilter,
                                            &filter);
  if (fn.isNull())
    return false;
  // the dialog leaves the name alone when the user typed one: follow the
  // chosen filter, so picking "PDF" and typing "plot" does write a PDF
  const QString chosen = filter == pdfFilter ? "pdf" : filter == pngFilter ? "png"
                       : filter == jpgFilter ? "jpg" : "svg";
  if (!suffixes.contains(QFileInfo(fn).suffix().toLower()))
    fn += "." + chosen;

  m_cfg->setString("QtDMM/LastImagePath", QDir().absoluteFilePath(fn));
  return exportImageFile(fn);
}

bool GraphWidget::exportImageFile(const QString &fileName, QSize size)
{
  // the crosshair follows the mouse and would be baked into the picture at
  // whatever point the user right-clicked
  const bool crosshairShown = m_crosshairVLine->isVisible() || m_crosshairHLine->isVisible();
  if (crosshairShown)
  {
    m_crosshairVLine->hide();
    m_crosshairHLine->hide();
  }
  const bool ok = writeImage(fileName, size);
  if (crosshairShown)
  {
    m_crosshairVLine->show();
    m_crosshairHLine->show();
  }
  return ok;
}

bool GraphWidget::writeImage(const QString &fileName, QSize size)
{
  if (size.isEmpty())
    size = m_chartView->size();
  if (size.isEmpty())
    size = QSize(1024, 640);
  const QString suffix = QFileInfo(fileName).suffix().toLower();

  if (suffix == "svg")
  {
    QSvgGenerator svg;
    svg.setFileName(fileName);
    svg.setSize(size);
    svg.setViewBox(QRect(QPoint(0, 0), size));
    svg.setTitle(m_store->startDateTime().isValid()
                 ? tr("QtDMM recording, %1").arg(m_store->startDateTime().toString(Qt::ISODate))
                 : tr("QtDMM graph"));
    svg.setDescription(tr("%1 readings, unit %2").arg(m_store->count()).arg(m_store->unit()));
    QPainter p;
    if (!p.begin(&svg))
    {
      Q_EMIT error(tr("Could not write %1").arg(fileName));
      return false;
    }
    // into the requested size, as PDF and PNG: at the view's own size the
    // drawing sat in a corner of a larger viewBox
    m_chartView->render(&p, QRectF(QPointF(0, 0), QSizeF(size)));
    p.end();
  }
  else if (suffix == "pdf")
  {
    QPdfWriter pdf(fileName);
    pdf.setTitle(tr("QtDMM graph"));
    pdf.setCreator("QtDMM");
    // the page takes the graph's proportions, so nothing is stretched
    pdf.setPageSize(QPageSize(QSizeF(size.width(), size.height()), QPageSize::Point,
                              QString(), QPageSize::ExactMatch));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter p;
    if (!p.begin(&pdf))
    {
      Q_EMIT error(tr("Could not write %1").arg(fileName));
      return false;
    }
    m_chartView->render(&p, QRectF(QPointF(0, 0), QSizeF(pdf.width(), pdf.height())));
    p.end();
  }
  else
  {
    QPixmap pixmap(size);
    pixmap.fill(Qt::white);
    QPainter p(&pixmap);
    m_chartView->render(&p, QRectF(QPointF(0, 0), QSizeF(size)));
    p.end();
    if (!pixmap.save(fileName))
    {
      Q_EMIT error(tr("Could not write %1").arg(fileName));
      return false;
    }
  }
  Q_EMIT info(tr("Graph written to %1").arg(QFileInfo(fileName).fileName()));
  return true;
}
