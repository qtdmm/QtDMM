// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphminimap.h"

#include <QMouseEvent>
#include <QPainter>

#include "recording/recordingstore.h"

GraphMinimap::GraphMinimap(QWidget *parent) :
  QWidget(parent)
{
  setMouseTracking(true);
  setFixedHeight(kHeight);
  setToolTip(tr("The whole recording: drag the window, or its brackets to zoom; at the right end it follows the "
                "newest reading"));
}

void GraphMinimap::setStore(RecordingStore *store)
{
  if (store == m_store && store)
    return;
  if (m_store)
    m_store->disconnect(this);
  m_store = store;
  if (m_store)
  {
    connect(m_store, &RecordingStore::appended, this, &GraphMinimap::onAppended);
    connect(m_store, &RecordingStore::cleared, this, &GraphMinimap::rebuild);
    connect(m_store, &RecordingStore::loaded, this, &GraphMinimap::rebuild);
    connect(m_store, &RecordingStore::stateChanged, this, &GraphMinimap::rebuild);
    connect(m_store, &RecordingStore::extraAppended, this, &GraphMinimap::onExtraAppended);
    connect(m_store, &RecordingStore::extrasChanged, this, [this]
    {
      if (m_value > 0)
        rebuild();
    });
    connect(m_store, &RecordingStore::marksChanged, this, qOverload<>(&QWidget::update));
  }
  rebuild();
}

void GraphMinimap::setView(qint64 start, qint64 width)
{
  if (start == m_viewStart && width == m_viewWidth)
    return;
  m_viewStart = start;
  m_viewWidth = width;
  ensureColumns();
  update();
}

void GraphMinimap::setColors(const QBrush &background, const QColor &lettering)
{
  m_background = background;
  m_lettering = lettering;
  update();
}

void GraphMinimap::setCurveColor(const QColor &curve)
{
  m_curve = curve;
  update();
}

void GraphMinimap::setValue(int id)
{
  if (id == m_value)
    return;
  m_value = id;
  rebuild();
}

qint64 GraphMinimap::from() const
{
  return m_store ? m_store->origin() : 0;
}

qint64 GraphMinimap::to() const
{
  const qint64 end = qMax(m_store ? m_store->duration() : 0, m_viewStart + m_viewWidth);
  return qMax(end, from() + 1);
}

// The right edge of the scale: to(), rounded up in steps of 1/64 to 1/32 of the
// strip. Between two steps a new reading moves nothing; with the exact end
// every reading would shrink the strip and its edges would jump by a pixel,
// each at its own moment. A finished recording has no use for the steps.
qint64 GraphMinimap::scaleEnd() const
{
  if (!m_store || m_store->state() == RecordingStore::View)
    return to();
  const qint64 span = to() - from();
  qint64 step = 1;
  while (step * 64 < span)
    step *= 2;
  return from() + (span + step - 1) / step * step;
}

const GraphMinimap::Column *GraphMinimap::column(qint64 t) const
{
  if (t < from() || t > to())
    return nullptr;
  const qint64 i = t / m_columnMs - m_firstColumn;
  return i >= 0 && i < m_columns.size() ? &m_columns[int(i)] : nullptr;
}

double GraphMinimap::xOf(qint64 t) const
{
  return double(t - from()) / double(scaleEnd() - from()) * width();
}

qint64 GraphMinimap::timeAt(double x) const
{
  return from() + qint64(std::llround(x / qMax(1, width()) * double(scaleEnd() - from())));
}

qint64 GraphMinimap::neededColumnMs() const
{
  const qint64 span = to() - from();
  const qint64 need = (span + qMax(1, width()) - 1) / qMax(1, width());
  qint64 ms = 1;
  while (ms < need)
    ms *= 2;
  return ms;
}

void GraphMinimap::ensureColumns()
{
  if (neededColumnMs() != m_columnMs)
    rebuild();
}

// A reading into its column: the range of the values, what holds at the end.
static void addReading(GraphMinimap::Column &c, const RawPoint &p)
{
  c.any = true;
  if (p.gap())
  {
    c.last = std::nan("");
    if (p.quality == Quality::Overload)
      c.overload = true;
    else
      c.gap = true;
    return;
  }
  c.min = std::fmin(c.min, p.value);
  c.max = std::fmax(c.max, p.value);
  c.last = p.value;
}

