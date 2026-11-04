// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// AppLog and its window: it rolls, one line per message, levels shown, the
// search with previous and next, the export.
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

#include "core/applog.h"
#include "ui/dialogs/logdlg.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // 1. it rolls, a message is one line
  {
    AppLog log(3);
    for (int i = 1; i <= 5; ++i)
      log.add(AppLog::Info, "app", QString("line %1").arg(i));
    const QList<AppLog::Entry> e = log.entries();
    check(e.size() == 3 && e.first().message == "line 3" && e.last().message == "line 5" && e.last().number == 5,
          "the oldest lines go");
    log.add(AppLog::Error, "UT61E", "lost\nretrying");
    check(log.entries().last().message == "lost · retrying", "one line");
    check(AppLog::line(log.entries().last()).endsWith("\tUT61E\tERROR\tlost · retrying"), AppLog::line(log.entries().last()));
  }

  // 2. the window: levels, new lines, search
  {
    AppLog log(4);
    log.add(AppLog::Debug, "app", "frame");
    log.add(AppLog::Info, "Sim A", "connected");
    log.add(AppLog::Warning, "UT61E", "timeout");
    LogDlg dlg(&log);
    check(dlg.rows() == 2, "Debug not shown at first");
    dlg.setLevelShown(AppLog::Debug, true);
    check(dlg.rows() == 3, "all three");
    dlg.setLevelShown(AppLog::Warning, false);
    check(dlg.rows() == 2, "without the warning");
    dlg.setLevelShown(AppLog::Warning, true);
    log.add(AppLog::Error, "UT61E", "lost connection");
    QCoreApplication::processEvents();
    check(dlg.rows() == 4, "a new line shows");
    log.add(AppLog::Info, "app", "more");
    QCoreApplication::processEvents();
    check(dlg.rows() == 4, "and the oldest goes");
    check(dlg.find("ut61e", true) && dlg.currentRow() == 1, "found by scope, any case");
    check(dlg.find("ut61e", true) && dlg.currentRow() == 2, "next");
    check(dlg.find("ut61e", true) && dlg.currentRow() == 1, "around");
    check(dlg.find("ut61e", false) && dlg.currentRow() == 2, "previous, around");
    check(!dlg.find("nothing", true), "not found");
    check(LogDlg::fileName(QDate(2026, 11, 4)) == "qtdmm-2026-11-04.log", "the file's name");

    QTemporaryDir dir;
    const QString file = dir.filePath("x.log");
    check(log.write(file), "written");
    QFile in(file);
    in.open(QIODevice::ReadOnly | QIODevice::Text);
    const QStringList lines = QString::fromUtf8(in.readAll()).split('\n', Qt::SkipEmptyParts);
    check(lines.size() == 5 && lines.first().startsWith("# QtDMM") && lines.last().endsWith("INFO\tmore"), "versions, then the lines");
    QString error;
    check(!log.write(dir.filePath("no/such/dir/x.log"), &error) && !error.isEmpty(), "says why it cannot write");
  }

  if (failed)
    qWarning() << failed << "check(s) failed";
  else
    qInfo() << "all checks passed";
  return failed ? 1 : 0;
}
