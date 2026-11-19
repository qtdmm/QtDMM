// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logic/legacynodes.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include "core/calcexpr.h"
#include "core/sampletypes.h"
#include "logic/formulanode.h"
#include "logic/generatornode.h"

namespace
{
// the simulated meter's second to fourth value before one was set: unit and
// formula (MeterSetup of before)
const char *const kVirtualDefaults[3][2] = {
  { "A", "0.5 + 0.2 * sin(2 * pi * t / 13)" },
  { "W", "v1 * v2" },
  { "°C", "21 + 2 * sin(2 * pi * t / 300)" },
};

QString model(const QVariantMap &keys)
{
  return keys.value("DMM/model").toString();
}

bool isSimulated(const QVariantMap &keys)
{
  const QString m = model(keys);
  return m.startsWith(QLatin1String("QtDMM ")) && (m.endsWith(QLatin1String("Simulated meter")) || m.endsWith(QLatin1String("Virtual meter")));
}

bool isCalculated(const QVariantMap &keys)
{
  return model(keys) == QLatin1String("QtDMM Calculated value");
}

QString key(const QVariantMap &keys, const QString &name, const QString &fallback)
{
  const QString v = keys.value(name).toString();
  return keys.contains(name) ? v : fallback;
}

// v1..v3 of before, the values before this one, are y1..y3 now
QString earlierAsY(QString formula)
{
  for (int i = 1; i < kMaxValues; ++i)
    formula = CalcExpr::renameVariable(formula, QString("v%1").arg(i), FormulaNode::outputName(i - 1));
  return formula;
}

// "V/AC" -> unit V, coupling AC
void splitUnit(const QString &token, QString *unit, QString *coupling)
{
  const int slash = token.indexOf('/');
  *unit = slash < 0 ? token : token.left(slash);
  const QString c = slash < 0 ? QString() : token.mid(slash + 1).toUpper();
  *coupling = c == QLatin1String("AC") || c == QLatin1String("DC") ? c : QString();
}
}

bool LegacyNodes::isLegacy(const QVariantMap &keys)
{
  return !keys.contains(QStringLiteral("Node/json")) && (isSimulated(keys) || isCalculated(keys));
}

std::unique_ptr<LogicNode> LegacyNodes::fromKeys(const QVariantMap &keys,
                                                 const std::function<QString(const QString &name)> &sourceOf, QString *error)
{
  if (isSimulated(keys))
  {
    // value 1: its shape; values 2-4: their formulas (MeterSetup of before)
    QList<GeneratorNode::Wave> waves;
    GeneratorNode::Wave first;
    first.unit = key(keys, "DMM/virtual-unit", "V").trimmed();
    first.coupling = key(keys, "DMM/virtual-coupling", "DC");
    first.shape = static_cast<CalcExpr::Waveform>(qBound(0, keys.value("DMM/virtual-waveform", 2).toInt(), int(CalcExpr::Custom)));
    first.min = key(keys, "DMM/virtual-min", "0");
    first.max = key(keys, "DMM/virtual-max", "10");
    first.period = key(keys, "DMM/virtual-period", "20");
    first.noise = key(keys, "DMM/virtual-noise", "0");
    first.formula = key(keys, "DMM/virtual-formula", QString());
    waves << first;
    const int values = qBound(1, keys.value("DMM/virtual-values", 1).toInt(), kMaxValues);
    for (int i = 0; i < values - 1; ++i)
    {
      const QString n = QString::number(i + 2);
      GeneratorNode::Wave w;
      w.unit = key(keys, "DMM/virtual-unit" + n, kVirtualDefaults[i][0]).trimmed();
      w.coupling = key(keys, "DMM/virtual-coupling" + n, "DC");
      w.shape = CalcExpr::Custom;
      w.formula = earlierAsY(key(keys, "DMM/virtual-formula" + n, kVirtualDefaults[i][1]).trimmed());
      waves << w;
    }
    auto node = std::make_unique<GeneratorNode>();
    node->setWaves(waves);
    return node;
  }

  // "<unit>[/<coupling>] <formula>", several separated by ';' (the CalcDevice of before)
  const QString device = key(keys, "DMM/calc-unit", "W").trimmed() + ' ' + keys.value("DMM/calc-expression").toString().trimmed();
  QList<FormulaNode::Formula> formulas;
  for (const QString &part : device.split(';'))
  {
    const QString d = part.trimmed();
    if (d.isEmpty())
      continue;
    const int space = d.indexOf(' ');
    FormulaNode::Formula f;
    splitUnit(space < 0 ? d : d.left(space), &f.unit, &f.coupling);
    f.text = earlierAsY(space < 0 ? QString() : d.mid(space + 1).trimmed());
    formulas << f;
  }
  formulas = formulas.mid(0, kMaxValues);
  // one that does not parse did not open before either
  for (const FormulaNode::Formula &f : std::as_const(formulas))
  {
    QString why;
    if (!CalcExpr::parse(f.text, &why))
    {
      if (error)
        *error = QCoreApplication::translate("LegacyNodes", "Formula error: %1").arg(why);
      return nullptr;
    }
  }
  // the devices it names are its inputs, in order of appearance
  QList<ValueRef> inputs;
  QStringList names;
  for (FormulaNode::Formula &f : formulas)
  {
    const std::optional<CalcExpr> expr = CalcExpr::parse(f.text);
    if (!expr)
      continue;
    for (const QString &var : expr->variables())
    {
      static const QRegularExpression earlier(QStringLiteral("^y[1-4]$"));
      if (var == QLatin1String("t") || earlier.match(var).hasMatch() || names.contains(var))
        continue;
      const QString source = sourceOf ? sourceOf(var) : QString();
      if (source.isEmpty())
      {
        if (error)
          *error = QCoreApplication::translate("LegacyNodes", "Unknown device in the formula: %1").arg(var);
        return nullptr;
      }
      names << var;
      inputs << ValueRef(0, source);
    }
  }
  for (FormulaNode::Formula &f : formulas)
    for (int i = 0; i < names.size(); ++i)
      f.text = CalcExpr::renameVariable(f.text, names[i], FormulaNode::inputName(i));
  if (inputs.isEmpty())
  {
    // only t: a generator of these formulas
    QList<GeneratorNode::Wave> waves;
    for (const FormulaNode::Formula &f : std::as_const(formulas))
    {
      GeneratorNode::Wave w;
      w.unit = f.unit;
      w.coupling = f.coupling;
      w.shape = CalcExpr::Custom;
      w.formula = f.text;
      waves << w;
    }
    auto node = std::make_unique<GeneratorNode>();
    node->setWaves(waves);
    return node;
  }
  auto node = std::make_unique<FormulaNode>();
  node->setInputs(inputs);
  node->setFormulas(formulas);
  return node;
}
