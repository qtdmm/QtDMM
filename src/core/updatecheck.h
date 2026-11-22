// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDate>
#include <QHash>
#include <QObject>
#include <QUrl>

#include <optional>

class QNetworkAccessManager;

/// One file of a release in latest.json: where, how big, its SHA256 (hex).
struct UpdateFile
{
  QUrl url;
  QString sha256;
  qint64 size = 0;
};

/// A release offered by the update check (one entry of latest.json).
struct UpdateRelease
{
  QString version;
  QDate date;
  QUrl notes;                          ///< the release page
  QHash<QString, UpdateFile> files;    ///< by kind: appimage, flatpak, windows-installer, ...
};

/// Looks for a newer QtDMM in latest.json on the Pages site, which the Pages
/// workflow writes from the GitHub releases (tools/make-update-feed.py):
/// "stable" is the newest release, "beta" a newer pre-release. Nothing is
/// sent but the request for that file. The URL can be set with
/// QTDMM_UPDATE_URL (tests: a file:// URL).
///
/// Which version and file fit this copy - the installed kind (installKind()),
/// stable or beta, a version skipped - is decided by offer(); the window
/// asks and shows. Builds by a package manager (CMake QTDMM_UPDATE_CHECK=OFF:
/// Copr, Arch, Nix) have no check at all: enabled() is false there.
class UpdateCheck : public QObject
{
  Q_OBJECT
public:
  /// How this copy was installed, which decides how it is updated.
  enum class Install { AppImage, Flatpak, WindowsInstaller, WindowsZip, MacBundle, Source };

  explicit UpdateCheck(QObject *parent = nullptr);

  /// False in builds whose updates the package manager brings.
  static bool enabled();
  /// From the running program: $APPIMAGE, /.flatpak-info, Inno Setup's
  /// uninstaller next to the program (or one level up), a macOS bundle.
  static Install installKind();
  /// The key of latest.json's "files" for @p kind; empty for Source.
  static QString fileKind(Install kind);

  /// <0, 0, >0 like strcmp for two versions of the scheme YY.N[.P][-preK]
  /// with an optional build part (".14+g1234abc"): a dev build comes before
  /// alpha, beta and rc of its version, which come before the release.
  /// A string that is no version sorts before every version.
  static int compareVersions(const QString &a, const QString &b);
  /// Whether @p version is a pre-release (alpha, beta, rc or dev).
  static bool isPreRelease(const QString &version);

  /// The release to offer for @p current from latest.json @p feed: the
  /// newest of "stable" and (with @p beta) "beta" that is newer than
  /// @p current and not @p skipped. None for a feed that cannot be read.
  static std::optional<UpdateRelease> offer(const QByteArray &feed, const QString &current, bool beta,
                                            const QString &skipped = {});

  /// What QtDMM passes the Windows installer for an update: silent with a
  /// progress window, the earlier folder and choices (Inno Setup keeps
  /// them), QtDMM started again afterwards (/QTDMMRESTART=1, read by
  /// assets/windows/update_restart.iss).
  static QStringList installerArguments();
  /// Starts the Windows installer @p path with installerArguments(), through
  /// the shell so that it can ask for administrator rights. False where it
  /// could not start, and on other systems.
  static bool runInstaller(const QString &path);

  /// The feed's address: QTDMM_UPDATE_URL, else the Pages site.
  static QUrl feedUrl();

  /// Fetches the feed; ends in checked() or failed().
  void check();

Q_SIGNALS:
  /// The feed as fetched.
  void checked(const QByteArray &feed);
  void failed(const QString &error);

private:
  QNetworkAccessManager *m_net = nullptr;
};
