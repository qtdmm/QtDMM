// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/updatedownload.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>

UpdateDownload::UpdateDownload(QObject *parent) : QObject(parent) {}

UpdateDownload::~UpdateDownload()
{
  cancel();
}

QString UpdateDownload::defaultFolder()
{
  return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/updates";
}

void UpdateDownload::start(const UpdateFile &file, const QString &folder)
{
  cancel();
  m_expected = file;
  const QString name = QFileInfo(file.url.path()).fileName();
  QDir dir(folder);
  if (name.isEmpty() || !dir.mkpath("."))
  {
    fail(tr("Cannot write into %1").arg(QDir::toNativeSeparators(folder)));
    return;
  }
  // older downloads: only the newest is wanted
  for (const QString &old : dir.entryList(QDir::Files))
    dir.remove(old);
  m_target = dir.filePath(name);
  m_file.setFileName(m_target + ".part");
  if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    fail(tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(m_file.fileName()), m_file.errorString()));
    return;
  }
  m_hash.reset();

  if (!m_net)
    m_net = new QNetworkAccessManager(this);
  QNetworkRequest request(file.url);
  request.setHeader(QNetworkRequest::UserAgentHeader, QString("QtDMM/%1").arg(QCoreApplication::applicationVersion()));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setTransferTimeout(30000);
  m_reply = m_net->get(request);
  connect(m_reply, &QNetworkReply::readyRead, this, [this] {
    const QByteArray chunk = m_reply->readAll();
    m_hash.addData(chunk);
    if (m_file.write(chunk) != chunk.size())
      fail(tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(m_file.fileName()), m_file.errorString()));
  });
  connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
    Q_EMIT progress(received, total > 0 ? total : m_expected.size);
  });
  connect(m_reply, &QNetworkReply::finished, this, [this] {
    QNetworkReply *reply = m_reply;
    if (!reply)
      return;   // failed or cancelled meanwhile
    m_reply.clear();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
      fail(reply->errorString());
      return;
    }
    const QByteArray rest = reply->readAll();
    m_hash.addData(rest);
    m_file.write(rest);
    m_file.close();
    const QString sha = QString::fromLatin1(m_hash.result().toHex());
    if ((m_expected.size > 0 && m_file.size() != m_expected.size)
        || m_expected.sha256.isEmpty() || sha.compare(m_expected.sha256, Qt::CaseInsensitive) != 0)
    {
      fail(tr("The download does not match the release (size or SHA256): it was removed."));
      return;
    }
    QFile::remove(m_target);
    if (!m_file.rename(m_target))
    {
      fail(tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(m_target), m_file.errorString()));
      return;
    }
    Q_EMIT finished(m_target);
  });
}

void UpdateDownload::cancel()
{
  if (QNetworkReply *reply = m_reply)
  {
    m_reply.clear();
    reply->abort();
    reply->deleteLater();
  }
  if (m_file.isOpen())
    m_file.close();
  if (m_file.fileName().endsWith(".part"))
    m_file.remove();
}

void UpdateDownload::fail(const QString &error)
{
  cancel();
  Q_EMIT failed(error);
}
