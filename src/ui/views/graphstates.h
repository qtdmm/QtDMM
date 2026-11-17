// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

#include "ui/views/graphstats.h"

/// The states a meter reported with the readings of a curve - HOLD, a
/// stored MIN / MAX / PEAK, REL, an overload - and its gaps by their cause,
/// as stretches of time: what GraphStateStrip draws. No drawing library; the
/// rules are the ones of the graph on QtCharts (GraphWidget::stateSpans()).
namespace GraphStates
{
struct Span
{
  int     kind = 0;   ///< GraphStateStrip::Kind
  QString label;      ///< "HOLD", "MIN", "MAX", "PEAK", "REL", "OL", a gap's cause
  qint64  from = 0;   ///< ms on the graph's time
  qint64  to = 0;
  Qt::BrushStyle pattern = Qt::SolidPattern;   ///< a gap's cause
};

/// The states of @p curve from @p from to @p to (ms on the graph's time),
/// each kind's stretches joined; a reading holds until the next one.
QList<Span> spans(const GraphStats::Curve &curve, qint64 from, qint64 to);

/// Joins the readings' states into spans, one reading after the other.
struct Scanner
{
  QList<Span> out;
  int open[5] = { -1, -1, -1, -1, -1 };   ///< per GraphStateStrip::Kind: its span still open, -1 none

  /// The reading @p p, holding from @p t0 to @p t1.
  void add(const RawPoint &p, qint64 t0, qint64 t1);

private:
  void put(int kind, const QString &label, qint64 t0, qint64 t1, Qt::BrushStyle pattern = Qt::SolidPattern);
};

/// spans() for a picture after each reading: the states of a whole series
/// are kept and each reading is scanned once, not every reading of the
/// window with every picture. A span reaching into the window is whole,
/// also where it begins before it.
class Cache
{
public:
  QList<Span> spans(const GraphStats::Curve &curve, qint64 from, qint64 to);

private:
  const RecordingSeries *m_series = nullptr;
  qint64  m_shift = 0;
  qint64  m_firstT = 0;
  int     m_done = 0;   ///< readings scanned, all but the newest
  Scanner m_scan;
};

/// "HOLD 3.0-5.0 | OL 8.0-9.0": the spans in seconds, for the tests.
QString text(const QList<Span> &spans);
}
