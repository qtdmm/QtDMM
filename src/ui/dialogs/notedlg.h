// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;

/// A note in the graph: its title (on the flag) and a longer text. Also for
/// the text of an alarm's mark.
class NoteDlg : public QDialog
{
  Q_OBJECT
public:
  /// @p when says where it is ("at 0:12.4"), shown above the fields.
  NoteDlg(const QString &when, QWidget *parent = nullptr);

  void        setTitle(const QString &title);
  QString     title() const;
  void        setText(const QString &text);
  QString     text() const;

private:
  QLineEdit      *m_title;
  QPlainTextEdit *m_text;
};
