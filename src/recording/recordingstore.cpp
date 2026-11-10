// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/recordingstore.h"

#include <algorithm>

#include <QCoreApplication>
#include <QJsonArray>
#include <cmath>
#include <vector>

#include "core/siprefix.h"

RecordingStore::RecordingStore(QObject *parent) :
  QObject(parent),
  m_monotonic(&Sample::now),
  m_wall([] { return QDateTime::currentDateTime(); })
{
  m_start = m_wall();
  m_t0 = m_monotonic();
  m_poll.setInterval(1000);
  connect(&m_poll, &QTimer::timeout, this, &RecordingStore::poll);
  m_poll.start();
}

void RecordingStore::setClock(std::function<qint64()> monotonic, std::function<QDateTime()> wall)
{
  m_monotonic = std::move(monotonic);
  m_wall = std::move(wall);
}

void RecordingStore::setSampleTime(int tenths)
{
  if (tenths >= 1)
    m_sampleTime = tenths;
}

void RecordingStore::setLiveWidth(int seconds)
{
  m_liveWidthMs = qint64(qMax(1, seconds)) * 1000;
  if (m_state == Live)
    trimForLimits();
}

void RecordingStore::setMaxPoints(int points)
{
  m_maxPoints = qMax(2, points);
  trimForLimits();
}

void RecordingStore::setThresholds(double falling, double raising)
{
  m_fallingThreshold = falling;
  m_raisingThreshold = raising;
}

qint64 RecordingStore::duration() const
{
  if (m_state != View)
    return elapsed();
  if (m_stopT >= 0)
    return m_stopT;
  return m_series.isEmpty() ? 0 : m_series.last().t;
}

RecordingStore::Stats RecordingStore::statsOf(const RecordingSeries &series, qint64 from, qint64 to) const
{
  Stats st;
  if (from > to)
    std::swap(from, to);
  to = qMin(to, duration());
  const int n = series.count();
  // a loaded file ends at its last reading, which then holds for no time:
  // it still counts (count, min, max). A file on its own grid has each row
  // as the mean over the step before it, so a row weighs that step, and the
  // first row (its step lies before the file) holds no time either
  const bool loaded = m_state == View && m_stopT < 0;
  const qint64 step = loaded ? qint64(m_loadedGrid) * 100 : 0;
  double sum = 0;
  // σ from the values less the first one: E[v²] - mean² of large values
  // with a small spread loses its digits (3 V steady gave 42 nV)
  double ref = qQNaN();
  double sumD = 0, sumD2 = 0;   // of the readings, for a file of no time
  double intD = 0, intD2 = 0;   // (v - ref) x s and (v - ref)² x s
  for (int i = qMax(0, series.holding(from)); i < n; ++i)
  {
    const RawPoint &p = series.at(i);
    qint64 begin = 0;
    qint64 end = 0;
    bool counts = false;
    if (step > 0)
    {
      if (p.t - step >= to && i > 0)
        break;
      begin = qMax(from, p.t - step);
      end = qMin(to, p.t);
      counts = i == 0 && p.t >= from && p.t <= to;
    }
    else
    {
      if (p.t >= to && !(loaded && i == n - 1 && p.t == to))
        break;
      begin = qMax(from, p.t);
      end = qMin(to, i + 1 < n ? series.at(i + 1).t : duration());
      counts = loaded && i == n - 1 && p.t >= from;
    }
    if (p.gap() || (end <= begin && !counts))
      continue;
    st.count++;
    st.min = st.count == 1 ? p.value : qMin(st.min, p.value);
    st.max = st.count == 1 ? p.value : qMax(st.max, p.value);
    sum += p.value;
    if (std::isnan(ref))
      ref = p.value;
    const double d = p.value - ref;
    sumD += d;
    sumD2 += d * d;
    if (end > begin)
    {
      st.integral += p.value * double(end - begin) / 1000;
      intD += d * double(end - begin) / 1000;
      intD2 += d * d * double(end - begin) / 1000;
      st.covered += end - begin;
    }
  }
  if (st.covered > 0)
  {
    st.mean = st.integral * 1000 / double(st.covered);
    const double m = intD * 1000 / double(st.covered);
    st.sigma = std::sqrt(qMax(0.0, intD2 * 1000 / double(st.covered) - m * m));
  }
  else if (st.count > 0)
  {
    st.mean = sum / st.count; // readings that held for no time (a file of one row)
    const double m = sumD / st.count;
    st.sigma = std::sqrt(qMax(0.0, sumD2 / st.count - m * m));
  }
  return st;
}

qint64 RecordingStore::origin() const
{
  // Live runs through what it keeps (an hour); a recording keeps all of it, unless it
  // outgrew the most points
  if (m_state == Live)
    return qMax<qint64>(0, duration() - liveKept());
  return m_floorMs;
}

qint64 RecordingStore::liveKept() const
{
  // without a recording length not the window: switching to 1 min and back
  // to All would have lost what was before
  return m_sampleLength > 0 ? liveWindow() : qMax(kLiveMaxMs, liveWindow());
}

qint64 RecordingStore::liveWindow() const
{
  // as wide as a recording will be, so the picture does not jump at the start
  // until stopped: as wide as the graph shows, at most an hour (with All
  // the window grows with what there is, Live with it)
  const qint64 length = m_sampleLength > 0 ? qint64(m_sampleLength) * 100 : qMin(m_liveWidthMs, kLiveMaxMs);
  return qMax<qint64>(length, m_preMs);
}

qint64 RecordingStore::lengthLimit() const
{
  switch (m_state)
  {
    case Live:   return m_sampleLength > 0 ? liveWindow() : qMax<qint64>(kLiveMaxMs, m_preMs);
    case Record: return m_sampleLength > 0 ? qint64(m_sampleLength) * 100 + m_preUsed : 0;
    default:     return duration() - origin();
  }
}

void RecordingStore::setSampleLength(int tenths)
{
  m_sampleLength = tenths;
  if (m_state == Live)
    trimForLimits();
}

int RecordingStore::remainingLength() const
{
  if (m_state != Record || m_sampleLength <= 0)
    return 0;
  return int(qMax<qint64>(0, m_sampleLength - (duration() - m_preUsed) / 100));
}

