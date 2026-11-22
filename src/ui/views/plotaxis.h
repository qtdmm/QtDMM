// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

/// The displayed range and unit for one numeric plot axis.
class PlotAxis
{
public:
  struct Range
  {
    double minimum = 0;
    double maximum = 0;
    double factor = 1;
    QString unit;
    QString title;
  };

  /// Chooses an SI prefix and pads a constant range in displayed units.
  static Range range(double minimum, double maximum, const QString &baseUnit);
};
