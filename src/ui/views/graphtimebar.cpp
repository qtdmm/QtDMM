// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphtimebar.h"

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
  if (text != m_state->text())
    m_state->setText(text);
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
