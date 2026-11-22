// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

#include <functional>

#include "core/updatecheck.h"

class QHBoxLayout;
class QLabel;
class Settings;

/// The update banner below the alarm banner: asks once whether QtDMM may
/// look for new versions, then offers a newer one with Download, What's new
/// and Skip this version; also the answer of Check for updates. Hidden
/// otherwise. The looking is UpdateCheck's; keys Update/check (unset: not
/// asked yet), Update/beta, Update/skip, Update/last (ISO date of the last
/// check at start, at most one a day).
///
/// A pre-release (rc, dev) is offered pre-releases too, until its release.
class UpdateBar : public QWidget
{
  Q_OBJECT
public:
  explicit UpdateBar(Settings *settings, QWidget *parent = nullptr);

  /// At program start: asks the first time, else checks once a day when allowed.
  void startupCheck();
  /// Check for updates: at once, also a skipped version, with an answer either way.
  void checkNow();

  /// Whether pre-releases are offered: Update/beta, or this is one.
  bool beta() const;

private:
  struct Button
  {
    QString text;
    std::function<void()> action;
  };
  /// Shows @p text with @p buttons and a close button.
  void showMessage(const QString &text, const QList<Button> &buttons);
  void ask();
  void checked(const QByteArray &feed);
  void offer(const UpdateRelease &release);

  Settings *m_settings;
  UpdateCheck *m_check;
  QLabel *m_text;
  QHBoxLayout *m_buttons;
  bool m_manual = false;   ///< the running check was asked for: answer it
};
