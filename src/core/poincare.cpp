// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/poincare.h"

#include <cmath>

#include "core/siprefix.h"

namespace
{
const double kSqrt2 = std::sqrt(2.0);

// 1, 2 or 5 times a power of ten, at least @p v
double niceStep(double v)
{
  if (!(v > 0) || !std::isfinite(v))
    return 1;
  const double decade = std::pow(10.0, std::floor(std::log10(v)));
  for (double m : { 1.0, 2.0, 5.0, 10.0 })
    if (m * decade >= v * (1 - 1e-12))
      return m * decade;
  return 10 * decade;
}

// one digit of the display text: "1.234" with the prefix "m" is 1e-6
double digitOf(const Reading &r)
{
  const int dot = int(r.text.indexOf(QLatin1Char('.')));
  int decimals = 0;
  if (dot >= 0)
    while (dot + 1 + decimals < r.text.size() && r.text.at(dot + 1 + decimals).isDigit())
      ++decimals;
  return std::pow(10.0, -decimals) * SiPrefix::factor(r.prefix);
}
}

bool PoincareSeries::feed(const Reading &r)
{
  bool reset = false;
  if (!r.overload && r.port.quantity != Quantity::Unknown)
  {
    if (m_port.isValid() && (r.port != m_port || r.baseUnit != m_baseUnit))
    {
      clear();
      reset = true;
    }
    m_port = r.port;
    m_baseUnit = r.baseUnit;
    m_unit = r.portUnit.base;
  }
  // an overload has no value, a held display shows an old one again
  if (r.overload || r.hold || !std::isfinite(r.value))
  {
    gap();
    return reset;
  }
  if (m_broken)
  {
    m_run++;
    m_broken = false;
  }
  m_values.append({ r.value, m_run });
  m_resolution = digitOf(r);
  if (m_values.size() > m_capacity)
    m_values.remove(0, m_values.size() - m_capacity);
  return reset;
}

void PoincareSeries::gap()
{
  m_broken = !m_values.isEmpty();
}

void PoincareSeries::clear()
{
  m_values.clear();
  m_broken = false;
  m_port = PortKey();
  m_baseUnit.clear();
  m_resolution = 0;
}

void PoincareSeries::setCapacity(int values)
{
  m_capacity = qMax(2, values);
  if (m_values.size() > m_capacity)
    m_values.remove(0, m_values.size() - m_capacity);
}

void PoincareSeries::setLag(int k)
{
  m_lag = qBound(1, k, kMaxLag);
}

QVector<PoincareSeries::Pair> PoincareSeries::pairs() const
{
  QVector<Pair> out;
  const int n = int(m_values.size());
  out.reserve(qMax(0, n - m_lag));
  for (int i = 0; i + m_lag < n; ++i)
    if (m_values[i].run == m_values[i + m_lag].run)
      out.append({ m_values[i].v, m_values[i + m_lag].v, 0 });
  for (int i = 0; i < out.size(); ++i)
    out[i].age = int(out.size()) - 1 - i;
  return out;
}

PoincareSeries::Stats PoincareSeries::stats() const
{
  Stats s;
  const QVector<Pair> p = pairs();
  s.count = int(p.size());
  if (s.count == 0)
  {
    s.meanX = s.meanY = s.sd1 = s.sd2 = qQNaN();
    return s;
  }
  // in the coordinates turned by 45°: across (y - x) and along (y + x)
  double sumX = 0, sumY = 0;
  for (const Pair &q : p)
  {
    sumX += q.x;
    sumY += q.y;
  }
  s.meanX = sumX / s.count;
  s.meanY = sumY / s.count;
  if (s.count < 2)
  {
    s.sd1 = s.sd2 = qQNaN();
    return s;
  }
  const double meanD = (s.meanY - s.meanX) / kSqrt2, meanS = (s.meanY + s.meanX) / kSqrt2;
  double varD = 0, varS = 0;
  for (const Pair &q : p)
  {
    const double d = (q.y - q.x) / kSqrt2 - meanD, a = (q.y + q.x) / kSqrt2 - meanS;
    varD += d * d;
    varS += a * a;
  }
  s.sd1 = std::sqrt(varD / (s.count - 1));
  s.sd2 = std::sqrt(varS / (s.count - 1));
  return s;
}

PoincareSeries::Axis PoincareSeries::axis(double lo, double hi, double resolution)
{
  double span = hi - lo;
  // all alike: not a span of 1e-12 (pV at a meter showing 0.000 V) but
  // the digits the meter shows
  if (!(span > 0))
    span = resolution > 0 ? 10 * resolution : hi != 0 ? std::abs(hi) * 1e-3 : 1;
  Axis a;
  a.step = niceStep(span * 1.16 / 5);
  a.lo = std::floor((lo - span * 0.08) / a.step) * a.step;
  a.hi = std::ceil((hi + span * 0.08) / a.step) * a.step;
  return a;
}
