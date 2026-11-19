// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/rack/nodestatus.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "logic/logicnode.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "service/noderunner.h"
#include "ui/panelframe.h"
#include "ui/rack/rackwindow.h"

namespace
{
bool waits(const MeterController *ctl)
{
  return ctl->linkState() == MeterConnection::LinkState::Timeout;
}
}

NodeStatus::NodeStatus(DeviceSession *session, QWidget *parent) :
  QWidget(parent), m_session(session)
{
  setObjectName("ui_nodeStatus");
  setWindowTitle(tr("Logic"));
  auto *top = new QVBoxLayout(this);
  top->setContentsMargins(14, 10, 14, 10);
  top->setSpacing(4);
  // the LEDs in the middle, the count and buttons below
  top->addStretch(1);
  m_leds = new QHBoxLayout;
  m_leds->setSpacing(2);
  m_leds->addStretch(1);
  m_leds->addStretch(1);
  top->addLayout(m_leds);
  top->addStretch(1);
  auto *bottom = new QHBoxLayout;
  m_count = new QLabel(this);
  m_count->setObjectName("ui_nodeStatusCount");
  bottom->addWidget(m_count, 1);
  m_warning = new QToolButton(this);
  m_warning->setObjectName("ui_nodeStatusWarning");
  m_warning->setAutoRaise(true);
  m_warning->setStyleSheet("QToolButton { color: #e0a020; font-weight: bold }");
  connect(m_warning, &QToolButton::clicked, this, [this]
  {
    Q_EMIT rackRequested(waiting().value(0));
  });
  bottom->addWidget(m_warning);
  auto *rack = new QToolButton(this);
  rack->setObjectName("ui_nodeStatusRack");
  rack->setText(tr("Rack"));
  rack->setToolTip(tr("Open the rack: the calculated values in full"));
  connect(rack, &QToolButton::clicked, this, [this] { Q_EMIT rackRequested(QString()); });
  bottom->addWidget(rack);
  top->addLayout(bottom);

  // LEDs and warnings follow the nodes twice a second while shown
  m_timer = new QTimer(this);
  m_timer->setInterval(500);
  connect(m_timer, &QTimer::timeout, this, &NodeStatus::refresh);
  connect(m_session, &DeviceSession::opened, this, &NodeStatus::rebuild);
  connect(m_session, &DeviceSession::closed, this, &NodeStatus::rebuild);
  rebuild();
}

void NodeStatus::setOrder(const QStringList &ids)
{
  if (ids == m_order)
    return;
  m_order = ids;
  rebuild();
}

QStringList NodeStatus::waiting() const
{
  QStringList ids;
  for (const QString &id : m_ids)
    if (const MeterController *ctl = m_session->controller("device:" + id); ctl && waits(ctl))
      ids << id;
  return ids;
}

void NodeStatus::rebuild()
{
  QList<MeterController *> nodes;
  for (MeterController *ctl : m_session->controllers())
    if (ctl->isNode() && NodeRunner::of(ctl))
      nodes << ctl;
  std::stable_sort(nodes.begin(), nodes.end(), [this](const MeterController *a, const MeterController *b)
  {
    const qsizetype ia = m_order.indexOf(a->deviceId());
    const qsizetype ib = m_order.indexOf(b->deviceId());
    return (ia < 0 ? std::numeric_limits<qsizetype>::max() : ia) < (ib < 0 ? std::numeric_limits<qsizetype>::max() : ib);
  });
  qDeleteAll(m_ledOf);
  m_ledOf.clear();
  m_ids.clear();
  for (MeterController *ctl : std::as_const(nodes))
  {
    const QString id = ctl->deviceId();
    auto *led = new QToolButton(this);
    led->setObjectName("ui_nodeStatusLed_" + id);
    led->setAutoRaise(true);
    led->setIconSize(QSize(16, 16));
    connect(led, &QToolButton::clicked, this, [this, id]
    {
      NodeRunner *r = NodeRunner::of(m_session->controller("device:" + id));
      if (!r)
        return;
      const bool on = !r->node()->isOn();
      r->setOn(on);
      refresh();
      Q_EMIT switched(id, on);
    });
    m_leds->insertWidget(1 + int(m_ids.size()), led);
    m_ledOf.insert(id, led);
    m_ids << id;
  }
  refresh();
}

void NodeStatus::refresh()
{
  int on = 0;
  QStringList warnings;
  for (const QString &id : std::as_const(m_ids))
  {
    const MeterController *ctl = m_session->controller("device:" + id);
    const NodeRunner *r = NodeRunner::of(ctl);
    QToolButton *led = m_ledOf.value(id);
    if (!r || !led)
      continue;
    const bool isOn = r->node()->isOn();
    on += isOn ? 1 : 0;
    led->setIcon(RackWindow::ledIcon(isOn));
    QString tip = isOn ? tr("%1: on (a click switches it off)").arg(ctl->deviceName())
                       : tr("%1: off (a click switches it on)").arg(ctl->deviceName());
    if (waits(ctl))
    {
      tip += '\n' + ctl->errorString();
      warnings << QString("%1: %2").arg(ctl->deviceName(), ctl->errorString());
    }
    led->setToolTip(tip);
  }
  m_count->setText(tr("%1 of %2 on").arg(on).arg(m_ids.size()));
  m_warning->setVisible(!warnings.isEmpty());
  m_warning->setText(QStringLiteral("⚠ %1").arg(warnings.size()));
  m_warning->setToolTip(warnings.join('\n') + '\n' + tr("A click shows the first in the rack"));
}

void NodeStatus::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  // a small front like the rack's inserts, on the bench's metal
  QPainter p(this);
  PanelFrame::smooth(p);
  const QPalette &pal = palette();
  PanelFrame::Colors c { pal.color(QPalette::Light), pal.color(QPalette::Dark), pal.color(QPalette::Button),
                         QColor(Qt::transparent) };
  const QRectF r = QRectF(rect()).adjusted(3, 3, -3, -3);
  PanelFrame::paint(p, r, c, r.topLeft(), r.width());
}

void NodeStatus::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  refresh();
  m_timer->start();
}

void NodeStatus::hideEvent(QHideEvent *event)
{
  QWidget::hideEvent(event);
  m_timer->stop();
}
