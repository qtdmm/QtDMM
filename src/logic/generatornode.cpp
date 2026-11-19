// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logic/generatornode.h"

#include <QJsonArray>

#include "core/sampletypes.h"

namespace
{
// the shapes as stored, in the order of CalcExpr::Waveform
const char *const kShapes[] = { "constant", "random", "sine", "triangle", "square", "sawtooth", "discharge", "formula" };
}

QString GeneratorNode::shapeName(CalcExpr::Waveform shape)
{
  const int i = int(shape);
  return QString::fromLatin1(i >= 0 && i <= int(CalcExpr::Custom) ? kShapes[i] : kShapes[CalcExpr::Sine]);
}

CalcExpr::Waveform GeneratorNode::shapeOf(const QString &name)
{
  for (int i = 0; i <= int(CalcExpr::Custom); ++i)
    if (name == QLatin1String(kShapes[i]))
      return static_cast<CalcExpr::Waveform>(i);
  return CalcExpr::Sine;
}

QString GeneratorNode::formulaOf(const Wave &wave)
{
  return wave.shape == CalcExpr::Custom ? wave.formula.trimmed()
                                        : CalcExpr::waveformFormula(wave.shape, wave.min, wave.max, wave.period, wave.noise);
}

void GeneratorNode::setWaves(const QList<Wave> &waves)
{
  m_waves = waves.mid(0, kMaxValues);
  QList<Formula> formulas;
  for (const Wave &w : std::as_const(m_waves))
    formulas << Formula { w.name, w.unit, formulaOf(w), w.coupling };
  setFormulas(formulas);
}

QJsonObject GeneratorNode::parameters() const
{
  QJsonArray out;
  for (const Wave &w : m_waves)
  {
    QJsonObject o { { "name", w.name }, { "unit", w.unit }, { "coupling", w.coupling }, { "shape", shapeName(w.shape) } };
    if (w.shape == CalcExpr::Custom)
      o["formula"] = w.formula;
    else
    {
      o["min"] = w.min;
      o["max"] = w.max;
      o["period"] = w.period;
      o["noise"] = w.noise;
    }
    out.append(o);
  }
  return { { "outputs", out } };
}

void GeneratorNode::setParameters(const QJsonObject &p)
{
  QList<Wave> waves;
  const Wave defaults;
  for (const QJsonValue &v : p["outputs"].toArray())
  {
    const QJsonObject o = v.toObject();
    Wave w;
    w.name = o["name"].toString();
    w.unit = o["unit"].toString(defaults.unit);
    w.coupling = o["coupling"].toString(defaults.coupling);
    w.shape = shapeOf(o["shape"].toString());
    w.min = o["min"].toString(defaults.min);
    w.max = o["max"].toString(defaults.max);
    w.period = o["period"].toString(defaults.period);
    w.noise = o["noise"].toString(defaults.noise);
    w.formula = o["formula"].toString();
    waves << w;
  }
  setWaves(waves);
}
