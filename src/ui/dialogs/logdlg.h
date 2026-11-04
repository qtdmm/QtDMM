// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QHash>

#include "core/applog.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QTableView;
class LogFilter;
class LogModel;

/// The application's log (AppLog) in a window of its own beside the others:
/// time, scope, level and message. Above the table the levels shown, a search
/// with previous and next, and the export to attach to a report. It follows
/// the newest line while it is at the end.
class LogDlg : public QDialog
{
  Q_OBJECT
public:
  explicit LogDlg(AppLog *log, QWidget *parent = nullptr);

  /// Show the lines of @p level, or not.
  void        setLevelShown(AppLog::Level level, bool on);
  /// The lines shown now.
  int         rows() const;
  /// The row selected, -1: none.
  int         currentRow() const;
  /// To the next (@p forward) or the previous line with the search text,
  /// from the end around; false: none has it.
  bool        find(const QString &text, bool forward);

  /// "qtdmm-2026-11-04.log".
  static QString fileName(const QDate &date);

private:
  void        search(bool forward);
  void        exportLog();

  AppLog     *m_log;
  LogModel   *m_model;
  LogFilter  *m_filter;
  QTableView *m_table;
  QLineEdit  *m_search;
  QLabel     *m_notFound;
  QHash<int, QCheckBox *> m_levels;
};