void GraphMinimap::rebuild()
{
  m_rebuilds++;
  m_columnMs = neededColumnMs();
  m_columns.clear();
  m_firstColumn = from() / m_columnMs;
  // the series of the value: the main one, or those of another value in turn
  QList<const RecordingSeries *> parts;
  if (m_store && m_value <= 0)
    parts << &m_store->series();
  for (int i = 0; m_store && m_value > 0 && i < m_store->extraCount(); ++i)
    if (m_store->extra(i).id == m_value)
      parts << &m_store->extra(i);
  for (const RecordingSeries *series : std::as_const(parts))
  {
    if (series->isEmpty())
      continue;
    const qint64 last = qMax(m_firstColumn, series->last().t / m_columnMs);
    if (last - m_firstColumn >= m_columns.size())
      m_columns.resize(int(last - m_firstColumn + 1));
    // the reading that holds at the start goes into the first column
    for (int i = qMax(0, series->holding(from())); i < series->count(); ++i)
    {
      const RawPoint &p = series->at(i);
      addReading(m_columns[int(qMax(m_firstColumn, p.t / m_columnMs) - m_firstColumn)], p);
    }
  }
  update();
}

void GraphMinimap::onAppended()
{
  if (!m_store || m_store->count() == 0)
    return;
  if (m_value > 0)
  {
    // another value is drawn: the main series only moves the strip on
    if (neededColumnMs() != m_columnMs)
      rebuild();
    else
    {
      dropFront();
      update();
    }
    return;
  }
  addLast(m_store->series().last());
}

void GraphMinimap::onExtraAppended(int index)
{
  if (m_store && m_value > 0 && index < m_store->extraCount() && m_store->extra(index).id == m_value)
    addLast(m_store->extra(index).last());
}

void GraphMinimap::addLast(const RawPoint &p)
{
  if (neededColumnMs() != m_columnMs || m_columns.isEmpty())
  {
    rebuild();
    return;
  }
  dropFront();
  const qint64 k = p.t / m_columnMs;
  if (k < m_firstColumn)
  {
    rebuild();
    return;
  }
  if (k - m_firstColumn >= m_columns.size())
    m_columns.resize(int(k - m_firstColumn + 1));
  // the store only appends: the reading joins its column
  addReading(m_columns[int(k - m_firstColumn)], p);
  update();
}

void GraphMinimap::dropFront()
{
  const qint64 first = from() / m_columnMs;
  if (first <= m_firstColumn)
    return;
  m_columns.remove(0, int(qMin<qint64>(first - m_firstColumn, m_columns.size())));
  m_firstColumn = first;
}

qint64 GraphMinimap::endTolerance() const
{
  return qMax<qint64>(1, 3 * (to() - from()) / qMax(1, width()));
}

GraphMinimap::Drag GraphMinimap::dragAt(double x) const
{
  const double x0 = xOf(m_viewStart), x1 = xOf(m_viewStart + m_viewWidth);
  const double grab = 6;
  const bool inside = x >= x0 && x <= x1;
  // a narrow window is moved from inside, its brackets taken from outside
  if (x1 - x0 >= 3 * grab || !inside)
  {
    if (std::abs(x - x1) <= grab)
      return RightBracket;
    if (std::abs(x - x0) <= grab)
      return LeftBracket;
  }
  return inside ? Move : NoDrag;
}

void GraphMinimap::resizeEvent(QResizeEvent *)
{
  ensureColumns();
}

void GraphMinimap::mousePressEvent(QMouseEvent *ev)
{
  if (ev->button() != Qt::LeftButton)
  {
    QWidget::mousePressEvent(ev);
    return;
  }
  const double x = ev->position().x();
  m_drag = dragAt(x);
  switch (m_drag)
  {
    case NoDrag:
      // beside the window: it jumps there, centred, and can be dragged on
      m_drag = Move;
      m_grip = m_viewWidth / 2;
      mouseMoveEvent(ev);
      break;
    case Move:
      m_grip = timeAt(x) - m_viewStart;
      break;
    case LeftBracket:
      m_grip = m_viewStart + m_viewWidth;   // the right bracket stays
      break;
    case RightBracket:
      m_grip = m_viewStart;                 // the left one stays
      break;
  }
  setCursor(m_drag == Move ? Qt::ClosedHandCursor : Qt::SizeHorCursor);
}

void GraphMinimap::mouseMoveEvent(QMouseEvent *ev)
{
  const double x = ev->position().x();
  const qint64 t = timeAt(x);
  const qint64 begin = from(), end = to(), tolerance = endTolerance();
  switch (m_drag)
  {
    case NoDrag:
    {
      const Drag d = dragAt(x);
      setCursor(d == Move ? Qt::OpenHandCursor : d == NoDrag ? Qt::PointingHandCursor : Qt::SizeHorCursor);
      return;
    }
    case Move:
    {
      const qint64 start = qBound(begin, t - m_grip, qMax(begin, end - m_viewWidth));
      if (start + m_viewWidth >= end - tolerance)
        Q_EMIT endRequested();
      else
        Q_EMIT moveRequested(start);
      return;
    }
    case LeftBracket:
    {
      const qint64 start = qMax(begin, qMin(t, m_grip - kMinWidthMs));
      Q_EMIT widthRequested(start, m_grip - start, m_grip >= end - tolerance);
      return;
    }
    case RightBracket:
    {
      const qint64 stop = qMax(m_grip + kMinWidthMs, qMin(t, end));
      Q_EMIT widthRequested(m_grip, stop - m_grip, stop >= end - tolerance);
      return;
    }
  }
}

