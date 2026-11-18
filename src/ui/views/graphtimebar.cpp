// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphtimebar.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

GraphTimeBar::GraphTimeBar(QWidget *parent) : QWidget(parent)
{
  auto *bar = new QHBoxLayout(this);
  bar->setContentsMargins(6, 2, 6, 0);
  bar->setSpacing(2);
  m_state = new QLabel(this);
  bar->addWidget(m_state);
  bar->addSpacing(12);
  m_legend = new GraphLegend(this);
  m_legend->hide();
  bar->addWidget(m_legend);
  bar->addStretch(1);
  const QList<QPair<QString, int>> steps = { { tr("All"), 0 }, { tr("1 min"), 60 }, { tr("5 min"), 300 },
                                             { tr("30 min"), 1800 } };
  for (const auto &step : steps)
  {
    auto *b = new QToolButton(this);
    b->setText(step.first);
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setProperty("seconds", step.second);
    b->setToolTip(step.second == 0 ? tr("Show the whole recording, growing with it")
                                   : tr("Show the last %1").arg(step.first));
    connect(b, &QToolButton::clicked, this, [this, seconds = step.second]
    {
      Q_EMIT windowChosen(seconds);
      setWindow(m_seconds, m_all);   // also when nothing changes: undo the click's own toggle
    });
    bar->addWidget(b);
    m_buttons.append(b);
  }
  bar->addSpacing(6);
  // signs and words of the context GraphWidget (graphwords.h)
  auto toggle = [&](const char *name, ushort sign, const char *tip)
  {
    auto *b = new QToolButton(this);
    b->setObjectName(name);
    b->setText(QString(QChar(sign)));
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setToolTip(QCoreApplication::translate("GraphWidget", tip));
    bar->addWidget(b);
    return b;
  };
  m_overview = toggle("ui_graphOverviewButton", 0x229f, "Overview below the graph");
  connect(m_overview, &QToolButton::clicked, this, &GraphTimeBar::overviewToggled);
  m_envelope = toggle("ui_graphEnvelopeButton", 0x224b, "Min/max band");
  connect(m_envelope, &QToolButton::clicked, this, &GraphTimeBar::envelopeToggled);
  m_note = new QToolButton(this);
  m_note->setObjectName("ui_graphNoteButton");
  m_note->setText(QString(QChar(0x270e)));   // the pencil
  m_note->setAutoRaise(true);
  m_note->setFocusPolicy(Qt::NoFocus);
  // a word of the context GraphWidget
  m_note->setToolTip(QCoreApplication::translate("GraphWidget", "Note now... (N)"));
  connect(m_note, &QToolButton::clicked, this, &GraphTimeBar::noteClicked);
  bar->addWidget(m_note);
  m_stats = new QToolButton(this);
  m_stats->setText(QString(QChar(0x03a3)));
  m_stats->setCheckable(true);
  m_stats->setAutoRaise(true);
  m_stats->setFocusPolicy(Qt::NoFocus);
  m_stats->setToolTip(tr("Statistics: the values at the cursors, from A to B and over the time shown"));
  connect(m_stats, &QToolButton::clicked, this, &GraphTimeBar::statsToggled);
  bar->addWidget(m_stats);
}

void GraphTimeBar::setOverviewChecked(bool on)
{
  m_overview->setChecked(on);
}

void GraphTimeBar::setEnvelopeChecked(bool on)
{
  m_envelope->setChecked(on);
}

void GraphTimeBar::setStatsChecked(bool on)
{
  m_stats->setChecked(on);
}

void GraphTimeBar::setNoteEnabled(bool on)
{
  m_note->setEnabled(on);
}

void GraphTimeBar::setWindow(int seconds, bool all)
{
  m_seconds = seconds;
  m_all = all;
  for (QToolButton *b : std::as_const(m_buttons))
  {
    const int own = b->property("seconds").toInt();
    b->setChecked(own == 0 ? all : !all && own == seconds);
  }
}

void GraphTimeBar::setStateText(const QString &text)
{
  if (text == m_state->text())
    return;
  m_state->setText(text);
  fitLegend();
}

void GraphTimeBar::setLegend(const QList<GraphLegend::Entry> &entries, const QColor &ink)
{
  m_entries = entries;
  m_ink = ink;
  fitLegend();
}

void GraphTimeBar::fitLegend()
{
  m_legend->setVisible(!m_entries.isEmpty());
  if (m_entries.isEmpty())
    return;
  // what the state and the buttons leave
  m_legend->setEntries(m_entries, m_ink, qMax(1, width() - fixedWidth()));
  m_legend->setFixedSize(m_legend->sizeHint());
}

int GraphTimeBar::fixedWidth() const
{
  int w = m_state->sizeHint().width() + 12 + 12 + 2 * 6;
  for (const QToolButton *b : std::as_const(m_buttons))
    w += b->sizeHint().width() + 2;
  return w + m_overview->sizeHint().width() + m_envelope->sizeHint().width() + m_note->sizeHint().width()
         + m_stats->sizeHint().width() + 14;
}

QString GraphTimeBar::legendText() const
{
  return m_entries.isEmpty() ? QString() : m_legend->text();
}

void GraphTimeBar::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  fitLegend();
}

QString GraphTimeBar::stateText() const
{
  return m_state->text();
}

void GraphTimeBar::setTextColor(const QColor &text)
{
  const QColor frame(text.red(), text.green(), text.blue(), 110);
  setStyleSheet(QString("QLabel { color: %1; }"
                        "QToolButton { color: %1; border: 1px solid transparent; border-radius: 2px; padding: 0 4px; }"
                        "QToolButton:hover { border-color: %2; }"
                        "QToolButton:checked { border-color: %1; }")
                .arg(text.name(), frame.name(QColor::HexArgb)));
}

QToolButton *GraphTimeBar::button(int seconds) const
{
  for (QToolButton *b : m_buttons)
    if (b->property("seconds").toInt() == seconds)
      return b;
  return nullptr;
}
