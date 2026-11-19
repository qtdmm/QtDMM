// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logic/logicnode.h"
#include "logic/formulanode.h"
#include "logic/generatornode.h"

#include <QJsonArray>
#include <QJsonDocument>
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

std::unique_ptr<LogicNode> LogicNode::create(const QString &type)
{
  if (type == QLatin1String(FormulaNode::kType))
    return std::make_unique<FormulaNode>();
  if (type == QLatin1String(GeneratorNode::kType))
    return std::make_unique<GeneratorNode>();
  return nullptr;
}

std::unique_ptr<LogicNode> LogicNode::fromJsonText(const QString &text)
{
  const QJsonObject o = QJsonDocument::fromJson(text.toUtf8()).object();
  std::unique_ptr<LogicNode> node = create(o["type"].toString());
  if (node)
    node->fromJson(o);
  return node;
}

QString LogicNode::toJsonText() const
{
  return QString::fromUtf8(QJsonDocument(toJson()).toJson(QJsonDocument::Compact));
}

QString LogicNode::canonicalUnit(const QString &unit)
{
  QString u = unit.trimmed();
  u.replace(QStringLiteral("Ω"), QStringLiteral("Ohm"));
  if (u == QStringLiteral("°C"))
    return QStringLiteral("C");
  if (u == QStringLiteral("°F"))
    return QStringLiteral("dF");
  return u;
}
