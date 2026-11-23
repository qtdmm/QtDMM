// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <optional>
#include <vector>

/// A small arithmetic expression: what a calculated value evaluates from
/// the readings of the other meters, e.g. "u * i" or "sqrt(a^2 + b^2)".
///
/// Grammar: the operators + - * / ^ (^ binds tightest and is right
/// associative, unary minus binds weaker than ^ so -2^2 is -4),
/// parentheses, numbers in decimal or exponent form with an optional SI
/// suffix ("1.5k", "22u", "4.7n"), variables (identifiers, in QtDMM the
/// names of the open meters; one may carry a value number, "v1.2", a whole
/// name of its own), the constant pi and the functions sqrt, abs,
/// log10, ln, sin, cos, exp, floor, min(a,b), max(a,b) and rand() (uniform in
/// [0, 1), new value per evaluation). Names are case-sensitive.
///
/// parse() reports the first error with its position; eval() yields nothing
/// when a variable is missing or the result is not finite (1/0, sqrt(-1)).
class CalcExpr
{
public:
  /// Parses @p text. On failure returns nothing and sets @p error (and
  /// @p errorPos, the 0-based offset into @p text) when given.
  static std::optional<CalcExpr> parse(const QString &text, QString *error = nullptr, int *errorPos = nullptr);

  /// The distinct variable names used, in order of first appearance.
  QStringList variables() const { return m_variables; }
  /// Evaluates with the given variable values.
  std::optional<double> eval(const QMap<QString, double> &values) const;
  /// The expression as parsed (trimmed source text).
  QString text() const { return m_text; }

  /// @p text with the variable @p from renamed to @p to: whole names only,
  /// not inside numbers ("1e3", "4.7n") and not a function of that name.
  /// Works on any text, also one that does not parse.
  static QString renameVariable(const QString &text, const QString &from, const QString &to);

  /// The signal shapes of the virtual meter; order = the settings combo.
  enum Waveform { Constant, Random, Sine, Triangle, Square, Sawtooth, Discharge, Custom };
  /// The formula over t (seconds) a waveform expands to: Constant is max,
  /// Random and the periodic shapes stay within [min, max] with the given
  /// period, Discharge decays from max to min with time constant period.
  /// A noise amplitude > 0 adds uniform noise of that peak-to-peak size.
  /// Custom gives an empty string. Arguments are taken as typed.
  static QString waveformFormula(Waveform wave, const QString &min, const QString &max,
                                 const QString &period, const QString &noise);

private:
  enum class Op { Num, Var, Neg, Add, Sub, Mul, Div, Pow, Sqrt, Abs, Log10, Ln, Sin, Cos, Exp, Floor, Min, Max, Rand };
  struct Node
  {
    Op op;
    double value = 0;   ///< Num
    int name = -1;      ///< Var: index into m_variables
    int a = -1, b = -1; ///< operands
  };
  struct Parser;

  std::vector<Node> m_nodes;
  QStringList m_variables;
  QString m_text;
  int m_root = -1;

  double evalNode(int idx, const QMap<QString, double> &values, bool &ok) const;
};
