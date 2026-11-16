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
/// "HOLD 3.0-5.0 | OL 8.0-9.0": the spans in seconds, for the tests.
QString text(const QList<Span> &spans);
}
