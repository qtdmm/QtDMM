// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QFontMetrics>
#include <QList>
#include <QString>
#include <functional>

#include "recording/recordingseries.h"
#include "recording/recordingstore.h"

class RecordingStore;

/// The statistics a graph shows next to its curves, as text: the values at
/// the time cursors A and B, what the curves did from A to B, and over the
/// time shown. No drawing library and no widget: the rules are those of the
/// graph on QtCharts (GraphWidget::statsHtml()), for the graph on QCustomPlot.
namespace GraphStats
{
/// One curve: its series one after the other (a second display that went
/// from Hz to % has two), on the graph's time @p shift ms after its store's.
struct Curve
{
  QString name;
  QString unit;                 ///< base unit, of its newest series
  QColor  colour;
  const RecordingStore *store = nullptr;
  QList<const RecordingSeries *> parts;
  qint64  shift = 0;
  /// Readings the graph does not draw (hidden outliers): the statistics pass them over.
  std::function<bool(const RecordingSeries &, int)> hidden;

  /// The statistics of @p series (one of parts) from @p from to @p to, the
  /// graph's time, without the hidden readings.
  RecordingStore::Stats statsOf(const RecordingSeries &series, qint64 from, qint64 to) const;
  /// The series whose reading holds at @p t (the graph's time); nullptr before the first.
  const RecordingSeries *seriesAt(qint64 t) const;
  /// The value that holds at @p t; NaN in a gap, before the first reading or after the recording.
  double  valueAt(qint64 t) const;
  /// The reading that says what the curve shows at @p t: the one that
  /// holds, or the next one when that is nearer and neither is a gap;
  /// nullptr outside the recording. @p at: its time on the graph's.
  const RawPoint *readingNear(qint64 t, qint64 *at = nullptr) const;
};

/// "1.234 mV", "--" for none.
QString withUnit(double v, const QString &unit);
/// "250 ms", "12.5 s", "1:05:09"
QString spanText(qint64 ms);
/// A time of the recording: "0:01:20.250"
QString timeText(qint64 ms);
/// What the meter said about a reading: "HOLD REL", "OL".
QString stateWords(quint32 flags, Quality quality);
/// Why a gap has no value: "fault", "no number", "paused", "connection lost", "no value".
QString gapCause(const RawPoint &p);

/// One prefix and one number of decimals for the numbers of a value, so
/// they stand below each other: four significant digits for the largest of
/// @p values; the smallest of @p fine (a σ, a peak-peak) keeps two, within
/// six decimals.
struct ColumnScale
{
  double  factor = 1;
  QString unit;
  int     decimals = 0;
  QString text(double v) const;
};
ColumnScale columnScale(const QList<double> &values, const QList<double> &fine, const QString &unit);

struct Input
{
  QList<Curve> curves;
  qint64 cursor[2] = { -1, -1 };   ///< A and B, ms on the graph's time; -1: none
  qint64 from = 0;                 ///< the time shown
  qint64 to = 0;
  qint64 end = 0;                  ///< the recording's end on the graph's time
  int    outliersHidden = 0;       ///< in the time shown, for its title
};
/// The statistics as HTML. With @p room > 0 (pixels for a name, measured
/// with @p metrics) names are cut; @p want: how wide the widest row would
/// like the column to be.
QString html(const Input &input, const QFontMetrics &metrics, int room = 0, int *want = nullptr);
/// The same as plain text (for the tests).
QString text(const Input &input);
}
