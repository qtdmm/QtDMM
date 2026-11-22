// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphstatspanel.h"

#include <QLabel>
#include <QScrollBar>

GraphStatsPanel::GraphStatsPanel(QWidget *parent) : QScrollArea(parent)
{
  setObjectName("ui_graphStatsPanel");
  setWidgetResizable(true);
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFocusPolicy(Qt::NoFocus);
  m_label = new QLabel(this);
  m_label->setTextFormat(Qt::RichText);
  m_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_label->setContentsMargins(6, 0, 6, 6);
  m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  setWidget(m_label);
}

void GraphStatsPanel::setInput(const GraphStats::Input &input)
{
  m_input = input;
  m_label->setText(GraphStats::html(input, m_label->fontMetrics(), width(), &m_want));
}

int GraphStatsPanel::wantedWidth(int most) const
{
  const int want = (qMax(170, m_want) + 15) / 16 * 16;
  return qMin(want, qMax(120, most));
}

void GraphStatsPanel::setColors(const QBrush &background, const QColor &text)
{
  QPalette p = palette();
  for (QPalette::ColorRole role : { QPalette::Window, QPalette::Base, QPalette::Button })
    p.setBrush(role, background);
  p.setColor(QPalette::ButtonText, text);
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Text, text);
  setPalette(p);
  m_label->setPalette(p);
  verticalScrollBar()->setPalette(p);   // a dark graph gets no light strip
  viewport()->setAutoFillBackground(true);
}

void GraphStatsPanel::setClockText(const QString &text)
{
  m_label->setToolTip(text);
}

QString GraphStatsPanel::text() const
{
  return GraphStats::text(m_input);
}
