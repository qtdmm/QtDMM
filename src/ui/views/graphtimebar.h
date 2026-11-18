// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QWidget>

#include "ui/views/graphlegend.h"

class QLabel;
class QToolButton;

/// The row above a graph: what it shows on the left ("Live", "Recording ·
/// 0:01:20"), the legend of its curves next to it, and the time buttons on
/// the right - "All" and the last 1, 5 or 30 minutes -, then the buttons
/// for the overview, the min/max band, a note and the statistics. It knows no graph: the
/// window is set and asked for.
class GraphTimeBar : public QWidget
{
  Q_OBJECT
public:
  explicit GraphTimeBar(QWidget *parent = nullptr);

  /// The button of the window shown is down: @p all, else the one of @p seconds if there is one.
  void        setWindow(int seconds, bool all);
  /// The overview and the min/max band buttons are down.
  void        setOverviewChecked(bool on);
  void        setEnvelopeChecked(bool on);
  /// The statistics button is down.
  void        setStatsChecked(bool on);
  /// The note button can be clicked.
  void        setNoteEnabled(bool on);
  void        setStateText(const QString &text);
  QString     stateText() const;
  /// The curves' names in @p ink, in the longest form that fits between state and buttons; none: no legend.
  void        setLegend(const QList<GraphLegend::Entry> &entries, const QColor &ink);
  QString     legendText() const;
  /// The legend's widget, for a picture of the graph.
  GraphLegend *legend() const { return m_legend; }
  /// Text and buttons in @p text on the graph's background.
  void        setTextColor(const QColor &text);
  /// The width of the row without the legend: margins, the state and all
  /// the buttons (the graph's minimum width builds on it).
  int         fixedWidth() const;
  /// For tests: the button of @p seconds (0: "All"); nullptr without one.
  QToolButton *button(int seconds) const;

Q_SIGNALS:
  /// A time button was clicked: @p seconds, 0 for "All".
  void        windowChosen(int seconds);
  /// The overview button or the min/max band button was clicked.
  void        overviewToggled(bool on);
  void        envelopeToggled(bool on);
  /// The note button (the pencil) was clicked.
  void        noteClicked();
  /// The statistics button (Σ, at the right end) was clicked.
  void        statsToggled(bool on);

protected:
  void        resizeEvent(QResizeEvent *event) override;

private:
  /// The legend in the room there is now.
  void        fitLegend();

  QLabel     *m_state;
  GraphLegend *m_legend;
  QList<GraphLegend::Entry> m_entries;
  QColor      m_ink;
  QList<QToolButton *> m_buttons;
  QToolButton *m_overview;
  QToolButton *m_envelope;
  QToolButton *m_note;
  QToolButton *m_stats;
  int         m_seconds = 0;
  bool        m_all = false;
};
