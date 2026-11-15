// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/qcpgraphcommon.h"

#include "3rdparty/qcustomplot/qcustomplot.h"

#include <QVBoxLayout>

bool QcpGraphCommon::s_openGlDefault = true;

QcpGraphCommon::QcpGraphCommon(QWidget *parent, const QString &noData) : GraphCommon(parent)
{
  setMinimumSize(220, 180);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  m_plot = new QCustomPlot(this);
  layout->addWidget(m_plot);
#ifdef QCUSTOMPLOT_USE_OPENGL
  m_plot->setOpenGl(s_openGlDefault, 4);
#endif
  // the window's menu: the plot's right click is this widget's
  m_plot->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_plot, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    Q_EMIT customContextMenuRequested(mapFromGlobal(m_plot->mapToGlobal(pos)));
  });

  m_plot->plotLayout()->insertRow(0);
  m_titleElement = new QCPTextElement(m_plot, QString(), font());
  m_plot->plotLayout()->addElement(0, 0, m_titleElement);

  m_noData = new QCPItemText(m_plot);
  m_noData->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_noData->position->setCoords(0.5, 0.5);
  m_noData->setPositionAlignment(Qt::AlignCenter);
  m_noData->setText(noData);

  m_holdItem = new QCPItemText(m_plot);
  m_holdItem->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_holdItem->position->setCoords(0.98, 0.02);
  m_holdItem->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
  m_holdItem->setText(QStringLiteral("HOLD"));
  m_holdItem->setVisible(false);

  // the crosshair on a layer of its own: a mouse move replots only that
  m_plot->addLayer(QStringLiteral("crosshair"), m_plot->layer(QStringLiteral("overlay")), QCustomPlot::limBelow);
  m_crossLayer = m_plot->layer(QStringLiteral("crosshair"));
  m_crossLayer->setMode(QCPLayer::lmBuffered);
  m_crossX = new QCPItemStraightLine(m_plot);
  m_crossY = new QCPItemStraightLine(m_plot);
  m_crossText = new QCPItemText(m_plot);
  m_crossText->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_crossText->position->setCoords(0.02, 0.02);
  m_crossText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
  for (QCPAbstractItem *item : std::initializer_list<QCPAbstractItem *>{ m_crossX, m_crossY, m_crossText })
  {
    item->setLayer(m_crossLayer);
    item->setSelectable(false);
    item->setVisible(false);
  }
  m_plot->setMouseTracking(true);
  m_plot->installEventFilter(this);
  connect(m_plot, &QCustomPlot::mouseMove, this, &QcpGraphCommon::moveCrosshair);

  m_refreshTimer.setSingleShot(true);
  connect(&m_refreshTimer, &QTimer::timeout, this, &QcpGraphCommon::refreshPlot);
}

void QcpGraphCommon::setOpenGlEnabledDefault(bool enabled)
{
  s_openGlDefault = enabled;
}

void QcpGraphCommon::setNoDataText(const QString &text)
{
  m_noData->setText(text);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QcpGraphCommon::setNoDataVisible(bool visible)
{
  m_noData->setVisible(visible);
  if (visible)
    hideCrosshair();
}

void QcpGraphCommon::setTitle(const QString &title)
{
  if (title == this->title())
    return;
  GraphCommon::setTitle(title);
  m_titleElement->setText(title);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QcpGraphCommon::setTheme(const PlotTheme &theme)
{
  GraphCommon::setTheme(theme);
  theme.applyTo(m_plot);
  m_titleElement->setTextColor(theme.labels);
  for (QCPItemText *text : { m_noData, m_holdItem, m_crossText })
    text->setColor(theme.labels);
  const QPen cross(theme.labels, 1, Qt::DotLine);
  m_crossX->setPen(cross);
  m_crossY->setPen(cross);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QcpGraphCommon::setHold(bool hold)
{
  if (hold == this->hold())
    return;
  m_refreshTimer.stop();
  GraphCommon::setHold(hold);
  holdChanged(hold);
  m_holdItem->setVisible(hold);
  refreshPlot();
}

void QcpGraphCommon::clear()
{
  m_refreshTimer.stop();
  clearSamples();
  refreshPlot();
}

void QcpGraphCommon::scheduleRefresh()
{
  if (!hold() && !m_refreshTimer.isActive())
    m_refreshTimer.start(kRefreshMs);
}

void QcpGraphCommon::setAxisRange(QCPAxis *axis, const PlotAxis::Range &range)
{
  if (!axis)
    return;
  axis->setRange(range.minimum / range.factor, range.maximum / range.factor);
  axis->setLabel(range.title);
  m_axisUnits.insert(axis, range.unit);
}

QString QcpGraphCommon::axisValueText(QCPAxis *axis, double value) const
{
  const QString unit = m_axisUnits.value(axis);
  return unit.isEmpty() ? QString::number(value, 'g', 4) : QString("%1 %2").arg(QString::number(value, 'g', 4), unit);
}

QString QcpGraphCommon::crosshairText(double x, double y) const
{
  return QString("%1   %2").arg(axisValueText(m_plot->xAxis, x), axisValueText(m_plot->yAxis, y));
}

void QcpGraphCommon::moveCrosshair(QMouseEvent *event)
{
  const QPointF pos = event->position();
  if (m_noData->visible() || !m_plot->axisRect()->rect().contains(pos.toPoint()))
  {
    hideCrosshair();
    return;
  }
  const double x = m_plot->xAxis->pixelToCoord(pos.x());
  const double y = m_plot->yAxis->pixelToCoord(pos.y());
  m_crossX->point1->setCoords(x, 0);
  m_crossX->point2->setCoords(x, 1);
  m_crossY->point1->setCoords(0, y);
  m_crossY->point2->setCoords(1, y);
  m_crossText->setText(crosshairText(x, y));
  for (QCPAbstractItem *item : std::initializer_list<QCPAbstractItem *>{ m_crossX, m_crossY, m_crossText })
    item->setVisible(true);
  m_crossLayer->replot();
}

void QcpGraphCommon::hideCrosshair()
{
  if (!m_crossText->visible())
    return;
  for (QCPAbstractItem *item : std::initializer_list<QCPAbstractItem *>{ m_crossX, m_crossY, m_crossText })
    item->setVisible(false);
  m_crossLayer->replot();
}

bool QcpGraphCommon::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_plot && event->type() == QEvent::Leave)
    hideCrosshair();
  return GraphCommon::eventFilter(watched, event);
}
