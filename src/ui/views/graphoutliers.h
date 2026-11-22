// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class RecordingSeries;

/// Readings a graph does not draw: outliers by one of three rules. Only the
/// drawing and the statistics pass them over, the store keeps them. No
/// drawing library.
namespace GraphOutliers
{
enum Mode
{
  Shown = 0,   ///< all drawn
  Sigma,       ///< more than n σ from the mean of its neighbours (kNeighbours each side)
  Jump,        ///< a spike: further than x from the readings before and after it, the same way
  Limits       ///< outside min .. max
};

/// The readings each side whose mean and spread a reading is held against (Sigma).
constexpr int kNeighbours = 25;

struct Rule
{
  Mode    mode = Shown;
  double  a = 3;       ///< Sigma: n; Jump: x; Limits: min (base unit)
  double  b = 0;       ///< Limits: max
  QString unit;        ///< Jump, Limits: the base unit they are in; other units' curves keep theirs

  bool    active() const { return mode != Shown; }
  /// "sigma 3", "jump 0.5 V", "limits 0 10 V", empty: shown (Windows/<graph>-outliers).
  QString setting() const;
  /// The rule of setting(); anything else: shown.
  static Rule fromSetting(const QString &text);
  bool    operator==(const Rule &other) const = default;
};

/// Whether reading @p i of @p series, a curve in @p unit (base unit), is an outlier by @p rule.
bool isOutlier(const Rule &rule, const RecordingSeries &series, int i, const QString &unit);
}
