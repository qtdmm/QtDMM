// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantMap>

/// The update of the Flatpak, done by Flatpak itself through its portal
/// (org.freedesktop.portal.Flatpak, open to every sandbox): an update
/// monitor (CreateUpdateMonitor) whose Update() pulls the newest commit of
/// the app from its remote, our repository, and reports how far it is
/// (Progress). The running sandbox keeps its version; restart() starts the
/// new one (Spawn with the latest version). Which version is out comes from
/// latest.json as for the other kinds (UpdateCheck).
///
/// Needs Qt DBus (QTDMM_WITH_DBUS, Linux); without it supported() is false
/// and the banner only points to the software centre.
class FlatpakUpdate : public QObject
{
  Q_OBJECT
public:
  explicit FlatpakUpdate(QObject *parent = nullptr);

  /// Built with the portal (Qt DBus).
  static bool supported();
  /// Starts the update; ends in done(), upToDate() or failed().
  virtual void update();
  /// Starts the newest installed version of QtDMM in a new sandbox.
  static bool restart();

Q_SIGNALS:
  void progress(int percent);
  void done();
  /// Nothing to pull: the repository has no newer commit (yet).
  void upToDate();
  void failed(const QString &error);

private Q_SLOTS:
  void onProgress(const QVariantMap &info);

private:
  QString m_monitor;   ///< the update monitor's object path
};
