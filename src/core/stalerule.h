// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

/// When a port's last value is too old to stand for the present
/// (kern_spezifikation §4.3): older than three of its observed intervals,
/// at least 1 s, at most 30 s. Until the second value shows an interval,
/// 3 s (the link timeout the meters always had).
///
/// The interval is a moving average; a gap longer than the current limit is
/// an outage, not the meter's rhythm, and does not count. Two such gaps in a
/// row are the rhythm after all: the limit was too short, as for a meter
/// slower than the 3 s, or after a burst of readings held up on the way.
class StaleRule
{
public:
  static constexpr qint64 kMinMs = 1000;
  static constexpr qint64 kMaxMs = 30000;
  static constexpr qint64 kDefaultMs = 3000;

  /// A value arrived at @p t (ms, monotonic).
  void arrived(qint64 t)
  {
    if (m_last >= 0)
    {
      const qint64 dt = t - m_last;
      if (dt > 0 && dt <= maxAgeMs())
      {
        m_interval = m_interval > 0 ? 0.8 * m_interval + 0.2 * double(dt) : double(dt);
        m_gapBefore = false;
      }
      else if (dt > 0)
      {
        if (m_gapBefore)
          m_interval = double(dt);
        m_gapBefore = !m_gapBefore;
      }
    }
    m_last = t;
  }

  /// Whether the last value is stale at @p t; no value yet is stale, too.
  bool stale(qint64 t) const { return m_last < 0 || t - m_last > maxAgeMs(); }

  qint64 maxAgeMs() const
  {
    if (m_interval <= 0)
      return kDefaultMs;
    return qBound(kMinMs, qint64(3 * m_interval + 0.5), kMaxMs);
  }

  /// Forget the rhythm (another meter, a new connection).
  void reset()
  {
    m_last = -1;
    m_interval = 0;
    m_gapBefore = false;
  }

private:
  qint64 m_last = -1;
  double m_interval = 0;
  bool m_gapBefore = false;   ///< the last interval was longer than the limit
};
