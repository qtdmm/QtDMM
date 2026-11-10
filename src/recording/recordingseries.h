// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>
#include <QString>
#include <QVector>

#include "core/sampletypes.h"

/// One reading of a recording, at the time it came.
struct RawPoint
{
  qint64  t = 0;            ///< ms since the start of the recording (core clock)
  double  value = 0;        ///< SI base units; NaN = none (an overload, or a gap)
  double  integral = 0;     ///< the integral of the values above the threshold up to t, unit x s; NaN in a gap
  Quality quality = Quality::Valid;   ///< Overload, Stale (a gap) or Valid
  quint32 flags = 0;        ///< SampleFlag of the reading

  bool    gap() const { return std::isnan(value); }
};

/// The readings of one port of a recording, in time order, with their times.
///
/// The recorder keeps one series for the main value and one for each other
/// value of the meter and the port it measures (a second display that goes
/// from Hz to % begins a new one). Views read it without knowing the graph:
/// count(), at(), lowerBound(), and the store's signals.
///
/// A value holds from its time until the next point: that is what the meter
/// showed meanwhile. A gap is a point of its own (NaN): an overload, or the
/// value went stale.
class RecordingSeries
{
public:
  PortKey port;    ///< what the series measures; invalid until the first value says
  QString unit;    ///< base unit without SI prefix ("V")
  int     id = 0;  ///< which value of the meter (Reading::id): 0 the main value, 1.. the others
  QString name;    ///< the meter's own name of the value (Reading::name), empty when none
  double  runningIntegral = 0;   ///< the integral up to the last point, carried over gaps (the store's)

  int      count() const { return int(m_points.size()) - m_head; }
  bool     isEmpty() const { return count() == 0; }
  const RawPoint &at(int i) const { return m_points[m_head + i]; }
  const RawPoint &first() const { return at(0); }
  const RawPoint &last() const { return m_points.last(); }

  /// The first point at or after @p t; count() when there is none.
  int lowerBound(qint64 t) const
  {
    int lo = 0, hi = count();
    while (lo < hi)
    {
      const int mid = (lo + hi) / 2;
      if (at(mid).t < t)
        lo = mid + 1;
      else
        hi = mid;
    }
    return lo;
  }
  /// The last point at or before @p t (the one whose value holds at t); -1
  /// when there is none.
  int holding(qint64 t) const { return lowerBound(t + 1) - 1; }

  void append(const RawPoint &p) { m_points.append(p); }
  /// More flags for the last point (a gap's cause known only later).
  void addLastFlags(quint32 flags) { m_points.last().flags |= flags; }
  /// Drops the @p n oldest points.
  void removeFirst(int n)
  {
    m_head += qBound(0, n, count());
    // the dropped ones go for good once they are half the storage
    if (m_head > 4096 && m_head * 2 > int(m_points.size()))
    {
      m_points.remove(0, m_head);
      m_head = 0;
    }
  }
  void clear()
  {
    m_points.clear();
    m_head = 0;
  }
  void reserve(int n) { m_points.reserve(m_head + n); }

private:
  QVector<RawPoint> m_points;
  int m_head = 0;   ///< the first point still kept
};
