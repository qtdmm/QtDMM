// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/powersupply.h"

#include <QDateTime>
#include <QJsonArray>
#include <QTimer>

#include <cmath>

namespace
{
qint64 now()
{
  return QDateTime::currentMSecsSinceEpoch();
}

// NaN as no entry: JSON has no NaN
void putNumber(QJsonObject &o, const char *key, double v)
{
  if (!std::isnan(v))
    o.insert(key, v);
}

double number(const QJsonObject &o, const char *key, double fallback)
{
  return o.contains(key) ? o.value(key).toDouble(fallback) : fallback;
}

bool same(double a, double b)
{
  return (std::isnan(a) && std::isnan(b)) || a == b;
}
}

// ---------------------------------------------------------------- settings

bool PowerSupply::ChannelSettings::operator==(const ChannelSettings &o) const
{
  return shown == o.shown && color == o.color && same(uMin, o.uMin) && same(uMax, o.uMax)
         && stepCoarse == o.stepCoarse && stepFine == o.stepFine;
}

QJsonObject PowerSupply::ChannelSettings::toJson() const
{
  QJsonObject o;
  o.insert("shown", shown);
  if (color.isValid())
    o.insert("color", color.name());
  putNumber(o, "uMin", uMin);
  putNumber(o, "uMax", uMax);
  o.insert("stepCoarse", stepCoarse);
  o.insert("stepFine", stepFine);
  return o;
}

PowerSupply::ChannelSettings PowerSupply::ChannelSettings::fromJson(const QJsonObject &o)
{
  ChannelSettings s;
  s.shown = o.value("shown").toBool(true);
  s.color = o.contains("color") ? QColor(o.value("color").toString()) : QColor();
  s.uMin = number(o, "uMin", kNone);
  s.uMax = number(o, "uMax", kNone);
  s.stepCoarse = qMax(1e-6, number(o, "stepCoarse", s.stepCoarse));
  s.stepFine = qMax(1e-6, number(o, "stepFine", s.stepFine));
  return s;
}

// ---------------------------------------------------------------- the supply

PowerSupply::PowerSupply(QObject *parent) : QObject(parent), m_timer(new QTimer(this))
{
  m_timer->setSingleShot(true);
  connect(m_timer, &QTimer::timeout, this, &PowerSupply::timeout);
}

PowerSupply::~PowerSupply() = default;

PowerSupply::ChannelState PowerSupply::state(int ch) const
{
  return valid(ch) ? m_states[ch] : ChannelState();
}

PowerSupply::ChannelSettings PowerSupply::settings(int ch) const
{
  return ch >= 0 && ch < int(m_settings.size()) ? m_settings[ch] : ChannelSettings();
}

void PowerSupply::setSettings(int ch, const ChannelSettings &settings)
{
  if (ch < 0 || ch >= int(m_settings.size()) || m_settings[ch] == settings)
    return;
  m_settings[ch] = settings;
  Q_EMIT settingsChanged(ch);
}

QJsonObject PowerSupply::settingsJson() const
{
  QJsonArray channels;
  for (const ChannelSettings &s : m_settings)
    channels.append(s.toJson());
  return QJsonObject{ { "channels", channels } };
}

void PowerSupply::setSettingsJson(const QJsonObject &o)
{
  const QJsonArray channels = o.value("channels").toArray();
  for (int ch = 0; ch < int(m_settings.size()); ++ch)
    setSettings(ch, ch < channels.size() ? ChannelSettings::fromJson(channels[ch].toObject()) : ChannelSettings());
}

QList<int> PowerSupply::shownChannels() const
{
  QList<int> list;
  for (int ch = 0; ch < int(m_settings.size()); ++ch)
    if (m_settings[ch].shown)
      list << ch;
  return list;
}

double PowerSupply::minVoltage(int ch) const
{
  const ChannelInfo info = channel(ch);
  const double chosen = settings(ch).uMin;
  return std::isnan(chosen) ? info.uMin : qBound(info.uMin, chosen, info.uMax);
}

double PowerSupply::maxVoltage(int ch) const
{
  const ChannelInfo info = channel(ch);
  const double chosen = settings(ch).uMax;
  return std::isnan(chosen) ? info.uMax : qBound(minVoltage(ch), chosen, info.uMax);
}

