// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/functiongenerator.h"

#include <QJsonArray>

#include <cmath>

#include "core/requestdeadlines.h"

namespace
{
using Shape = FunctionGenerator::Shape;
using Value = FunctionGenerator::Value;

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

// rounded by division: 3.3 stays 3.3, not 3.3000000000000003
double roundTo(double v, int decimals)
{
  const double scale = std::pow(10.0, decimals);
  return std::round(v * scale) / scale;
}

double roundSignificant(double v, int digits)
{
  if (v == 0 || !std::isfinite(v))
    return v;
  const int decimals = digits - 1 - int(std::floor(std::log10(std::abs(v))));
  return decimals >= 0 ? roundTo(v, decimals) : std::round(v / std::pow(10.0, -decimals)) * std::pow(10.0, -decimals);
}

// a limit the user may narrow: theirs within the device's
double narrowed(double device, double chosen)
{
  return std::isnan(chosen) ? device : qBound(0.0, chosen, device);
}

constexpr double kDutyMin = 1, kDutyMax = 99;
}

// ---------------------------------------------------------------- settings

bool FunctionGenerator::ChannelSettings::operator==(const ChannelSettings &o) const
{
  return shown == o.shown && color == o.color && same(ampMax, o.ampMax) && same(offsetMax, o.offsetMax)
         && ampStepCoarse == o.ampStepCoarse && ampStepFine == o.ampStepFine && offsetStepCoarse == o.offsetStepCoarse
         && offsetStepFine == o.offsetStepFine;
}

QJsonObject FunctionGenerator::ChannelSettings::toJson() const
{
  QJsonObject o;
  o.insert("shown", shown);
  if (color.isValid())
    o.insert("color", color.name());
  putNumber(o, "ampMax", ampMax);
  putNumber(o, "offsetMax", offsetMax);
  o.insert("ampStepCoarse", ampStepCoarse);
  o.insert("ampStepFine", ampStepFine);
  o.insert("offsetStepCoarse", offsetStepCoarse);
  o.insert("offsetStepFine", offsetStepFine);
  return o;
}

FunctionGenerator::ChannelSettings FunctionGenerator::ChannelSettings::fromJson(const QJsonObject &o)
{
  ChannelSettings s;
  s.shown = o.value("shown").toBool(true);
  s.color = o.contains("color") ? QColor(o.value("color").toString()) : QColor();
  s.ampMax = number(o, "ampMax", kNone);
  s.offsetMax = number(o, "offsetMax", kNone);
  s.ampStepCoarse = qMax(1e-6, number(o, "ampStepCoarse", s.ampStepCoarse));
  s.ampStepFine = qMax(1e-6, number(o, "ampStepFine", s.ampStepFine));
  s.offsetStepCoarse = qMax(1e-6, number(o, "offsetStepCoarse", s.offsetStepCoarse));
  s.offsetStepFine = qMax(1e-6, number(o, "offsetStepFine", s.offsetStepFine));
  return s;
}

// ---------------------------------------------------------------- the generator

FunctionGenerator::FunctionGenerator(QObject *parent) : QObject(parent), m_deadlines(new RequestDeadlines(this))
{
  m_deadlines->setTimeout(kDefaultConfirmTimeout);
  connect(m_deadlines, &RequestDeadlines::expired, this, &FunctionGenerator::expired);
}

FunctionGenerator::~FunctionGenerator() = default;

FunctionGenerator::ChannelState FunctionGenerator::state(int ch) const
{
  return valid(ch) ? m_states[ch] : ChannelState();
}

FunctionGenerator::ChannelSettings FunctionGenerator::settings(int ch) const
{
  return ch >= 0 && ch < int(m_settings.size()) ? m_settings[ch] : ChannelSettings();
}

void FunctionGenerator::setSettings(int ch, const ChannelSettings &settings)
{
  if (ch < 0 || ch >= int(m_settings.size()) || m_settings[ch] == settings)
    return;
  m_settings[ch] = settings;
  Q_EMIT settingsChanged(ch);
}

QJsonObject FunctionGenerator::settingsJson() const
{
  QJsonArray channels;
  for (const ChannelSettings &s : m_settings)
    channels.append(s.toJson());
  return QJsonObject{ { "channels", channels } };
}

