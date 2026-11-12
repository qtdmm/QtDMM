// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/plotaxis.h"

#include "core/siprefix.h"

#include <cmath>

PlotAxis::Range PlotAxis::range(double minimum, double maximum, const QString &baseUnit)
{
  QString prefix;
  SiPrefix::scale(qMax(std::abs(minimum), std::abs(maximum)), baseUnit, &prefix);
  const double factor = SiPrefix::factor(prefix);
  if (minimum == maximum)
  {
    const double shown = minimum / factor;
    const double padding = qMax(std::abs(shown) * 0.05, 0.5);
    minimum -= padding * factor;
    maximum += padding * factor;
  }

  const QString unit = SiPrefix::displayText(prefix + baseUnit);
  const QString title = baseUnit.isEmpty() ? QString() : QStringLiteral("[%1]").arg(unit);
  return { minimum, maximum, factor, unit, title };
}
