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
    out << Output { f.name, f.unit, OutputKind::Value, f.coupling };
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
  if (!m_start)
    m_start = ctx.t;
  vars.insert(QStringLiteral("t"), (ctx.t - *m_start) / 1000.0);
  auto take = [&vars](const QString &name, const Reading &r)
  {
    if (!r.noNumber() && std::isfinite(r.value))
      vars.insert(name, r.value);
  };
  // the devices' values; v1.1 is v1
  for (int i = 0; i < ctx.values.size(); ++i)
    for (auto it = ctx.values[i].cbegin(); it != ctx.values[i].cend(); ++it)
    {
      take(inputName(i, it.key()), it.value());
      if (it.key() == 0)
        take(inputName(i) + ".1", it.value());
    }
  // without them (a runner of single values): the inputs as v1, v2 ...
  if (ctx.values.isEmpty())
    for (int i = 0; i < ctx.inputs.size(); ++i)
      if (ctx.inputs[i])
        take(inputName(i), *ctx.inputs[i]);
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
  {
    QJsonObject o { { "name", f.name }, { "unit", f.unit }, { "formula", f.text } };
    if (!f.coupling.isEmpty())
      o["coupling"] = f.coupling;
    out.append(o);
  }
  // the inputs are devices (a node of before: values)
  return { { "outputs", out }, { "deviceInputs", true } };
}

void FormulaNode::setParameters(const QJsonObject &p)
{
  // a node of before: inputs one value each, its formulas over the devices now
  const QStringList renamed = p["deviceInputs"].toBool() ? QStringList() : takeValueInputs(p);
  QList<Formula> formulas;
  const QJsonArray outputs = p["outputs"].toArray();
  for (int i = 0; i < outputs.size(); ++i)
  {
    const QJsonObject o = outputs[i].toObject();
    formulas << Formula { o["name"].toString(), o["unit"].toString(), renamed.value(i, o["formula"].toString()),
                          o["coupling"].toString() };
  }
  setFormulas(formulas);
}

QStringList FormulaNode::takeValueInputs(const QJsonObject &p)
{
  // one value per input ("1@device:i") -> one device per input, the value
  // a variable of it; v2 of "1@device:i" is v1.2 when device:i is input 1
  const QList<ValueRef> before = inputs();
  QList<ValueRef> devices;
  QStringList targets;
  for (const ValueRef &r : before)
  {
    int at = -1;
    for (int d = 0; d < devices.size() && at < 0; ++d)
      if (devices[d].source == r.source)
        at = d;
    if (at < 0)
    {
      at = int(devices.size());
      devices << ValueRef(0, r.source);
    }
    targets << inputName(at, r.id);
  }
  const int leading = leadingInput();
  setInputs(devices);
  if (leading >= 0 && leading < targets.size())
  {
    // the device of the value that led
    const ValueRef led = before[leading];
    for (int d = 0; d < devices.size(); ++d)
      if (devices[d].source == led.source)
        setLeadingInput(d);
  }
  QStringList renamed;
  for (const QJsonValue &v : p["outputs"].toArray())
  {
    QString text = v.toObject()["formula"].toString();
    // through names of their own, so v1 -> v2 and v2 -> v1 do not meet
    for (int i = 0; i < targets.size(); ++i)
      text = CalcExpr::renameVariable(text, inputName(i), QStringLiteral("_input%1_").arg(i));
    for (int i = 0; i < targets.size(); ++i)
      text = CalcExpr::renameVariable(text, QStringLiteral("_input%1_").arg(i), targets[i]);
    renamed << text;
  }
  return renamed;
}

QString FormulaNode::renameInput(const QString &text, int from, int to)
{
  return renameInput(text, from, inputName(to));
}

namespace
{
// @p text with the variables of @p from (v2, v2.1 .. v2.4) as those of @p to
QString renameBase(const QString &text, const QString &from, const QString &to)
{
  QString out = CalcExpr::renameVariable(text, from, to);
  for (int v = 0; v < kMaxValues; ++v)
    out = CalcExpr::renameVariable(out, from + QStringLiteral(".%1").arg(v + 1), to + QStringLiteral(".%1").arg(v + 1));
  return out;
}
}

QString FormulaNode::renameInput(const QString &text, int from, const QString &base)
{
  return renameBase(text, inputName(from), base);
}

QString FormulaNode::followInputs(const QString &text, const QList<int> &from)
{
  // the former inputs: those that stay, and below the highest of them or the
  // new count the ones gone (one gone above both keeps its name, already
  // unknown)
  int former = int(from.size());
  for (int f : from)
    former = qMax(former, f + 1);
  // through names of their own, so v1 -> v2 and v2 -> v1 do not meet
  QString out = text;
  for (int k = 0; k < former; ++k)
    out = renameInput(out, k, QStringLiteral("_input%1_").arg(k));
  for (int k = 0; k < former; ++k)
  {
    const int to = int(from.indexOf(k));
    out = renameBase(out, QStringLiteral("_input%1_").arg(k), to >= 0 ? inputName(to) : QStringLiteral("removed_") + inputName(k));
  }
  return out;
}

QString FormulaNode::followRemovedOutput(const QString &text, int removed, int count)
{
  QString out = CalcExpr::renameVariable(text, outputName(removed), QStringLiteral("removed_") + outputName(removed));
  for (int i = removed + 1; i < count; ++i)
    out = CalcExpr::renameVariable(out, outputName(i), outputName(i - 1));
  return out;
}

QString FormulaNode::check(const QString &text, int index, int inputs, int *pos)
{
  QString error;
  const std::optional<CalcExpr> expr = CalcExpr::parse(text, &error, pos);
  if (!expr)
    return error;
  for (const QString &name : expr->variables())
  {
    bool known = name == QLatin1String("t");
    for (int i = 0; i < inputs && !known; ++i)
    {
      known = name == inputName(i) || name == inputName(i) + ".1";
      for (int v = 1; v < kMaxValues && !known; ++v)
        known = name == inputName(i, v);
    }
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

std::optional<QString> FormulaNode::suggestUnit(const QString &text, const QMap<QString, QString> &given)
{
  QMap<QString, QString> units = given;
  if (!units.contains(QStringLiteral("t")))
    units.insert(QStringLiteral("t"), QStringLiteral("s"));
  for (auto it = units.begin(); it != units.end(); ++it)
    it.value() = LogicNode::canonicalUnit(it.value());
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