void RecordingStore::setPreTrigger(int ms)
{
  m_preMs = qMax(0, ms);
  if (m_state == Live)
    trimForLimits();
}

QVector<GridPoint> RecordingStore::grid(int tenths) const
{
  return gridOf(m_series, tenths);
}

QVector<GridPoint> RecordingStore::gridOf(const RecordingSeries &series, int tenths) const
{
  QVector<GridPoint> out;
  const int n = series.count();
  // a loaded file on this grid: its rows are the steps already (each one the
  // mean over the step before it), holding them on would shift them by one
  if (m_loadedGrid > 0 && m_loadedGrid == tenths)
  {
    out.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      const RawPoint &p = series.at(i);
      out.append({ p.t, p.value, p.integral, p.quality, p.flags & (SampleFlag::Limit | SampleFlag::State) });
    }
    return out;
  }

  const qint64 step = qint64(qMax(1, tenths)) * 100;
  const qint64 end = duration();
  const qint64 kFirst = (origin() + step - 1) / step;
  const qint64 kLast = end / step;
  if (kLast < kFirst)
    return out;
  out.reserve(int(qMin<qint64>(kLast - kFirst + 1, 10000000)));

  // the worse of two qualities: an overload says more than a fault, a fault
  // more than no number shown, that more than a gap
  auto worse = [](Quality a, Quality b)
  {
    for (Quality q : { Quality::Overload, Quality::Fault, Quality::NoValue, Quality::Stale })
      if (a == q || b == q)
        return q;
    return Quality::Valid;
  };
  // the value at the moment t: the last reading before it (one that comes
  // at t counts from t on), the first one at the start
  auto before = [&](qint64 t)
  {
    const int j = series.lowerBound(t);
    return j > 0 ? j - 1 : j < n && series.at(j).t == t ? j : -1;
  };
  // the integral at time t: the last point's, plus its value since then
  auto integralAt = [&](qint64 t)
  {
    const int j = before(t);
    if (j < 0 || series.at(j).gap())
      return qQNaN();
    const RawPoint &p = series.at(j);
    return p.value > m_integrationThreshold ? p.integral + p.value * double(t - p.t) / 1000.0 : p.integral;
  };

  for (qint64 k = kFirst; k <= kLast; ++k)
  {
    GridPoint g;
    g.t = k * step;
    g.integral = integralAt(g.t);
    if (k == 0)
    {
      // the first step is the value at the start
      const int j = before(0);
      g.value = j < 0 ? qQNaN() : series.at(j).value;
      g.quality = j < 0 ? Quality::Stale : series.at(j).quality;
      g.limit = j < 0 ? 0 : series.at(j).flags & (SampleFlag::Limit | SampleFlag::State);
    }
    else
    {
      // the mean over (t - step, t], each value weighted with how long it held
      const qint64 a = g.t - step, b = g.t;
      double sum = 0, held = 0;
      Quality q = Quality::Valid;
      double overload = qQNaN();   // the last one in the step, with its sign
      int j = series.holding(a);
      if (j < 0)
      {
        q = Quality::Stale;   // before the first reading: no value
        j = 0;
      }
      for (; j < n && series.at(j).t < b; ++j)
      {
        const RawPoint &p = series.at(j);
        const qint64 until = j + 1 < n ? series.at(j + 1).t : qMax(end, p.t);
        const qint64 from = qMax(p.t, a), to = qMin(until, b);
        if (to <= from)
          continue;
        g.limit |= p.flags & (SampleFlag::Limit | SampleFlag::State);   // a FAIL in the step is not hidden by a PASS
        if (p.gap())
        {
          q = worse(q, p.quality == Quality::Valid ? Quality::Stale : p.quality);
          if (p.quality == Quality::Overload)
            overload = p.value;
        }
        else
        {
          sum += p.value * double(to - from);
          held += double(to - from);
        }
      }
      g.value = held > 0 ? sum / held : q == Quality::Overload ? overload : qQNaN();
      // no value without a reason: the main value went stale
      g.quality = held > 0 || q != Quality::Valid ? q : Quality::Stale;
    }
    out.append(g);
  }
  return out;
}

namespace
{
// what the file says of a point: a number, OL or -OL (the NaN's sign), or a gap
CellState cellState(double value, Quality quality)
{
  if (std::isfinite(value))
    return CellState::Ok;
  if (quality == Quality::Overload)
    return std::signbit(value) ? CellState::NegativeOverload : CellState::Overload;
  if (quality == Quality::NoValue)
    return CellState::NoValue;
  if (quality == Quality::Fault)
    return CellState::Fault;
  return CellState::Gap;
}
}

qint64 RecordingStore::offsetOf(const RecordingStore &other) const
{
  if (m_fromFile || other.m_fromFile)
    return m_start.msecsTo(other.m_start);
  return other.m_t0 - m_t0;
}

