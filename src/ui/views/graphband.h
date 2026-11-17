// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

/// A curve as a band: per pixel column the lowest and the highest of the
/// curve as drawn, its points and the lines between them. For a curve too
/// dense for a line and for the min/max band behind a curve. No drawing
/// library; the rules are the ones of the graph on QtCharts
/// (GraphWidget::columnBand()).
namespace GraphBand
{
/// A curve is drawn as a band when its window has more readings than this
/// per pixel column: a denser line zigzags inside the columns, its peaks cut
/// at random by the antialiasing (a moiré that moves with every new scale).
constexpr double kDensePoints = 1.0;
/// ... and when it turns more often than once in this many columns: a dense
/// smooth curve (a sine, a ramp) stays a line, a band of it would be a
/// ladder of two stepped outlines.
constexpr int kTurnColumns = 4;
/// A turn counts when the curve goes back by at least this many pixels:
/// noise of a pixel or two on a ramp is no zigzag.
constexpr double kTurnPixels = 3.0;
/// A dense curve's band takes the extremes of this many columns either
/// side: a bucket of the thinned curve is one to two columns wide, a column
/// alone may hold only a part of the line between two of its points.
constexpr int kDenseColumns = 1;
/// The min/max band takes the extremes of this many columns either side.
constexpr int kEnvelopeColumns = 4;

/// The band of @p points (sorted by x; NaN: a gap, which parts the band)
/// over the plot @p plot (pixels) showing x from @p x0 to @p x1 and y from
/// @p y0 to @p y1: one closed outline per stretch between gaps. Each
/// column gets the extremes of the columns within @p k of it on its side of
/// a gap. The columns are counted from x = 0, not from the left edge: a
/// window moving on by a part of a pixel leaves each point in its column.
QList<QPolygonF> columnBand(const QList<QPointF> &points, double x0, double x1, double y0, double y1, const QRectF &plot, int k);

/// For the tests: the lowest and highest y (as the points') of the band's
/// column at @p x; false where it has none.
bool columnAt(const QList<QPointF> &points, double x0, double x1, const QRectF &plot, int k, double x, double *lo, double *hi);
}
