// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logic/formulanode.h"

#include <array>
#include <cmath>

#include <QCoreApplication>
#include <QJsonArray>
#include <QMap>

#include "core/sampletypes.h"
#include "core/siprefix.h"

namespace
{
// the electrical units as powers of V, A and s; "C" is °C at the decoders,
// so no coulomb
struct Dim
{
  const char *unit;
  std::array<int, 3> e;   // V, A, s
};
constexpr Dim kDims[] = {
  { "", { 0, 0, 0 } },   { "V", { 1, 0, 0 } },   { "A", { 0, 1, 0 } },   { "s", { 0, 0, 1 } },
  { "W", { 1, 1, 0 } },  { "Ohm", { 1, -1, 0 } }, { "S", { -1, 1, 0 } }, { "Hz", { 0, 0, -1 } },
  { "J", { 1, 1, 1 } },
};

const Dim *dimOf(const QString &unit)
{
  for (const Dim &d : kDims)
    if (unit == QLatin1String(d.unit))
      return &d;
  return nullptr;
}
}

QList<LogicNode::Output> FormulaNode::outputs() const
{
  QList<Output> out;
  for (const Formula &f : m_formulas)
    out << Output { f.name, f.unit };
  return out;
}

void FormulaNode::setFormulas(const QList<Formula> &formulas)
{
  m_formulas = formulas.mid(0, kMaxValues);
  m_exprs.clear();
  for (const Formula &f : std::as_const(m_formulas))
    m_exprs << CalcExpr::parse(f.text);
}

LogicNode::Values FormulaNode::evaluate(const Context &ctx)
{
  QMap<QString, double> vars;
  for (int i = 0; i < ctx.inputs.size(); ++i)
    if (const std::optional<Reading> &r = ctx.inputs[i]; r && !r->noNumber() && std::isfinite(r->value))
      vars.insert(inputName(i), r->value);
  Values out;
  for (int i = 0; i < m_formulas.size(); ++i)
  {
    // in the base unit of the output's unit; the runner takes it in the unit
    std::optional<double> v = m_exprs[i] ? m_exprs[i]->eval(vars) : std::nullopt;
    if (v)
      vars.insert(outputName(i), *v);
    const double factor = SiPrefix::factor(SiPrefix::split(m_formulas[i].unit).prefix);
    out << (v ? std::optional(*v / factor) : std::nullopt);
  }
  return out;
}

QJsonObject FormulaNode::parameters() const
{
  QJsonArray out;
  for (const Formula &f : m_formulas)
    out.append(QJsonObject { { "name", f.name }, { "unit", f.unit }, { "formula", f.text } });
  return { { "outputs", out } };
}

void FormulaNode::setParameters(const QJsonObject &p)
{
  QList<Formula> formulas;
  for (const QJsonValue &v : p["outputs"].toArray())
  {
    const QJsonObject o = v.toObject();
    formulas << Formula { o["name"].toString(), o["unit"].toString(), o["formula"].toString() };
  }
  setFormulas(formulas);
}

QString FormulaNode::check(const QString &text, int index, int inputs, int *pos)
{
  QString error;
  const std::optional<CalcExpr> expr = CalcExpr::parse(text, &error, pos);
  if (!expr)
    return error;
  for (const QString &name : expr->variables())
  {
    bool known = false;
    for (int i = 0; i < inputs && !known; ++i)
      known = name == inputName(i);
    for (int i = 0; i < index && !known; ++i)
      known = name == outputName(i);
    if (!known)
    {
      if (pos)
        *pos = int(text.indexOf(name));
      return QCoreApplication::translate("FormulaNode", "Unknown variable: %1").arg(name);
    }
  }
  return QString();
}

std::optional<QString> FormulaNode::suggestUnit(const QString &text, const QMap<QString, QString> &units)
{
  const std::optional<CalcExpr> expr = CalcExpr::parse(text);
  if (!expr || text.contains(QLatin1String("rand")))
    return std::nullopt;
  // the dimensions: V, A, s, then each other unit of a variable on its own
  QStringList others;
  for (const QString &var : expr->variables())
  {
    const QString unit = SiPrefix::split(units.value(var)).baseUnit;
    if (!dimOf(unit) && !others.contains(unit))
      others << unit;
  }
  const int dims = 3 + int(others.size());
  // each variable a number made of its dimensions; scaling one dimension by
  // k scales the result by k^e when it has that dimension's power e
  auto evalWith = [&](int scaled, double k) -> std::optional<double>
  {
    QMap<QString, double> vars;
    int n = 0;
    for (const QString &var : expr->variables())
    {
      const QString unit = SiPrefix::split(units.value(var)).baseUnit;
      double v = 1.0 + 0.137 * ++n;   // distinct, so v1 - v2 is no 0
      std::array<int, 3> e { 0, 0, 0 };
      if (const Dim *d = dimOf(unit))
        e = d->e;
      for (int i = 0; i < 3; ++i)
        v *= std::pow(i == scaled ? k * 1.3 : 1.3, e[i]);
      if (const int o = int(others.indexOf(unit)); o >= 0)
        v *= 3 + o == scaled ? k * 1.7 : 1.7;
      vars.insert(var, v);
    }
    return expr->eval(vars);
  };
  const std::optional<double> base = evalWith(-1, 1);
  if (!base || *base == 0)
    return std::nullopt;
  std::array<int, 3> e { 0, 0, 0 };
  int other = -1;
  for (int d = 0; d < dims; ++d)
  {
    const std::optional<double> by2 = evalWith(d, 2);
    const std::optional<double> by3 = evalWith(d, 3);
    if (!by2 || !by3 || *by2 / *base <= 0 || *by3 / *base <= 0)
      return std::nullopt;
    const double p2 = std::log2(*by2 / *base);
    const double p3 = std::log(*by3 / *base) / std::log(3.0);
    const double p = std::round(p2);
    // a whole power, the same for both: a product, quotient or sum of one unit
    if (std::fabs(p2 - p) > 1e-9 || std::fabs(p3 - p) > 1e-9)
      return std::nullopt;
    if (d < 3)
      e[d] = int(p);
    else if (p != 0)
    {
      if (p != 1 || other >= 0)
        return std::nullopt;   // °C², two units of their own
      other = d - 3;
    }
  }
  if (other >= 0)
    return e == std::array<int, 3> { 0, 0, 0 } ? std::optional(others[other]) : std::nullopt;
  for (const Dim &d : kDims)
    if (d.e == e)
      return QString::fromLatin1(d.unit);
  return std::nullopt;
}
