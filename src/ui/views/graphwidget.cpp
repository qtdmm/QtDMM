//======================================================================
// File:		dmmgraph.cpp
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
// GraphWidget: construction, the store, the time window, the recorder
// state and the settings. The other parts are in graphwidget_*.cpp:
// axes, curves, overlays, input, theme and export.

#include <QtGui>
#include <QtWidgets>
#include <QPen>
#include <cmath>

#include "ui/frameclock.h"
#include "ui/views/graphlegend.h"
#include "ui/views/graphstatestrip.h"
#include "ui/views/graphwidget.h"
#include "ui/views/graphwidget_p.h"
#include "ui/views/graphflags.h"
#include "ui/views/graphminimap.h"
#include "core/settings.h"
#include "ui/engnumbervalidator.h"

GraphWidget::GraphWidget(QWidget *parent): GraphWidget(parent, nullptr)
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

  // the overview in place of the scroll bar: it asks, the graph moves
  m_minimap = new GraphMinimap(this);
  m_minimap->setStore(m_store);
  // its times are those of the store it draws: another meter's is shifted
  connect(m_minimap, &GraphMinimap::moveRequested, this, [this](qint64 start) { showFrom(start + m_minimapShift); });
  connect(m_minimap, &GraphMinimap::endRequested, this, &GraphWidget::followEnd);
  connect(m_minimap, &GraphMinimap::widthRequested, this, [this](qint64 start, qint64 width, bool toEnd)
  {
    showSpan(start + m_minimapShift, width, toEnd);
  });

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
  // the right axes for the other values in other units, the third one
  // outside (added after it); hidden until one comes. Their grids would be
  // more: off
  m_yTitle = new QGraphicsSimpleTextItem(m_chart);
  m_yTitle->setFont(m_xAxis->titleFont());
  m_yTitle->setBrush(m_xAxis->titleBrush());
  for (SideAxis &side : m_right)
  {
    side.axis = new QCategoryAxis();
    m_chart->addAxis(side.axis, Qt::AlignRight);
    side.axis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    side.axis->setGridLineVisible(false);
    side.axis->setMinorGridLineVisible(false);
    side.axis->setVisible(false);
    side.title = new QGraphicsSimpleTextItem(m_chart);
    side.title->setFont(m_xAxis->titleFont());
    side.title->setBrush(m_xAxis->titleBrush());
    side.title->setVisible(false);
  }
  connect(m_chart, &QChart::plotAreaChanged, this, [this](const QRectF &)
  {
    placeYTitle();
    updateCentreTicks();
    updateXLabels();
    updateYLabels();
    updateStateLabel();
    updateLegend();
    // the view repaints only what changed: the old labels and titles beside
    // a plot area that moved (an axis on or off) would stay
    if (m_chartView)
      m_chartView->viewport()->update();
  });
  m_centreTicks = new QGraphicsPathItem(m_chart);
  m_centreTicks->setZValue(5);   // above the grid, below the curves' markers
  // the min/max band: below the reference line and the curve
  m_envelope = new QGraphicsPathItem(m_chart);
  m_envelope->setZValue(4);
  m_envelope->setPen(Qt::NoPen);
  m_envelope->setVisible(false);
  m_referenceLine = new QGraphicsLineItem(m_chart);
  m_referenceLine->setZValue(5);
  m_referenceLine->setData(0, QStringLiteral("reference"));
  m_referenceLine->setVisible(false);
  m_referenceLabel = new QGraphicsSimpleTextItem(m_chart);
  m_referenceLabel->setZValue(5);
  m_referenceLabel->setData(0, QStringLiteral("reference"));
  m_referenceLabel->setVisible(false);
  // the limit test's band: below the curve, as the min/max band
  m_limitBand = new QGraphicsRectItem(m_chart);
  m_limitBand->setZValue(4);
  m_limitBand->setPen(Qt::NoPen);
  m_limitBand->setBrush(QColor(0x3c, 0xb0, 0x4a, 28));
  m_limitBand->setData(0, QStringLiteral("limit"));
  m_limitBand->setVisible(false);
  for (QGraphicsLineItem *&line : m_limitLines)
  {
    line = new QGraphicsLineItem(m_chart);
    line->setZValue(5);
    line->setPen(QPen(QColor(0x3c, 0xb0, 0x4a), 1, Qt::DashLine));
    line->setData(0, QStringLiteral("limit"));
    line->setVisible(false);
  }
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
  for (int i = 0; i < 2; ++i)
  {
    m_cursorLine[i] = new QGraphicsLineItem(m_chart);
    m_cursorLine[i]->setZValue(1000);
    m_cursorLine[i]->setVisible(false);
    m_cursorName[i] = new GraphFlagItem(m_chart);
    m_cursorName[i]->setText(i ? QStringLiteral("B") : QStringLiteral("A"));
    m_cursorName[i]->setData(0, QStringLiteral("cursor-flag"));
    m_cursorName[i]->setZValue(1000);
    m_cursorName[i]->setVisible(false);
    m_cursorLevel[i] = new QGraphicsLineItem(m_chart);
    m_cursorLevel[i]->setPen(QPen(Qt::DashLine));   // the colour comes with the design
    m_cursorLevel[i]->setZValue(1000);
    m_cursorLevel[i]->setData(0, QStringLiteral("cursor-level"));
    m_cursorLevel[i]->setVisible(false);
  }

  m_chartView = new QChartView(m_chart, this);
  m_chartView->setRenderHint(QPainter::Antialiasing);
  // the whole view each time: the plot changes all over with each reading anyway, and partial updates
  // left pieces of old axis labels behind in a graph just added
  m_chartView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
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
  connect(m_popup, &QMenu::triggered, this, &GraphWidget::popupSLOT);

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

  // the statistics beside the plot, on or off with a button on the time bar
  m_statsPanel = new QScrollArea(this);
  m_statsPanel->setObjectName("ui_graphStatsPanel");
  m_statsPanel->setWidgetResizable(true);
  m_statsPanel->setFrameShape(QFrame::NoFrame);
  m_statsPanel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_statsLabel = new QLabel(m_statsPanel);
  m_statsLabel->setObjectName("ui_graphStats");
  m_statsLabel->setTextFormat(Qt::RichText);
  m_statsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_statsLabel->setWordWrap(true);
  m_statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_statsLabel->setMargin(6);
  m_statsPanel->setWidget(m_statsLabel);
  m_statsTimer.setSingleShot(true);
  m_statsTimer.setInterval(250);
  connect(&m_statsTimer, &QTimer::timeout, this, &GraphWidget::updateStats);
  bar->addSpacing(6);
  // the graph's switches, also in the menu's View (and hidden there)
  m_viewButtons = new QWidget(m_timeBar);
  auto *views = new QHBoxLayout(m_viewButtons);
  views->setContentsMargins(0, 0, 0, 0);
  views->setSpacing(2);
  auto viewButton = [this, views](const char *name, const QString &text, const QString &tip, bool checkable)
  {
    auto *b = new QToolButton(m_viewButtons);
    b->setObjectName(name);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(checkable);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::ArrowCursor);
    views->addWidget(b);
    return b;
  };
  m_overviewButton = viewButton("ui_graphOverviewButton", QString(QChar(0x229f)), tr("Overview below the graph"), true);   // ⊟
  connect(m_overviewButton, &QToolButton::toggled, this, &GraphWidget::setMinimapVisible);
  m_envelopeButton = viewButton("ui_graphEnvelopeButton", QString(QChar(0x224b)), tr("Min/max band"), true);   // ≋
  connect(m_envelopeButton, &QToolButton::toggled, this, &GraphWidget::setEnvelopeVisible);
  m_referenceButton = viewButton("ui_graphReferenceButton", QString(QChar(0x2015)), tr("Reference line"), true);   // ―
  {
    auto *menu = new QMenu(m_referenceButton);
    connect(menu, &QMenu::aboutToShow, this, [this, menu] { fillReferenceMenu(menu); });
    connect(menu, &QMenu::triggered, this, &GraphWidget::popupSLOT);
    // pressed while a line shows, whatever a click did to it
    connect(menu, &QMenu::aboutToHide, this, [this] { QTimer::singleShot(0, this, &GraphWidget::updateViewButtons); });
    m_referenceButton->setMenu(menu);
    m_referenceButton->setPopupMode(QToolButton::InstantPopup);
  }
  m_clearCursorsButton = viewButton("ui_graphClearCursorsButton", QStringLiteral("A\u2009B\u2009") + QChar(0x2715),
                                    tr("Remove time cursors (Esc)"), false);   // A B ✕
  connect(m_clearCursorsButton, &QToolButton::clicked, this, &GraphWidget::clearTimeCursors);
  QToolButton *note = viewButton("ui_graphNoteButton", QString(QChar(0x270e)), tr("Note now... (N)"), false);   // ✎
  connect(note, &QToolButton::clicked, this, [this] { askNote(); });
  bar->addWidget(m_viewButtons);
  updateViewButtons();
  m_statsButton = new QToolButton(m_timeBar);
  m_statsButton->setObjectName("ui_graphStatsButton");
  m_statsButton->setText(QString(QChar(0x03a3)));   // Σ
  m_statsButton->setCheckable(true);
  m_statsButton->setChecked(m_statsOn);
  m_statsButton->setAutoRaise(true);
  m_statsButton->setFocusPolicy(Qt::NoFocus);
  m_statsButton->setCursor(Qt::ArrowCursor);
  m_statsButton->setToolTip(tr("Statistics beside the graph: the cursors, from A to B, the window shown"));
  connect(m_statsButton, &QToolButton::toggled, this, &GraphWidget::setStatsVisible);
  bar->addWidget(m_statsButton);

  m_stateLabel = new QLabel(m_chartView);
  m_stateLabel->setObjectName("ui_graphState");
  m_stateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_stateLabel->hide();

  m_legend = new GraphLegend(m_chartView);
  m_otherTimer.setSingleShot(true);
  m_otherTimer.setInterval(250);
  connect(&m_otherTimer, &QTimer::timeout, this, &GraphWidget::onOtherReadings);
  m_legend->setObjectName("ui_graphLegend");
  m_legend->hide();
  // the meter's states over the time, at the bottom of the plot
  m_stateStrip = new GraphStateStrip(m_chartView);
  m_stateStrip->setObjectName("ui_graphStates");
  m_stateStrip->setProvider([this]
  {
    QList<GraphStateStrip::Span> out;
    const double from = m_xAxis->min() * 1000, width = (m_xAxis->max() - m_xAxis->min()) * 1000;
    if (width > 0)
      for (const StateSpan &s : stateSpans())
        out << GraphStateStrip::Span{ GraphStateStrip::Kind(s.kind), s.label, qBound(0.0, (s.from - from) / width, 1.0),
                                      qBound(0.0, (s.to - from) / width, 1.0), s.pattern };
    return out;
  });

  connectStore();
  // Live runs on with the time between the readings
  connect(FrameClock::instance(), &FrameClock::tick, this, &GraphWidget::frameTick);
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
  // the meter's other values
  connect(m_store, &RecordingStore::extraAppended, this, &GraphWidget::onExtraAppended);
  connect(m_store, &RecordingStore::extrasChanged, this, &GraphWidget::rebuildExtras);
  connect(m_store, &RecordingStore::cleared, this, &GraphWidget::rebuildExtras);
  connect(m_store, &RecordingStore::loaded, this, &GraphWidget::rebuildExtras);
}

