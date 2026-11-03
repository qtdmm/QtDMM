// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/frontpanel.h"

#include <QEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

#include "core/reading.h"
#include "device/meterconnection.h"
#include "service/metercontroller.h"
#include "service/valuefeed.h"
#include "ui/controlbar.h"
#include "ui/views/lcdwidget.h"

FrontPanel::FrontPanel(DeviceSession *session, QWidget *parent)
  : QWidget(parent)
  , m_feed(new ValueFeed(session, this))
  , m_display(new LcdWidget(this))
  , m_controls(new ControlBar(this))
  , m_fold(new FoldButton(m_display))
  , m_blinkTimer(new QTimer(this))
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  layout->addWidget(m_display, 1);
  layout->addWidget(m_controls);
  m_fold->hide();
  m_controls->hide();
  m_display->installEventFilter(this);

  connect(m_feed, &ValueFeed::reading, m_display, &LcdWidget::showReading);
  connect(m_feed, &ValueFeed::reading, m_controls, &ControlBar::showReading);
  connect(m_feed, &ValueFeed::staleChanged, m_display, &LcdWidget::setStale);
  connect(m_feed, &ValueFeed::minimumChanged, m_display, &LcdWidget::showMinimum);
  connect(m_feed, &ValueFeed::maximumChanged, m_display, &LcdWidget::showMaximum);
  connect(m_feed, &ValueFeed::minMaxReset, m_display, &LcdWidget::clearMinMax);
  connect(m_feed, &ValueFeed::controllerChanged, this, &FrontPanel::attach);
  connect(m_feed, &ValueFeed::noticeChanged, m_display, &LcdWidget::setNotice);
  m_blinkTimer->setInterval(500);
  connect(m_blinkTimer, &QTimer::timeout, this, [this]
  {
    m_phase = !m_phase;
    showAlert();
  });
  connect(m_fold, &FoldButton::toggled, this, [this](bool folded)
  {
    setFolded(folded);
    Q_EMIT foldToggled(folded);
  });
  connect(m_controls, &ControlBar::keyPressed, this, [this](const QString &key)
  {
    // a Bluetooth link takes a few seconds before it takes keys
    if (!m_ctl || !m_ctl->pressKey(key))
      Q_EMIT keyFailed();
  });
}

void FrontPanel::setValue(const ValueRef &value)
{
  m_feed->setValue(value);
  // the LCD itself falls back while the meter has fewer values (primaryShown())
  m_display->setPrimary(value.id);
}

ValueRef FrontPanel::value() const
{
  return m_feed->value();
}

void FrontPanel::setSecondaries(const std::optional<QList<int>> &ids)
{
  m_secondariesChosen = ids.has_value();
  m_display->setSecondaries(ids);
}

QList<int> FrontPanel::secondaries() const
{
  return m_display->secondaries();
}

MeterController *FrontPanel::controller() const
{
  return m_feed->controller();
}

void FrontPanel::attach(MeterController *ctl)
{
  // another meter: its lines afresh, none of the one before; its name above
  m_display->setCaption(ctl ? ctl->deviceName() : QString());
  m_display->setNotice(m_feed->notice());
  m_display->clearMinMax();
  for (int id = 0; id < kMaxValues; ++id)
  {
    Reading none;
    none.id = id;
    none.quality = Quality::NoValue;
    m_display->showReading(none);
  }
  disconnect(m_link);
  for (const QMetaObject::Connection &c : std::as_const(m_rules))
    disconnect(c);
  m_rules.clear();
  m_ctl = ctl;
  m_alertColor = ctl ? ctl->alertColor() : QColor();
  m_display->setLimitTest(ctl && ctl->limitTest().enabled);
  showVerdict(ctl ? ctl->limitTest().value.id : 0, ctl ? ctl->verdict() : LimitTest::NoVerdict, std::nan(""));
  if (ctl)
  {
    m_link = connect(ctl->dmm(), &MeterConnection::linkStateChanged, this, &FrontPanel::updateConnected);
    m_rules << connect(ctl, &MeterController::alertColorChanged, this, [this](const QColor &color)
    {
      m_alertColor = color;
      m_phase = true;
      showAlert();
    });
    m_rules << connect(ctl, &MeterController::limitJudged, this, &FrontPanel::showVerdict);
    m_rules << connect(ctl, &MeterController::limitTestChanged, this, [this](const LimitTest &test)
    {
      m_display->setLimitTest(test.enabled);
    });
  }
  updateKeys();
}

void FrontPanel::setBlink(bool blink)
{
  m_blink = blink;
  showAlert();
}

void FrontPanel::showVerdict(int id, LimitTest::Verdict v, double deviation)
{
  m_display->setVerdict(id == m_display->primaryShown() ? v : LimitTest::NoVerdict, deviation);
  const bool tint = m_ctl && m_ctl->limitTest().tint;
  m_limitColor = !tint || v == LimitTest::NoVerdict ? QColor()
                 : v == LimitTest::Pass ? QColor(0x3c, 0xb0, 0x4a) : QColor(0xd8, 0x22, 0x22);
  showAlert();
}

void FrontPanel::showAlert()
{
  const bool blinking = m_alertColor.isValid() && m_blink;
  if (blinking && !m_blinkTimer->isActive())
    m_blinkTimer->start();
  else if (!blinking)
  {
    m_blinkTimer->stop();
    m_phase = true;
  }
  // the alarm's colour over the limit test's; the colour is never the only
  // sign (banner, sound)
  m_display->setAlertColor(m_alertColor.isValid() ? (m_phase ? m_alertColor : QColor()) : m_limitColor);
}

void FrontPanel::updateKeys()
{
  const bool before = keysShown();
  m_keysSupported = m_ctl && ControlBar::supported(m_ctl->dmm()->dmmInfo());
  if (m_ctl)
    m_controls->setProtocol(m_ctl->dmm()->format(), m_ctl->dmm()->dmmInfo().model);
  m_fold->setVisible(m_keysSupported);
  m_controls->setVisible(keysShown());
  updateConnected();
  if (keysShown() != before)
    Q_EMIT keysChanged();
}

void FrontPanel::updateConnected()
{
  m_controls->setConnected(m_ctl && m_ctl->dmm()->isWanted());
}

void FrontPanel::setFolded(bool folded)
{
  const bool before = keysShown();
  m_fold->setFolded(folded);
  m_controls->setVisible(keysShown());
  if (keysShown() != before)
    Q_EMIT keysChanged();
}

bool FrontPanel::isFolded() const
{
  return m_fold->isFolded();
}

bool FrontPanel::keysShown() const
{
  return m_keysSupported && !m_fold->isFolded();
}

int FrontPanel::keysHeight() const
{
  return keysShown() ? m_controls->sizeHint().height() + 4 : 0;
}

bool FrontPanel::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_display && event->type() == QEvent::Resize)
    m_fold->move(m_display->width() - m_fold->width() - 6, 6);
  return QWidget::eventFilter(watched, event);
}
