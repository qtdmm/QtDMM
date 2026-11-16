// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphband.h"

#include <algorithm>
#include <cmath>

namespace
{
struct Column
{
  double lo = qInf(), hi = -qInf();
  int run = -1;   ///< which stretch between gaps; -1 no point
};

struct Columns
{
  QList<Column> col;
  double first = 0;       ///< the first column, counted from x = 0
  double perColumn = 0;
};

// The lowest and highest of the curve as drawn in each column of a pixel
// over the plot and k beyond each edge, so the band runs out of the plot as
// the curve does. The curve has a point for every bucket, its minimum and
// maximum, so this reads a few points per column, not the readings.
Columns columnsOf(const QList<QPointF> &pts, double x0, double x1, double width, int k)
{
  Columns out;
  if (pts.isEmpty() || width < 2 || !(x1 > x0))
    return out;
  out.perColumn = (x1 - x0) / width;
  out.first = std::floor(x0 / out.perColumn) - k;
  const int columns = int(width) + 2 * k + 2;
  const double perColumn = out.perColumn, from = out.first * perColumn, to = from + columns * perColumn;
  out.col.resize(columns);
  auto take = [&](int c, double lo, double hi, int r)
  {
    Column &cc = out.col[c];
    cc.lo = qMin(cc.lo, lo);
    cc.hi = qMax(cc.hi, hi);
    cc.run = r;
  };
  auto columnOf = [&](double x) { return qBound(0, int((x - from) / perColumn), columns - 1); };
  // the point before the window too: its line runs into it
  auto it = std::lower_bound(pts.cbegin(), pts.cend(), from, [](const QPointF &p, double x) { return p.x() < x; });
  if (it != pts.cbegin())
    --it;
  int run = 0;
  const QPointF *prev = nullptr;
  for (; it != pts.cend(); ++it)
  {
    if (std::isnan(it->y()))
    {
      // a gap parts the curve and the band
      if (it->x() > to)
        break;
      run++;
      prev = nullptr;
      continue;
    }
    if (it->x() >= from)
    {
      const int c = columnOf(it->x());
      take(c, it->y(), it->y(), run);
      // the line from the point before: in each column it crosses, the part of it there
      if (prev)
        for (int j = columnOf(prev->x()); j <= c; ++j)
        {
          const double xa = qMax(prev->x(), from + j * perColumn), xb = qMin(it->x(), from + (j + 1) * perColumn);
          const double dx = it->x() - prev->x();
          auto yAt = [&](double x) { return dx > 0 ? prev->y() + (it->y() - prev->y()) * (x - prev->x()) / dx : it->y(); };
          take(j, qMin(yAt(xa), yAt(xb)), qMax(yAt(xa), yAt(xb)), run);
        }
    }
    if (it->x() > to)
      break;
    prev = &*it;
  }
  return out;
}

// column c with the extremes of its neighbours within k on its side of a gap
void widened(const QList<Column> &col, int c, int k, double &lo, double &hi)
{
  lo = col[c].lo;
  hi = col[c].hi;
  for (int j = qMax(0, c - k); j <= qMin(int(col.size()) - 1, c + k); ++j)
    if (col[j].run == col[c].run)
    {
      lo = qMin(lo, col[j].lo);
      hi = qMax(hi, col[j].hi);
    }
}
}

namespace GraphBand
{
QList<QPolygonF> columnBand(const QList<QPointF> &points, double x0, double x1, double y0, double y1, const QRectF &plot, int k)
{
  QList<QPolygonF> out;
  if (plot.height() <= 0 || !(y1 > y0))
    return out;
  const Columns columns = columnsOf(points, x0, x1, plot.width(), k);
  const QList<Column> &col = columns.col;
  auto yOf = [&](double v) { return plot.bottom() - (v - y0) / (y1 - y0) * plot.height(); };
  auto xOf = [&](int c) { return plot.left() + (columns.first + c + 0.5) - x0 / columns.perColumn; };
  QList<QPointF> upper, lower;
  auto flush = [&]
  {
    if (upper.size() > 1)
    {
      QPolygonF poly(upper);
      for (qsizetype i = lower.size() - 1; i >= 0; --i)
        poly << lower[i];
      poly << upper.first();
      out << poly;
    }
    upper.clear();
    lower.clear();
  };
  int lastRun = -1;
  for (int c = 0; c < col.size(); ++c)
  {
    if (col[c].run < 0)
      continue;
    if (col[c].run != lastRun)
      flush();
    lastRun = col[c].run;
    double lo = 0, hi = 0;
    widened(col, c, k, lo, hi);
    upper << QPointF(xOf(c), yOf(hi));
    lower << QPointF(xOf(c), yOf(lo));
  }
  flush();
  return out;
}

bool columnAt(const QList<QPointF> &points, double x0, double x1, const QRectF &plot, int k, double x, double *lo, double *hi)
{
  const Columns columns = columnsOf(points, x0, x1, plot.width(), k);
  if (columns.col.isEmpty())
    return false;
  const int c = int(std::floor(x / columns.perColumn) - columns.first);
  if (c < 0 || c >= columns.col.size() || columns.col[c].run < 0)
    return false;
  double low = 0, high = 0;
  widened(columns.col, c, k, low, high);
  if (lo)
    *lo = low;
  if (hi)
    *hi = high;
  return true;
}
}
