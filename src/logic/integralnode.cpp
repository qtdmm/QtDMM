// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logic/integralnode.h"

#include <cmath>

#include <QCoreApplication>

#include "core/siprefix.h"

namespace
{
constexpr double kMsPerHour = 3600.0 * 1000.0;
}

QString IntegralNode::integratedUnit(const QString &baseUnit)
{
  // the units of their own of an integral over hours; the others get an h
  return baseUnit.isEmpty() ? QStringLiteral("h") : baseUnit + 'h';
}

LogicNode::Values IntegralNode::evaluate(const Context &ctx)
{
  // the sum in the output's unit: "kWh" takes it in thousands
  const double factor = SiPrefix::factor(SiPrefix::split(m_unit).prefix);
  const std::optional<Reading> r = ctx.inputs.value(0);
  if (!r || r->noNumber() || !std::isfinite(r->value))
  {
    // no number: a gap, the next trapezoid starts after it
    m_last.reset();
    return { m_sum / factor };
  }
  // the sum's unit: Wh sums W; a reading in V (the meter switched) adds
  // nothing, a gap
  const QString base = SiPrefix::split(m_unit).baseUnit;
  const QString expected = base.endsWith('h') ? base.chopped(1) : m_inputUnit;
  if (!expected.isEmpty() && canonicalUnit(r->baseUnit) != canonicalUnit(expected))
  {
    m_last.reset();
    m_problem = QCoreApplication::translate("IntegralNode", "Unit does not fit: %1 instead of %2")
                  .arg(SiPrefix::displayText(r->baseUnit), SiPrefix::displayText(expected));
    return { m_sum / factor };
  }
  m_problem.clear();
  if (m_inputUnit.isEmpty())
    m_inputUnit = r->baseUnit;
  const double v = r->value;
  // rising above the threshold: from 0 again
  if (m_threshold)
  {
    const bool below = v < *m_threshold;
    if (m_below && *m_below && !below)
    {
      m_sum = 0;
      m_last.reset();
    }
    m_below = below;
  }
  if (m_last && r->t > m_last->second && r->t - m_last->second <= maxAgeMs())
    m_sum += (m_last->first + v) / 2.0 * double(r->t - m_last->second) / kMsPerHour;
  m_last = std::make_pair(v, r->t);
  return { m_sum / factor };
}

LogicNode::Values IntegralNode::bypass(const Context &ctx)
{
  Q_UNUSED(ctx);
  // off: the sum held, the time off no part of the next trapezoid
  m_last.reset();
  return { m_sum / SiPrefix::factor(SiPrefix::split(m_unit).prefix) };
}

void IntegralNode::reset()
{
  m_sum = 0;
  m_last.reset();
  m_below.reset();
  m_inputUnit.clear();
}

QJsonObject IntegralNode::state() const
{
  return { { "sum", m_sum }, { "inputUnit", m_inputUnit } };
}

void IntegralNode::setState(const QJsonObject &s)
{
  m_sum = s["sum"].toDouble();
  m_inputUnit = s["inputUnit"].toString();
}

QJsonObject IntegralNode::parameters() const
{
  QJsonObject o { { "name", m_name }, { "unit", m_unit }, { "resetOnRecording", m_resetOnRecording } };
  if (m_threshold)
    o["threshold"] = *m_threshold;
  return o;
}

void IntegralNode::setParameters(const QJsonObject &p)
{
  m_name = p["name"].toString();
  m_unit = p["unit"].toString(QStringLiteral("Wh"));
  m_resetOnRecording = p["resetOnRecording"].toBool(true);
  m_threshold = p.contains("threshold") ? std::optional(p["threshold"].toDouble()) : std::nullopt;
}

QList<LogicNode::Control> IntegralNode::controls() const
{
  return { { QStringLiteral("zero"), QStringLiteral("0"),
             QCoreApplication::translate("IntegralNode", "Set the integral to 0"), Control::Kind::Key },
           { QStringLiteral("hold"), QStringLiteral("HOLD"),
             QCoreApplication::translate("IntegralNode", "Hold: the sum stays as it is, nothing is added"),
             Control::Kind::Switch } };
}

bool IntegralNode::checked(const QString &id) const
{
  return id == QLatin1String("hold") && !isOn();
}

void IntegralNode::press(const QString &id)
{
  if (id == QLatin1String("zero"))
    reset();
  else if (id == QLatin1String("hold"))
    setOn(!isOn());
}
