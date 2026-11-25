// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPointF>
#include <QString>

/// The reference line of a graph and the lines added to it, as numbers and
/// texts: where the line stands, how the settings keep it, where a curve
/// crosses it and where a drag puts it. No drawing library.
namespace GraphReference
{
/// What the line stands at: 0, a value of its own (dragged there), the
/// mean, minimum or maximum of the time shown - those follow the readings -
/// or an alarm threshold; or no line, the default [E 25.11.].
enum Kind { Zero = 0, Value, Mean, Min, Max, Alarm, Off };

struct Line
{
  Kind   kind = Off;
  double value = 0;     ///< base unit; of Value and Alarm (the others get theirs from the readings)

  /// Mean, Min, Max: the line moves with the readings.
  bool follows() const { return kind == Mean || kind == Min || kind == Max; }
  bool operator==(const Line &other) const;
};

/// A line of @p kind at @p value as it is kept: a value of 0 is Zero, only Value and Alarm keep one.
Line line(Kind kind, double value = 0);
/// "" for 0, "mean", "min", "max", "alarm <value>", "<value>", "off".
QString setting(const Line &line);
Line fromSetting(const QString &text);

/// The most lines added to the reference line.
constexpr int kMaxLines = 4;
/// The finite ones of @p values, kMaxLines at most.
QList<double> lines(const QList<double> &values);
/// "1.5 -2": the added lines, base unit.
QString linesSetting(const QList<double> &values);
QList<double> linesFromSetting(const QString &text);

/// Where a curve crosses a value.
struct Crossing
{
  double t = 0;       ///< as the points' x
  bool   up = false;  ///< rising through it
};
/// The crossings of @p points (sorted by x; NaN: a gap) with @p value from
/// @p from to @p to. Those nearer than @p merge to the one before count as
/// that one, with the newer direction: a noisy edge is one crossing.
QList<Crossing> crossings(const QList<QPointF> &points, double from, double to, double merge, double value);

/// A place the dragged line snaps to.
struct Snap
{
  Kind   kind = Zero;
  double value = 0;
};
/// @p value on a round step: a hundredth of @p span on 1-2-5.
double rounded(double value, double span);
/// The line dragged to @p value on an axis showing @p span, a pixel being
/// @p perPixel of it: the first of @p snaps within 6 pixels, else a value
/// of its own, rounded.
Line dragged(double value, double span, double perPixel, const QList<Snap> &snaps);
}
