// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <optional>

#include "core/calcexpr.h"
#include "logic/logicnode.h"

/// A logic node that computes up to kMaxValues outputs from formulas
/// (CalcExpr) over any values (concept §8, PR 1b): power from a voltage and
/// a current meter ("v1 * v2"), a difference, a mean.
///
/// - **Variables:** the inputs are devices (takesDevices()): v1..vn their
///   main values, v1.2 .. v1.4 their other values (v1.1 is v1), in SI base
///   units as the readings carry them (12.3 mV is 0.0123); an output may
///   use the ones before it as y1, y2, y3; t is the seconds since the node
///   started (reset() starts it anew). A node stored before the inputs
///   were devices (one value each, "1@device:x") is read so: its inputs
///   become their devices, v2 of a second value v1.2.
/// - **Units:** a formula's result is in the base unit of its output's unit;
///   an output in "mW" shows 0.0123 as 12.3 mW. suggestUnit() proposes one
///   from the units of the inputs (V * A -> W).
/// - **Gaps:** an output whose formula lacks a variable (an input without a
///   current reading, an output before it without a value) or whose result
///   is not finite gives none. Switched off it gives none at all.
class FormulaNode : public LogicNode
{
public:
  /// One output: its name (empty: none), its unit with prefix ("W", "mA",
  /// empty: none) and its formula.
  struct Formula
  {
    QString name;
    QString unit;
    QString text;
    QString coupling;   ///< "AC", "DC" or empty (taken over from an old calculated value)
    bool    operator==(const Formula &o) const = default;
  };

  static constexpr const char *kType = "formula";

  QString type() const override { return QString::fromLatin1(kType); }
  QList<Output> outputs() const override;
  Values  evaluate(const Context &ctx) override;
  void    reset() override { m_start.reset(); }
  bool    takesDevices() const override { return true; }
  QJsonObject parameters() const override;
  void    setParameters(const QJsonObject &p) override;

  QList<Formula> formulas() const { return m_formulas; }
  /// Up to kMaxValues; those that do not parse give no value.
  void    setFormulas(const QList<Formula> &formulas);

  /// The variable of input @p index (0-based): "v1" ...; with @p value
  /// (0-based, 0 the main one) above 0 that value of it: "v1.2" ...
  static QString inputName(int index, int value = 0)
  {
    return value > 0 ? QStringLiteral("v%1.%2").arg(index + 1).arg(value + 1) : QStringLiteral("v%1").arg(index + 1);
  }
  /// @p text with input @p from's variables (v2, v2.1 .. v2.4) as input @p to's.
  static QString renameInput(const QString &text, int from, int to);
  /// The variable of output @p index (0-based): "y1" ...
  static QString outputName(int index) { return QStringLiteral("y%1").arg(index + 1); }

  /// What is wrong with output @p index's formula @p text among @p inputs
  /// inputs: it does not parse (with @p pos the 0-based offset), or uses a
  /// variable that is no input, no earlier output and not t. Empty when fine.
  static QString check(const QString &text, int index, int inputs, int *pos = nullptr);

  /// The unit output @p index's formula @p text gives from the base units of
  /// its variables (@p units: "v1" -> "V", "y1" -> "W"; t is in s), when it tells one:
  /// products and quotients of V, A, s, W, Ohm, Hz, S, J, C and sums of one
  /// unit ("v1 + v2" in °C is °C). Empty string: no unit (a pure number);
  /// none: it does not tell (sin(v1), a mix).
  static std::optional<QString> suggestUnit(const QString &text, const QMap<QString, QString> &units);

private:
  /// A node of before (inputs one value each): its inputs as devices; the
  /// formulas of @p p renamed to them.
  QStringList takeValueInputs(const QJsonObject &p);

  QList<Formula> m_formulas;
  QList<std::optional<CalcExpr>> m_exprs;
  std::optional<qint64> m_start;   ///< the core clock at the first computation, for t
};
