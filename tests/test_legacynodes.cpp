// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Entries of before read as logic nodes (PR 2, LegacyNodes): a simulated
// meter is a generator that gives what the old CalcDevice computed from
// the device string MeterSetup built (rebuilt here as it was), at the same t; a calculated value a formula
// whose inputs are the devices it names; one naming a device not there,
// or with a formula that does not parse, is refused.

#include <QCoreApplication>
#include <cmath>

#include "core/calcexpr.h"
#include "logic/formulanode.h"
#include "logic/generatornode.h"
#include "logic/legacynodes.h"
#include "testutil.h"

namespace
{
// what CalcDevice computed from "calc V/DC f1; A/DC f2 ...": each formula
// with t and the values before as v1, v2 ...
QList<double> oldValues(const QString &device, double t)
{
  QList<double> out;
  const QString list = device.section(' ', 1);
  for (const QString &part : list.split(';'))
  {
    const QString d = part.trimmed();
    const std::optional<CalcExpr> e = CalcExpr::parse(d.mid(d.indexOf(' ') + 1));
    QMap<QString, double> vars { { "t", t } };
    for (int i = 0; i < out.size(); ++i)
      vars.insert(QString("v%1").arg(i + 1), out[i]);
    out << (e ? e->eval(vars).value_or(NAN) : NAN);
  }
  return out;
}

// the device string MeterSetup built for a simulated meter, before 2b:
// value 1 its waveform, values 2-4 their formulas, after ';'
QString oldDevice(const QVariantMap &keys)
{
  const char *const defaults[3][2] = { { "A", "0.5 + 0.2 * sin(2 * pi * t / 13)" }, { "W", "v1 * v2" },
                                       { "°C", "21 + 2 * sin(2 * pi * t / 300)" } };
  auto key = [&keys](const QString &k, const QString &d) { return keys.contains(k) ? keys.value(k).toString() : d; };
  const int wave = keys.value("DMM/virtual-waveform", 2).toInt();
  const QString first = wave == CalcExpr::Custom
                          ? key("DMM/virtual-formula", QString())
                          : CalcExpr::waveformFormula(static_cast<CalcExpr::Waveform>(wave), key("DMM/virtual-min", "0"),
                                                      key("DMM/virtual-max", "10"), key("DMM/virtual-period", "20"),
                                                      key("DMM/virtual-noise", "0"));
  QString device = QString("calc %1/%2 %3").arg(key("DMM/virtual-unit", "V"), key("DMM/virtual-coupling", "DC"), first);
  for (int i = 0; i < keys.value("DMM/virtual-values", 1).toInt() - 1; ++i)
  {
    const QString n = QString::number(i + 2);
    device += QString("; %1/%2 %3").arg(key("DMM/virtual-unit" + n, defaults[i][0]), key("DMM/virtual-coupling" + n, "DC"),
                                        key("DMM/virtual-formula" + n, defaults[i][1]));
  }
  return device;
}

LogicNode::Values at(LogicNode &node, qint64 ms, const QList<std::optional<Reading>> &inputs = {})
{
  LogicNode::Context ctx;
  ctx.t = ms;
  ctx.inputs = inputs;
  return node.evaluate(ctx);
}

std::optional<Reading> reading(double v)
{
  Reading r;
  r.value = v;
  r.msecs = 1;
  return r;
}
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // 1. a simulated meter of three values: a triangle, value 2 and 3 formulas over the values before
  {
    const QVariantMap keys { { "DMM/model", "QtDMM Simulated meter" }, { "DMM/virtual-waveform", int(CalcExpr::Triangle) },
                             { "DMM/virtual-min", "1" }, { "DMM/virtual-max", "5" }, { "DMM/virtual-period", "4" },
                             { "DMM/virtual-unit", "V" }, { "DMM/virtual-coupling", "AC" }, { "DMM/virtual-values", 3 },
                             { "DMM/virtual-formula3", "v1 * v2 + t" }, { "DMM/virtual-unit3", "W" } };
    check(LegacyNodes::isLegacy(keys), "a simulated meter is one of before");
    QString error;
    std::unique_ptr<LogicNode> node = LegacyNodes::fromKeys(keys, {}, &error);
    auto *gen = dynamic_cast<GeneratorNode *>(node.get());
    check(gen && error.isEmpty() && gen->periodMs() == GeneratorNode::kDefaultPeriodMs, "a generator, on 250 ms: " + error);
    if (!gen)
      return 1;
    const QList<GeneratorNode::Wave> waves = gen->waves();
    check(waves.size() == 3 && waves[0].shape == CalcExpr::Triangle && waves[0].coupling == "AC" && waves[1].unit == "A"
            && waves[1].shape == CalcExpr::Custom && waves[2].formula == "y1 * y2 + t" && waves[2].unit == "W",
          "its waves: triangle AC, value 2 the default in A, value 3 over y1, y2");
    const QString device = oldDevice(keys);
    for (const qint64 ms : { 0, 1300, 7700, 61000 })
    {
      const LogicNode::Values now = at(*gen, ms);
      const QList<double> before = oldValues(device, ms / 1000.0);
      bool same = now.size() == 3;
      for (int i = 0; same && i < 3; ++i)
        same = now[i] && std::fabs(*now[i] - before[i]) < 1e-9;
      check(same, QString("at %1 s the same as before: %2 %3 %4").arg(ms / 1000.0).arg(before.value(0)).arg(before.value(1)).arg(before.value(2)));
    }
    check(LegacyNodes::isLegacy({ { "DMM/model", "QtDMM Virtual meter" } }), "the name before 26.2 too");
    check(!LegacyNodes::isLegacy({ { "DMM/model", "QtDMM Simulated meter" }, { "Node/json", "{}" } }), "saved as a node: none");
  }

