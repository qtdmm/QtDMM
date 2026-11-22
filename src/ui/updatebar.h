// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

#include <functional>

#include "core/updatecheck.h"

class UpdateDownload;

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
///
/// The Windows installer is downloaded and checked here (UpdateDownload),
/// with the progress in the banner; Install now hands it to the window
/// (installReady()), which closes and starts it. Other kinds download in
/// the browser.
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
  /// How this copy is installed (UpdateCheck::installKind(); tests set another).
  void setInstallKind(UpdateCheck::Install kind) { m_kind = kind; }

Q_SIGNALS:
  /// The installer @p path is downloaded and checked, and the user wants it
  /// now: close QtDMM and run it (UpdateCheck::runInstaller()).
  void installReady(const QString &path);

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
  /// Downloads the installer of @p release, the banner shows how far.
  void download(const UpdateRelease &release, const UpdateFile &file);

  Settings *m_settings;
  UpdateCheck *m_check;
  UpdateDownload *m_download;
  UpdateCheck::Install m_kind;
  QLabel *m_text;
  QHBoxLayout *m_buttons;
  bool m_manual = false;   ///< the running check was asked for: answer it
};