Recording RecordingStore::toRecording(bool raw, const QList<int> &extras, const QList<Foreign> &foreign,
                                       const QString &device, const QJsonObject &info) const
{
  Recording rec;
  rec.meta = m_fileInfo;
  for (auto it = info.begin(); it != info.end(); ++it)
    rec.meta[it.key()] = it.value();
  rec.sampleTimeTenths = m_sampleTime;
  rec.unit = m_unit;
  rec.device = device;
  QList<const RecordingSeries *> others;
  QStringList devices;   // of each of others
  for (int e : extras)
    if (e >= 0 && e < m_extra.size() && !m_extra[e].isEmpty())
    {
      others << &m_extra[e];
      devices << device;
    }
  // another meter's series on this time, as if they were this one's
  std::vector<RecordingSeries> shifted;
  for (const Foreign &f : foreign)
  {
    if (!f.store)
      continue;
    const qint64 offset = offsetOf(*f.store);
    auto add = [&](const RecordingSeries &s, const QString &unit)
    {
      if (s.isEmpty())
        return;
      RecordingSeries copy;
      copy.id = f.id;
      copy.unit = unit;
      copy.reserve(s.count());
      for (int i = 0; i < s.count(); ++i)
      {
        RawPoint p = s.at(i);
        p.t = qMax<qint64>(0, p.t + offset);
        copy.append(p);
      }
      shifted.push_back(std::move(copy));
      devices << f.device;
    };
    if (f.id == 0)
      add(f.store->series(), f.store->unit());
    else
      for (const RecordingSeries &s : std::as_const(f.store->m_extra))
        if (s.id == f.id)
          add(s, s.unit);
  }
  for (const RecordingSeries &s : shifted)
    others << &s;
  rec.extra.resize(others.size());
  for (int c = 0; c < others.size(); ++c)
  {
    rec.extra[c].id = others[c]->id;
    rec.extra[c].unit = others[c]->unit;
    rec.extra[c].device = devices[c];
  }
  if (raw)
  {
    // from the reading that holds at the origin on, a row at every reading
    // of any of the values; each value holds until its next one
    const int n = m_series.count();
    const int first = qMax(0, m_series.holding(origin()));
    const qint64 t0 = first < n ? m_series.at(first).t : 0;
    rec.start = m_start.addMSecs(t0);
    if (n == 0)
      return rec;
    QVector<qint64> times;
    times.reserve(n - first);
    for (int i = first; i < n; ++i)
      times << m_series.at(i).t;
    for (const RecordingSeries *s : std::as_const(others))
    {
      QVector<qint64> merged;
      merged.reserve(times.size() + s->count());
      int i = 0, j = qMax(0, s->lowerBound(t0));
      while (i < times.size() || j < s->count())
      {
        const qint64 t = j >= s->count() || (i < times.size() && times[i] <= s->at(j).t) ? times[i] : s->at(j).t;
        merged << t;
        while (i < times.size() && times[i] == t)
          ++i;
        while (j < s->count() && s->at(j).t == t)
          ++j;
      }
      times = merged;
    }
    rec.values.reserve(times.size());
    rec.states.reserve(times.size());
    rec.times.reserve(times.size());
    for (qint64 t : std::as_const(times))
    {
      // the last of the readings at this time
      const RawPoint &p = m_series.at(qMax(0, m_series.holding(t)));
      rec.values << p.value;
      rec.states << cellState(p.value, p.quality);
      rec.limits << (p.flags & (SampleFlag::Limit | SampleFlag::State));
      rec.times << t - t0;
      for (int c = 0; c < others.size(); ++c)
      {
        const int j = others[c]->holding(t);
        rec.extra[c].values << (j < 0 ? qQNaN() : others[c]->at(j).value);
        rec.extra[c].states << (j < 0 ? CellState::None : cellState(others[c]->at(j).value, others[c]->at(j).quality));
        rec.extra[c].limits << (j < 0 ? 0u : others[c]->at(j).flags & (SampleFlag::Limit | SampleFlag::State));
      }
    }
    return rec;
  }
  const QVector<GridPoint> steps = grid(m_sampleTime);
  rec.start = m_start.addMSecs(steps.isEmpty() ? 0 : steps.first().t);
  rec.values.reserve(steps.size());
  rec.states.reserve(steps.size());
  for (const GridPoint &g : steps)
  {
    rec.values << g.value;
    rec.states << cellState(g.value, g.quality);
    rec.limits << g.limit;
  }
  // the other values on the same steps; a step before one began (all of it
  // before its first reading) has none, nor has a row a loaded file had
  // without it (gridOf() gives its rows as they were)
  const bool asLoaded = m_loadedGrid > 0 && m_loadedGrid == m_sampleTime;
  for (int c = 0; c < others.size(); ++c)
  {
    const RecordingSeries &s = *others[c];
    const QVector<GridPoint> own = gridOf(s, m_sampleTime);
    RecordingColumn &col = rec.extra[c];
    col.values.reserve(steps.size());
    col.states.reserve(steps.size());
    int j = 0;
    for (const GridPoint &g : steps)
    {
      while (j < own.size() && own[j].t < g.t)
        ++j;
      // the step ends at g.t: one that ends when the value comes has none
      // of it (but the first, the value at the start)
      const bool begun = asLoaded || s.first().t < g.t || s.first().t == 0;
      const bool there = begun && j < own.size() && own[j].t == g.t;
      col.values << (there ? own[j].value : qQNaN());
      col.states << (there ? cellState(own[j].value, own[j].quality) : CellState::None);
      col.limits << (there ? own[j].limit : 0u);
    }
  }
  // what the columns do not keep, for a QtDMM recording
  QJsonArray marks;
  for (const Mark &m : m_marks)
  {
    QJsonObject o{ { "t", double(m.t) }, { "color", QString("#%1").arg(m.color, 8, 16, QChar('0')) }, { "name", m.name } };
    // an alarm's mark as before: older QtDMM read it the same
    if (!m.note.isEmpty())
      o["note"] = m.note;
    if (m.kind != MarkKind::Alarm)
      o["kind"] = m.kind == MarkKind::Note ? "note" : "trigger";
    marks.append(o);
  }
  if (!marks.isEmpty())
    rec.meta["marks"] = marks;
  return rec;
}

bool RecordingStore::write(const QString &path, QString *error, bool raw, const QList<int> &extras,
                           const QList<Foreign> &foreign, const QString &device, const QJsonObject &info)
{
  if (!RecordingFile::writeAny(toRecording(raw, extras, foreign, device, info), path, error))
    return false;
  m_dirty = false;
  for (const Foreign &f : foreign)
    if (f.store)
      f.store->m_dirty = false;
  return true;
}

