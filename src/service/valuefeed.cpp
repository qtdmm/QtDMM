// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "service/valuefeed.h"

#include "core/devicelibrary.h"
#include "device/meterconnection.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"

ValueFeed::ValueFeed(DeviceSession *session, QObject *parent)
  : QObject(parent)
  , m_session(session)
{
  connect(session, &DeviceSession::opened, this, &ValueFeed::attach);
  connect(session, &DeviceSession::closed, this, &ValueFeed::attach);
}

void ValueFeed::setValue(const ValueRef &value)
{
  const bool other = value.id != m_value.id;
  m_value = value;
  if (other)
    m_heard = hasValueNow();
  attach();
  updateTexts();
}

void ValueFeed::attach()
{
  MeterController *ctl = m_session->controller(m_value);
  // the same meter: nothing to do (a closed one is gone from m_ctl already)
  if (ctl && ctl == m_ctl)
    return;
  for (const QMetaObject::Connection &c : std::as_const(m_links))
    disconnect(c);
  m_links.clear();
  m_ctl = ctl;
  // a meter connected for a while has the value already
  m_heard = hasValueNow();
  if (ctl)
  {
    m_links << connect(ctl, &MeterController::reading, this, &ValueFeed::reading)
            << connect(ctl, &MeterController::staleChanged, this, &ValueFeed::staleChanged)
            << connect(ctl, &MeterController::minimumChanged, this, &ValueFeed::minimumChanged)
            << connect(ctl, &MeterController::maximumChanged, this, &ValueFeed::maximumChanged)
            << connect(ctl, &MeterController::minMaxReset, this, &ValueFeed::minMaxReset)
            << connect(ctl, &MeterController::valueMinMaxChanged, this, &ValueFeed::valueMinMaxChanged);
    m_links << connect(ctl, &MeterController::reading, this, [this](const Reading &r)
    {
      if (r.id != m_value.id || r.quality == Quality::NoValue)
        return;
      m_heard = true;
      updateTexts();
    });
    m_links << connect(ctl, &MeterController::deviceNameChanged, this, &ValueFeed::updateTexts);
    // a new connection: nothing heard of it yet; a silent meter keeps its value
    m_links << connect(ctl->dmm(), &MeterConnection::linkStateChanged, this, [this](MeterConnection::LinkState state)
    {
      if (state == MeterConnection::LinkState::Connecting)
        m_heard = false;
      updateTexts();
    });
  }
  Q_EMIT controllerChanged(ctl);
  updateTexts();
  if (!ctl)
  {
    // the meter is not open: no value, not the last one it had
    Reading none;
    none.id = m_value.id;
    none.source = m_value.source;
    none.quality = Quality::NoValue;
    Q_EMIT staleChanged(false);
    Q_EMIT minMaxReset();
    Q_EMIT reading(none);
  }
}

bool ValueFeed::hasValueNow() const
{
  return m_ctl && m_ctl->lastReading(m_value.id).msecs > 0
         && m_ctl->dmm()->linkState() == MeterConnection::LinkState::Connected;
}

void ValueFeed::updateTexts()
{
  QString caption;
  QString notice;
  if (m_ctl)
  {
    caption = m_ctl->deviceName();
    if (!m_heard)
      switch (m_ctl->dmm()->linkState())
      {
        case MeterConnection::LinkState::Closed:     notice = tr("Not connected"); break;
        case MeterConnection::LinkState::Connecting: notice = tr("Connecting ..."); break;
        case MeterConnection::LinkState::Timeout:    notice = tr("No data"); break;
        case MeterConnection::LinkState::Error:      notice = tr("No connection"); break;
        // frames arrive, but not of this value yet
        case MeterConnection::LinkState::Connected:  notice = tr("Waiting for data"); break;
      }
  }
  else if (!m_value.source.isEmpty())
  {
    // an entry of My devices not connected: its name all the same
    if (const DeviceLibrary *library = m_session->library(); library && m_value.source.startsWith("device:"))
      if (const auto device = library->find(m_value.source.mid(7)))
        caption = device->name;
    notice = tr("Not connected");
  }
  if (caption != m_caption)
  {
    m_caption = caption;
    Q_EMIT captionChanged(caption);
  }
  if (notice != m_notice)
  {
    m_notice = notice;
    Q_EMIT noticeChanged(notice);
  }
}
