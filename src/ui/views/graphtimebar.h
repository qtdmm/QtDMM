// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QWidget>

class QLabel;
class QToolButton;

/// The row above a graph: what it shows on the left ("Live", "Recording ·
/// 0:01:20") and the time buttons on the right - "All" and the last 1, 5 or
/// 30 minutes. It knows no graph: the window is set and asked for.
class GraphTimeBar : public QWidget
{
  Q_OBJECT
public:
  explicit GraphTimeBar(QWidget *parent = nullptr);

  /// The button of the window shown is down: @p all, else the one of @p seconds if there is one.
  void        setWindow(int seconds, bool all);
  void        setStateText(const QString &text);
  QString     stateText() const;
  /// Text and buttons in @p text on the graph's background.
  void        setTextColor(const QColor &text);
  /// For tests: the button of @p seconds (0: "All"); nullptr without one.
  QToolButton *button(int seconds) const;

Q_SIGNALS:
  /// A time button was clicked: @p seconds, 0 for "All".
  void        windowChosen(int seconds);

private:
  QLabel     *m_state;
  QList<QToolButton *> m_buttons;
  int         m_seconds = 0;
  bool        m_all = false;
};
