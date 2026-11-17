// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphstates.h"

#include "recording/recordingseries.h"
#include "recording/recordingstore.h"
#include "ui/views/graphstatestrip.h"

#include <QStringList>

#include <algorithm>

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
static_assert(sizeof(Scanner::open) / sizeof(int) == GraphStateStrip::KindCount, "one open span per kind");

void Scanner::add(const RawPoint &p, qint64 t0, qint64 t1)
{
  const quint32 flags = p.flags;
  if (p.quality == Quality::Overload)
    put(GraphStateStrip::Overload, QStringLiteral("OL"), t0, t1);
  else if (p.gap() && t1 > t0)
    put(GraphStateStrip::Gap, GraphStats::gapCause(p), t0, t1, gapPattern(p));
  if (flags & SampleFlag::Hold)
    put(GraphStateStrip::Hold, QStringLiteral("HOLD"), t0, t1);
  if (flags & (SampleFlag::Peak | SampleFlag::Min | SampleFlag::Max))
    put(GraphStateStrip::Extreme, flags & SampleFlag::Peak ? QStringLiteral("PEAK")
                                  : (flags & SampleFlag::Min) && (flags & SampleFlag::Max) ? QStringLiteral("MIN MAX")
                                  : flags & SampleFlag::Min ? QStringLiteral("MIN") : QStringLiteral("MAX"), t0, t1);
  if (flags & SampleFlag::Relative)
    put(GraphStateStrip::Relative, QStringLiteral("REL"), t0, t1);
}

void Scanner::put(int kind, const QString &label, qint64 t0, qint64 t1, Qt::BrushStyle pattern)
{
  if (open[kind] >= 0 && out[open[kind]].label == label && out[open[kind]].to >= t0)
    out[open[kind]].to = t1;
  else
  {
    open[kind] = int(out.size());
    out << Span{ kind, label, t0, t1, pattern };
  }
}

QList<Span> spans(const GraphStats::Curve &curve, qint64 from, qint64 to)
{
  if (!curve.store)
    return {};
  // the series at the end of the time shown, as the reference line
  const RecordingSeries *series = curve.seriesAt(qMin(to, curve.store->duration() + curve.shift));
  if (!series || series->isEmpty())
    return {};
  const RecordingSeries &s = *series;
  const qint64 end = qMax(curve.store->duration(), s.last().t) + curve.shift;
  Scanner scan;
  for (int i = qMax(0, s.holding(from - curve.shift)); i < s.count(); ++i)
  {
    const qint64 t0 = s.at(i).t + curve.shift;
    if (t0 > to)
      break;
    scan.add(s.at(i), t0, i + 1 < s.count() ? s.at(i + 1).t + curve.shift : qMax(t0, end));
  }
  return scan.out;
}

QList<Span> Cache::spans(const GraphStats::Curve &curve, qint64 from, qint64 to)
{
  if (!curve.store)
    return {};
  const RecordingSeries *series = curve.seriesAt(qMin(to, curve.store->duration() + curve.shift));
  if (!series || series->isEmpty())
    return {};
  const RecordingSeries &s = *series;
  // another series, older readings gone or fewer readings: all anew
  if (series != m_series || curve.shift != m_shift || s.first().t != m_firstT || s.count() < m_done + 1)
  {
    m_series = series;
    m_shift = curve.shift;
    m_firstT = s.first().t;
    m_done = 0;
    m_scan = Scanner();
  }
  // the readings before the newest are final: each scanned once
  for (; m_done + 1 < s.count(); ++m_done)
    m_scan.add(s.at(m_done), s.at(m_done).t + m_shift, s.at(m_done + 1).t + m_shift);

  // the newest holds until the end of the recording, which still grows: on
  // a copy of the open spans, the only ones it can lengthen
  Scanner last;
  QList<int> taken;
  for (int k = 0; k < GraphStateStrip::KindCount; ++k)
    if (m_scan.open[k] >= 0)
    {
      last.open[k] = int(last.out.size());
      last.out << m_scan.out.at(m_scan.open[k]);
      taken << m_scan.open[k];
    }
  const qint64 t0 = s.last().t + m_shift;
  last.add(s.last(), t0, qMax(t0, qMax(curve.store->duration(), s.last().t) + m_shift));

  QList<Span> out;
  const auto shown = [&](const Span &span) { return span.to >= from && span.from <= to; };
  for (int i = 0; i < m_scan.out.size(); ++i)
    if (!taken.contains(i) && shown(m_scan.out.at(i)))
      out << m_scan.out.at(i);
  for (const Span &span : std::as_const(last.out))
    if (shown(span))
      out << span;
  std::stable_sort(out.begin(), out.end(), [](const Span &a, const Span &b) { return a.from < b.from; });
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