QColor PowerSupply::defaultColor(int ch)
{
  static const QColor colors[kMaxChannels] = { QColor(0xe0, 0xa0, 0x00), QColor(0x20, 0x90, 0xd0),
                                               QColor(0xd0, 0x40, 0x80), QColor(0x40, 0xa0, 0x40) };
  return colors[qBound(0, ch, kMaxChannels - 1)];
}

QColor PowerSupply::color(int ch) const
{
  const QColor chosen = settings(ch).color;
  return chosen.isValid() ? chosen : defaultColor(ch);
}

// ---------------------------------------------------------------- requests

bool PowerSupply::sendable(int ch) const
{
  return valid(ch) && m_settings[ch].shown && m_states[ch].connected && controlAllowed();
}

bool PowerSupply::requestVoltage(int ch, double u)
{
  if (!sendable(ch) || m_states[ch].external || !(channel(ch).caps & SetVoltage) || std::isnan(u))
    return false;
  const double step = std::pow(10.0, -channel(ch).uDecimals);
  const double v = qBound(minVoltage(ch), std::round(u / step) * step, maxVoltage(ch));
  ChannelState &s = m_states[ch];
  s.requested = v;
  s.setState = SetState::Pending;
  s.error.clear();
  m_deadline[ch] = now() + m_timeout;
  // the earliest deadline wakes the timer
  if (!m_timer->isActive() || m_timer->remainingTime() > m_timeout)
    m_timer->start(m_timeout);
  Q_EMIT stateChanged(ch);
  sendVoltage(ch, v);
  return true;
}

bool PowerSupply::requestOutput(int ch, bool on)
{
  if (!sendable(ch))
    return false;
  sendOutput(ch, on);
  return true;
}

void PowerSupply::allOff()
{
  for (int ch : shownChannels())
    requestOutput(ch, false);
}

void PowerSupply::timeout()
{
  const qint64 t = now();
  qint64 next = 0;
  for (int ch = 0; ch < int(m_states.size()); ++ch)
  {
    if (m_states[ch].setState != SetState::Pending || m_deadline[ch] == 0)
      continue;
    if (m_deadline[ch] <= t)
      failVoltage(ch, tr("No answer"));
    else
      next = next == 0 ? m_deadline[ch] : qMin(next, m_deadline[ch]);
  }
  if (next > 0)
    m_timer->start(int(next - t));
}

// ---------------------------------------------------------------- the backend's answers

void PowerSupply::confirmVoltage(int ch, double u)
{
  if (!valid(ch))
    return;
  ChannelState &s = m_states[ch];
  s.setpoint = u;
  s.setState = SetState::Confirmed;
  s.error.clear();
  m_deadline[ch] = 0;
  Q_EMIT stateChanged(ch);
}

void PowerSupply::failVoltage(int ch, const QString &error)
{
  if (!valid(ch))
    return;
  m_states[ch].setState = SetState::Failed;
  m_states[ch].error = error;
  m_deadline[ch] = 0;
  Q_EMIT stateChanged(ch);
}

void PowerSupply::confirmOutput(int ch, bool on)
{
  if (!valid(ch) || m_states[ch].output == on)
    return;
  m_states[ch].output = on;
  Q_EMIT stateChanged(ch);
}

void PowerSupply::setMeasured(int ch, double u, double i, double p)
{
  if (!valid(ch))
    return;
  m_states[ch].u = u;
  m_states[ch].i = i;
  m_states[ch].p = p;
  Q_EMIT stateChanged(ch);
}

void PowerSupply::setMode(int ch, Mode mode)
{
  if (!valid(ch) || m_states[ch].mode == mode)
    return;
  m_states[ch].mode = mode;
  Q_EMIT stateChanged(ch);
}

void PowerSupply::setExternal(int ch, bool external)
{
  if (!valid(ch) || m_states[ch].external == external)
    return;
  m_states[ch].external = external;
  Q_EMIT stateChanged(ch);
}

void PowerSupply::setConnected(bool connected)
{
  for (int ch = 0; ch < int(m_states.size()); ++ch)
    if (m_states[ch].connected != connected)
    {
      m_states[ch].connected = connected;
      Q_EMIT stateChanged(ch);
    }
}

void PowerSupply::channelsChanged()
{
  const int n = qBound(0, channelCount(), kMaxChannels);
  m_states.resize(n);
  m_settings.resize(n);
  m_deadline.resize(n);
  for (int ch = 0; ch < n; ++ch)
    Q_EMIT settingsChanged(ch);
}
