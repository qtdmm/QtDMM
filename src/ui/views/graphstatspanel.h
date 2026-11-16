// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QScrollArea>

#include "ui/views/graphstats.h"

class QLabel;

/// The statistics column next to a graph: the text of GraphStats, scrolled
/// when it is longer than the graph is high. It says how wide it would like
/// to be (wantedWidth()); whoever lays it out gives it that.
class GraphStatsPanel : public QScrollArea
{
  Q_OBJECT
public:
  explicit GraphStatsPanel(QWidget *parent = nullptr);

  /// The statistics of @p input, the names cut to the column's width now.
  void        setInput(const GraphStats::Input &input);
  /// As wide as the widest row wants, in steps of 16 so it does not twitch;
  /// at least for a short name and a number, at most @p most.
  int         wantedWidth(int most) const;
  /// The lettering on the graph's background.
  void        setColors(const QBrush &background, const QColor &text);
  /// Shown below the text's pointer: the cursors' clock times.
  void        setClockText(const QString &text);
  QString     text() const;

private:
  QLabel     *m_label;
  GraphStats::Input m_input;
  int         m_want = 0;
};