  // 2. a calculated value naming two devices, with t and a second value over the first
  {
    const QVariantMap keys { { "DMM/model", "QtDMM Calculated value" }, { "DMM/calc-unit", "W/AC" },
                             { "DMM/calc-expression", "ut61e_bench * psu + t; mA v1 / psu" } };
    auto sourceOf = [](const QString &name)
    {
      return name == "ut61e_bench" ? QString("device:a") : name == "psu" ? QString("device:b") : QString();
    };
    QString error;
    std::unique_ptr<LogicNode> node = LegacyNodes::fromKeys(keys, sourceOf, &error);
    auto *f = dynamic_cast<FormulaNode *>(node.get());
    check(f && node->type() == "formula" && error.isEmpty(), "a formula: " + error);
    if (!f)
      return 1;
    check(f->inputs().size() == 2 && f->inputs()[0].toString() == "0@device:a" && f->inputs()[1].toString() == "0@device:b"
            && f->leadingInput() == 0,
          "its inputs: the devices named, the first leads");
    const QList<FormulaNode::Formula> fs = f->formulas();
    check(fs.size() == 2 && fs[0].text == "v1 * v2 + t" && fs[0].unit == "W" && fs[0].coupling == "AC" && fs[1].text == "y1 / v2"
            && fs[1].unit == "mA",
          "its formulas: " + fs.value(0).text + " | " + fs.value(1).text);
    at(*f, 1000, { reading(2), reading(4) });
    const LogicNode::Values v = at(*f, 3500, { reading(2), reading(4) });
    // t 2.5 s; y1 in its base unit, an output in mA gives its number in mA
    check(v.size() == 2 && v[0] && std::fabs(*v[0] - 10.5) < 1e-9 && v[1] && std::fabs(*v[1] - 10.5 / 4 * 1000) < 1e-6,
          QString("2 * 4 + 2.5 and that / 4 in mA: %1 %2").arg(v.value(0).value_or(NAN)).arg(v.value(1).value_or(NAN)));

    // a device it names is not in My devices
    QString missing;
    check(!LegacyNodes::fromKeys(keys, [](const QString &) { return QString(); }, &missing) && missing.contains("ut61e_bench"),
          "a device not there: refused, named: " + missing);
    // a formula that does not parse
    QString broken;
    check(!LegacyNodes::fromKeys({ { "DMM/model", "QtDMM Calculated value" }, { "DMM/calc-expression", "1 +" } }, sourceOf, &broken)
            && !broken.isEmpty(),
          "a formula error: refused: " + broken);
    // only t: a generator
    std::unique_ptr<LogicNode> clock = LegacyNodes::fromKeys(
      { { "DMM/model", "QtDMM Calculated value" }, { "DMM/calc-unit", "V" }, { "DMM/calc-expression", "12 + sin(t)" } }, sourceOf);
    check(clock && clock->type() == "generator" && clock->periodMs() > 0, "only t: a generator");
  }

  return failed == 0 ? 0 : 1;
}
