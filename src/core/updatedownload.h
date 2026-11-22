// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QPointer>

#include "core/updatecheck.h"

class QNetworkAccessManager;
class QNetworkReply;

/// Downloads one file of a release (UpdateFile) into a folder and checks it
/// against the size and SHA256 of latest.json before it counts: the file is
/// written as "<name>.part", hashed on the way, and renamed when both match;
/// a wrong one is removed. Used for the Windows installer, later for the
/// AppImage and the macOS disk image.
class UpdateDownload : public QObject
{
  Q_OBJECT
public:
  explicit UpdateDownload(QObject *parent = nullptr);
  ~UpdateDownload() override;

  /// The folder for downloads: QtDMM's cache folder, "updates".
  static QString defaultFolder();

  /// Starts downloading @p file into @p folder (made when missing; other
  /// files there are removed first: old downloads). One at a time.
  void start(const UpdateFile &file, const QString &folder = defaultFolder());
  /// Stops a running download and removes what it wrote.
  void cancel();
  bool isRunning() const { return !m_reply.isNull(); }

Q_SIGNALS:
  void progress(qint64 received, qint64 total);
  /// The file is complete and checked: its path.
  void finished(const QString &path);
  void failed(const QString &error);

private:
  void fail(const QString &error);

  QNetworkAccessManager *m_net = nullptr;
  QPointer<QNetworkReply> m_reply;
  QFile m_file;
  QCryptographicHash m_hash{ QCryptographicHash::Sha256 };
  UpdateFile m_expected;
  QString m_target;
};
