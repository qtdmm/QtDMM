// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/outputfront.h"

#include <QEvent>
#include <QIcon>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/controlbar.h"
#include "ui/views/lcdpanel.h"

OutputFront::OutputFront(QWidget *parent) : QWidget(parent), m_controls(new QWidget(this)), m_fold(new FoldButton(this))
{
  connect(m_fold, &FoldButton::toggled, this, [this](bool folded)
  {
    setFolded(folded);
    Q_EMIT foldToggled(folded);
  });
}

void OutputFront::setDisplay(LcdPanel *display)
{
  m_lcd = display;
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  // the LCD takes the room up to the height its width fills (resizeEvent());
  // more goes above and below: LCD and controls stay together
  layout->addStretch(1);
  layout->addWidget(m_lcd, 100);
  layout->addWidget(m_controls);
  layout->addWidget(m_fold, 0, Qt::AlignHCenter);
  layout->addStretch(1);

  m_gear = new PanelGear(m_lcd);
  m_gear->setInk(m_lcd->plateInk(false), m_lcd->plateInk(true));
  m_gear->hide();
  connect(m_gear, &PanelGear::clicked, this, &OutputFront::settingsRequested);
  m_lcd->installEventFilter(this);
}

void OutputFront::setCaption(const QString &caption)
{
  m_lcd->setCaption(caption);
  placeCorner();
}

void OutputFront::setFolded(bool folded)
{
  m_fold->setFolded(folded);
  m_controls->setVisible(!folded);
}

bool OutputFront::isFolded() const
{
  return m_fold->isFolded();
}

int OutputFront::controlsHeight() const
{
  const int spacing = layout() ? layout()->spacing() : 0;
  return (isFolded() ? 0 : m_controls->sizeHint().height() + spacing) + m_fold->sizeHint().height() + spacing;
}

QIcon OutputFront::led(bool on)
{
  QPixmap pm(12, 12);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(QColor(0x10, 0x10, 0x10), 1));
  p.setBrush(on ? QColor(0x30, 0xe0, 0x40) : QColor(0x3a, 0x40, 0x3a));
  p.drawEllipse(QRectF(1, 1, 10, 10));
  return QIcon(pm);
}

bool OutputFront::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_lcd && event->type() == QEvent::Resize)
    placeCorner();
  return QWidget::eventFilter(watched, event);
}

void OutputFront::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  // taller than the width fills at its narrowest shape, the LCD only
  // painted empty room above and below itself
  if (m_lcd)
    m_lcd->setMaximumHeight(qMax(m_lcd->minimumHeight(), int(width() / m_lcd->minimumAspect()) + 2));
}

void OutputFront::placeCorner()
{
  // at the right end of the name plate, as high as its letters (FrontPanel)
  const QRectF band = m_lcd ? m_lcd->plateBand() : QRectF();
  if (band.isEmpty())
  {
    m_gear->hide();
    return;
  }
  const int size = qBound(10, int(band.height() * 0.8), 18);
  m_gear->setFixedSize(size, size);
  m_gear->move(int(band.right() - size), int(band.center().y() - size / 2.0));
  m_gear->show();
  m_lcd->setPlateReserve(size + 6);
}
