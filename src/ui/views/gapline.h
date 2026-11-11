// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPen>
#include <QPointF>
#include <QtNumeric>

class QAbstractAxis;
class QChart;
class QLineSeries;
class QScatterSeries;

/// A line with gaps: Qt Charts draws a line across a NaN point as if it were
/// not there, so a gap (overload, no value) splits the line into segments,
/// one QLineSeries each.
///
/// The points come as for one series; a point with a NaN y is the gap. The
/// first segment is the series the chart maps positions with; the others
/// are kept in a pool and reused, so a gap that comes and goes does not
/// grow the chart.
///
/// The line keeps all its points, but gives the chart only those around the
/// span on the time axis (setSpan()): Qt Charts lays out every point of a
/// series with each new one and each move of the axis, and a long recording
/// made each reading and each frame cost as much as the whole curve.
///
/// The hold (setHold()) draws the newest value on to a time, faint: the
/// curve runs on with the clock between the readings instead of jumping.
///
/// The markers (setMarkers()) get the same points as the segments, without
/// the gaps: a scatter series with the whole recording cost as much.
class GapLine
{
public:
  /// @p first is in the chart and on its axes already.
  explicit GapLine(QLineSeries *first);

  QLineSeries *first() const { return m_segments.first(); }

  void setPen(const QPen &pen);
  QPen pen() const { return m_pen; }
  void setVisible(bool on);
  /// The points also go to @p markers from the next replace() on; nullptr:
  /// none (the old ones are emptied).
  void setMarkers(QScatterSeries *markers);
  /// Moves every segment onto @p axes (the x axis and one y axis).
  void setAxes(const QList<QAbstractAxis *> &axes);

  /// All points anew.
  void replace(const QList<QPointF> &points);
  /// Points after the last ones.
  void append(const QList<QPointF> &points);
  /// Takes back the last @p count points given to append() or replace()
  /// (gap points included).
  void removeLast(int count);
  void clear();

  /// The x range on the axis: the chart gets the points in it and half as
  /// much again on each side, anew only when the range leaves them.
  void setSpan(double min, double max);
  /// The newest value drawn on to @p x, faint; NaN: none.
  void setHold(double x);

  /// Segments that have points (for the tests).
  int segments() const;

  static constexpr int kUndo = 8;

private:
  struct Step
  {
    bool gap = false;         ///< the point was a gap
    bool newSegment = false;  ///< the point opened a segment
    bool pendingBefore = false;
  };

  QLineSeries *segment(int i);
  QLineSeries *newSeries(const QPen &pen);
  void         add(const QPointF &p);
  /// The points of the span into the segments.
  void         load();
  void         updateHold();

  QList<QLineSeries *> m_segments;
  int          m_used = 1;          ///< segments in use; the last is the open one
  bool         m_pending = false;   ///< a gap came: the next point opens a segment
  QList<Step>  m_steps;             ///< the last points, for removeLast()
  QPen         m_pen;
  bool         m_visible = true;

  QList<QPointF> m_all;             ///< every point, the chart has m_from.. of them
  qsizetype    m_from = 0;
  bool         m_toEnd = true;      ///< the chart has the points up to the newest
  double       m_spanMin = -qInf(), m_spanMax = qInf();
  double       m_loadMin = -qInf(), m_loadMax = qInf();   ///< the x range loaded
  QScatterSeries *m_markers = nullptr;
  QLineSeries *m_hold = nullptr;
  double       m_holdX = qQNaN();
};
