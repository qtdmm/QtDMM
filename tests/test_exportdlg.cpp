// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ExportDlg: formats per kind, where to, the last choice kept.

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QTemporaryDir>

#include "core/settings.h"
#include "ui/dialogs/exportdlg.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir tmp;
  Settings cfg("exportdlg", tmp.path());

  check(ExportDlg::formats(ExportDlg::Recording).size() == 4 && ExportDlg::formats(ExportDlg::Table).size() == 3
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
    auto combos = dlg.findChildren<QComboBox *>();
    combos[0]->setCurrentIndex(0);   // the recording
    combos[1]->setCurrentIndex(1);   // every reading
    check(dlg.format() == "csv" && dlg.everyReading(), "every reading: csv");
    check(!dlg.toBasket(), "a file was chosen last time");
    dlg.findChildren<QRadioButton *>().first()->setChecked(true);
    check(dlg.toBasket() && dlg.file().isEmpty(), "into the basket: no file");
  }

  if (failed == 0)
    qInfo() << "All export dialog tests passed.";
  else
    qWarning() << failed << "export dialog test(s) failed.";
  return failed == 0 ? 0 : 1;
}
