// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>
#include <QWidget>

#include "recording/readingsmodel.h"

class QAction;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QSpinBox;
class QTableView;
class QTimer;
class QToolButton;

/// The readings table panel: a ReadingsModel in a table that follows the
/// newest row, a statistics line, and clear / export / copy. Lives in a
/// dock next to the display and the analog meter (Ctrl+4).
class ReadingsWidget : public QWidget
{
  Q_OBJECT
public:
  explicit ReadingsWidget(QWidget *parent = nullptr);

  ReadingsModel *log() { return m_log; }
  /// Entries of the window around the table ("Hide window"), first in its
  /// context menu; not owned.
  void setWindowActions(const QList<QAction *> &actions) { m_windowActions = actions; }
  /// Rows to keep (persisted by MainWindow).
  int maxRows() const;
  void setMaxRows(int rows);

public Q_SLOTS:
  void clearSLOT();
  void exportSLOT();
  /// Selected rows (all when nothing is selected) to the clipboard, tab
  /// separated.
  void copySLOT();

  /// As narrow as the widest of the table's two control groups: in a
  /// narrow cell they go on two rows (see resizeEvent()).
  QSize minimumSizeHint() const override;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  /// Another design or font: the columns are measured afresh.
  void changeEvent(QEvent *event) override;

private Q_SLOTS:
  void updateStats();
  void followSLOT();
  /// Widens m_need to rows @p first..@p last (at most the newest
  /// kMeasureRows of them) and the header; a reset passes an empty range.
  void measureRows(int first, int last);
  /// Column widths: m_need each, the rest of the viewport shared evenly.
  void fitColumns();
  /// The table on the model, or on the filter while a value is left out.
  void applyFilter();

private:
  ReadingsModel *m_log;
  QList<QAction *> m_windowActions;
  class ValueFilter;
  ValueFilter *m_filter = nullptr;
  QTableView *m_view;
  QToolButton *m_pause;
  QCheckBox *m_follow;
  QSpinBox *m_maxRows;
  QLabel *m_stats;
  QTimer *m_statsTimer;
  QWidget *m_barLeft;                     ///< pause and follow
  QWidget *m_barRight;                    ///< keep, export, clear
  QHBoxLayout *m_row1;              ///< both groups, or the left one
  QHBoxLayout *m_row2;              ///< the right one when it does not fit
  void placeBar();
  QVector<int> m_need;                    ///< width each column needs, only grows
  static constexpr int kMeasureRows = 50;
};
