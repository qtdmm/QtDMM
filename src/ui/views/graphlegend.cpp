// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphlegend.h"

#include <QPainter>

GraphLegend::GraphLegend(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
}

void GraphLegend::setEntries(const QList<Entry> &entries, const QColor &ink, int room)
{
  m_entries = entries;
  m_ink = ink;
  m_room = room;
  int forms = 1;
  for (const Entry &e : entries)
    forms = qMax(forms, int(e.names.size()));
  m_form = 0;
  while (room > 0 && m_form + 1 < forms && widthOf(m_form) > room)
    ++m_form;
  resize(sizeHint());
  update();
}

QString GraphLegend::text() const
{
  QStringList names;
  for (const Entry &e : m_entries)
    names << name(e, m_form);
  return names.join("  ");
}

int GraphLegend::widthOf(int form) const
{
  const QFontMetrics fm(font());
  int w = 0;
  for (const Entry &e : m_entries)
    w += (w ? kSpace : 0) + kSample + kGap + fm.horizontalAdvance(name(e, form));
  return w + 2;
}

QList<QPoint> GraphLegend::places() const
{
  const QFontMetrics fm(font());
  const int lineH = fm.height() + 2;
  QList<QPoint> out;
  int x = 1, y = 0;
  for (const Entry &e : m_entries)
  {
    const int w = kSample + kGap + fm.horizontalAdvance(name(e, m_form));
    if (m_room > 0 && x > 1 && x + w + 1 > m_room)
    {
      x = 1;
      y += lineH;
    }
    out << QPoint(x, y);
    x += w + kSpace;
  }
  return out;
}

QSize GraphLegend::sizeHint() const
{
  const QFontMetrics fm(font());
  const int lineH = fm.height() + 2;
  const int one = widthOf(m_form);
  if (m_room <= 0 || one <= m_room || m_entries.isEmpty())
    return QSize(one, lineH);
  // in lines: as wide as the widest line
  const QList<QPoint> at = places();
  int w = 0;
  for (int i = 0; i < at.size(); ++i)
    w = qMax(w, at[i].x() + kSample + kGap + fm.horizontalAdvance(name(m_entries[i], m_form)) + 1);
  return QSize(w, at.last().y() + lineH);
}

void GraphLegend::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QFontMetrics fm(font());
  const int lineH = fm.height() + 2;
  const QList<QPoint> at = places();
  for (int i = 0; i < m_entries.size(); ++i)
  {
    const Entry &e = m_entries[i];
    int x = at[i].x();
    const double mid = at[i].y() + lineH / 2.0;
    // the line as drawn, a little stronger, so a thin dotted one shows
    QPen pen = e.pen;
    pen.setWidthF(qBound(1.5, pen.widthF(), 3.0));
    p.setPen(pen);
    p.drawLine(QPointF(x + 1, mid), QPointF(x + kSample - 1, mid));
    x += kSample + kGap;
    p.setPen(m_ink);
    const QString text = name(e, m_form);
    p.drawText(QRect(x, at[i].y(), fm.horizontalAdvance(text) + 1, lineH), Qt::AlignVCenter | Qt::AlignLeft, text);
  }
}
