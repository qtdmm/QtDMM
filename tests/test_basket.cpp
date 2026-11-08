// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// BasketWidget: the folder of a project, copies in under free names.

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

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

  if (failed == 0)
    qInfo() << "All basket tests passed.";
  else
    qWarning() << failed << "basket test(s) failed.";
  return failed == 0 ? 0 : 1;
}
