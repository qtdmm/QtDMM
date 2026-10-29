// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/alarm.h"
#include "core/sampletypes.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>

#include "ui/engnumbervalidator.h"

namespace
{
const char *conditionKey(Alarm::Condition c)
{
  switch (c)
  {
    case Alarm::Below: return "below";
    case Alarm::Above: return "above";
    case Alarm::Outside: return "outside";
    case Alarm::Inside: return "inside";
    case Alarm::Overload: return "overload";
    case Alarm::NoReadings: return "no-readings";
  }
  return "below";
}

Alarm::Condition conditionFromKey(const QString &k)
{
  if (k == "above") return Alarm::Above;
  if (k == "outside") return Alarm::Outside;
  if (k == "inside") return Alarm::Inside;
  if (k == "overload") return Alarm::Overload;
  if (k == "no-readings") return Alarm::NoReadings;
  return Alarm::Below;
}

// A threshold is named with the digits it was entered with: "below 4.7512V",
// not "below 4.8V" for an alarm that raises at 4.7512.
QString eng(double v, const QString &unit)
{
  return EngNumberValidator::engText(v) + unit;
}
}

QJsonObject Alarm::toJson() const
{
  QJsonObject o;
  o["name"] = name;
  o["enabled"] = enabled;
  if (value != ValueRef())
    o["value"] = value.toJson();
  o["condition"] = conditionKey(condition);
  o["a"] = a;
  o["b"] = b;
  o["seconds"] = seconds;
  o["hysteresis"] = hysteresis;
  o["message"] = message;
  o["color"] = color.name();
  o["banner"] = banner;
  o["tint-display"] = tintDisplay;
  o["sound"] = sound;
  o["repeat"] = repeat == WhileRaised ? "while-raised" : repeat == Times ? "times" : "once";
  o["repeat-count"] = repeatCount;
  o["clear-sound"] = clearSound;
  o["popup"] = popup;
  o["raise-window"] = raiseWindow;
  o["command"] = command;
  o["disconnect"] = disconnect;
  o["recorder"] = recorder == RecorderStart ? "start" : recorder == RecorderStop ? "stop" : "none";
  o["mark-graph"] = markGraph;
  o["mark-table"] = markTable;
  return o;
}

Alarm Alarm::fromJson(const QJsonObject &o)
{
  Alarm al;
  al.name = o["name"].toString();
  al.enabled = o["enabled"].toBool(true);
  al.value = ValueRef::fromJson(o["value"]);
  al.condition = conditionFromKey(o["condition"].toString());
  al.a = o["a"].toDouble();
  al.b = o["b"].toDouble();
  al.seconds = o["seconds"].toDouble();
  al.hysteresis = o["hysteresis"].toDouble();
  al.message = o["message"].toString();
  al.color = QColor(o["color"].toString(al.color.name()));
  al.banner = o["banner"].toBool(true);
  al.tintDisplay = o["tint-display"].toBool(true);
  // before the sounds there was only "beep", on or off
  al.sound = o.contains("sound") ? o["sound"].toString() : o["beep"].toBool(true) ? QStringLiteral("beep") : QString();
  const QString rep = o["repeat"].toString();
  al.repeat = rep == "while-raised" ? WhileRaised : rep == "times" ? Times : Once;
  al.repeatCount = qMax(1, o["repeat-count"].toInt(3));
  al.clearSound = o["clear-sound"].toBool(false);
  al.popup = o["popup"].toBool(false);
  al.raiseWindow = o["raise-window"].toBool(false);
  al.command = o["command"].toString();
  al.disconnect = o["disconnect"].toBool(false);
  const QString rec = o["recorder"].toString();
  al.recorder = rec == "start" ? RecorderStart : rec == "stop" ? RecorderStop : RecorderNone;
  al.markGraph = o["mark-graph"].toBool(true);
  al.markTable = o["mark-table"].toBool(true);
  return al;
}

QList<Alarm> Alarm::listFromJson(const QString &json)
{
  QList<Alarm> list;
  const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
  for (const QJsonValue &v : doc.array())
    if (v.isObject())
      list << fromJson(v.toObject());
  return list;
}