void FunctionGenerator::setSettingsJson(const QJsonObject &o)
{
  const QJsonArray channels = o.value("channels").toArray();
  for (int ch = 0; ch < int(m_settings.size()); ++ch)
    setSettings(ch, ch < channels.size() ? ChannelSettings::fromJson(channels[ch].toObject()) : ChannelSettings());
}

QList<int> FunctionGenerator::shownChannels() const
{
  QList<int> list;
  for (int ch = 0; ch < int(m_settings.size()); ++ch)
    if (m_settings[ch].shown)
      list << ch;
  return list;
}

double FunctionGenerator::maxFrequency(int ch, Shape shape) const
{
  return channel(ch).fMax[size_t(shape)];
}

bool FunctionGenerator::hasValue(Shape shape, Value v)
{
  switch (v)
  {
    case Value::Duty:
      return shape == Shape::Pulse;
    case Value::Symmetry:
      return shape == Shape::Ramp;
    default:
      return true;
  }
}

double FunctionGenerator::minimum(int ch, Value v) const
{
  const ChannelInfo info = channel(ch);
  switch (v)
  {
    case Value::Frequency:
      return info.fMin;
    case Value::Amplitude:
      return info.ampMin;
    case Value::Offset:
      return -maximum(ch, Value::Offset);
    case Value::Duty:
      return kDutyMin;
    case Value::Symmetry:
      return 0;
  }
  return 0;
}

double FunctionGenerator::maximum(int ch, Value v) const
{
  const ChannelInfo info = channel(ch);
  const ChannelSettings s = settings(ch);
  const ChannelState st = state(ch);
  const double offsetLimit = narrowed(info.offsetMax, s.offsetMax);
  switch (v)
  {
    case Value::Frequency:
      return maxFrequency(ch, st.shape);
    case Value::Amplitude:
    {
      // offset ± amplitude/2 within the offset's limit
      const double offset = std::isnan(st.get(Value::Offset)) ? 0 : std::abs(st.get(Value::Offset));
      return qMax(info.ampMin, qMin(narrowed(info.ampMax, s.ampMax), 2 * (offsetLimit - offset)));
    }
    case Value::Offset:
    {
      const double amp = std::isnan(st.get(Value::Amplitude)) ? 0 : st.get(Value::Amplitude);
      return qMax(0.0, offsetLimit - amp / 2);
    }
    case Value::Duty:
      return kDutyMax;
    case Value::Symmetry:
      return 100;
  }
  return 0;
}

double FunctionGenerator::limited(int ch, Value v, double value) const
{
  const ChannelInfo info = channel(ch);
  double r = value;
  switch (v)
  {
    case Value::Frequency:
      r = roundSignificant(value, info.fDigits);
      break;
    case Value::Amplitude:
      r = roundTo(value, info.ampDecimals);
      break;
    case Value::Offset:
      r = roundTo(value, info.offsetDecimals);
      break;
    case Value::Duty:
    case Value::Symmetry:
      r = roundTo(value, info.percentDecimals);
      break;
  }
  return qBound(minimum(ch, v), r, maximum(ch, v));
}

QColor FunctionGenerator::defaultColor(int ch)
{
  // yellow and cyan, as many generators mark their channels
  static const QColor colors[kMaxChannels] = { QColor(0xe0, 0xb0, 0x00), QColor(0x20, 0xa8, 0xc8) };
  return colors[qBound(0, ch, kMaxChannels - 1)];
}

QColor FunctionGenerator::color(int ch) const
{
  const QColor chosen = settings(ch).color;
  return chosen.isValid() ? chosen : defaultColor(ch);
}

QString FunctionGenerator::shapeName(Shape shape)
{
  switch (shape)
  {
    case Shape::Sine: return QStringLiteral("sine");
    case Shape::Square: return QStringLiteral("square");
    case Shape::Ramp: return QStringLiteral("ramp");
    case Shape::Pulse: return QStringLiteral("pulse");
    case Shape::Arbitrary: return QStringLiteral("arbitrary");
  }
  return QStringLiteral("sine");
}

FunctionGenerator::Shape FunctionGenerator::shapeOf(const QString &name)
{
  for (Shape s : { Shape::Sine, Shape::Square, Shape::Ramp, Shape::Pulse, Shape::Arbitrary })
    if (name == shapeName(s))
      return s;
  return Shape::Sine;
}

// ---------------------------------------------------------------- requests

int FunctionGenerator::confirmTimeout() const
{
  return m_deadlines->timeout();
}

