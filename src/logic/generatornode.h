// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/calcexpr.h"
#include "logic/formulanode.h"

/// A logic node without inputs that makes up to kMaxValues signals on a
/// clock (concept §8, PR 2): to try QtDMM without a meter, to test a setup.
/// Each value has a shape - constant, random, sine, triangle, square,
/// sawtooth, discharge between min and max with a period, plus noise -
/// or a formula over t (the seconds since it started) and the values
/// before it (y1, y2, y3). It is a FormulaNode whose formulas the shapes
/// give (CalcExpr::waveformFormula()); "QtDMM Simulated meter" of before
/// opens as one.
class GeneratorNode : public FormulaNode
{
public:
  /// One value: what it is called and measures, and its shape.
  struct Wave
  {
    QString name;
    QString unit = QStringLiteral("V");
    QString coupling = QStringLiteral("DC");   ///< "DC", "AC" or empty
    CalcExpr::Waveform shape = CalcExpr::Sine;
    QString min = QStringLiteral("0");
    QString max = QStringLiteral("10");
    QString period = QStringLiteral("20");      ///< s (the time constant of a discharge)
    QString noise = QStringLiteral("0");        ///< peak to peak, 0: none
    QString formula;                            ///< CalcExpr::Custom: over t and y1..
    bool    operator==(const Wave &o) const = default;
  };

  static constexpr const char *kType = "generator";
  static constexpr int kDefaultPeriodMs = 250;   ///< as the simulated meter ticked

  GeneratorNode() { setPeriodMs(kDefaultPeriodMs); }

  QString type() const override { return QString::fromLatin1(kType); }
  QJsonObject parameters() const override;
  void    setParameters(const QJsonObject &p) override;

  QList<Wave> waves() const { return m_waves; }
  /// Up to kMaxValues; sets the formulas they give.
  void    setWaves(const QList<Wave> &waves);
  /// The formula of @p wave: its shape's, or its own.
  static QString formulaOf(const Wave &wave);
  /// The shape as stored ("sine") and back; unknown: sine.
  static QString shapeName(CalcExpr::Waveform shape);
  static CalcExpr::Waveform shapeOf(const QString &name);

private:
  QList<Wave> m_waves;
};