void RecordingStore::load(const Recording &rec)
{
  const State was = m_state;
  m_state = View;
  m_start = rec.start;
  m_unit = rec.unit;
  m_unitPending = false;
  m_recordPort = PortKey();
  m_sampleTime = qMax(1, rec.sampleTimeTenths);
  m_series.clear();
  m_extra.clear();
  m_floorMs = 0;
  m_series.reserve(int(rec.values.size()));
  m_marks.clear();
  const bool onGrid = rec.onGrid();
  auto quality = [](CellState state)
  {
    switch (state)
    {
      case CellState::Ok:               return Quality::Valid;
      case CellState::Overload:
      case CellState::NegativeOverload: return Quality::Overload;
      case CellState::NoValue:          return Quality::NoValue;
      case CellState::Fault:            return Quality::Fault;
      default:                          return Quality::Stale;
    }
  };
  for (int i = 0; i < rec.values.size(); i++)
  {
    // another meter's part of a file has rows before it began
    if (rec.stateAt(i) == CellState::None)
      continue;
    RawPoint p;
    p.t = onGrid ? qint64(i) * m_sampleTime * 100 : rec.timeAt(i);
    p.value = rec.values[i];
    p.quality = quality(rec.stateAt(i));
    p.flags = rec.limits.value(i);
    appendPoint(p);
  }
  // the other values, a series each (value 2 is id 1 ...), a point in every
  // row they are in
  for (int c = 0; c < rec.extra.size(); ++c)
  {
    const RecordingColumn &col = rec.extra[c];
    RecordingSeries s;
    s.id = col.id > 0 ? col.id : c + 1;
    s.unit = col.unit;
    m_extra << s;
    for (int i = 0; i < rec.values.size() && i < col.states.size(); i++)
      if (col.states[i] != CellState::None)
      {
        RawPoint p;
        p.t = onGrid ? qint64(i) * m_sampleTime * 100 : rec.timeAt(i);
        p.value = col.values.value(i, qQNaN());
        p.quality = quality(col.states[i]);
        p.flags = col.limits.value(i);
        appendExtraPoint(int(m_extra.size()) - 1, p);
      }
    if (m_extra.last().isEmpty())
      m_extra.removeLast();
  }
  // a file on its own grid: each row is already the mean over the step
  // before it
  m_loadedGrid = onGrid ? m_sampleTime : 0;
  m_fromFile = true;
  m_stopT = -1;
  m_dirty = false;
  // a QtDMM recording brings its marks back, and keeps what it says of its
  // devices for the next export
  m_fileInfo = rec.meta;
  for (const char *written : { "marks", "format", "qtdmm", "written", "start", "end", "sample-time" })
    m_fileInfo.remove(QLatin1String(written));
  for (const QJsonValue &v : rec.meta.value("marks").toArray())
  {
    const QJsonObject m = v.toObject();
    const QString kind = m.value("kind").toString();
    m_marks.append(Mark{ qint64(m.value("t").toDouble()), m.value("color").toString().mid(1).toUInt(nullptr, 16),
                         m.value("name").toString(), m.value("note").toString(),
                         kind == "note" ? MarkKind::Note : kind == "trigger" ? MarkKind::Trigger : MarkKind::Alarm });
  }
  Q_EMIT marksChanged();
  Q_EMIT loaded();
  if (was != View)
  {
    Q_EMIT stateChanged(View);
    if (was == Record)
      Q_EMIT runningChanged(false);
  }
}

// "Voltage DC (V)", "Temperature (°F)": what a recording measures, for the UI
QString RecordingStore::describe(const PortKey &port, const QString &baseUnit)
{
  QString text = Quantities::name(port.quantity);
  if (port.defining & (SampleFlag::AC | SampleFlag::DC))
    text += ' ' + couplingText(port.defining);
  const QString unit = SiPrefix::displayText(baseUnit);
  if (!unit.isEmpty())
    text += QString(" (%1)").arg(unit);
  return text;
}

void RecordingStore::setUnit(const QString &baseUnit)
{
  // the readings keep the unit they were recorded in: switching the meter
  // from V to Ohm after a recording must not relabel it. Live follows the
  // meter: another function starts its window anew anyway
  if (count() > 0 && m_state != Live)
  {
    m_nextUnit = baseUnit;
    m_unitPending = baseUnit != m_unit;
  }
  else
  {
    m_unit = baseUnit;
    m_unitPending = false;
  }
}

void RecordingStore::setReading(const Reading &reading)
{
  logReading(reading);
  if (reading.id != 0)
  {
    appendExtra(reading);
    // a trigger on this value crosses with its point in the window (pre-trigger)
    if (m_state == Live && m_triggerValue.matches(reading))
      trigger(reading.text.isEmpty() ? qQNaN() : reading.overload ? overloadValue(reading.value) : reading.value);
    return;
  }
  m_reading = reading;
  m_haveReading = true;
  // an overload's NaN keeps the sign of -OL (ReadingAdapter), for the export
  const double value = reading.overload ? overloadValue(reading.value) : reading.value;

  // one recording, one quantity: the first value says which; another port
  // (V DC -> Ohm, DC -> AC) or another unit (°C -> °F) stops it. A prefix
  // (mV -> V) is no change, and an overload, no number ("----", "FUSE") or a
  // value without a known quantity says nothing about the function. Live starts its window anew:
  // a trigger compares within one function (0 V, then 1000 Ohm crosses
  // nothing), and the pre-trigger takes no other one into a recording
  if (m_state != View && !reading.overload && !reading.noNumber() && reading.port.quantity != Quantity::Unknown)
  {
    if (!m_recordPort.isValid())
    {
      m_recordPort = reading.port;
      m_recordBaseUnit = reading.baseUnit;
      m_series.port = reading.port;
      m_series.unit = reading.baseUnit;
      m_series.name = reading.name;
    }
    else if (reading.port != m_recordPort || reading.baseUnit != m_recordBaseUnit)
    {
      if (m_state == Record)
      {
        const QString from = describe(m_recordPort, m_recordBaseUnit);
        stop();
        Q_EMIT functionChanged(from, describe(reading.port, reading.baseUnit));
        return;
      }
      clear();
      m_lastValValid = false;
      m_recordPort = reading.port;
      m_recordBaseUnit = reading.baseUnit;
      m_series.port = reading.port;
      m_series.unit = reading.baseUnit;
      m_series.name = reading.name;
    }
  }
  if (m_state == View)
    return;
  closeSilentExtras(reading.t);

  // the time starts with the first reading, not when the graph was cleared
  // or the meter was still connecting: the curve begins at 0
  if (m_series.isEmpty() && m_marks.isEmpty() && !m_holdOrigin && reading.t > m_t0)
  {
    m_start = m_start.addMSecs(reading.t - m_t0);
    m_t0 = reading.t;
    Q_EMIT progressChanged();
  }

  RawPoint p;
  p.t = qMax<qint64>(0, reading.t - m_t0);
  p.value = value;
  p.quality = pointQuality(reading);
  p.flags = reading.flags;
  if (m_state == Live)
  {
    // into the window first: the pre-trigger takes the crossing reading along
    append(p);
    if (m_triggerValue.matches(reading))
      trigger(value);
    return;
  }
  if (!lengthReached(p.t))
    append(p);
}