void GraphWidget::setStore(RecordingStore *store)
{
  if (!store || store == m_store)
    return;
  m_store->disconnect(this);
  m_store = store;
  m_liveHold = false;
  m_minimap->setStore(store);
  connectStore();
  // the view shows the new store as it is
  onCleared();
  showUnit();
  rebuildSeries();
  syncMarks();
  onStateChanged();   // and the clock for its state
}

void GraphWidget::timeButtonClicked(int seconds)
{
  // "the last 5 min": Live follows again
  if (m_liveHold)
    followEnd();
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
    b->setChecked(seconds == 0 ? m_followAll : !m_followAll && seconds == m_windowSeconds);
  }
  placeTimeBar();
}

void GraphWidget::placeTimeBar()
{
  if (!m_timeBar)
    return;
  // with a right axis its title takes the corner: the buttons end at the
  // plot, and before the titles of the right axes
  int right = m_chartView->width() - 6;
  for (const SideAxis &side : m_right)
    if (side.title && side.title->isVisible())
    {
      right = qMin(right, m_chartView->mapFromScene(m_chart->plotArea().topRight()).x());
      right = qMin(right, m_chartView->mapFromScene(side.title->scenePos()).x() - 4);
    }
  // they begin after the left axis' title; the graph's switches give way
  // first when there is no room (the menu's View has them too)
  const int left = m_yTitle ? m_chartView->mapFromScene(m_yTitle->sceneBoundingRect().topRight()).x() + 6 : 0;
  // a window longer than the recording is no choice
  const int limit = lengthLimit();
  for (QToolButton *b : m_timeButtons)
  {
    const int seconds = b->property("seconds").toInt();
    b->setVisible(seconds == 0 || limit <= 0 || seconds <= limit);
  }
  if (m_viewButtons)
  {
    m_viewButtons->setVisible(m_viewButtonsOn);
    m_timeBar->adjustSize();
    if (m_viewButtonsOn && m_timeBar->width() > right - left)
      m_viewButtons->hide();
  }
  // still no room: the longest windows go, "All" stays (a graph not shown
  // has no width to go by)
  for (auto it = m_timeButtons.crbegin(); isVisible() && it != m_timeButtons.crend(); ++it)
  {
    m_timeBar->adjustSize();
    if (m_timeBar->width() <= right - left)
      break;
    if ((*it)->property("seconds").toInt() != 0)
      (*it)->hide();
  }
  m_timeBar->adjustSize();
  m_timeBar->move(qMax(left, right - m_timeBar->width()), 3);
  m_timeBar->raise();
}

