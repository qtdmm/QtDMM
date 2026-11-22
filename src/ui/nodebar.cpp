// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/nodebar.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QToolButton>

#include "logic/logicnode.h"
#include "service/metercontroller.h"
#include "service/noderunner.h"
#include "ui/controlbar.h"

namespace
{
const QColor kGreen(60, 210, 70);
const QColor kYellow(235, 180, 30);
const QColor kDark(40, 55, 40);
constexpr int kLed = 14;
}

QPixmap NodeBar::led(const QColor &color, int size)
{
  const qreal dpr = qApp->devicePixelRatio();
  QPixmap pm(QSize(size, size) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const bool lit = color != kDark;
  QRadialGradient g(QPointF(size * 0.4, size * 0.4), size * 0.55);
  g.setColorAt(0, lit ? color.lighter(160) : color.lighter(130));
  g.setColorAt(1, lit ? color.darker(130) : color.darker(140));
  p.setBrush(g);
  p.setPen(QPen(QColor(20, 20, 20), 1));
  p.drawEllipse(QRectF(1.5, 1.5, size - 3, size - 3));
  return pm;
}

NodeBar::NodeBar(QWidget *parent) :
  QWidget(parent)
{
  setObjectName("ui_nodeBar");
  m_row = new QHBoxLayout(this);
  m_row->setContentsMargins(6, 0, 6, 6);
  m_row->setSpacing(4);
  m_status = new QLabel(this);
  m_status->setObjectName("ui_nodeStatusLed");
  m_status->setFixedSize(kLed, kLed);
  m_row->addWidget(m_status);
  m_row->addSpacing(6);
  m_row->addStretch(1);
}

void NodeBar::setController(MeterController *ctl)
{
  for (const QMetaObject::Connection &c : std::as_const(m_links))
    disconnect(c);
  m_links.clear();
  qDeleteAll(m_controls);
  m_controls.clear();
  m_ctl = ctl;
  NodeRunner *runner = NodeRunner::of(ctl);
  if (!runner)
    return;
  m_links << connect(ctl, &MeterController::linkStateChanged, this, &NodeBar::refresh);
  m_links << connect(runner, &NodeRunner::controlsChanged, this, &NodeBar::refresh);
  // the node's own, left to right after the status LED
  int at = m_row->count() - 1;   // before the stretch
  for (const LogicNode::Control &c : runner->node()->controls())
  {
    QWidget *w = nullptr;
    if (c.kind == LogicNode::Control::Kind::Led)
    {
      auto *led = new QLabel(this);
      led->setFixedSize(kLed, kLed);
      w = led;
    }
    else
    {
      auto *b = new QToolButton(this);
      b->setText(c.text);
      b->setToolButtonStyle(Qt::ToolButtonTextOnly);
      b->setCheckable(c.kind == LogicNode::Control::Kind::Switch);
      // all keys alike and neutral; a switch (HOLD) while on in the blue of
      // a meter's HOLD, pressed in - off must not look lit
      const QColor on = ControlBar::holdColour();
      b->setStyleSheet(QString("QToolButton { background: palette(button); color: palette(button-text);"
                               " border: 1px solid palette(mid); border-radius: 3px; padding: 2px 6px; }"
                               " QToolButton:hover { border-color: palette(highlight); }"
                               " QToolButton:pressed { background: palette(mid); }"
                               " QToolButton:checked { background: %1; color: white; border: 1px solid %2;"
                               " border-top-color: %3; padding: 3px 6px 1px 6px; }")
                         .arg(on.name(), on.darker(150).name(), on.darker(220).name()));
      connect(b, &QToolButton::clicked, this, [this, id = c.id]
      {
        if (NodeRunner *r = NodeRunner::of(m_ctl))
          r->press(id);
      });
      w = b;
    }
    w->setObjectName("ui_nodeControl_" + c.id);
    w->setToolTip(c.toolTip);
    m_row->insertWidget(at++, w);
    m_controls.insert(c.id, w);
  }
  refresh();
}

void NodeBar::refresh()
{
  const NodeRunner *runner = NodeRunner::of(m_ctl);
  if (!runner)
    return;
  const LogicNode *node = runner->node();
  QString why;
  if (!m_ctl->isWanted())
  {
    m_color = kDark;
    why = tr("Disconnected: it computes nothing");
  }
  else if (m_ctl->linkState() == MeterConnection::LinkState::Timeout)
  {
    m_color = kYellow;
    why = m_ctl->errorString();
  }
  else if (m_ctl->linkState() != MeterConnection::LinkState::Connected)
  {
    m_color = kYellow;
    why = tr("Waiting for its first computation");
  }
  else if (!node->isOn())
  {
    m_color = kYellow;
    why = tr("Held: the value stays as it is");
  }
  else
  {
    m_color = kGreen;
    why = tr("Computing");
  }
  m_status->setPixmap(led(m_color, kLed));
  m_status->setToolTip(why);
  for (auto it = m_controls.cbegin(); it != m_controls.cend(); ++it)
  {
    const bool on = node->checked(it.key());
    if (auto *b = qobject_cast<QToolButton *>(it.value()))
    {
      if (b->isCheckable())
        b->setChecked(on);
    }
    else if (auto *l = qobject_cast<QLabel *>(it.value()))
      l->setPixmap(led(on ? kGreen : kDark, kLed));
  }
}