void FunctionGenerator::setConfirmTimeout(int ms)
{
  m_deadlines->setTimeout(ms);
}

bool FunctionGenerator::sendable(int ch) const
{
  return valid(ch) && m_settings[ch].shown && m_states[ch].connected && !m_states[ch].external && controlAllowed();
}

void FunctionGenerator::pending(int ch)
{
  ChannelState &s = m_states[ch];
  s.setState = SetState::Pending;
  s.error.clear();
  m_deadlines->start(ch);
  Q_EMIT stateChanged(ch);
}

bool FunctionGenerator::requestShape(int ch, Shape shape, const QString &arbitrary)
{
  if (!sendable(ch))
    return false;
  const ChannelInfo info = channel(ch);
  if (shape == Shape::Arbitrary ? !info.arbitrary.contains(arbitrary) : !info.shapes.contains(shape))
    return false;
  pending(ch);
  sendShape(ch, shape, shape == Shape::Arbitrary ? arbitrary : QString());
  return true;
}

bool FunctionGenerator::requestValue(int ch, Value v, double value)
{
  if (!sendable(ch) || std::isnan(value) || !hasValue(m_states[ch].shape, v))
    return false;
  const double r = limited(ch, v, value);
  m_states[ch].requested[size_t(v)] = r;
  pending(ch);
  sendValue(ch, v, r);
  return true;
}

bool FunctionGenerator::requestOutput(int ch, bool on)
{
  if (!sendable(ch))
    return false;
  sendOutput(ch, on);
  return true;
}

void FunctionGenerator::allOff()
{
  for (int ch : shownChannels())
    if (valid(ch) && m_states[ch].connected && controlAllowed())
      sendOutput(ch, false);   // even while external: off is always safe
}

void FunctionGenerator::expired(int ch)
{
  if (valid(ch) && m_states[ch].setState == SetState::Pending)
    failRequest(ch, tr("No answer"));
}

// ---------------------------------------------------------------- the backend's answers

void FunctionGenerator::confirmShape(int ch, Shape shape, const QString &arbitrary)
{
  if (!valid(ch))
    return;
  ChannelState &s = m_states[ch];
  const bool changed = s.shape != shape || s.arbitrary != arbitrary;
  s.shape = shape;
  s.arbitrary = shape == Shape::Arbitrary ? arbitrary : QString();
  s.setState = SetState::Confirmed;
  s.error.clear();
  m_deadlines->clear(ch);
  Q_EMIT stateChanged(ch);
  if (changed)
    Q_EMIT setpointChanged(ch);
}

void FunctionGenerator::confirmValue(int ch, Value v, double value)
{
  if (!valid(ch))
    return;
  ChannelState &s = m_states[ch];
  const bool changed = !same(s.value[size_t(v)], value);
  s.value[size_t(v)] = value;
  s.setState = SetState::Confirmed;
  s.error.clear();
  m_deadlines->clear(ch);
  Q_EMIT stateChanged(ch);
  if (changed)
    Q_EMIT setpointChanged(ch);
}

void FunctionGenerator::failRequest(int ch, const QString &error)
{
  if (!valid(ch))
    return;
  m_states[ch].setState = SetState::Failed;
  m_states[ch].error = error;
  m_deadlines->clear(ch);
  Q_EMIT stateChanged(ch);
}

void FunctionGenerator::confirmOutput(int ch, bool on)
{
  if (!valid(ch) || m_states[ch].output == on)
    return;
  m_states[ch].output = on;
  Q_EMIT stateChanged(ch);
  Q_EMIT setpointChanged(ch);
}

void FunctionGenerator::setExternal(int ch, bool external)
{
  if (!valid(ch) || m_states[ch].external == external)
    return;
  m_states[ch].external = external;
  Q_EMIT stateChanged(ch);
}

void FunctionGenerator::setConnected(bool connected)
{
  for (int ch = 0; ch < int(m_states.size()); ++ch)
    if (m_states[ch].connected != connected)
    {
      m_states[ch].connected = connected;
      Q_EMIT stateChanged(ch);
    }
}

void FunctionGenerator::channelsChanged()
{
  const int n = qBound(0, channelCount(), kMaxChannels);
  m_states.resize(n);
  m_settings.resize(n);
  m_deadlines->resize(n);
  for (int ch = 0; ch < n; ++ch)
    Q_EMIT settingsChanged(ch);
}