void GraphWidget::placeCursorLabel()
{
  // above the plot, left-aligned so only its end moves with the text; with
  // no room beside the buttons inside the plot
  m_cursorLabel->adjustSize();
  const QPoint topLeft = m_chartView->mapFromScene(m_chart->plotArea().topLeft());
  const bool room = !m_timeBar || topLeft.x() + 4 + m_cursorLabel->width() + 4 <= m_timeBar->x();
  // inside: at the bottom, the legend is at the top
  const int bottom = m_chartView->mapFromScene(m_chart->plotArea().bottomLeft()).y();
  m_cursorLabel->move(topLeft.x() + 4, room ? 3 : bottom - m_cursorLabel->height() - 2);
  m_cursorLabel->show();
  m_cursorLabel->raise();
}

GraphWidget::~GraphWidget()
{
}

int GraphWidget::statsWidth() const
{
  if (!m_statsOn)
    return 0;
  // as wide as its widest row wants (in steps of 16), at least for a short
  // name and a number, at most half of the graph
  const int want = (qMax(170, m_statsWant) + 15) / 16 * 16;
  return qMin(want, qMax(120, width() / 2));
}

void GraphWidget::layoutParts()
{
  const int strip = m_minimapOn ? GraphMinimap::kHeight : 16;
  // the statistics at the left (or right), about a third of the graph
  const int side = statsWidth();
  const int w = width() - side;
  const int x = m_statsLeft ? side : 0;
  m_chartView->setGeometry(x, 0, w, height() - strip);
  scrollbar->setGeometry(x, height() - 16, w, 16);
  m_minimap->setGeometry(x, height() - strip, w, strip);
  if (m_statsPanel)
  {
    m_statsPanel->setGeometry(m_statsLeft ? 0 : w, 0, side, height());
    m_statsPanel->setVisible(m_statsOn);
  }
  scrollbar->setVisible(!m_minimapOn);
  m_minimap->setVisible(m_minimapOn);
}

