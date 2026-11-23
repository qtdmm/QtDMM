// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class SourceBuild;

/// Update and build, for a build from the sources behind master: says what
/// will happen (pull, build, how the program is replaced), runs it with
/// SourceBuild on Start with its output below, and offers Restart now when
/// the new version is in place. Closing it cancels a run.
class RebuildDlg : public QDialog
{
  Q_OBJECT
public:
  /// @p build: the run (tests pass one set up); the dialog owns it.
  explicit RebuildDlg(SourceBuild *build, QWidget *parent = nullptr);

  SourceBuild *build() const { return m_build; }
  /// What the dialog says at the top / now / in its log (tests).
  QString intro() const;
  QString status() const;
  QString log() const;

public Q_SLOTS:
  void start();
  void reject() override;

Q_SIGNALS:
  /// Restart now: close QtDMM and start @p path.
  void restartRequested(const QString &path);

private:
  SourceBuild *m_build;
  QLabel *m_intro;
  QLabel *m_status;
  QPlainTextEdit *m_log;
  QPushButton *m_start;
  QPushButton *m_close;
  QPushButton *m_restart;
};
