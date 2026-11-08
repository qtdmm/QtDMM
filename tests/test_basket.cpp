// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// BasketWidget: the folder of a project, copies in under free names.

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "recording/zipfile.h"
#include "ui/views/basketwidget.h"
#include "testutil.h"

static void write(const QString &path, const QByteArray &data)
{
  QFile f(path);
  check(f.open(QIODevice::WriteOnly) && f.write(data) == data.size(), "test file written: " + path);
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir tmp;
  const QDir dir(tmp.path());

  // free names: never over a file
  write(dir.filePath("a.csv"), "1");
  write(dir.filePath("a (2).csv"), "2");
  check(BasketWidget::freeName(tmp.path(), "b.csv") == dir.filePath("b.csv"), "a free name stays");
  check(BasketWidget::freeName(tmp.path(), "a.csv") == dir.filePath("a (3).csv"), "a taken name counts on");
  check(BasketWidget::freeName(tmp.path(), "notes") == dir.filePath("notes"), "no suffix");

  BasketWidget basket;
  QSignalSpy errors(&basket, &BasketWidget::error);
  QSignalSpy changed(&basket, &BasketWidget::folderChanged);
  check(basket.addFiles({ dir.filePath("a.csv") }).isEmpty(), "no basket open: nothing goes in");
  // a basket whose folder went away is made again with the first file
  basket.setFolder(dir.filePath("Bench"));
  basket.setFolder(dir.filePath("Bench"));
  check(changed.size() == 1 && basket.folder() == dir.filePath("Bench"), "opened once");

  // copies in, the originals stay; the same name twice gets a second name
  const QStringList added = basket.addFiles({ dir.filePath("a.csv"), dir.filePath("a.csv"), dir.filePath("missing.pdf") });
  check(added == QStringList({ dir.filePath("Bench/a.csv"), dir.filePath("Bench/a (2).csv") }),
        "copied under free names: " + added.join(", "));
  check(QFileInfo::exists(dir.filePath("a.csv")), "the original stays");
  check(errors.size() == 1, QString("the missing file is named once, %1 errors").arg(errors.size()));
  // a file of the basket itself is not copied again
  check(basket.addFiles({ dir.filePath("Bench/a.csv") }).isEmpty(), "a basket file stays as it is");

  // project.json: written once, never over the user's
  check(BasketWidget::writeProjectFile(dir.filePath("Bench")), "project.json written");
  QFile project(dir.filePath("Bench/project.json"));
  check(project.open(QIODevice::ReadOnly) && project.readAll().contains("\"name\": \"Bench\""), "it names the project");
  project.close();
  write(dir.filePath("Bench/project.json"), "{\"name\": \"mine\"}");
  check(BasketWidget::writeProjectFile(dir.filePath("Bench")), "project.json there: fine");
  check(project.open(QIODevice::ReadOnly) && project.readAll().contains("mine"), "and left as it was");
  project.close();   // Windows keeps an open file from being replaced

  // the description: set, the other keys stay; a basket without project.json gets one
  check(BasketWidget::setProjectDescription(dir.filePath("Bench"), "Ripple of the 12 V supply"), "description set");
  QJsonObject p = BasketWidget::readProject(dir.filePath("Bench"));
  check(p.value("description").toString() == "Ripple of the 12 V supply" && p.value("name").toString() == "mine",
        "description in, name kept");
  QDir().mkpath(dir.filePath("Other"));
  check(BasketWidget::setProjectDescription(dir.filePath("Other"), "x")
          && BasketWidget::readProject(dir.filePath("Other")).value("format").toInt() == 1, "made first when missing");
  check(BasketWidget::readProject(dir.filePath("none")).isEmpty(), "no project.json: empty");

  // the project as a ZIP: every file under the basket's name, hidden ones
  // and the archive itself left out, project.json with the file list
  QDir().mkpath(dir.filePath("Bench/pictures"));
  QDir().mkpath(dir.filePath("Bench/.git"));
  write(dir.filePath("Bench/pictures/graph.png"), "not really a PNG");
  write(dir.filePath("Bench/.git/HEAD"), "ref");
  write(dir.filePath("Bench/.directory"), "x");
  write(dir.filePath("Bench/Bench.zip"), "an older export");
  QString why;
  check(BasketWidget::exportProject(dir.filePath("Bench"), dir.filePath("Bench/Bench.zip"), &why),
        "exported into the basket itself: " + why);
  QFile zipFile(dir.filePath("Bench/Bench.zip"));
  check(zipFile.open(QIODevice::ReadOnly), "the archive is there");
  const QByteArray zip = zipFile.readAll();
  zipFile.close();
  check(zipEntry(zip, "Bench/a.csv") == QByteArray("1") && zipEntry(zip, "Bench/a (2).csv") == QByteArray("1"),
        "the files under the basket's name");
  check(zipEntry(zip, "Bench/pictures/graph.png") == QByteArray("not really a PNG"), "sub folders too, stored");
  check(!zipEntry(zip, "Bench/.git/HEAD") && !zipEntry(zip, "Bench/.directory") && !zipEntry(zip, "Bench/Bench.zip"),
        "hidden files and the archive itself stay out");
  const QJsonObject manifest = QJsonDocument::fromJson(zipEntry(zip, "Bench/project.json").value_or("")).object();
  check(manifest.value("description").toString() == "Ripple of the 12 V supply", "project.json keeps its keys");
  const QJsonObject exported = manifest.value("export").toObject();
  const QJsonArray files = exported.value("files").toArray();
  QStringList paths;
  for (const QJsonValue &f : files)
    paths << f.toObject().value("path").toString();
  check(paths == QStringList({ "a (2).csv", "a.csv", "pictures/graph.png" }), "the file list: " + paths.join(", "));
  check(files.at(2).toObject().value("size").toInt() == 16
          && files.at(2).toObject().value("sha256").toString()
               == QCryptographicHash::hash("not really a PNG", QCryptographicHash::Sha256).toHex(),
        "size and SHA-256");
  check(!exported.value("date").toString().isEmpty(), "export date");
  check(!BasketWidget::exportProject(dir.filePath("none"), dir.filePath("none.zip"), &why) && !why.isEmpty(),
        "no folder: an error that says so");

  if (failed == 0)
    qInfo() << "All basket tests passed.";
  else
    qWarning() << failed << "basket test(s) failed.";
  return failed == 0 ? 0 : 1;
}