void GraphWidget::setMinimapVisible(bool on)
{
  if (on == m_minimapOn)
    return;
  m_minimapOn = on;
  // without the overview nothing would bring a paused Live back
  if (!on)
    followEnd();
  layoutParts();
  updateViewButtons();
}

void GraphWidget::setStatsVisible(bool on)
{
  if (on == m_statsOn)
    return;
  m_statsOn = on;
  m_statsButton->setChecked(on);
  layoutParts();
  placeTimeBar();
  if (on)
    updateStats();
}

void GraphWidget::setStatsLeft(bool left)
{
  if (left == m_statsLeft)
    return;
  m_statsLeft = left;
  layoutParts();
}

void GraphWidget::setStatesVisible(bool on)
{
  m_statesOn = on;
  updateStateStrip();
}

void GraphWidget::updateStateStrip()
{
  if (!m_stateStrip)
    return;
  // over the plot's bottom, as wide as it; it draws only what there is
  const QRectF plot = m_chart->plotArea();
  const QPoint bottomLeft = m_chartView->mapFromScene(plot.bottomLeft());
  m_stateStrip->setGeometry(bottomLeft.x(), bottomLeft.y() - GraphStateStrip::fullHeight(), int(plot.width()),
                            GraphStateStrip::fullHeight());
  m_stateStrip->setVisible(m_statesOn);
  m_stateStrip->raise();
  m_stateStrip->update();
}

