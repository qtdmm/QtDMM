// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// BasketWidget: the folder of a project, copies in under free names.

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/projectfolder.h"
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
  // the save dialogs start in it: a proposal keeps its name, goes into the basket
  check(ProjectFolder::current() == dir.filePath("Bench")
          && ProjectFolder::proposal("/elsewhere/ripple.csv") == dir.filePath("Bench/ripple.csv")
          && ProjectFolder::proposal(QString()) == dir.filePath("Bench"),
        "save dialogs: into the open basket, got " + ProjectFolder::proposal("/elsewhere/ripple.csv"));

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

  // no basket: the proposals as they were
  basket.setFolder(QString());
  check(ProjectFolder::current().isEmpty() && ProjectFolder::proposal("/elsewhere/ripple.csv") == "/elsewhere/ripple.csv",
        "save dialogs: without a basket where they were");
  // and back: unpacked into a folder of its own, checked, without "export"
  auto zipOf = [&](const QString &name, const QList<QPair<QByteArray, QByteArray>> &entries)
  {
    ZipWriter w;
    for (const auto &e : entries)
      w.add(e.first, e.second, false);
    write(dir.filePath(name), w.finish());
    return dir.filePath(name);
  };
  QDir().mkpath(dir.filePath("base"));
  QString folder;
  check(BasketWidget::importProject(dir.filePath("Bench/Bench.zip"), dir.filePath("base"), &folder, &why)
          && folder == dir.filePath("base/Bench"),
        "imported as base/Bench: " + why + " " + folder);
  auto read = [](const QString &path)
  {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("(none)");
  };
  check(read(dir.filePath("base/Bench/pictures/graph.png")) == "not really a PNG"
          && read(dir.filePath("base/Bench/a (2).csv")) == "1",
        "the files where they were");
  const QJsonObject back = BasketWidget::readProject(dir.filePath("base/Bench"));
  check(back.value("description").toString() == "Ripple of the 12 V supply" && !back.contains("export"),
        "project.json without the export part");
  check(BasketWidget::importProject(dir.filePath("Bench/Bench.zip"), dir.filePath("base"), &folder, &why)
          && folder == dir.filePath("base/Bench (2)"),
        "the name taken: Bench (2), got " + folder);
  // a file changed after the export: refused, nothing written
  const QByteArray exportedProject = zipEntry(zip, "Bench/project.json").value_or("");
  const QString tampered = zipOf("changed.zip", { { "Bench/project.json", exportedProject }, { "Bench/a.csv", "2" },
                                                  { "Bench/a (2).csv", "1" },
                                                  { "Bench/pictures/graph.png", "not really a PNG" } });
  check(!BasketWidget::importProject(tampered, dir.filePath("base"), &folder, &why) && why.contains("a.csv")
          && !QFileInfo::exists(dir.filePath("base/Bench (3)")),
        "a changed file: refused, nothing written: " + why);
  // no way out of the folder
  const QString slip = zipOf("slip.zip", { { "Bench/../../evil.txt", "x" } });
  check(!BasketWidget::importProject(slip, dir.filePath("base"), &folder, &why)
          && !QFileInfo::exists(dir.filePath("evil.txt")),
        "a path with ..: refused: " + why);
  check(!BasketWidget::importProject(zipOf("abs.zip", { { "/evil.txt", "x" } }), dir.filePath("base"), &folder, &why),
        "an absolute path: refused");
  // any ZIP without one top folder: named like the file, a basket then
  check(BasketWidget::importProject(zipOf("Loose files.zip", { { "x.csv", "1" }, { "sub/y.txt", "2" } }),
                                    dir.filePath("base"), &folder, &why)
          && folder == dir.filePath("base/Loose files") && read(dir.filePath("base/Loose files/sub/y.txt")) == "2"
          && BasketWidget::readProject(folder).value("format").toInt() == 1,
        "loose files: a basket named like the ZIP: " + why);
  write(dir.filePath("nozip.zip"), "plain text");
  check(!BasketWidget::importProject(dir.filePath("nozip.zip"), dir.filePath("base"), &folder, &why) && !why.isEmpty(),
        "no ZIP: an error");

  if (failed == 0)
    qInfo() << "All basket tests passed.";
  else
    qWarning() << failed << "basket test(s) failed.";
  return failed == 0 ? 0 : 1;
}
