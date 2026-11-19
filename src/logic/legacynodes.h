// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariantMap>
#include <functional>
#include <memory>

class LogicNode;

/// The entries of My devices from before the logic nodes, read as nodes
/// when they are opened (PR 2). Their keys stay as they are until the entry
/// is saved in its dialog, which writes Node/json instead.
///
/// - "QtDMM Simulated meter" ("Virtual meter" before 26.2, DMM/virtual-*):
///   a GeneratorNode. Value 1 keeps its shape, values 2-4 their formulas;
///   the values before, v1..v3 there, are y1..y3 now.
/// - "QtDMM Calculated value" (DMM/calc-unit, DMM/calc-expression): a
///   FormulaNode whose inputs are the devices the formula names
///   ("uni_t_803" for "UNI-T 803", DeviceSession::variableName()), the
///   first named leading; v1.. of before are y1... Without a device it
///   only runs on t: a GeneratorNode with the formulas as its own.
namespace LegacyNodes
{
  /// Whether @p keys are of such an entry.
  bool isLegacy(const QVariantMap &keys);
  /// The node of @p keys. @p sourceOf gives the source ("device:<id>") of a
  /// device's variable name, empty for none. Null and why in @p error for
  /// a device the formula names that is not in My devices, or a formula
  /// that does not parse (it did not open before either).
  std::unique_ptr<LogicNode> fromKeys(const QVariantMap &keys, const std::function<QString(const QString &name)> &sourceOf,
                                      QString *error = nullptr);
}
