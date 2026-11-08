// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ExportDlg: formats per kind, where to, the last choice kept.

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QTemporaryDir>

#include "core/settings.h"
#include "ui/dialogs/exportdlg.h"
#include "ui/labplot.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir tmp;
  Settings cfg("exportdlg", tmp.path());

  check(ExportDlg::formats(ExportDlg::Recording).size() == 7 && ExportDlg::formats(ExportDlg::Table).size() == 3
          && ExportDlg::formats(ExportDlg::Picture).first().first == "png", "formats of each kind");

  {
    // no basket: only a file; the format gives the suffix
    ExportDlg dlg(ExportDlg::Table, QString(), &cfg);
    auto *basket = dlg.findChildren<QRadioButton *>().first();
    check(!basket->isEnabled() && !dlg.toBasket(), "no basket open: a file");
    dlg.findChild<QLineEdit *>()->setText(tmp.path() + "/table");
    auto combos = dlg.findChildren<QComboBox *>();
    combos[1]->setCurrentIndex(1);   // xlsx
    check(dlg.what() == ExportDlg::Table && dlg.file() == tmp.path() + "/table.xlsx", "suffix from the format: " + dlg.file());
    dlg.accept();
  }
  {
    // the last format of the kind comes back; every reading is csv too
    ExportDlg dlg(ExportDlg::Table, tmp.path(), &cfg);
    check(dlg.format() == "xlsx", "last format kept: " + dlg.format());
    // a basket open: the file field is in it too, not in the last folder
    const auto edits = dlg.findChildren<QLineEdit *>();
    check(!edits.isEmpty() && QDir::fromNativeSeparators(edits.first()->text()).startsWith(tmp.path() + '/'),
          "file field in the basket: " + (edits.isEmpty() ? QString() : edits.first()->text()));
    auto combos = dlg.findChildren<QComboBox *>();
    combos[0]->setCurrentIndex(0);   // the recording
    combos[1]->setCurrentIndex(1);   // every reading
    check(dlg.format() == "csv" && dlg.everyReading(), "every reading: csv");
    check(!dlg.toBasket(), "a file was chosen last time");
    dlg.findChildren<QRadioButton *>().first()->setChecked(true);
    check(dlg.toBasket() && dlg.file().isEmpty(), "into the basket: no file");
  }

  {
    // LabPlot: the checkbox takes the format made for it, another format drops it
    ExportDlg dlg(ExportDlg::Recording, QString(), &cfg);
    auto *labPlot = dlg.findChild<QCheckBox *>();
    auto *fileEdit = dlg.findChild<QLineEdit *>();
    labPlot->setChecked(true);
    check(dlg.openInLabPlot() && dlg.format() == "lml", "LabPlot: its project " + dlg.format());
    fileEdit->setText(tmp.filePath("rec"));
    check(dlg.file() == tmp.filePath("rec.lml"), "the project's suffix: " + dlg.file());
    auto combos = dlg.findChildren<QComboBox *>();
    combos[1]->setCurrentIndex(combos[1]->findData(QString("labplot.csv")));
    check(!dlg.openInLabPlot(), "the CSV for LabPlot: not opened");
    check(dlg.file() == tmp.filePath("rec.labplot.csv"), "two dots: " + dlg.file());
    fileEdit->setText(tmp.filePath("rec.labplot.csv"));
    check(dlg.file() == tmp.filePath("rec.labplot.csv"), "not twice: " + dlg.file());
    labPlot->setChecked(true);
    fileEdit->setText(tmp.filePath("rec"));
    dlg.accept();
    combos[1]->setCurrentIndex(0);
    check(!dlg.openInLabPlot(), "another format: no LabPlot");
    combos[0]->setCurrentIndex(1);   // the table
    check(!labPlot->isEnabled(), "only for the recording");
  }
  {
    ExportDlg dlg(ExportDlg::Recording, QString(), &cfg);
    check(dlg.openInLabPlot(), "LabPlot kept as last choice");
  }
  // the program set is taken as it is, a missing one is none
  const QString exe = QCoreApplication::applicationFilePath();
  check(LabPlot::command("x.csv", exe) == QStringList({ exe, "x.csv" }), "the program set");
  check(LabPlot::command("x.csv", tmp.filePath("no-labplot")).isEmpty(), "a missing program: none");
  QString why;
  check(!LabPlot::open("x.csv", tmp.filePath("no-labplot"), &why) && !why.isEmpty(), "not found: says so");

  if (failed == 0)
    qInfo() << "All export dialog tests passed.";
  else
    qWarning() << failed << "export dialog test(s) failed.";
  return failed == 0 ? 0 : 1;
}
