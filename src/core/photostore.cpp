// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/photostore.h"

#include "core/projectfolder.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>

namespace
{
// a free name for a photo taken at @p when: a second one of the same second counts on
QString freeName(const QString &dir, const QDateTime &when, const QString &suffix)
{
  const QString base = "photo_" + when.toString("yyyy-MM-dd_HH-mm-ss");
  QString path = QDir(dir).filePath(base + '.' + suffix);
  for (int n = 2; QFileInfo::exists(path); ++n)
    path = QDir(dir).filePath(QString("%1_%2.%3").arg(base).arg(n).arg(suffix));
  return path;
}
}   // namespace

bool PhotoStore::isPhoto(const QByteArray &data, QString *suffix)
{
  QBuffer buffer;
  buffer.setData(data);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer);
  const QByteArray format = reader.format().toLower();
  if (format.isEmpty() || !reader.size().isValid())
    return false;
  if (suffix)
    *suffix = format == "jpeg" ? QStringLiteral("jpg") : QString::fromLatin1(format);
  return true;
}

QString PhotoStore::save(const QString &project, const QByteArray &data, const QDateTime &when, QString *error)
{
  QString suffix;
  if (!isPhoto(data, &suffix))
  {
    if (error)
      *error = QCoreApplication::translate("PhotoStore", "This is no picture.");
    return {};
  }
  const QString dir = ProjectFolder::dirFor(project, ProjectFolder::Photos);
  const QString path = freeName(dir, when, suffix);
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
  {
    if (error)
      *error = QCoreApplication::translate("PhotoStore", "Could not write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
    return {};
  }
  return path;
}

QString PhotoStore::addFile(const QString &project, const QString &file, const QDateTime &when, QString *error)
{
  QFile in(file);
  if (!in.open(QIODevice::ReadOnly))
  {
    if (error)
      *error = QCoreApplication::translate("PhotoStore", "Could not read %1: %2").arg(QDir::toNativeSeparators(file), in.errorString());
    return {};
  }
  return save(project, in.readAll(), when, error);
}

QString PhotoStore::relative(const QString &project, const QString &photo)
{
  if (project.isEmpty())
    return photo;
  const QString rel = QDir(project).relativeFilePath(photo);
  return rel.startsWith("..") || QDir::isAbsolutePath(rel) ? photo : rel;
}

QString PhotoStore::absolute(const QString &project, const QString &path)
{
  if (path.isEmpty() || QDir::isAbsolutePath(path) || project.isEmpty())
    return path;
  return QDir(project).filePath(path);
}

QImage PhotoStore::load(const QString &path)
{
  QImageReader reader(path);
  reader.setAutoTransform(true);
  return reader.read();
}
