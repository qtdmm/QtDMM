// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>

#include "core/reading.h"

/// The values of a Poincaré plot: each main value x(n) against the one k
/// readings later, x(n + k). A steady signal is a small cloud on the
/// diagonal; noise widens it across the diagonal (SD1), drift stretches it
/// along it (SD2); jumps between levels make separate clusters.
///
/// It keeps the last capacity() values in SI base units, so a range change
/// (mV -> V) goes on; another port or unit (V DC -> Ω, °C -> °F) starts
/// afresh, the same rule as MinMaxMemory. No pair spans a gap: an overload,
/// a held display or a value gone stale (gap()) breaks the sequence. Only
/// QtCore, so it can be tested on its own.
class PoincareSeries
{
public:
  /// One point of the plot.
  struct Pair
  {
    double x = 0;   ///< x(n)
    double y = 0;   ///< x(n + k)
    int    age = 0; ///< 0 for the newest pair, counting up to the oldest
  };

  /// The cloud in numbers; NaN where there are fewer than two pairs.
  struct Stats
  {
    int    count = 0;   ///< pairs
    double meanX = 0;
    double meanY = 0;
    double sd1 = 0;     ///< spread across the diagonal: std((y - x) / sqrt 2)
    double sd2 = 0;     ///< spread along it: std((y + x) / sqrt 2)
  };

  /// One scale for both axes: grid lines every @c step from @c lo to @c hi.
  struct Axis
  {
    double lo = 0;
    double hi = 1;
    double step = 0.5;
  };

  static constexpr int kDefaultCapacity = 500;
  static constexpr int kMaxLag = 10;

  /// A reading of the main value. True when it started the series afresh
  /// (another port or unit).
  bool        feed(const Reading &r);
  /// The value went stale: no pair across the time without one.
  void        gap();
  void        clear();

  /// Values kept (at least 2); the oldest go.
  void        setCapacity(int values);
  int         capacity() const { return m_capacity; }
  /// k, the distance of the pairs in readings (1 .. kMaxLag).
  void        setLag(int k);
  int         lag() const { return m_lag; }

  int         valueCount() const { return int(m_values.size()); }
  QVector<Pair> pairs() const;
  Stats       stats() const;
  /// The unit of the values ("V", "Ω"); empty before the first one.
  QString     unit() const { return m_unit; }
  /// One digit of the display in the unit of the values ("0.001" for
  /// "1.234 V"); 0 before the first value.
  double      resolution() const { return m_resolution; }

  /// The axis for values from @p lo to @p hi, with some room. All values
  /// alike (a meter at 0) give ten digits of @p resolution around them.
  static Axis axis(double lo, double hi, double resolution);

private:
  struct Value
  {
    double v;
    int    run;   ///< values of one run have no gap between them
  };

  QVector<Value> m_values;
  int     m_capacity = kDefaultCapacity;
  int     m_lag = 1;
  int     m_run = 0;
  bool    m_broken = false;   ///< the next value starts a new run
  PortKey m_port;             ///< invalid until a reading with a known quantity
  QString m_baseUnit;
  QString m_unit;
  double  m_resolution = 0;
};