QString Alarm::listWithoutPrograms(const QString &json, bool *removed)
{
  if (removed)
    *removed = false;
  if (json.isEmpty())
    return json;
  QList<Alarm> alarms = listFromJson(json);
  for (Alarm &a : alarms)
  {
    if (removed && !a.command.trimmed().isEmpty())
      *removed = true;
    a.command.clear();
    a.disconnect = false;
  }
  return listToJson(alarms);
}

QString Alarm::listToJson(const QList<Alarm> &alarms)
{
  QJsonArray arr;
  for (const Alarm &al : alarms)
    arr << al.toJson();
  return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString Alarm::describe(const QString &unit) const
{
  QString text;
  switch (condition)
  {
    case Below:      text = QCoreApplication::translate("Alarm", "below %1").arg(eng(a, unit)); break;
    case Above:      text = QCoreApplication::translate("Alarm", "above %1").arg(eng(a, unit)); break;
    case Outside:    text = QCoreApplication::translate("Alarm", "outside %1 to %2").arg(eng(a, unit), eng(b, unit)); break;
    case Inside:     text = QCoreApplication::translate("Alarm", "inside %1 to %2").arg(eng(a, unit), eng(b, unit)); break;
    case Overload:   text = QCoreApplication::translate("Alarm", "overload"); break;
    case NoReadings: return QCoreApplication::translate("Alarm", "no readings for %1 s").arg(seconds);
  }
  if (seconds > 0)
    text += " " + QCoreApplication::translate("Alarm", "for %1 s").arg(seconds);
  return text;
}

bool Alarm::matches(double value, bool overload) const
{
  switch (condition)
  {
    case Overload:   return overload;
    case NoReadings: return false;
    default: break;
  }
  if (overload)
    return false;   // OL is neither below nor above anything
  switch (condition)
  {
    case Below:   return value < a;
    case Above:   return value > a;
    case Outside: return value < qMin(a, b) || value > qMax(a, b);
    case Inside:  return value >= qMin(a, b) && value <= qMax(a, b);
    default:      return false;
  }
}

bool Alarm::released(double value, bool overload) const
{
  switch (condition)
  {
    case Overload:   return !overload;
    case NoReadings: return true;   // a reading arrived
    default: break;
  }
  if (overload)
    return false;
  const double lo = qMin(a, b), hi = qMax(a, b), h = qAbs(hysteresis);
  switch (condition)
  {
    case Below:   return value >= a + h;
    case Above:   return value <= a - h;
    case Outside: return value >= lo + h && value <= hi - h;
    case Inside:  return value < lo - h || value > hi + h;
    default:      return true;
  }
}

// ---------------------------------------------------------------------------

AlarmManager::AlarmManager(QObject *parent) : QObject(parent)
{
  // connected first: the order is known to the slots of raised()
  connect(this, &AlarmManager::raised, this, [this](int, const Alarm &alarm, double)
  {
    m_raiseOrder.removeAll(alarm.name);
    m_raiseOrder << alarm.name;
  });
}

void AlarmManager::setAlarms(const QList<Alarm> &alarms)
{
  // raised alarms that survive the edit (same name) keep their state
  QList<State> state;
  QList<qint64> since;
  for (const Alarm &al : alarms)
  {
    State s = Off;
    qint64 t = 0;
    for (int i = 0; i < m_alarms.size(); ++i)
      if (m_alarms[i].name == al.name && al.enabled)
      {
        s = m_state[i];
        t = m_since[i];
      }
    state << s;
    since << t;
  }
  m_alarms = alarms;
  m_state = state;
  m_since = since;
}

bool AlarmManager::anyRaised() const
{
  for (State s : m_state)
    if (s == Raised)
      return true;
  return false;
}

QColor AlarmManager::displayColor() const
{
  for (int k = m_raiseOrder.size() - 1; k >= 0; --k)
    for (int i = 0; i < m_alarms.size(); ++i)
      if (m_alarms[i].name == m_raiseOrder[k] && m_alarms[i].tintDisplay
          && (m_state[i] == Raised || m_state[i] == Acknowledged))
        return m_alarms[i].color;
  return QColor();
}

void AlarmManager::feed(double value, bool overload, qint64 nowMs, int id, const QString &source)
{
  if (id == 0)
  {
    m_lastReading = nowMs;
    m_lastValue = value;
    m_lastOverload = overload;
  }
  for (int i = 0; i < m_alarms.size(); ++i)
  {
    const Alarm &al = m_alarms[i];
    if (al.condition == Alarm::NoReadings)
    {
      if (id != 0)
        continue;
      // silence ended
      if (m_state[i] != Off)
      {
        m_state[i] = Off;
        Q_EMIT cleared(i, al);
      }
      m_since[i] = nowMs;
      continue;
    }
    if (!al.value.matches(id, source))
      continue;
    evaluate(i, al.matches(value, overload), al.released(value, overload), value, nowMs);
  }
}

void AlarmManager::setConnected(bool on, qint64 nowMs)
{
  if (on == (m_silenceFrom >= 0))
    return;
  m_silenceFrom = on ? nowMs : -1;
  m_lastReading = -1;
  if (on)
    return;
  // nothing is expected from a meter that is not connected
  for (int i = 0; i < m_alarms.size(); ++i)
    if (m_alarms[i].condition == Alarm::NoReadings && m_state[i] != Off)
    {
      const bool wasRaised = m_state[i] != Pending;
      m_state[i] = Off;
      if (wasRaised)
        Q_EMIT cleared(i, m_alarms[i]);
    }
}

void AlarmManager::tick(qint64 nowMs)
{
  for (int i = 0; i < m_alarms.size(); ++i)
  {
    const Alarm &al = m_alarms[i];
    if (al.condition != Alarm::NoReadings)
      continue;
    if (!al.enabled)
      continue;
    // silence is measured from the last reading, or from the moment the
    // meter was connected when none has arrived yet. Before that there is
    // nothing to be silent about - counting from 0 (the epoch) would raise
    // every alarm on the first tick after the program starts, actions and
    // all.
    const qint64 since = m_lastReading >= 0 ? m_lastReading : m_silenceFrom;
    if (since < 0)
      continue;
    const bool silent = nowMs - since >= qint64(al.seconds * 1000.0);
    if (silent && m_state[i] == Off)
    {
      m_state[i] = Raised;
      Q_EMIT raised(i, al, m_lastValue);
    }
  }
}

void AlarmManager::evaluate(int i, bool matches, bool releasedNow, double value, qint64 nowMs)
{
  const Alarm &al = m_alarms[i];
  State &s = m_state[i];
  if (!al.enabled)
  {
    if (s != Off)
    {
      s = Off;
      Q_EMIT cleared(i, al);
    }
    return;
  }
  switch (s)
  {
    case Off:
      if (matches)
      {
        m_since[i] = nowMs;
        s = Pending;
        if (al.seconds <= 0)
        {
          s = Raised;
          Q_EMIT raised(i, al, value);
        }
      }
      break;
    case Pending:
      if (!matches)
        s = Off;
      else if (nowMs - m_since[i] >= qint64(al.seconds * 1000.0))
      {
        s = Raised;
        Q_EMIT raised(i, al, value);
      }
      break;
    case Raised:
    case Acknowledged:
      if (releasedNow)
      {
        s = Off;
        Q_EMIT cleared(i, al);
      }
      break;
  }
}

void AlarmManager::acknowledge(int index)
{
  if (index >= 0 && index < m_state.size() && m_state[index] == Raised)
    m_state[index] = Acknowledged;
}

void AlarmManager::acknowledgeAll()
{
  for (int i = 0; i < m_state.size(); ++i)
    acknowledge(i);
}

QString Alarm::title(const QString &name)
{
  const QString word = QCoreApplication::translate("Alarm", "Alarm");
  if (name.startsWith(word, Qt::CaseInsensitive))
    return name;
  return QCoreApplication::translate("Alarm", "Alarm %1").arg(name);
}