void GraphMinimap::mouseReleaseEvent(QMouseEvent *ev)
{
  if (ev->button() != Qt::LeftButton)
    return;
  m_drag = NoDrag;
  const Drag d = dragAt(ev->position().x());
  setCursor(d == Move ? Qt::OpenHandCursor : d == NoDrag ? Qt::PointingHandCursor : Qt::SizeHorCursor);
}

void GraphMinimap::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.fillRect(rect(), m_background.style() != Qt::NoBrush ? m_background : palette().base());
  const QColor lettering = m_lettering.isValid() ? m_lettering : palette().color(QPalette::Text);
  const QColor curve = m_curve.isValid() ? m_curve : lettering;

  // the scale of the strip: everything it shows
  double lo = std::nan(""), hi = std::nan("");
  for (const Column &c : std::as_const(m_columns))
  {
    lo = std::fmin(lo, std::fmin(c.min, c.last));
    hi = std::fmax(hi, std::fmax(c.max, c.last));
  }
  // the last digit of a steady value must not fill the strip: it would turn
  // into a square wave from top to bottom
  const double least = leastSpan(lo, hi, m_store ? m_store->displayStep(m_value) : 0);
  if (hi - lo < least)
  {
    const double middle = (lo + hi) / 2;
    lo = middle - least / 2;
    hi = middle + least / 2;
  }
  const double pad = hi > lo ? (hi - lo) * 0.08 : 1.0;
  lo -= pad;
  hi += pad;
  const double top = 4, bottom = height() - 6;
  auto yOf = [&](double v) { return bottom - (v - lo) / (hi - lo) * (bottom - top); };

  const qint64 begin = from(), end = to();
  const QColor overload(0xd3, 0x2f, 0x2f);
  const QBrush hatch(QColor(lettering.red(), lettering.green(), lettering.blue(), 80), Qt::BDiagPattern);
  double held = std::nan("");   // the value that holds from the column before
  for (int j = 0; j < m_columns.size(); ++j)
  {
    const Column &c = m_columns[j];
    const qint64 k = m_firstColumn + j;
    const double x0 = std::floor(xOf(qMax(begin, k * m_columnMs)));
    const double x1 = std::floor(xOf(qMin(end, (k + 1) * m_columnMs)));
    const QRectF column(x0, 0, qMax(1.0, x1 - x0), height());
    // the line comes from the value before: a column that only holds it
    // draws it, one with readings joins it to them
    const double a = std::fmin(c.min, held), b = std::fmax(c.max, held);
    if (!std::isnan(a))
    {
      const double y0 = yOf(b), y1 = yOf(a);
      p.fillRect(QRectF(column.left(), y0 - 0.5, column.width(), qMax(1.5, y1 - y0 + 1)), curve);
    }
    else if (!c.overload)
      p.fillRect(QRectF(column.left(), top, column.width(), bottom - top), hatch);
    if (c.overload)
      p.fillRect(QRectF(column.left(), 0, column.width(), 3), overload);
    if (c.any)
      held = c.last;
  }

  // the marks: dots on the bottom edge
  if (m_store)
  {
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    for (const RecordingStore::Mark &m : m_store->marks())
      if (m.t >= begin && m.t <= end)
      {
        p.setBrush(QColor::fromRgba(m.color));
        p.drawEllipse(QPointF(xOf(m.t), height() - 3.0), 2.5, 2.5);
      }
    p.setRenderHint(QPainter::Antialiasing, false);
  }

  // the window: tinted, between two brackets
  if (m_viewWidth > 0)
  {
    const double x0 = qBound(0.0, xOf(m_viewStart), double(width() - 1));
    const double x1 = qBound(x0 + 2, xOf(m_viewStart + m_viewWidth), double(width()));
    p.fillRect(QRectF(x0, 0, x1 - x0, height()), QColor(lettering.red(), lettering.green(), lettering.blue(), 30));
    p.setPen(QPen(lettering, 2, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    const double l = x0 + 1, r = x1 - 1, h = height() - 1;
    p.drawPolyline(QPolygonF({ QPointF(l + 4, 1), QPointF(l, 1), QPointF(l, h), QPointF(l + 4, h) }));
    p.drawPolyline(QPolygonF({ QPointF(r - 4, 1), QPointF(r, 1), QPointF(r, h), QPointF(r - 4, h) }));
  }
}
