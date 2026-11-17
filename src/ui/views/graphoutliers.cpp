// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphoutliers.h"

#include "recording/recordingseries.h"

#include <QStringList>
#include <cmath>

namespace GraphOutliers
{
QString Rule::setting() const
{
  const QString u = unit.isEmpty() ? QString() : ' ' + unit;
  switch (mode)
  {
    case Sigma:  return QString("sigma %1").arg(a, 0, 'g', 12);
    case Jump:   return QString("jump %1").arg(a, 0, 'g', 12) + u;
    case Limits: return QString("limits %1 %2").arg(a, 0, 'g', 12).arg(b, 0, 'g', 12) + u;
    case Shown:  break;
  }
  return QString();
}

Rule Rule::fromSetting(const QString &text)
{
  const QStringList p = text.split(' ', Qt::SkipEmptyParts);
  bool okA = false, okB = false;
  const double a = p.value(1).toDouble(&okA), b = p.value(2).toDouble(&okB);
  if (p.value(0) == QLatin1String("sigma") && okA && a > 0)
    return { Sigma, a, 0, QString() };
  if (p.value(0) == QLatin1String("jump") && okA && a > 0)
    return { Jump, a, 0, p.value(2) };
  if (p.value(0) == QLatin1String("limits") && okA && okB && b > a)
    return { Limits, a, b, p.value(3) };
  return {};
}

bool isOutlier(const Rule &rule, const RecordingSeries &s, int i, const QString &unit)
{
  if (rule.mode == Shown || i < 0 || i >= s.count())
    return false;
  const double v = s.at(i).value;
  if (!std::isfinite(v))
    return false;
  // a jump or limits are in a unit: other units' curves stay as they are
  if (rule.mode != Sigma && unit != rule.unit)
    return false;
  switch (rule.mode)
  {
    case Limits:
      return v < rule.a || v > rule.b;
    case Jump:
    {
      // a spike: away from the reading before and the one after, the same
      // way; a step to another level is away from one side only
      int before = i - 1, after = i + 1;
      while (before >= 0 && !std::isfinite(s.at(before).value))
        before--;
      while (after < s.count() && !std::isfinite(s.at(after).value))
        after++;
      if (before < 0 || after >= s.count())
        return false;
      const double up = v - s.at(before).value, down = v - s.at(after).value;
      return std::abs(up) > rule.a && std::abs(down) > rule.a && (up > 0) == (down > 0);
    }
    case Sigma:
    {
      // its neighbours, without it: the mean and the spread there. Not the
      // window's - a drifting value would hide its ends, and Live would
      // draw every curve anew at each reading
      double sum = 0, sum2 = 0;
      int n = 0;
      const int from = qMax(0, i - kNeighbours), to = qMin(s.count() - 1, i + kNeighbours);
      const double ref = v;   // differences, not large values squared
      for (int j = from; j <= to; ++j)
        if (const double w = s.at(j).value; j != i && std::isfinite(w))
        {
          sum += w - ref;
          sum2 += (w - ref) * (w - ref);
          n++;
        }
      if (n < 10)
        return false;
      const double mean = sum / n;
      const double sigma = std::sqrt(qMax(0.0, sum2 / n - mean * mean));
      // mean is the neighbours' mean less v: |v - their mean| = |mean|
      return std::abs(mean) > rule.a * sigma && std::abs(mean) > 1e-12 * qMax(1.0, std::abs(v));
    }
    case Shown:
      break;
  }
  return false;
}
}
