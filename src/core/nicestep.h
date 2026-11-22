// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

/// 1, 2 or 5 times a power of ten, at least @p v: the step of an axis grid.
/// 1 for a value that is not positive and finite.
inline double niceStep(double v)
{
  if (!(v > 0) || !std::isfinite(v))
    return 1;
  const double decade = std::pow(10.0, std::floor(std::log10(v)));
  for (double m : { 1.0, 2.0, 5.0, 10.0 })
    if (m * decade >= v * (1 - 1e-12))
      return m * decade;
  return 10 * decade;
}

/// The decimals @p v needs (0 to 9).
inline int stepDecimals(double v)
{
  for (int d = 0; d < 9; ++d)
  {
    const double x = std::abs(v) * std::pow(10.0, d);
    if (std::abs(x - std::round(x)) < 1e-9 * (1 + x))
      return d;
  }
  return 9;
}

/// The grid of a fixed range over @p divisions: the first tick and the
/// step, so that first + divisions * step covers @p min .. @p max. The step
/// is 1, 1.5, 2, 3, 4, 5, 6 or 8 times a power of ten (0 .. 100 over 8
/// divisions: 0 .. 120 in 15); in the lowest power of ten where some
/// cover, the one whose labels need the fewest decimals, then the smallest
/// (49.5 .. 50.5: 49.4 in 0.2, not 49.5 in 0.15).
inline std::pair<double, double> fixedRangeGrid(double min, double max, int divisions)
{
  const double want = (max - min) / std::max(1, divisions);
  if (!(want > 0) || !std::isfinite(want))
    return { min, niceStep(want) };
  const int e0 = int(std::floor(std::log10(want)));
  double bestFirst = 0, bestStep = 0;
  int bestDecimals = 99;
  for (int e = e0; e <= e0 + 1 && bestStep == 0; ++e)
    for (double m : { 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0 })
    {
      const double step = m * std::pow(10.0, e);
      const double first = std::floor(min / step + 1e-9) * step;
      if (first + divisions * step < max - step * 1e-9)
        continue;
      const int decimals = std::max(stepDecimals(step), stepDecimals(first));
      if (decimals < bestDecimals || (decimals == bestDecimals && step < bestStep))
      {
        bestFirst = first;
        bestStep = step;
        bestDecimals = decimals;
      }
    }
  return { bestFirst, bestStep };
}
