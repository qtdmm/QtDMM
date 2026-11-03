// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/meterview.h"

#include <QTimer>

#include "device/meterconnection.h"
#include "service/metercontroller.h"
#include "service/valuefeed.h"

MeterView::MeterView(DeviceSession *session, AnalogMeter *meter, QObject *parent) :
  QObject(parent),
  m_meter(meter),
  m_feed(new ValueFeed(session, this)),
  m_alertTimer(new QTimer(this))
{
  connect(m_feed, &ValueFeed::reading, meter, &AnalogMeter::showReading);
  connect(m_feed, &ValueFeed::staleChanged, meter, &AnalogMeter::setStale);
  // the memory of the value it shows, whichever the LCD follows
  connect(m_feed, &ValueFeed::valueMinMaxChanged, meter, &AnalogMeter::showValueMinMax);
  connect(m_feed, &ValueFeed::controllerChanged, this, &MeterView::follow);
  connect(m_feed, &ValueFeed::captionChanged, meter, &AnalogMeter::setCaption);
  connect(m_feed, &ValueFeed::noticeChanged, meter, &AnalogMeter::setNotice);
  // in the colour of an alarm of the meter it shows, blinking if wanted, with
  // that meter's PASS/FAIL band; the banner and the beep stay, the colour is
  // never the only sign
  m_alertTimer->setInterval(500);
  connect(m_alertTimer, &QTimer::timeout, this, [this]
  {
    m_alertPhase = !m_alertPhase;
    showAlertColor();
  });
  applyStyle();
  follow(nullptr);
}

void MeterView::setValue(const ValueRef &value)
{
  m_value = value;
  m_feed->setValue(value);
  m_meter->setValueId(value.id);
  if (const MeterController *ctl = m_feed->controller(); ctl && !ctl->valueMinMax(value.id).isEmpty())
    m_meter->showValueMinMax(value.id, ctl->valueMinMax(value.id).minimum(), ctl->valueMinMax(value.id).maximum());
}

void MeterView::setStyle(int style)
{
  m_style = style;
  applyStyle();
}

void MeterView::setLook(const Look &look)
{
  m_look = look;
  applyStyle();
  m_meter->setScaleMode(look.scaleMode);
  // the scale of the meter that has the value
  if (const MeterController *ctl = m_feed->controller())
    m_meter->setDisplayCounts(ctl->dmm()->dmmInfo().display);
  showAlertColor();
}

void MeterView::follow(const MeterController *ctl)
{
  m_meter->reset();
  m_meter->clearMinMax();
  if (ctl)
    m_meter->setDisplayCounts(ctl->dmm()->dmmInfo().display);
  disconnect(m_alertLink);
  disconnect(m_limitLink);
  if (ctl)
    m_limitLink = connect(ctl, &MeterController::limitTestChanged, this, &MeterView::applyLimits);
  applyLimits();
  m_alertColor = ctl ? ctl->alertColor() : QColor();
  m_alertPhase = true;
  if (ctl)
    m_alertLink = connect(ctl, &MeterController::alertColorChanged, this, [this](const QColor &color)
    {
      m_alertColor = color;
      m_alertPhase = true;
      showAlertColor();
    });
  showAlertColor();
}

void MeterView::applyLimits()
{
  // the test of the meter whose value it shows
  if (const MeterController *ctl = m_feed->controller())
  {
    const LimitTest &shown = ctl->limitTest();
    m_meter->setLimits(shown.enabled && shown.value.source.isEmpty() ? shown.value.id : -1, shown.low(), shown.high());
  }
  else
    m_meter->setLimits(-1);
}

void MeterView::applyStyle()
{
  AnalogMeterStyle style = shownStyle() == 1 ? AnalogMeterStyle::ivory() : AnalogMeterStyle::dark();
  style.ballistics = m_look.ballistics;
  style.redZoneFrom = m_look.redZone / 100.0;
  m_meter->setStyle(style);
}

void MeterView::showAlertColor()
{
  const bool blinking = m_alertColor.isValid() && m_look.blink;
  if (blinking && !m_alertTimer->isActive())
    m_alertTimer->start();
  else if (!blinking)
  {
    m_alertTimer->stop();
    m_alertPhase = true;
  }
  // the alarm's colour alone (not the limit test's), the needle stays easy
  // to follow against the green band
  m_meter->setAlertColor(m_alertPhase ? m_alertColor : QColor());
}
