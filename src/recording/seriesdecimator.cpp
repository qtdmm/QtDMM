// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/seriesdecimator.h"

#include <cmath>

qint64 SeriesDecimator::bucketSize(qint64 shownMs, int columns)
{
  columns = qMax(1, columns);
  const qint64 need = (shownMs + columns - 1) / columns;
  qint64 ms = 1;
  while (ms < need)
    ms *= 2;
  return ms;
}

int SeriesDecimator::bucketStart(const RecordingSeries &series, int i, qint64 bucket)
{
  const qint64 begin = series.at(i).t / bucket * bucket;
  return qMin(i, series.lowerBound(begin));
}

int SeriesDecimator::pointsOf(const RecordingSeries &series, int first, int last, QList<QPointF> &out,
                              const Options &options)
{
  auto value = [&](int i) { return series.at(i).value; };
  auto x = [&](int i) { return series.at(i).t / 1000.0; };
  auto hidden = [&](int i) { return options.hidden && options.hidden(i); };
  bool gap = false;
  while (first <= last && (std::isnan(value(first)) || hidden(first)))
    gap = std::isnan(value(first++)) || gap;
  if (first > last)
  {
    if (!gap)
      return 0;   // only hidden readings
    out.append(QPointF(x(last), qQNaN()));
    return 1;
  }
  int lo = first, hi = first;
  double loValue = value(first), hiValue = loValue;
  for (int i = first + 1; i <= last; i++)
  {
    const double v = value(i);
    if (std::isnan(v) || hidden(i))
      continue;
    if (v < loValue)
    {
      loValue = v;
      lo = i;
    }
    if (v > hiValue)
    {
      hiValue = v;
      hi = i;
    }
  }
  // a bucket that ends where the meter fell silent: the gap goes on to the
  // next reading, in a later bucket - the line must not bridge it (a sensor
  // silent for minutes was drawn as a straight line whenever its last value
  // and the gap fell into one column). An overload comes at the meter's
  // pace and is as short as a reading.
  const bool gapAfter = std::isnan(value(last)) && series.at(last).quality == Quality::Stale;
  int n = 1;
  if (lo == hi)
    out.append(QPointF(x(lo), loValue));
  else
  {
    if (lo > hi)
    {
      qSwap(lo, hi);
      qSwap(loValue, hiValue);
    }
    out.append(QPointF(x(lo), loValue));
    out.append(QPointF(x(hi), hiValue));
    n = 2;
  }
  if (gapAfter)
  {
    out.append(QPointF(x(last), qQNaN()));
    n++;
  }
  return n;
}

int SeriesDecimator::append(const RecordingSeries &series, int first, int last, qint64 bucket, QList<QPointF> &out,
                            const Options &options)
{
  int tail = 0;
  while (first <= last)
  {
    const qint64 end = (series.at(first).t / bucket + 1) * bucket;
    const int bucketLast = qMin(last, series.lowerBound(end) - 1);
    tail = pointsOf(series, first, bucketLast, out, options);
    first = bucketLast + 1;
  }
  return tail;
}
