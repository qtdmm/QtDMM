// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/plotaxis.h"

#include <QCoreApplication>
#include <QDebug>
#include <cmath>

static bool near(double a, double b)
{
  return std::abs(a - b) < 1e-12;
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  const PlotAxis::Range millivolts = PlotAxis::range(0.001, 0.001, QStringLiteral("V"));
  if (millivolts.unit != QStringLiteral("mV") || millivolts.title != QStringLiteral("[mV]") ||
      !near(millivolts.factor, 0.001) || !near(millivolts.minimum, 0.0005) ||
      !near(millivolts.maximum, 0.0015))
  {
    qCritical() << "constant voltage range was not padded in displayed units";
    return 1;
  }

  const PlotAxis::Range varying = PlotAxis::range(-0.002, 0.003, QStringLiteral("V"));
  if (varying.unit != QStringLiteral("mV") || !near(varying.minimum, -0.002) || !near(varying.maximum, 0.003))
  {
    qCritical() << "varying range changed unexpectedly";
    return 1;
  }

  const PlotAxis::Range unitless = PlotAxis::range(0, 0, QString());
  if (!unitless.unit.isEmpty() || !unitless.title.isEmpty() || !near(unitless.minimum, -0.5) ||
      !near(unitless.maximum, 0.5))
  {
    qCritical() << "unitless constant range is invalid";
    return 1;
  }

  return 0;
}