int RecordingStore::extraIndex(int id) const
{
  for (int i = int(m_extra.size()) - 1; i >= 0; --i)
    if (m_extra[i].id == id)
      return i;
  return -1;
}

void RecordingStore::appendExtraPoint(int i, RawPoint p)
{
  RecordingSeries &s = m_extra[i];
  // the integral as for the main value (appendPoint()): the graph can draw
  // it for a value other than the main one
  double run = s.runningIntegral;
  if (!s.isEmpty())
  {
    const RawPoint &prev = s.last();
    p.t = qMax(p.t, prev.t);
    if (!prev.gap() && prev.value > m_integrationThreshold)
      run += prev.value * double(p.t - prev.t) / 1000.0;
  }
  else
    run = 0;
  if (!p.gap() && p.value <= m_integrationThreshold)
    run = 0;
  s.runningIntegral = run;
  p.integral = p.gap() ? qQNaN() : run;
  s.append(p);
  if (m_state == Record)
    m_dirty = true;
}

// A value that stops coming while the main one goes on (a second display
// whose frames end without an "off") is held up to the stale limit, as the
// main value is when the meter falls silent (setStale()), no longer.
void RecordingStore::closeSilentExtras(qint64 now)
{
  bool closed = false;
  for (auto it = m_extraReading.cbegin(); it != m_extraReading.cend(); ++it)
  {
    if (now - it->t <= m_staleMs)
      continue;
    const int i = extraIndex(it.key());
    if (i < 0 || m_extra[i].isEmpty() || m_extra[i].last().gap())
      continue;
    // never after now: a value taken along at the start (appendCurrent())
    // stands at 0 however old it was
    const RawPoint &last = m_extra[i].last();
    RawPoint p;
    p.t = qMin(last.t + m_staleMs, qMax(last.t, now - m_t0));
    p.value = qQNaN();
    p.quality = Quality::Stale;
    appendExtraPoint(i, p);
    closed = true;
  }
  if (closed)
    Q_EMIT extrasChanged();
}

void RecordingStore::appendExtra(const Reading &reading)
{
  m_extraReading[reading.id] = reading;
  // the time counts from the main value's first reading: before it there is
  // no axis to put this one on
  if (m_state == View || m_series.isEmpty())
    return;

  // a second display that is off sends an empty value: a gap
  const bool off = reading.text.isEmpty();
  RawPoint p;
  p.t = qMax<qint64>(0, reading.t - m_t0);
  p.value = off ? qQNaN() : reading.overload ? overloadValue(reading.value) : reading.value;
  p.quality = off ? Quality::Stale : pointQuality(reading);
  p.flags = reading.flags;
  // the main value stops the recording at its length; nothing after it
  if (m_state == Record && m_sampleLength > 0 && p.t >= qint64(m_sampleLength) * 100 + m_preUsed)
    return;

  // what the value measures: another port or unit ends its series and
  // begins a new one (Hz -> %); an overload or a value without a known
  // quantity says nothing about it
  const bool says = !off && !reading.overload && reading.port.quantity != Quantity::Unknown;
  int i = extraIndex(reading.id);
  bool began = false;
  if (i >= 0 && says && m_extra[i].port.isValid()
      && (reading.port != m_extra[i].port || reading.baseUnit != m_extra[i].unit))
  {
    if (!m_extra[i].last().gap())
    {
      RawPoint end;
      end.t = p.t;
      end.value = qQNaN();
      end.quality = Quality::Stale;
      appendExtraPoint(i, end);
    }
    i = -1;
  }
  if (i < 0)
  {
    if (off)
      return;   // nothing to end, nothing to begin
    // back to what the value measured before (Hz, %, Hz ...): that series
    // goes on after its gap and becomes the newest, so a display switched
    // back and forth makes no new series each time
    int earlier = -1;
    for (int j = int(m_extra.size()) - 1; j >= 0 && says; --j)
      if (m_extra[j].id == reading.id && m_extra[j].port == reading.port && m_extra[j].unit == reading.baseUnit)
      {
        earlier = j;
        break;
      }
    if (earlier >= 0)
      m_extra.move(earlier, m_extra.size() - 1);
    else
    {
      RecordingSeries s;
      s.id = reading.id;
      m_extra.append(s);
    }
    i = int(m_extra.size()) - 1;
    began = true;
  }
  if (says && !m_extra[i].port.isValid())
  {
    m_extra[i].port = reading.port;
    m_extra[i].unit = reading.baseUnit;
    m_extra[i].name = reading.name;
  }
  if (off && m_extra[i].last().gap())
    return;   // one gap is enough
  appendExtraPoint(i, p);
  // a value as fast as the main one is kept as long; beyond the most points
  // the oldest go
  bool trimmed = false;
  if (m_extra[i].count() > m_maxPoints + m_maxPoints / 100)
  {
    m_extra[i].removeFirst(m_extra[i].count() - m_maxPoints);
    trimmed = true;
  }
  if (began || trimmed)
    Q_EMIT extrasChanged();
  else
    Q_EMIT extraAppended(i);
}

bool RecordingStore::trimExtras()
{
  if (m_extra.isEmpty())
    return false;
  const qint64 from = m_state == Live ? origin() : m_floorMs;
  const qint64 slack = m_state == Live ? qMax<qint64>(1000, liveKept() / 20) : 0;
  bool changed = false;
  for (int i = int(m_extra.size()) - 1; i >= 0; --i)
  {
    RecordingSeries &s = m_extra[i];
    // a series that ended before what is kept goes; the newest of its value
    // keeps the point that holds at the start
    const bool newest = extraIndex(s.id) == i;
    if (!newest && !s.isEmpty() && s.last().t < from)
    {
      m_extra.removeAt(i);
      changed = true;
      continue;
    }
    if (s.count() > 1 && s.at(1).t < from - slack)
    {
      const int drop = qMax(0, s.holding(from));
      if (drop > 0)
      {
        s.removeFirst(drop);
        changed = true;
      }
    }
  }
  return changed;
}