void GraphWidget::setViewButtonsVisible(bool on)
{
  m_viewButtonsOn = on;
  placeTimeBar();   // shows them where there is room
}

void GraphWidget::updateViewButtons()
{
  if (!m_viewButtons)
    return;
  const QSignalBlocker a(m_overviewButton), b(m_envelopeButton), c(m_referenceButton);
  m_overviewButton->setChecked(m_minimapOn);
  m_envelopeButton->setChecked(m_envelopeOn);
  // pressed while a reference line shows
  m_referenceButton->setChecked(m_referenceKind != RefOff);
  m_clearCursorsButton->setEnabled(m_cursorT[0] >= 0 || m_cursorT[1] >= 0);
}

void GraphWidget::setEnvelopeVisible(bool on)
{
  if (on == m_envelopeOn)
    return;
  m_envelopeOn = on;
  updateEnvelope();
  updateViewButtons();
}

void GraphWidget::updateMinimapView()
{
  const qint64 width = stretching() ? qint64(stretchedSeconds() * 1000) : qint64(qMax(1, m_windowSeconds)) * 1000;
  // on the time of the store it draws
  m_minimap->setView(windowStart() - m_minimapShift, width);
}

void GraphWidget::showFrom(qint64 start)
{
  const qint64 origin = m_store->origin();
  if (m_store->state() == RecordingStore::Live)
  {
    // the store runs on, the window stays where it was put
    m_liveHold = true;
    m_holdStart = qMax(origin, start);
    updateXAxisRange();
    updateMarkPositions();
    updateStateLabel();
  }
  else
    scrollbar->setValue(int(qBound<qint64>(0, (start - origin) / 100, qint64(scrollbar->maximum()))));
}

void GraphWidget::followEnd()
{
  if (m_store->state() != RecordingStore::Live)
  {
    scrollbar->setValue(scrollbar->maximum());
    return;
  }
  if (!m_liveHold)
    return;
  m_liveHold = false;
  updateXAxisRange();
  updateMarkPositions();
  updateStateLabel();
}

