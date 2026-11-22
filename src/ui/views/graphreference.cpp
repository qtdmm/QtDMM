// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphreference.h"

#include "core/nicestep.h"

#include <QStringList>
#include <algorithm>
#include <cmath>

namespace GraphReference
{
bool Line::operator==(const Line &other) const
{
  return kind == other.kind && ((kind != Value && kind != Alarm) || value == other.value);
}

Line line(Kind kind, double value)
{
  if (kind == Value && value == 0)
    kind = Zero;
  return Line{ kind, kind == Value || kind == Alarm ? value : 0 };
}

QString setting(const Line &line)
{
  switch (line.kind)
  {
    case Mean:  return QStringLiteral("mean");
    case Min:   return QStringLiteral("min");
    case Max:   return QStringLiteral("max");
    case Alarm: return QStringLiteral("alarm ") + QString::number(line.value, 'g', 12);
    case Value: return QString::number(line.value, 'g', 12);
    case Off:   return QStringLiteral("off");
    case Zero:  break;
  }
  return QString();
}

Line fromSetting(const QString &text)
{
  const QString t = text.trimmed();
  bool ok = false;
  if (t == QLatin1String("mean"))
    return line(Mean);
  if (t == QLatin1String("min"))
    return line(Min);
  if (t == QLatin1String("max"))
    return line(Max);
  if (t == QLatin1String("off"))
    return line(Off);
  if (const double v = t.section(' ', 1).toDouble(&ok); t.startsWith(QLatin1String("alarm ")) && ok && std::isfinite(v))
    return line(Alarm, v);
  if (const double v = t.toDouble(&ok); ok && std::isfinite(v))
    return line(Value, v);
  return line(Zero);
}

QList<double> lines(const QList<double> &values)
{
  QList<double> out;
  for (double v : values)
    if (std::isfinite(v) && out.size() < kMaxLines)
      out << v;
  return out;
}

QString linesSetting(const QList<double> &values)
{
  QStringList out;
  for (double v : values)
    out << QString::number(v, 'g', 12);
  return out.join(' ');
}

QList<double> linesFromSetting(const QString &text)
{
  QList<double> out;
  for (const QString &part : text.split(' ', Qt::SkipEmptyParts))
  {
    bool ok = false;
    if (const double v = part.toDouble(&ok); ok)
      out << v;
  }
  return lines(out);
}

QList<Crossing> crossings(const QList<QPointF> &points, double from, double to, double merge, double value)
{
  QList<Crossing> out;
  if (points.size() < 2 || !(to > from))
    return out;
  auto it = std::lower_bound(points.cbegin(), points.cend(), from, [](const QPointF &p, double x) { return p.x() < x; });
  if (it != points.cbegin())
    --it;
  for (auto next = it + 1; next != points.cend() && it->x() <= to; it = next++)
  {
    const double a = it->y(), b = next->y();
    if (!std::isfinite(a) || !std::isfinite(b) || (a > value) == (b > value))
      continue;
    // where the line between them meets the value
    const double t = it->x() + (next->x() - it->x()) * (value - a) / (b - a);
    if (t < from || t > to)
      continue;
    if (!out.isEmpty() && t - out.last().t < merge)
      out.last() = Crossing{ t, b > a };
    else
      out << Crossing{ t, b > a };
  }
  return out;
}

double rounded(double value, double span)
{
  const double step = niceStep(span / 100);
  return std::round(value / step) * step;
}

Line dragged(double value, double span, double perPixel, const QList<Snap> &snaps)
{
  for (const Snap &snap : snaps)
    if (std::abs(snap.value - value) <= 6 * perPixel)
      return line(snap.kind, snap.value);
  return line(Value, rounded(value, span));
}
}