void RecordingStore::applySetup(const Setup &setup, bool lead)
{
  setSampleTime(setup.sampleTime);
  setSampleLength(setup.sampleLength);
  setPreTrigger(setup.preTrigger);
  setStartMode(lead ? setup.mode : Manual);
  setStartTime(setup.startTime);
  setThresholds(setup.falling, setup.raising);
  setTriggerValue(lead ? setup.trigger : ValueRef(-1));
}

void RecordingStore::setTriggerValue(const ValueRef &value)
{
  if (value == m_triggerValue)
    return;
  m_triggerValue = value;
  m_lastValValid = false;   // the last value was another one's
}

bool RecordingStore::trigger(double val)
{
  bool started = false;
  if (m_state == Live && (m_mode == Raising || m_mode == Falling))
  {
    const bool crossed = m_mode == Raising
                           ? m_lastValValid && m_lastVal < m_raisingThreshold && val >= m_raisingThreshold
                           : m_lastValValid && m_lastVal > m_fallingThreshold && val <= m_fallingThreshold;
    if (crossed)
    {
      Q_EMIT alert();
      begin(m_preMs);
      started = true;
    }
  }

  // a gap (NaN: overload) crosses nothing: the triggers compare with the
  // last value there was
  if (std::isfinite(val))
  {
    m_lastValValid = true;
    m_lastVal = val;
  }
  return started;
}

void RecordingStore::setStale(bool stale)
{
  if (!stale || m_state == View)
    return;
  // the meter fell silent: the other values too
  bool extras = false;
  for (int i = 0; i < m_extra.size(); ++i)
  {
    const RecordingSeries &s = m_extra[i];
    if (s.isEmpty() || s.last().gap())
      continue;
    RawPoint p;
    p.t = qMin(s.last().t + m_staleMs, qMax(s.last().t, elapsed()));
    p.value = qQNaN();
    p.quality = Quality::Stale;
    appendExtraPoint(i, p);
    extras = true;
  }
  if (extras)
    Q_EMIT extrasChanged();
  if (m_series.isEmpty() || m_series.last().gap())
    return;
  // the last value held until the stale limit, no longer
  RawPoint p;
  p.t = qMin(m_series.last().t + m_staleMs, qMax(m_series.last().t, elapsed()));
  p.value = qQNaN();
  p.quality = Quality::Stale;
  append(p);
}

void RecordingStore::poll()
{
  if (m_state == Live && m_mode == Time)
  {
    // a timer may miss the exact second
    const int diff = m_startTime.secsTo(m_wall().time());
    if (diff >= 0 && diff < 2)
    {
      Q_EMIT alert();
      start();
    }
  }
  // the status bar's time, also while live: there only a window that
  // grows ("All") said it anew, a fixed one stood still
  if (m_state == Live || (m_state == Record && !lengthReached(elapsed())))
    Q_EMIT progressChanged();
}

bool RecordingStore::lengthReached(qint64 t)
{
  // counted from the trigger, after the pre-trigger time
  const qint64 limit = qint64(m_sampleLength) * 100 + m_preUsed;
  if (m_state != Record || m_sampleLength <= 0 || t < limit)
    return false;
  Q_EMIT alert();
  stop();
  m_stopT = limit;
  return true;
}

void RecordingStore::start()
{
  begin(0);
}

void RecordingStore::begin(qint64 preMs)
{
  // the pre-trigger: what the live window holds of that time, from the
  // reading that held at its start on
  LivePart pre;
  if (preMs > 0 && m_state == Live)
    pre = livePart(elapsed() - preMs);
  const State was = m_state;
  m_state = Record;
  clear();
  m_stopT = -1;
  m_preUsed = 0;
  m_holdOrigin = false;

  if (!pre.main.isEmpty())
  {
    // the recording reaches back by the pre-trigger time, or as far as
    // there were readings
    const qint64 back = qBound<qint64>(0, m_t0 - (pre.t0 + pre.main.first().t), preMs);
    m_t0 -= back;
    m_start = m_start.addMSecs(-back);
    m_preUsed = back;
    appendLivePart(pre);
    m_marks.append(Mark { back, kTriggerColor, QCoreApplication::translate("RecordingStore", "Trigger"), {}, MarkKind::Trigger });
    Q_EMIT marksChanged();
  }
  else
    appendCurrent();
  started(was);
}

void RecordingStore::startAt(qint64 t0, const QDateTime &start)
{
  LivePart since;
  if (m_state == Live)
    since = livePart(t0 - m_t0);
  const State was = m_state;
  m_state = Record;
  clear();
  m_stopT = -1;
  m_preUsed = 0;
  m_t0 = t0;
  m_start = start;
  m_holdOrigin = true;
  // what came since the start; when that is only the reading that held
  // then, the one shown now if it is fresh, at the time it came: the
  // start may lie back (the pre-trigger)
  if (since.main.size() > 1)
    appendLivePart(since);
  else
    appendCurrent(true);
  started(was);
}

RecordingStore::LivePart RecordingStore::livePart(qint64 from) const
{
  LivePart part;
  part.t0 = m_t0;
  for (int i = qMax(0, m_series.holding(from)); i < m_series.count(); ++i)
    part.main.append(m_series.at(i));
  for (const RecordingSeries &s : std::as_const(m_extra))
  {
    if (s.isEmpty() || s.last().t < from)
      continue;
    RecordingSeries extra;
    extra.id = s.id;
    extra.port = s.port;
    extra.unit = s.unit;
    extra.name = s.name;
    for (int i = qMax(0, s.holding(from)); i < s.count(); ++i)
      extra.append(s.at(i));
    part.extra.append(extra);
  }
  return part;
}

