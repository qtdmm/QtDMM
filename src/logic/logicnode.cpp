// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logic/logicnode.h"

#include <QJsonArray>
#include <QSet>

QJsonObject LogicNode::toJson() const
{
  QJsonObject o;
  o["type"] = type();
  QJsonArray in;
  for (const ValueRef &r : m_inputs)
    in.append(r.toString());
  o["inputs"] = in;
  o["leading"] = m_leading;
  o["maxAgeMs"] = m_maxAgeMs;
  o["periodMs"] = m_periodMs;
  o["on"] = m_on;
  o["parameters"] = parameters();
  return o;
}

void LogicNode::fromJson(const QJsonObject &o)
{
  QList<ValueRef> in;
  for (const QJsonValue &v : o["inputs"].toArray())
    in << ValueRef::fromString(v.toString());
  m_inputs = in;
  m_leading = o["leading"].toInt(0);
  setMaxAgeMs(o["maxAgeMs"].toInt(3000));
  setPeriodMs(o["periodMs"].toInt(0));
  m_on = o["on"].toBool(true);
  setParameters(o["parameters"].toObject());
}

bool LogicNode::makesLoop(const QString &source, const QList<ValueRef> &inputs,
                          const std::function<QList<ValueRef>(const QString &source)> &inputsOf)
{
  // walk back from the inputs: reaching the node itself closes a loop
  QSet<QString> seen;
  QList<QString> todo;
  for (const ValueRef &r : inputs)
    todo << r.source;
  while (!todo.isEmpty())
  {
    const QString s = todo.takeLast();
    if (s == source)
      return true;
    if (s.isEmpty() || seen.contains(s))
      continue;
    seen.insert(s);
    for (const ValueRef &r : inputsOf(s))
      todo << r.source;
  }
  return false;
}
