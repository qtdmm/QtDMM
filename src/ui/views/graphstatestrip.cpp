// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphstatestrip.h"

#include <QPainter>

GraphStateStrip::GraphStateStrip(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
}

QColor GraphStateStrip::colorOf(Kind kind)
{
  switch (kind)
  {
    case Gap:      return QColor(0xa4, 0xaa, 0xb2);
    case Overload: return QColor(0xe0, 0x4a, 0x4a);
    case Hold:     return QColor(0xe6, 0xa2, 0x2e);
    case Extreme:  return QColor(0x3f, 0x9c, 0xdc);
    default:       return QColor(0xa0, 0x74, 0xdc);
  }
}

void GraphStateStrip::paintEvent(QPaintEvent *)
{
  if (!m_provider)
    return;
  const QList<Span> spans = m_provider();
  if (spans.isEmpty())
    return;
  // the lanes of the kinds there are, from the bottom
  int lane[KindCount];
  int lanes = 0;
  for (int k = 0; k < KindCount; ++k)
  {
    const bool used = std::any_of(spans.cbegin(), spans.cend(), [k](const Span &s) { return s.kind == k; });
    lane[k] = used ? lanes++ : -1;
  }
  QPainter p(this);
  QFont f = font();
  f.setPixelSize(kLane - 3);
  f.setBold(true);
  p.setFont(f);
  const QFontMetrics fm(f);
  for (const Span &s : spans)
  {
    const int x0 = int(s.from * width()), x1 = qMax(x0 + 2, int(s.to * width()));   // a single reading still shows
    const QRect bar(x0, height() - (lane[s.kind] + 1) * kLane + 1, x1 - x0, kLane - 2);
    QColor fill = colorOf(s.kind);
    fill.setAlpha(205);
    p.fillRect(bar, fill);
    // a gap's cause: hatched, so the causes tell apart without a colour each
    if (s.pattern != Qt::SolidPattern)
      p.fillRect(bar, QBrush(QColor(0x30, 0x34, 0x3a), s.pattern));
    if (fm.horizontalAdvance(s.label) + 6 <= bar.width())
    {
      // on a hatching the name gets a plain ground
      if (s.pattern != Qt::SolidPattern)
        p.fillRect(QRect(bar.left() + 1, bar.top(), fm.horizontalAdvance(s.label) + 4, bar.height()), colorOf(s.kind));
      p.setPen(Qt::black);
      p.drawText(bar.adjusted(3, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, s.label);
    }
  }
}