void RecordingStore::appendLivePart(const LivePart &part)
{
  for (RawPoint p : part.main)
  {
    p.t = qMax<qint64>(0, part.t0 + p.t - m_t0);
    append(p);
  }
  for (const RecordingSeries &extra : part.extra)
  {
    RecordingSeries s;
    s.id = extra.id;
    s.port = extra.port;
    s.unit = extra.unit;
    s.name = extra.name;
    m_extra.append(s);
    for (int i = 0; i < extra.count(); ++i)
    {
      RawPoint p = extra.at(i);
      p.t = qMax<qint64>(0, part.t0 + p.t - m_t0);
      appendExtraPoint(int(m_extra.size()) - 1, p);
    }
  }
  if (!part.extra.isEmpty())
    Q_EMIT extrasChanged();
}

void RecordingStore::started(State was)
{
  Q_EMIT progressChanged();
  if (was != Record)
    Q_EMIT stateChanged(Record);
  Q_EMIT runningChanged(true);
}

void RecordingStore::appendCurrent(bool atItsTime)
{
  // the first value is the one the meter shows at the start
  if (!m_haveReading || m_monotonic() - m_reading.t > m_staleMs)
    return;
  RawPoint p;
  if (atItsTime)
    p.t = qMax<qint64>(0, m_reading.t - m_t0);
  p.value = m_reading.overload ? overloadValue(m_reading.value) : m_reading.value;
  p.quality = pointQuality(m_reading);
  p.flags = m_reading.flags;
  append(p);
  // and what the other displays show, at the same moment
  const QMap<int, Reading> current = m_extraReading;
  for (const Reading &r : current)
    if (m_monotonic() - r.t <= m_staleMs && !r.text.isEmpty())
    {
      Reading now = r;
      now.t = atItsTime ? qMax(r.t, m_t0) : m_t0;
      appendExtra(now);
      m_extraReading[r.id] = r;   // it came when it came
    }
}

void RecordingStore::forgetReading()
{
  m_haveReading = false;
  m_extraReading.clear();
}

void RecordingStore::stop()
{
  if (m_state != Record)
    return;
  m_stopT = elapsed();
  m_state = View;
  Q_EMIT progressChanged();
  Q_EMIT stateChanged(View);
  Q_EMIT runningChanged(false);
}

void RecordingStore::live()
{
  const State was = m_state;
  m_state = Live;
  clear();
  m_preUsed = 0;
  m_holdOrigin = false;
  // the first reading in the window has nothing to cross from
  m_lastValValid = false;
  appendCurrent();
  Q_EMIT progressChanged();
  if (was != Live)
    Q_EMIT stateChanged(Live);
  if (was == Record)
    Q_EMIT runningChanged(false);
}

void RecordingStore::clear()
{
  m_series.clear();
  m_extra.clear();
  m_series.port = PortKey();
  m_series.unit.clear();
  m_series.name.clear();
  m_floorMs = 0;
  m_recordPort = PortKey();
  m_recordBaseUnit.clear();
  if (m_unitPending)
  {
    m_unit = m_nextUnit;
    m_unitPending = false;
  }
  const bool hadMarks = !m_marks.isEmpty();
  m_marks.clear();
  m_t0 = m_monotonic();
  m_start = m_wall();
  m_stopT = m_state != View ? -1 : 0;
  m_loadedGrid = 0;
  m_fromFile = false;
  m_fileInfo = QJsonObject();
  m_integral = 0;
  m_dirty = false;
  Q_EMIT cleared();
  if (hadMarks)
    Q_EMIT marksChanged();
}

qint64 RecordingStore::markTime() const
{
  // now while recording or live; else on the newest reading
  return m_state != View ? elapsed() : m_series.isEmpty() ? 0 : m_series.last().t;
}

void RecordingStore::addMark(quint32 argb, const QString &name)
{
  m_marks.append(Mark { markTime(), argb, name });
  Q_EMIT marksChanged();
}

int RecordingStore::addNote(qint64 t, const QString &title, const QString &note)
{
  if (t < 0)
    t = markTime();
  // after the marks of the same time: numbered as they came
  const auto at = std::upper_bound(m_marks.begin(), m_marks.end(), t, [](qint64 v, const Mark &m) { return v < m.t; });
  const int index = int(at - m_marks.begin());
  m_marks.insert(index, Mark { t, kNoteColor, title, note, MarkKind::Note });
  Q_EMIT marksChanged();
  return index;
}

void RecordingStore::setMarkText(int index, const QString &title, const QString &note)
{
  if (index < 0 || index >= m_marks.size())
    return;
  m_marks[index].name = title;
  m_marks[index].note = note;
  Q_EMIT marksChanged();
}

void RecordingStore::removeMark(int index)
{
  if (index < 0 || index >= m_marks.size())
    return;
  m_marks.removeAt(index);
  Q_EMIT marksChanged();
}

int RecordingStore::setMarkTime(int index, qint64 t)
{
  if (index < 0 || index >= m_marks.size())
    return index;
  Mark mark = m_marks.takeAt(index);
  mark.t = t;
  // after the marks of the same time, as addNote() puts it
  const auto at = std::upper_bound(m_marks.begin(), m_marks.end(), t, [](qint64 v, const Mark &m) { return v < m.t; });
  const int moved = int(at - m_marks.begin());
  m_marks.insert(moved, mark);
  Q_EMIT marksChanged();
  return moved;
}

void RecordingStore::appendPoint(RawPoint p)
{
  if (!m_series.isEmpty())
  {
    const RawPoint &prev = m_series.last();
    p.t = qMax(p.t, prev.t);
    // integration: the values above the threshold over the time they held;
    // at or below it the integral is back to 0 (the first value, too), a
    // gap adds nothing and the integral carries over it
    if (!prev.gap() && prev.value > m_integrationThreshold)
      m_integral += prev.value * double(p.t - prev.t) / 1000.0;
  }
  else
    m_integral = 0;
  if (!p.gap() && p.value <= m_integrationThreshold)
    m_integral = 0;
  p.integral = p.gap() ? qQNaN() : m_integral;
  m_series.append(p);
}

void RecordingStore::append(const RawPoint &p)
{
  appendPoint(p);
  // the live window is no recording: nothing to lose
  if (m_state == Record)
    m_dirty = true;
  Q_EMIT appended(trim());
}

