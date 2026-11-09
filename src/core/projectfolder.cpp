// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/projectfolder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
QString &folder()
{
  static QString open;
  return open;
}

bool &sortByType()
{
  static bool on = true;
  return on;
}

struct KindNames
{
  ProjectFolder::Kind kind;
  const char *name;
  const char *de, *fr, *es, *pl;
};

// fixed, not tr(): project.json carries every language, so a project passed
// on shows the names of the one who opens it [E 09.11.]
const KindNames kNames[] = {
  { ProjectFolder::Benches, "benches", "Messplätze", "Bancs de mesure", "Puestos de medida", "Stanowiska pomiarowe" },
  { ProjectFolder::Recordings, "recordings", "Aufzeichnungen", "Enregistrements", "Grabaciones", "Nagrania" },
  { ProjectFolder::Images, "images", "Bilder", "Images", "Imágenes", "Obrazy" },
  { ProjectFolder::Analysis, "analysis", "Auswertung", "Analyse", "Análisis", "Analiza" },
  { ProjectFolder::Documents, "documents", "Dokumente", "Documents", "Documentos", "Dokumenty" },
};

QJsonObject readJson(const QString &project)
{
  QFile file(QDir(project).filePath("project.json"));
  if (!file.open(QIODevice::ReadOnly))
    return {};
  return QJsonDocument::fromJson(file.readAll()).object();
}

// enters the folder's names in project.json (made when missing)
void enterFolder(const QString &project, const KindNames &k)
{
  QJsonObject json = readJson(project);
  QJsonObject folders = json.value("folders").toObject();
  if (folders.contains(k.name))
    return;
  folders[k.name] = QJsonObject{ { "en", k.name },
                                 { "de", QString::fromUtf8(k.de) },
                                 { "fr", QString::fromUtf8(k.fr) },
                                 { "es", QString::fromUtf8(k.es) },
                                 { "pl", QString::fromUtf8(k.pl) } };
  json["folders"] = folders;
  QSaveFile file(QDir(project).filePath("project.json"));
  if (file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(json).toJson()) > 0)
    file.commit();
}
}

void ProjectFolder::set(const QString &dir)
{
  folder() = dir;
}

QString ProjectFolder::current()
{
  return folder();
}

void ProjectFolder::setByType(bool on)
{
  sortByType() = on;
}

bool ProjectFolder::byType()
{
  return sortByType();
}

ProjectFolder::Kind ProjectFolder::kindOf(const QString &file)
{
  const QString suffix = QFileInfo(file).suffix().toLower();
  if (suffix == "qtdmm-workspace")
    return Benches;
  if (suffix == "qtdmm-rec" || suffix == "csv")
    return Recordings;
  static const QStringList pictures{ "png", "jpg", "jpeg", "svg", "bmp", "gif", "webp" };
  if (pictures.contains(suffix))
    return Images;
  if (suffix == "lml")
    return Analysis;
  return Documents;
}

QString ProjectFolder::folderName(Kind kind)
{
  for (const KindNames &k : kNames)
    if (k.kind == kind)
      return QString::fromLatin1(k.name);
  return QString();
}

QString ProjectFolder::dirFor(const QString &project, Kind kind)
{
  if (project.isEmpty() || kind == Root || !byType())
    return project;
  for (const KindNames &k : kNames)
  {
    if (k.kind != kind)
      continue;
    // the names first: the project view shows the folder by them as it appears
    enterFolder(project, k);
    const QString dir = QDir(project).filePath(k.name);
    return QDir().mkpath(dir) ? dir : project;
  }
  return project;
}

QString ProjectFolder::proposal(const QString &proposal)
{
  if (folder().isEmpty())
    return proposal;
  const QString name = QFileInfo(proposal).fileName();
  return name.isEmpty() ? folder() : QDir(folder()).filePath(name);
}

ProjectFolder::Proposal::Proposal(const QString &proposal, Kind kind)
{
  const QString project = folder();
  if (project.isEmpty())
  {
    m_path = proposal;
    return;
  }
  const QString dir = QDir(project).filePath(folderName(kind));
  const bool existed = QFileInfo(dir).isDir();
  const QString target = dirFor(project, kind);
  if (!existed && target != project)
    m_made = target;
  const QString name = QFileInfo(proposal).fileName();
  m_path = name.isEmpty() ? target : QDir(target).filePath(name);
}

void ProjectFolder::Proposal::keep(const QString &chosen)
{
  if (!m_made.isEmpty() && QFileInfo(chosen).absolutePath() == QFileInfo(m_made).absoluteFilePath())
    m_made.clear();
}

ProjectFolder::Proposal::~Proposal()
{
  // rmdir() leaves a folder with a file in it alone
  if (!m_made.isEmpty())
    QDir().rmdir(m_made);
}
