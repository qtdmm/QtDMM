// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include "logic/logicnode.h"

/// A logic node that integrates one value over time (concept §8, PR 3):
/// energy in Wh from a power in W, charge in Ah from a current, light
/// exposure in lxh. It replaces the integral curve of the old graph.
///
/// - **Input:** one value of a device (ValueRef with its id); it leads.
/// - **Sum:** trapezoids between two readings, in the input's base unit
///   times hours. A reading without a number, or one further from the one
///   before than the maximum age, adds nothing: the sum pauses over a gap.
/// - **Unit:** derived from the input's (integratedUnit()), with a prefix
///   if wanted (kWh); the output is the sum in it. A reading in another
///   base unit than the sum's (the meter switched from W to V) is a gap,
///   and problem() says so.
/// - **Reset:** by hand (reset()), at the start of a recording when
///   resetsOnRecording(), and when the input rises above a threshold (from
///   below it): the sum starts at 0 there. After a restart it starts at 0.
/// - **Off:** the sum stays as it is and is shown so; nothing is added,
///   and the time off is no part of the next trapezoid.
class IntegralNode : public LogicNode
{
public:
  static constexpr const char *kType = "integral";

  QString type() const override { return QString::fromLatin1(kType); }
  QList<Output> outputs() const override { return { { m_name, m_unit } }; }
  Values  evaluate(const Context &ctx) override;
  Values  bypass(const Context &ctx) override;
  void    reset() override;
  bool    resetsOnRecording() const override { return m_resetOnRecording; }
  QJsonObject state() const override;
  void    setState(const QJsonObject &s) override;
  QString problem() const override { return m_problem; }
  /// "0" sets it to 0, "HOLD" keeps the sum (off).
  QList<Control> controls() const override;
  bool    checked(const QString &id) const override;
  void    press(const QString &id) override;
  bool    holds() const override { return true; }
  QJsonObject parameters() const override;
  void    setParameters(const QJsonObject &p) override;

  QString name() const { return m_name; }
  void    setName(const QString &name) { m_name = name; }
  /// The output's unit, with prefix ("Wh", "kWh", "mAh").
  QString unit() const { return m_unit; }
  void    setUnit(const QString &unit) { m_unit = unit; }
  bool    resetOnRecording() const { return m_resetOnRecording; }
  void    setResetOnRecording(bool on) { m_resetOnRecording = on; }
  /// The input's level above which the sum starts anew; none: no threshold.
  std::optional<double> threshold() const { return m_threshold; }
  void    setThreshold(std::optional<double> t) { m_threshold = t; }

  /// The sum so far, in the input's base unit times hours.
  double  sum() const { return m_sum; }

  /// The unit of the integral of @p baseUnit over hours: W Wh, A Ah, VA VAh,
  /// var varh, lx lxh, others with h after them; none: h.
  static QString integratedUnit(const QString &baseUnit);

private:
  QString m_name;
  QString m_unit = QStringLiteral("Wh");
  bool    m_resetOnRecording = true;
  std::optional<double> m_threshold;

  double  m_sum = 0;
  /// The reading before: its value and time (core clock, ms); none after a
  /// gap, a reset or while off.
  std::optional<std::pair<double, qint64>> m_last;
  /// Whether the input was below the threshold at its last number.
  std::optional<bool> m_below;
  /// The input's base unit the sum is in: from the output's unit (Wh: W),
  /// else the first reading's after a reset.
  QString m_inputUnit;
  QString m_problem;
};
