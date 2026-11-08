// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// BasketWidget: the folder of a bench, copies in under free names.

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
  f.open(QIODevice::WriteOnly);
  f.write(data);
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir tmp;
  const QDir dir(tmp.path());

  // the folder: next to the bench file, its name without the suffix
  check(BasketWidget::folderFor(dir.filePath("Power supply.qtdmm-workspace")) == dir.filePath("Power supply"),
        "folder next to the bench, named after it: " + BasketWidget::folderFor(dir.filePath("Power supply.qtdmm-workspace")));
  check(BasketWidget::folderFor(QString()).isEmpty(), "no bench, no folder");

  // free names: never over a file
  write(dir.filePath("a.csv"), "1");
  write(dir.filePath("a (2).csv"), "2");
  check(BasketWidget::freeName(tmp.path(), "b.csv") == dir.filePath("b.csv"), "a free name stays");
  check(BasketWidget::freeName(tmp.path(), "a.csv") == dir.filePath("a (3).csv"), "a taken name counts on");
  check(BasketWidget::freeName(tmp.path(), "notes") == dir.filePath("notes"), "no suffix");

  BasketWidget basket;
  QSignalSpy errors(&basket, &BasketWidget::error);
  check(basket.addFiles({ dir.filePath("a.csv") }).isEmpty(), "no bench: nothing goes in");
  basket.setBench(dir.filePath("Bench.qtdmm-workspace"));
  check(!QFileInfo::exists(dir.filePath("Bench")), "the folder waits for the first file");

  // copies in, the originals stay; the same name twice gets a second name
  const QStringList added = basket.addFiles({ dir.filePath("a.csv"), dir.filePath("a.csv"), dir.filePath("missing.pdf") });
  check(added == QStringList({ dir.filePath("Bench/a.csv"), dir.filePath("Bench/a (2).csv") }),
        "copied under free names: " + added.join(", "));
  check(QFileInfo::exists(dir.filePath("a.csv")), "the original stays");
  check(errors.size() == 1, QString("the missing file is named once, %1 errors").arg(errors.size()));
  // a file of the basket itself is not copied again
  check(basket.addFiles({ dir.filePath("Bench/a.csv") }).isEmpty(), "a basket file stays as it is");

  if (failed == 0)
    qInfo() << "All basket tests passed.";
  else
    qWarning() << failed << "basket test(s) failed.";
  return failed == 0 ? 0 : 1;
}