void GraphWidget::showSpan(qint64 start, qint64 width, bool atEnd)
{
  // a window of its own, as a zoom: back through the settings
  const int seconds = qMax(1, int((width + 500) / 1000));
  m_followAll = false;
  if (seconds != m_windowSeconds || seconds != m_savedWindow)
    Q_EMIT windowRequested(seconds);
  if (atEnd)
    followEnd();
  else
    showFrom(start);
  updateTimeButtons();
}

void GraphWidget::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  updateClockWant();
}

void GraphWidget::hideEvent(QHideEvent *event)
{
  QWidget::hideEvent(event);
  updateClockWant();
}

void GraphWidget::resizeEvent(QResizeEvent *)
{
  layoutParts();
  placeTimeBar();
  updateLegend();   // its names as long as the plot has room for

  // Simpler than tracking/restoring hover state across a resize: just hide it,
  // the next mouse move will reposition it correctly.
  hideCrosshair();

  updateThresholdLinePositions();
  // the thinning follows the plot's width
  if (bucketSize() != m_bucket)
    rebuildSeries();
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
  if (m_control.start)
    m_control.start();
  else
    m_store->start();
}

void GraphWidget::stopSLOT()
{
  if (m_control.stop)
    m_control.stop();
  else
    m_store->stop();
}

void GraphWidget::recordingSetupChanged()
{
  updateTimeButtons();
  updateThresholdLinesVisibility();
}

void GraphWidget::liveSLOT()
{
  endZoom(false);   // Live: the axes automatic again, the window follows
  followEnd();
  if (m_control.live)
    m_control.live();
  else
    m_store->live();
}

void GraphWidget::onStateChanged()
{
  const bool live = m_store->state() == RecordingStore::Live;
  m_liveHold = false;
  // a file viewed is gone with Live or a recording
  if (m_store->state() != RecordingStore::View && !m_sourceFile.isEmpty())
  {
    m_sourceFile.clear();
    dropFileStores();
    otherStoresChanged();   // the meters open now again
  }
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
  updateClockWant();
  Q_EMIT stateChanged(int(m_store->state()));
}

QString GraphWidget::stateText() const
{
  switch (m_store->state())
  {
    case RecordingStore::Live:
      return m_liveHold ? tr("Live paused") : QString();
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

void GraphWidget::clearSLOT()
{
  if (m_control.clear)
    m_control.clear();
  else
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
    // without a value the axis shows 0..1, not the range of what was before
    setYRange(m_scaleMin, m_scaleMax);
  }

  m_dataLine->clear();
  m_curvePts.clear();
  m_envelope->setVisible(false);
  m_intLine->clear();
  m_tailData = m_tailInt = 0;
  clearTimeCursors();
  if (m_liveHold)
  {
    m_liveHold = false;
    updateStateLabel();
  }
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
  // what the numbers are, on a second line for the tooltip
  QString tip;
  switch (m_store->state())
  {
    case RecordingStore::Live:
      txt += " - " + tr("Live");
      tip = tr("Live: the graph holds the readings of the last %1; older ones drop out. "
               "Record keeps them.").arg(durationText(limit / 1000));
      break;
    case RecordingStore::Record:
      txt += " - " + tr("Recording");
      tip = limit > 0 ? tr("Recording: the time recorded / the recording length set") : tr("Recording: the time recorded");
      break;
    case RecordingStore::View:
      txt += " - " + tr("Stopped");
      tip = tr("Stopped: the length of the recording shown");
      break;
  }
  Q_EMIT info(txt + '\n' + tip);
}

void GraphWidget::setMode(GraphWidget::SampleMode mode)
{
  m_store->setStartMode(RecordingStore::StartMode(mode));

  updateThresholdLinesVisibility();
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
