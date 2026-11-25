// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// BasketWidget: the folder of a project, copies in under free names.

#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QToolButton>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QTabWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
#include <QFileSystemModel>

#include "core/photostore.h"
#include "core/projectfolder.h"
#include "ui/dialogs/photodlg.h"
#include "recording/zipfile.h"
#include "ui/views/basketwidget.h"
#include "testutil.h"

#include <QLocale>

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
  // the symbols of the list, by suffix
  check(BasketWidget::fileIconName("Bilder", true) == "folder", "symbol: a folder");
  check(BasketWidget::fileIconName("2026-11-09 UT61E.CSV", false) == "text-csv"
          && BasketWidget::fileIconName("a.png", false) == "image-x-generic"
          && BasketWidget::fileIconName("a.lml", false) == "office-chart-line"
          && BasketWidget::fileIconName("solar.qtdmm-workspace", false) == "qtdmm-arrange"
          && BasketWidget::fileIconName("a.xlsx", false) == "x-office-spreadsheet",
        "symbols: CSV, picture, LabPlot project, bench, spreadsheet");
  check(BasketWidget::fileIconName("a.xyz", false).isEmpty(), "symbol: an unknown suffix leaves it to Qt");
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
  // (by kind off: into the project itself; folders of a kind further down)
  ProjectFolder::setByType(false);
  const QStringList added = basket.addFiles({ dir.filePath("a.csv"), dir.filePath("a.csv"), dir.filePath("missing.pdf") });
  check(added == QStringList({ dir.filePath("Bench/a.csv"), dir.filePath("Bench/a (2).csv") }),
        "copied under free names: " + added.join(", "));
  check(QFileInfo::exists(dir.filePath("a.csv")), "the original stays");
  check(errors.size() == 1, QString("the missing file is named once, %1 errors").arg(errors.size()));
  // a file of the basket itself is not copied again
  check(basket.addFiles({ dir.filePath("Bench/a.csv") }).isEmpty(), "a basket file stays as it is");

  // folders in a project: made under free names, files copied or moved in
  {
    basket.setFolder(dir.filePath("Folders"));
    const QString f1 = basket.newFolder();
    const QString f2 = basket.newFolder();
    check(QFileInfo(f1).isDir() && QFileInfo(f2).isDir() && f1 != f2
            && QFileInfo(f1).absolutePath() == dir.filePath("Folders"),
          "two new folders at the top: " + f1 + ", " + f2);
    const QString sub = basket.newFolder(f1);
    check(QFileInfo(sub).absolutePath() == f1, "a folder in a folder");
    const QStringList in = basket.addFiles({ dir.filePath("a.csv") }, f1);
    check(in == QStringList { QDir(f1).filePath("a.csv") }, "copied into a folder: " + in.join(", "));
    const QStringList moved = basket.moveInto(in, f2);
    check(moved == QStringList { QDir(f2).filePath("a.csv") } && !QFileInfo::exists(in.value(0)),
          "moved into another folder");
    check(basket.moveInto({ f1 }, sub).isEmpty() && QFileInfo(f1).isDir(), "a folder not into one under itself");
    check(basket.moveInto({ f1 }, f1).isEmpty(), "nor into itself");
    check(basket.moveInto(moved, f2).isEmpty(), "nor where it is");
    // its name is taken at the top: it gets a free one
    const QString up = basket.moveInto({ sub }, dir.filePath("Folders")).value(0);
    check(QFileInfo(up).isDir() && QFileInfo(up).absolutePath() == dir.filePath("Folders") && up != f1 && up != f2,
          "a folder up to the top: " + up);
    basket.setFolder(dir.filePath("Bench"));
  }

  // project.qtdmm: written once, never over the user's
  check(BasketWidget::writeProjectFile(dir.filePath("Bench")), "project.qtdmm written");
  QFile project(dir.filePath("Bench/project.qtdmm"));
  check(project.open(QIODevice::ReadOnly) && project.readAll().contains("\"name\": \"Bench\""), "it names the project");
  project.close();
  write(dir.filePath("Bench/project.qtdmm"), "{\"name\": \"mine\"}");
  check(BasketWidget::writeProjectFile(dir.filePath("Bench")), "project.qtdmm there: fine");
  check(project.open(QIODevice::ReadOnly) && project.readAll().contains("mine"), "and left as it was");
  project.close();   // Windows keeps an open file from being replaced

  // the description: set, the other keys stay; a basket without project.qtdmm gets one
  check(BasketWidget::setProjectDescription(dir.filePath("Bench"), "Ripple of the 12 V supply"), "description set");
  QJsonObject p = BasketWidget::readProject(dir.filePath("Bench"));
  check(p.value("description").toString() == "Ripple of the 12 V supply" && p.value("name").toString() == "mine",
        "description in, name kept");
  check(BasketWidget::setProjectValue(dir.filePath("Bench"), "lastBench", "benches/Lab.qtdmm-workspace"), "a key set");
  p = BasketWidget::readProject(dir.filePath("Bench"));
  check(p.value("lastBench").toString() == "benches/Lab.qtdmm-workspace" && p.value("description").toString().startsWith("Ripple"),
        "the bench used last in, the description kept");
  QDir().mkpath(dir.filePath("Other"));
  check(BasketWidget::setProjectDescription(dir.filePath("Other"), "x")
          && BasketWidget::readProject(dir.filePath("Other")).value("format").toInt() == 1, "made first when missing");
  check(BasketWidget::readProject(dir.filePath("none")).isEmpty(), "no project.qtdmm: empty");
  // broken by hand: neither the description nor anything else overwrites it (R19-07)
  QDir().mkpath(dir.filePath("Broken"));
  const QByteArray broken = "{\"name\": \"mine\", \"created\": \"2026\",}";
  write(dir.filePath("Broken/project.qtdmm"), broken);
  bool parsed = true;
  BasketWidget::readProject(dir.filePath("Broken"), &parsed);
  check(!parsed && !BasketWidget::setProjectDescription(dir.filePath("Broken"), "x"),
        "broken project.qtdmm: description refused");
  {
    QFile f(dir.filePath("Broken/project.qtdmm"));
    check(f.open(QIODevice::ReadOnly) && f.readAll() == broken, "and the file left as it was");
  }
  check(BasketWidget::projectName(dir.filePath("Bench")) == "mine", "shown by the name in project.qtdmm");
  check(BasketWidget::projectName(dir.filePath("none")) == "none", "without one by its folder");
  // [E 25.11.] the default project: in the folder for new projects, made
  // once needed, made anew when gone or broken
  {
    QDir().mkpath(dir.filePath("newbase"));
    basket.setBaseFolder(dir.filePath("newbase"));
    const QString def = QDir(dir.filePath("newbase/default")).absolutePath();
    check(ProjectFolder::defaultProject() == def && ProjectFolder::isDefault(dir.filePath("newbase/default"))
            && !ProjectFolder::isDefault(dir.filePath("Bench")),
          "the default project: default in the folder for new projects");
    check(BasketWidget::ensureDefaultProject(false) == def && !QFileInfo::exists(def), "not made before it is needed");
    check(BasketWidget::ensureDefaultProject() == def && QFileInfo::exists(def + "/project.qtdmm"), "made when needed");
    check(BasketWidget::projectName(def) == "Default", "shown as Default, not by its folder");
    write(def + "/project.qtdmm", "{ broken");
    write(def + "/a.csv", "1");
    check(BasketWidget::ensureDefaultProject(false) == def && QFileInfo::exists(def + "/project.qtdmm.broken")
            && BasketWidget::readProject(def).value("format").toInt() == 1 && QFileInfo::exists(def + "/a.csv"),
          "broken: a new project file, the broken one kept beside, the files stay");
    QDir(def).removeRecursively();
    check(BasketWidget::ensureDefaultProject() == def && BasketWidget::readProject(def).value("format").toInt() == 1,
          "deleted: made anew, empty");
  }

  // a project of before 27.1: project.json read, renamed on the first write
  QDir().mkpath(dir.filePath("Legacy"));
  write(dir.filePath("Legacy/project.json"), "{\"name\": \"old one\"}");
  check(BasketWidget::projectName(dir.filePath("Legacy")) == "old one" && BasketWidget::projectOf(dir.filePath("Legacy/x.csv")) == dir.filePath("Legacy"),
        "project.json of before 27.1 read, the folder a project");
  check(BasketWidget::setProjectValue(dir.filePath("Legacy"), "lastBench", "a.qtdmm-workspace")
          && QFileInfo::exists(dir.filePath("Legacy/project.qtdmm")) && !QFileInfo::exists(dir.filePath("Legacy/project.json"))
          && BasketWidget::readProject(dir.filePath("Legacy")).value("name").toString() == "old one",
        "written: renamed to project.qtdmm, its keys kept");

  // project.qtdmm is QtDMM's: not listed, and alone it leaves the project empty
  {
    basket.setFolder(dir.filePath("Other"));
    auto *view = basket.findChild<QTreeView *>();
    auto *model = qobject_cast<QFileSystemModel *>(view ? view->model() : nullptr);
    QLabel *empty = nullptr;
    for (QLabel *l : basket.findChildren<QLabel *>())
      if (l->text().startsWith("Empty"))
        empty = l;
    check(model && empty, "the list and the empty note are there");
    if (model && empty)
    {
      const QModelIndex file = model->index(dir.filePath("Other/project.qtdmm"));
      for (int i = 0; i < 100 && model->rowCount(file.parent()) == 0; ++i)
        QTest::qWait(20);
      check(view->isRowHidden(file.row(), file.parent()), "project.qtdmm not listed");
      check(!empty->isHidden(), "project.qtdmm alone: empty");
      write(dir.filePath("Other/a.csv"), "1");
      for (int i = 0; i < 150 && !empty->isHidden(); ++i)
        QTest::qWait(20);
      check(empty->isHidden(), "a file in it: not empty");
      const QModelIndex again = model->index(dir.filePath("Other/project.qtdmm"));
      check(view->isRowHidden(again.row(), again.parent()), "project.qtdmm stays unlisted with a file next to it");
    }
    basket.setFolder(dir.filePath("Bench"));
  }

  // the project as a ZIP: every file under the basket's name, hidden ones
  // and the archive itself left out, project.qtdmm with the file list
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
  check(zipEntry(zip, "Bench/a.csv", qint64(4) << 30) == QByteArray("1")
          && zipEntry(zip, "Bench/a (2).csv", qint64(4) << 30) == QByteArray("1"),
        "the files under the basket's name");
  check(zipEntry(zip, "Bench/pictures/graph.png", qint64(4) << 30) == QByteArray("not really a PNG"),
        "sub folders too, stored");
  check(!zipEntry(zip, "Bench/.git/HEAD", qint64(4) << 30) && !zipEntry(zip, "Bench/.directory", qint64(4) << 30)
          && !zipEntry(zip, "Bench/Bench.zip", qint64(4) << 30),
        "hidden files and the archive itself stay out");
  const QJsonObject manifest = QJsonDocument::fromJson(zipEntry(zip, "Bench/project.qtdmm", qint64(4) << 30).value_or("")).object();
  check(manifest.value("description").toString() == "Ripple of the 12 V supply", "project.qtdmm keeps its keys");
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
        "project.qtdmm without the export part");
  check(BasketWidget::importProject(dir.filePath("Bench/Bench.zip"), dir.filePath("base"), &folder, &why)
          && folder == dir.filePath("base/Bench 2") && BasketWidget::projectName(folder) == "Bench 2",
        "the name taken: Bench 2, also in its project file, got " + folder);
  check(BasketWidget::zipProjectName(dir.filePath("Bench/Bench.zip")) == "Bench", "the name before a clash: Bench");
  // a file changed after the export: refused, nothing written
  const QByteArray exportedProject = zipEntry(zip, "Bench/project.qtdmm", qint64(4) << 30).value_or("");
  const QString tampered = zipOf("changed.zip", { { "Bench/project.qtdmm", exportedProject }, { "Bench/a.csv", "2" },
                                                  { "Bench/a (2).csv", "1" },
                                                  { "Bench/pictures/graph.png", "not really a PNG" } });
  check(!BasketWidget::importProject(tampered, dir.filePath("base"), &folder, &why) && why.contains("a.csv")
          && !QFileInfo::exists(dir.filePath("base/Bench 3")),
        "a changed file: refused, nothing written: " + why);
  // no way out of the folder
  const QString slip = zipOf("slip.zip", { { "Bench/../../evil.txt", "x" } });
  check(!BasketWidget::importProject(slip, dir.filePath("base"), &folder, &why)
          && !QFileInfo::exists(dir.filePath("evil.txt")),
        "a path with ..: refused: " + why);
  check(!BasketWidget::importProject(zipOf("abs.zip", { { "/evil.txt", "x" } }), dir.filePath("base"), &folder, &why),
        "an absolute path: refused");
  // absolute only after the top folder is cut off (R19-03)
  const QString hidden = dir.filePath("hidden/evil.txt");
  check(!BasketWidget::importProject(zipOf("abs2.zip", { { "Bench/ok.csv", "1" }, { "Bench/" + hidden.toUtf8(), "x" } }),
                                     dir.filePath("base"), &folder, &why)
          && !QFileInfo::exists(hidden),
        "Bench//<absolute path>: refused: " + why);
  check(!BasketWidget::importProject(zipOf("drive.zip", { { "Bench/ok.csv", "1" }, { "Bench/C:/evil.txt", "x" } }),
                                     dir.filePath("base"), &folder, &why),
        "Bench/C:/...: refused");
  // [E 25.11.] a ZIP without a project file: refused, nothing written
  check(!BasketWidget::importProject(zipOf("Loose files.zip", { { "x.csv", "1" }, { "sub/y.txt", "2" } }),
                                     dir.filePath("base"), &folder, &why)
          && why.contains("project file") && !QFileInfo::exists(dir.filePath("base/Loose files")),
        "no project file: refused: " + why);
  // a project without one top folder: named like the file
  check(BasketWidget::importProject(zipOf("Flat.zip", { { "project.qtdmm", "{\"format\": 1}" }, { "sub/y.txt", "2" } }),
                                    dir.filePath("base"), &folder, &why)
          && folder == dir.filePath("base/Flat") && read(dir.filePath("base/Flat/sub/y.txt")) == "2"
          && BasketWidget::zipProjectName(dir.filePath("Flat.zip")) == "Flat",
        "project.qtdmm at the top: named like the ZIP: " + why);
  // an export of before 27.1: project.json in it, unpacked as project.qtdmm
  check(BasketWidget::importProject(zipOf("Old export.zip", { { "Old export/project.json", "{\"name\": \"old\"}" },
                                                              { "Old export/a.csv", "1" } }),
                                    dir.filePath("base"), &folder, &why)
          && QFileInfo::exists(dir.filePath("base/Old export/project.qtdmm"))
          && !QFileInfo::exists(dir.filePath("base/Old export/project.json"))
          && BasketWidget::projectName(folder) == "old",
        "an old export: project.json becomes project.qtdmm: " + why);
  write(dir.filePath("nozip.zip"), "plain text");
  check(!BasketWidget::importProject(dir.filePath("nozip.zip"), dir.filePath("base"), &folder, &why) && !why.isEmpty(),
        "no ZIP: an error");

  // a bench belongs to the project in whose folder it lies, subfolders too;
  // the open one counts without a project.qtdmm (made before 08.11.)
  {
    QDir().mkpath(dir.filePath("P/sub/deeper"));
    BasketWidget::writeProjectFile(dir.filePath("P"));
    QDir().mkpath(dir.filePath("Old/benches"));
    QDir().mkpath(dir.filePath("Elsewhere"));
    const QString proj = QDir(dir.filePath("P")).absolutePath();
    check(BasketWidget::projectOf(dir.filePath("P/a.qtdmm-ws")) == proj, "in the project folder");
    check(BasketWidget::projectOf(dir.filePath("P/sub/deeper/b.qtdmm-ws")) == proj, "in a subfolder of it");
    check(BasketWidget::projectOf(dir.filePath("Elsewhere/c.qtdmm-ws")).isEmpty()
            || !BasketWidget::projectOf(dir.filePath("Elsewhere/c.qtdmm-ws")).startsWith(dir.path() + "/Elsewhere"),
          "outside: not a project of its own");
    const QString old = QDir(dir.filePath("Old")).absolutePath();
    check(BasketWidget::projectOf(dir.filePath("Old/benches/d.qtdmm-ws"), old) == old,
          "the open project without project.qtdmm");
    check(BasketWidget::projectOf(dir.filePath("Old/benches/d.qtdmm-ws")) != old, "not open, no project.qtdmm: none");
  }

  // folders of a kind: English on disk, made with the first file, the names
  // in each language in project.qtdmm
  {
    ProjectFolder::setByType(true);
    const QString proj = QDir(dir.filePath("Kinds")).absolutePath();
    QDir().mkpath(proj);
    BasketWidget::writeProjectFile(proj);
    check(ProjectFolder::kindOf("x.qtdmm-workspace") == ProjectFolder::Benches
            && ProjectFolder::kindOf("x.CSV") == ProjectFolder::Recordings
            && ProjectFolder::kindOf("x.qtdmm-rec") == ProjectFolder::Recordings
            && ProjectFolder::kindOf("x.png") == ProjectFolder::Images
            && ProjectFolder::kindOf("x.lml") == ProjectFolder::Analysis
            && ProjectFolder::kindOf("x.pdf") == ProjectFolder::Documents
            && ProjectFolder::kindOf("notes") == ProjectFolder::Documents,
          "kinds by suffix");
    check(!QFileInfo::exists(proj + "/recordings"), "no folder before its first file");
    check(ProjectFolder::dirFor(proj, ProjectFolder::Recordings) == proj + "/recordings"
            && QFileInfo(proj + "/recordings").isDir(), "made when wanted");
    const QJsonObject names = BasketWidget::readProject(proj).value("folders").toObject().value("recordings").toObject();
    check(names.value("de").toString() == "Aufzeichnungen" && names.value("pl").toString() == "Nagrania"
            && BasketWidget::readProject(proj).value("name").toString() == "Kinds",
          "project.qtdmm: its names, the other keys stay");
    check(ProjectFolder::dirFor(proj, ProjectFolder::Root) == proj, "Root: the project");
    {
      const QString brokenDir = QDir(dir.filePath("Broken")).absolutePath();
      ProjectFolder::dirFor(brokenDir, ProjectFolder::Images);
      QFile f(brokenDir + "/project.qtdmm");
      check(QFileInfo(brokenDir + "/images").isDir() && f.open(QIODevice::ReadOnly) && f.readAll() == broken,
            "a folder of a kind in a broken project: project.qtdmm left as it was");
    }
    ProjectFolder::setByType(false);
    check(ProjectFolder::dirFor(proj, ProjectFolder::Images) == proj && !QFileInfo::exists(proj + "/images"),
          "by kind off: the project");
    ProjectFolder::setByType(true);

    // added into the project: each into the folder of its kind; onto a folder: there
    basket.setFolder(proj);
    write(dir.filePath("plot.png"), "png");
    write(dir.filePath("sheet.pdf"), "pdf");
    write(dir.filePath("bench.qtdmm-workspace"), "{}");
    const QStringList in = basket.addFiles({ dir.filePath("a.csv"), dir.filePath("plot.png"),
                                             dir.filePath("sheet.pdf"), dir.filePath("bench.qtdmm-workspace") });
    check(in == QStringList({ proj + "/recordings/a.csv", proj + "/images/plot.png", proj + "/documents/sheet.pdf",
                              proj + "/benches/bench.qtdmm-workspace" }),
          "sorted by kind: " + in.join(", "));
    check(BasketWidget::projectOf(proj + "/benches/bench.qtdmm-workspace") == proj, "a bench in benches/ belongs");
    QDir().mkpath(proj + "/mine");
    check(basket.addFiles({ dir.filePath("plot.png") }, proj + "/mine") == QStringList { proj + "/mine/plot.png" },
          "onto a folder: into it");

    // a save dialog's folder: kept with a file, gone again without one; only
    // a folder with a file is named in project.qtdmm
    auto named = [&proj](const char *folder)
    {
      QFile f(proj + "/project.qtdmm");
      return f.open(QIODevice::ReadOnly)
             && QJsonDocument::fromJson(f.readAll()).object().value("folders").toObject().contains(folder);
    };
    {
      ProjectFolder::Proposal where("/elsewhere/run.log", ProjectFolder::Analysis);
      check(where.path() == proj + "/analysis/run.log" && QFileInfo(proj + "/analysis").isDir(),
            "proposed in its folder: " + where.path());
    }
    check(!QFileInfo::exists(proj + "/analysis") && !named("analysis"), "cancelled: the folder is gone again, not named");
    {
      ProjectFolder::Proposal where("x.pdf", ProjectFolder::Documents);
      check(where.path() == proj + "/documents/x.pdf", "an existing folder");
    }
    check(QFileInfo(proj + "/documents").isDir(), "and it stays");
    {
      ProjectFolder::Proposal where("x.png", ProjectFolder::Analysis);
      where.keep(proj + "/analysis/x.png");
    }
    check(QFileInfo(proj + "/analysis").isDir() && named("analysis"), "chosen there, written later: it stays, named");
    {
      // a project of its own, without any folder yet
      const QString other = dir.filePath("other");
      QDir().mkpath(other);
      ProjectFolder::set(other);
      {
        ProjectFolder::Proposal where("y.png", ProjectFolder::Images);
        write(other + "/images/y.png", "png");
      }
      QFile f(other + "/project.qtdmm");
      check(f.open(QIODevice::ReadOnly)
              && QJsonDocument::fromJson(f.readAll()).object().value("folders").toObject().contains("images"),
            "a file written while the dialog's proposal lives: named");
      ProjectFolder::set(proj);
    }

    // the view shows the names of the program's language, F2 without a change keeps the disk's
    QLocale::setDefault(QLocale(QLocale::German));
    auto *view = basket.findChild<QTreeView *>();
    QAbstractItemModel *model = view ? view->model() : nullptr;
    auto *fs = qobject_cast<QFileSystemModel *>(model);
    if (fs)
    {
      const QModelIndex rec = fs->index(proj + "/recordings");
      for (int i = 0; i < 100 && fs->rowCount(rec.parent()) < 5; ++i)
        QTest::qWait(20);
      check(fs->data(fs->index(proj + "/recordings")).toString() == "Aufzeichnungen",
            "shown in German: " + fs->data(fs->index(proj + "/recordings")).toString());
      check(fs->data(fs->index(proj + "/mine")).toString() == "mine", "a folder of one's own: its name");
      check(fs->setData(fs->index(proj + "/recordings"), "Aufzeichnungen") && QFileInfo(proj + "/recordings").isDir(),
            "renamed to the name shown: stays on disk");
    }
    else
      check(false, "the list's model");
    QLocale::setDefault(QLocale(QLocale::English));
  }

  // photos of the setup: into "photos", named by their time, kept as they came
  {
    ProjectFolder::setByType(true);
    const QString proj = QDir(dir.filePath("Photos")).absolutePath();
    QDir().mkpath(proj);
    BasketWidget::writeProjectFile(proj);
    QImage wide(40, 20, QImage::Format_RGB32);
    wide.fill(Qt::red);
    QByteArray png;
    {
      QBuffer buffer(&png);
      buffer.open(QIODevice::WriteOnly);
      wide.save(&buffer, "PNG");
    }
    QString suffix;
    check(PhotoStore::isPhoto(png, &suffix) && suffix == "png" && !PhotoStore::isPhoto("no picture"), "photo: a picture or not");
    const QDateTime when(QDate(2026, 11, 22), QTime(14, 3, 12));
    QString why;
    const QString first = PhotoStore::save(proj, png, when, &why);
    const QString second = PhotoStore::save(proj, png, when, &why);
    check(first == proj + "/photos/photo_2026-11-22_14-03-12.png" && second == proj + "/photos/photo_2026-11-22_14-03-12_2.png",
          "photo: in photos, named by its time, a second one of the second counts on " + first + " " + second);
    QFile saved(first);
    check(saved.open(QIODevice::ReadOnly) && saved.readAll() == png, "photo: kept as it came");
    check(BasketWidget::readProject(proj).value("folders").toObject().value("photos").toObject().value("de").toString() == "Fotos",
          "photo: the folder's names in project.qtdmm");
    check(PhotoStore::save(proj, "no picture", when, &why).isEmpty() && !why.isEmpty(), "photo: no picture, not saved");
    check(PhotoStore::relative(proj, first) == "photos/photo_2026-11-22_14-03-12.png"
            && PhotoStore::absolute(proj, "photos/photo_2026-11-22_14-03-12.png") == first
            && PhotoStore::relative(proj, dir.filePath("elsewhere.png")) == dir.filePath("elsewhere.png")
            && PhotoStore::absolute(proj, dir.filePath("elsewhere.png")) == dir.filePath("elsewhere.png"),
          "photo: its path in the project and back, outside it stays full");
    check(PhotoStore::load(first).size() == QSize(40, 20), "photo: loaded");

    // the dialog: a file taken in, shown, said
    PhotoDlg dlg(proj);
    QSignalSpy added(&dlg, &PhotoDlg::photoAdded);
    QSignalSpy failedSpy(&dlg, &PhotoDlg::error);
    const QString src = dir.filePath("camera.png");
    write(src, png);
    write(dir.filePath("broken.jpg"), "no picture");
    check(dlg.addFile(src) && added.size() == 1 && dlg.photoCount() == 1
            && QFileInfo(added.at(0).at(0).toString()).dir().absolutePath() == proj + "/photos",
          "dialog: a file into the photos");
    check(!dlg.addFile(dir.filePath("broken.jpg")) && failedSpy.size() == 1 && dlg.photoCount() == 1,
          "dialog: no picture, an error");

    // deleted: into the trash (or gone where there is none), out of the
    // strip, said; what lands in the trash is taken out of it again
    QDir trash;
    const auto untrash = [&](const QString &name) {
      if (trash.path() == ".")
        return;
      QFile::remove(trash.filePath(name));
      QFile::remove(trash.filePath("../info/" + name + ".trashinfo"));
    };
    {
      const QString doomed = PhotoStore::save(proj, png, when, &why);
      QString trashed;
      check(PhotoStore::remove(doomed, &trashed, &why) && !QFileInfo::exists(doomed), "photo: removed");
      if (!trashed.isEmpty())
        trash = QFileInfo(trashed).dir();
      untrash(QFileInfo(trashed).fileName());
    }
    QSignalSpy removed(&dlg, &PhotoDlg::photoRemoved);
    check(dlg.addFile(src) && dlg.photoCount() == 2, "dialog: a second photo");
    const QString newest = added.last().at(0).toString();
    const QString older = added.first().at(0).toString();
    // shown, on the file's tab: the strip has its places, no server runs
    auto *sources = dlg.findChild<QTabWidget *>("ui_photoSources");
    sources->setCurrentIndex(sources->count() - 1);
    dlg.show();
    check(QTest::qWaitForWindowExposed(&dlg), "dialog: shown");
    auto *strip = dlg.findChild<QListWidget *>("ui_photoStrip");
    if (qEnvironmentVariableIsSet("QTDMM_SHOT"))
      dlg.grab().save(qEnvironmentVariable("QTDMM_SHOT") + "/photo_delete.png");
    const QRect button = dlg.removeRect(0);
    check(!button.isEmpty() && strip->visualItemRect(strip->item(0)).contains(button), "dialog: a delete button on the picture");
    QTest::mouseClick(strip->viewport(), Qt::LeftButton, Qt::NoModifier, button.center());
    check(removed.size() == 1 && removed.at(0).at(0).toString() == newest && !QFileInfo::exists(newest)
            && QFileInfo::exists(older) && dlg.photoCount() == 1 && strip->currentRow() == 0,
          "dialog: its button deletes the newest, the other one shows");
    untrash(QFileInfo(newest).fileName());
    strip->setFocus();
    QTest::keyClick(strip, Qt::Key_Delete);
    check(removed.size() == 2 && !QFileInfo::exists(older) && dlg.photoCount() == 0 && strip->isHidden()
            && dlg.findChild<QLabel *>("ui_photoPreview")->text() == "No photo yet",
          "dialog: Del deletes the one chosen; none left, the strip goes");
    untrash(QFileInfo(older).fileName());
    dlg.hide();

    // the project window's button asks for it
    BasketWidget photoBasket;
    photoBasket.setFolder(proj);
    QSignalSpy asked(&photoBasket, &BasketWidget::photoRequested);
    if (auto *button = photoBasket.findChild<QToolButton *>("ui_basketPhoto"))
    {
      button->click();
      check(asked.size() == 1, "project window: the photo button");
    }
    else
      check(false, "project window: the photo button");
  }

  if (failed == 0)
    qInfo() << "All basket tests passed.";
  else
    qWarning() << failed << "basket test(s) failed.";
  return failed == 0 ? 0 : 1;
}
