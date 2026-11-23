// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>

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