bool RecordingStore::trim()
{
  const bool trimmed = trimMain();
  // the other values keep what the main one keeps; when the main one lost
  // points too, appended(true) says it for both
  if (trimExtras() && !trimmed)
    Q_EMIT extrasChanged();
  return trimmed;
}

void RecordingStore::trimForLimits()
{
  // no appended(true) follows here to say it for the other values
  if (trim())
    Q_EMIT extrasChanged();
}

bool RecordingStore::trimMain()
{
  if (m_series.isEmpty())
    return false;
  // what holds at the origin stays, everything before it goes; in steps of
  // a twentieth, so a full store does not drop something with every reading
  qint64 from = origin();
  int drop = 0;
  if (m_state == Live)
  {
    const qint64 slack = qMax<qint64>(1000, liveKept() / 20);
    if (m_series.count() > 1 && m_series.at(1).t < from - slack)
      drop = qMax(0, m_series.holding(from));
  }
  if (m_series.count() - drop > m_maxPoints + m_maxPoints / 100)
    drop = m_series.count() - m_maxPoints;
  if (drop <= 0)
    return false;
  m_series.removeFirst(drop);
  // a recording that outgrew the most points starts at what is left
  if (m_state != Live && !m_series.isEmpty())
  {
    m_floorMs = m_series.first().t;
    from = m_floorMs;
  }

  // the marks before what is kept fall off
  const qint64 first = qMax(from, m_series.isEmpty() ? from : m_series.first().t);
  bool dropped = false;
  for (int i = int(m_marks.size()) - 1; i >= 0; --i)
    if (m_marks[i].t < first)
    {
      m_marks.removeAt(i);
      dropped = true;
    }
  if (dropped)
    Q_EMIT marksChanged();
  return true;
}

double RecordingStore::displayStep(int id) const
{
  constexpr int kLook = 64;   // the newest rows are enough: other values lie between them
  for (int i = readingCount() - 1; i >= qMax(0, readingCount() - kLook); --i)
  {
    const LoggedReading &e = readingAt(i);
    const QString text = QString(e.text).remove(' ');
    bool number = false;
    text.toDouble(&number);
    if (e.id != id || e.quality != Quality::Valid || !number)
      continue;
    const int point = text.indexOf('.');
    const int decimals = point < 0 ? 0 : int(text.size()) - point - 1;
    return std::pow(10.0, -decimals) * SiPrefix::factor(SiPrefix::split(e.unit).prefix);
  }
  return 0;
}

void RecordingStore::logReading(const Reading &reading)
{
  // a secondary display that is off (an empty second value) is no reading
  if (m_readingsPaused || (reading.id > 0 && reading.text.isEmpty()))
    return;

  LoggedReading row;
  row.when = reading.msecs;
  row.value = reading.value;
  row.quality = pointQuality(reading);
  row.flags = reading.flags;
  row.text = reading.text;
  row.unit = reading.unit;
  row.port = reading.port;
  row.range = reading.range;
  row.id = reading.id;
  row.source = reading.source;
  logRow(row);
}

void RecordingStore::logRow(const LoggedReading &row)
{
  if (m_readingsPaused)
    return;
  if (m_readings.size() >= m_readingCapacity)
    removeOldestReadings(m_readings.size() - m_readingCapacity + 1);
  const int at = m_readings.size();
  Q_EMIT readingsAboutToInsert(at, at);
  m_readings.append(row);
  Q_EMIT readingsInserted();
}

void RecordingStore::logRows(const QList<LoggedReading> &rows)
{
  if (m_readingsPaused || rows.isEmpty())
    return;
  const QList<LoggedReading> kept = rows.mid(qMax<qsizetype>(0, rows.size() - m_readingCapacity));
  const qsizetype over = m_readings.size() + kept.size() - m_readingCapacity;
  if (over > 0)
    removeOldestReadings(int(over));
  const int at = m_readings.size();
  Q_EMIT readingsAboutToInsert(at, at + int(kept.size()) - 1);
  m_readings.append(kept);
  Q_EMIT readingsInserted();
}

void RecordingStore::removeOldestReadings(int count)
{
  Q_EMIT readingsAboutToRemove(0, count - 1);
  m_readings.remove(0, count);
  Q_EMIT readingsRemoved();
}

void RecordingStore::setReadingCapacity(int rows)
{
  m_readingCapacity = qMax(1, rows);
  if (m_readings.size() > m_readingCapacity)
    removeOldestReadings(m_readings.size() - m_readingCapacity);
}

void RecordingStore::clearReadings()
{
  if (m_readings.isEmpty())
    return;
  Q_EMIT readingsAboutToClear();
  m_readings.clear();
  Q_EMIT readingsCleared();
}

int RecordingStore::readingNearest(qint64 wall) const
{
  if (m_readings.isEmpty())
    return -1;
  // the readings come in the order of their time
  const auto at = std::lower_bound(m_readings.cbegin(), m_readings.cend(), wall,
                                   [](const LoggedReading &r, qint64 v) { return r.when < v; });
  int row = int(at - m_readings.cbegin());
  if (row == m_readings.size() || (row > 0 && wall - m_readings[row - 1].when < m_readings[row].when - wall))
    --row;
  return row;
}

void RecordingStore::unmarkReadingAt(qint64 wall, const QString &name)
{
  const int row = readingNearest(wall);
  if (row < 0 || m_readings[row].alarmName != name)
    return;
  m_readings[row].alarmArgb = 0;
  m_readings[row].alarmName.clear();
  Q_EMIT readingMarked(row);
}

void RecordingStore::markReadingAt(qint64 wall, quint32 argb, const QString &name)
{
  const int row = readingNearest(wall);
  if (row < 0)
    return;
  m_readings[row].alarmArgb = argb;
  m_readings[row].alarmName = name;
  Q_EMIT readingMarked(row);
}

void RecordingStore::markLastReading(quint32 argb, const QString &name)
{
  if (m_readings.isEmpty())
    return;
  const int row = m_readings.size() - 1;
  m_readings[row].alarmArgb = argb;
  m_readings[row].alarmName = name;
  Q_EMIT readingMarked(row);
}
