// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphstates.h"

#include "recording/recordingseries.h"
#include "recording/recordingstore.h"
#include "ui/views/graphstatestrip.h"

#include <QStringList>

namespace
{
// the hatching of a gap's cause, as the graph on QtCharts has it
Qt::BrushStyle gapPattern(const RawPoint &p)
{
  if (p.quality == Quality::Fault)
    return Qt::FDiagPattern;
  if (p.quality == Quality::NoValue)
    return Qt::Dense5Pattern;
  if (p.flags & SampleFlag::GapPause)
    return Qt::HorPattern;
  if (p.flags & SampleFlag::GapLost)
    return Qt::DiagCrossPattern;
  return Qt::BDiagPattern;
}
}

namespace GraphStates
{
QList<Span> spans(const GraphStats::Curve &curve, qint64 from, qint64 to)
{
  QList<Span> out;
  if (!curve.store)
    return out;
  // the series at the end of the time shown, as the reference line
  const RecordingSeries *series = curve.seriesAt(qMin(to, curve.store->duration() + curve.shift));
  if (!series || series->isEmpty())
    return out;
  const RecordingSeries &s = *series;
  const qint64 end = qMax(curve.store->duration(), s.last().t) + curve.shift;
  // the open stretch of each kind
  int open[GraphStateStrip::KindCount];
  std::fill(std::begin(open), std::end(open), -1);
  auto put = [&](int kind, const QString &label, qint64 t0, qint64 t1, Qt::BrushStyle pattern = Qt::SolidPattern)
  {
    if (open[kind] >= 0 && out[open[kind]].label == label && out[open[kind]].to >= t0)
      out[open[kind]].to = t1;
    else
    {
      open[kind] = int(out.size());
      out << Span{ kind, label, t0, t1, pattern };
    }
  };
  for (int i = qMax(0, s.holding(from - curve.shift)); i < s.count(); ++i)
  {
    const qint64 t0 = s.at(i).t + curve.shift;
    if (t0 > to)
      break;
    const qint64 t1 = i + 1 < s.count() ? s.at(i + 1).t + curve.shift : qMax(t0, end);
    const quint32 flags = s.at(i).flags;
    if (s.at(i).quality == Quality::Overload)
      put(GraphStateStrip::Overload, QStringLiteral("OL"), t0, t1);
    else if (s.at(i).gap() && t1 > t0)
      put(GraphStateStrip::Gap, GraphStats::gapCause(s.at(i)), t0, t1, gapPattern(s.at(i)));
    if (flags & SampleFlag::Hold)
      put(GraphStateStrip::Hold, QStringLiteral("HOLD"), t0, t1);
    if (flags & (SampleFlag::Peak | SampleFlag::Min | SampleFlag::Max))
      put(GraphStateStrip::Extreme, flags & SampleFlag::Peak ? QStringLiteral("PEAK")
                                    : (flags & SampleFlag::Min) && (flags & SampleFlag::Max) ? QStringLiteral("MIN MAX")
                                    : flags & SampleFlag::Min ? QStringLiteral("MIN") : QStringLiteral("MAX"), t0, t1);
    if (flags & SampleFlag::Relative)
      put(GraphStateStrip::Relative, QStringLiteral("REL"), t0, t1);
  }
  return out;
}

QString text(const QList<Span> &spans)
{
  QStringList parts;
  for (const Span &s : spans)
    parts << QString("%1 %2-%3").arg(s.label).arg(s.from / 1000.0, 0, 'f', 1).arg(s.to / 1000.0, 0, 'f', 1);
  return parts.join(" | ");
}
}
