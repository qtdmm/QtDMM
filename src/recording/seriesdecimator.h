// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "recording/recordingseries.h"

#include <QList>
#include <QPointF>
#include <functional>

/// Thins the readings of a series for drawing: one column of the picture
/// gets the minimum and the maximum of its readings, so a spike survives.
/// The store keeps every reading; only the drawing is thinned.
///
/// The columns are buckets of time counted from the start of the recording
/// (bucketSize(), a power of two in ms): while the window scrolls, a reading
/// stays in its bucket and the drawn minima and maxima stay put instead of
/// jittering with every new reading.
///
/// The rules are those of the QtCharts graph (GraphWidget::pointsOf()), kept
/// here without a widget for the graph on QCustomPlot.
namespace SeriesDecimator
{
struct Options
{
  bool   integral = false;       ///< the points are the integral, not the value
  double integralOffset = 0;     ///< integral: offset + integral x scale
  double integralScale = 1;
  /// Readings passed over (hidden outliers): the line runs from the reading
  /// before to the one after them, no gap. Never asked for the integral.
  std::function<bool(int)> hidden;
};

/// The bucket's length in ms for @p shownMs of time on @p columns pixels: the
/// smallest power of two that leaves at most one bucket per column.
qint64 bucketSize(qint64 shownMs, int columns);

/// The index of the first reading in the bucket of reading @p i.
int bucketStart(const RecordingSeries &series, int i, qint64 bucket);

/// Appends the points of the readings @p first..@p last, one bucket, to
/// @p out (x in seconds) and returns how many: the reading itself, or with
/// more than one the minimum and the maximum in time order. Readings without
/// a value (NaN: overload, stale) do not count; a bucket of nothing else is
/// one NaN point, the gap, and one that ends where the meter fell silent
/// ends with it.
int pointsOf(const RecordingSeries &series, int first, int last, QList<QPointF> &out, const Options &options = {});

/// Appends the points of the readings @p first..@p last, bucket by bucket,
/// and returns how many of them belong to the last bucket (the tail a new
/// reading replaces).
int append(const RecordingSeries &series, int first, int last, qint64 bucket, QList<QPointF> &out,
           const Options &options = {});
}
