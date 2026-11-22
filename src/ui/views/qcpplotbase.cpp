// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/qcpplotbase.h"

#include "ui/views/qcp.h"

#include <QVBoxLayout>

bool QcpPlotBase::s_openGlDefault = false;

QcpPlotBase::QcpPlotBase(QWidget *parent) : GraphCommon(parent)
{
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

  // the crosshair on a layer of its own: a mouse move replots only that
  // (NeoQCP wants the result looked at; the layer is fetched right below)
  [[maybe_unused]] const bool added = m_plot->addLayer(QStringLiteral("crosshair"), m_plot->layer(QStringLiteral("overlay")),
                                                       QCustomPlot::limBelow);
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
  connect(m_plot, &QCustomPlot::mouseMove, this, &QcpPlotBase::moveCrosshair);

  m_refreshTimer.setSingleShot(true);
  connect(&m_refreshTimer, &QTimer::timeout, this, &QcpPlotBase::refreshPlot);
}

void QcpPlotBase::setOpenGlEnabledDefault(bool enabled)
{
  s_openGlDefault = enabled;
}

void QcpPlotBase::setTheme(const PlotTheme &theme)
{
  GraphCommon::setTheme(theme);
  theme.applyTo(m_plot);
  const QColor labels = theme.resolved(palette()).labels;
  m_crossText->setColor(labels);
  const QPen cross(labels, 1, Qt::DotLine);
  m_crossX->setPen(cross);
  m_crossY->setPen(cross);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QcpPlotBase::scheduleRefresh()
{
  if (!hold() && !m_refreshTimer.isActive())
    m_refreshTimer.start(kRefreshMs);
}

void QcpPlotBase::cancelRefresh()
{
  m_refreshTimer.stop();
}

void QcpPlotBase::setCrosshairEnabled(bool enabled)
{
  m_crosshairEnabled = enabled;
  if (!enabled)
    hideCrosshair();
}

void QcpPlotBase::setAxisRange(QCPAxis *axis, const PlotAxis::Range &range)
{
  if (!axis)
    return;
  axis->setRange(range.minimum / range.factor, range.maximum / range.factor);
  axis->setLabel(range.title);
  m_axisUnits.insert(axis, range.unit);
}

QString QcpPlotBase::axisValueText(QCPAxis *axis, double value) const
{
  const QString unit = m_axisUnits.value(axis);
  return unit.isEmpty() ? QString::number(value, 'g', 4) : QString("%1 %2").arg(QString::number(value, 'g', 4), unit);
}

QString QcpPlotBase::crosshairText(double x, double y) const
{
  return QString("%1   %2").arg(axisValueText(m_plot->xAxis, x), axisValueText(m_plot->yAxis, y));
}

QcpPlotBase::CrosshairAt QcpPlotBase::crosshairAt(double x, double y)
{
  return { x, y, true, crosshairText(x, y) };
}

void QcpPlotBase::setCrosshairPen(const QPen &pen)
{
  m_crossX->setPen(pen);
  m_crossY->setPen(pen);
}

void QcpPlotBase::moveCrosshair(QMouseEvent *event)
{
  const QPointF pos = event->position();
  if (!m_crosshairEnabled || !m_plot->axisRect()->rect().contains(pos.toPoint()))
  {
    hideCrosshair();
    return;
  }
  const CrosshairAt at = crosshairAt(m_plot->xAxis->pixelToCoord(pos.x()), m_plot->yAxis->pixelToCoord(pos.y()));
  m_crossX->point1->setCoords(at.x, 0);
  m_crossX->point2->setCoords(at.x, 1);
  m_crossY->point1->setCoords(0, at.y);
  m_crossY->point2->setCoords(1, at.y);
  m_crossText->setText(at.text);
  m_crossX->setVisible(true);
  m_crossText->setVisible(true);
  m_crossY->setVisible(at.horizontal);
  m_crossLayer->replot();
}

void QcpPlotBase::hideCrosshair()
{
  if (!m_crossText->visible())
    return;
  for (QCPAbstractItem *item : std::initializer_list<QCPAbstractItem *>{ m_crossX, m_crossY, m_crossText })
    item->setVisible(false);
  m_crossLayer->replot();
}

bool QcpPlotBase::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_plot && event->type() == QEvent::Leave)
    hideCrosshair();
  return GraphCommon::eventFilter(watched, event);
}
