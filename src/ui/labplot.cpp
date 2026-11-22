// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/labplot.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace
{
QString tr(const char *text)
{
  return QCoreApplication::translate("LabPlot", text);
}

// a program name is searched in the PATH, a path taken as it is
QString program(const QString &name)
{
  if (name.contains('/') || name.contains('\\'))
    return QFileInfo(name).isExecutable() ? name : QString();
  return QStandardPaths::findExecutable(name);
}
}

QStringList LabPlot::command(const QString &file, const QString &configured)
{
  if (!configured.trimmed().isEmpty())
  {
    const QString exe = program(configured.trimmed());
    return exe.isEmpty() ? QStringList() : QStringList{ exe, file };
  }
  for (const char *name : { "labplot", "labplot2" })
    if (const QString exe = program(name); !exe.isEmpty())
      return { exe, file };
#if defined(Q_OS_WIN)
  for (const QString &base : { qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)") })
    for (const char *name : { "labplot.exe", "labplot2.exe" })
      if (const QString exe = QDir(base).filePath(QString("LabPlot/bin/") + name);
          !base.isEmpty() && QFileInfo(exe).isExecutable())
        return { exe, file };
#elif defined(Q_OS_MACOS)
  for (const char *exe : { "/Applications/LabPlot.app/Contents/MacOS/labplot",
                           "/Applications/LabPlot2.app/Contents/MacOS/labplot2" })
    if (QFileInfo(exe).isExecutable())
      return { exe, file };
#else
  // the Flatpak, for the user or the system; the file through the document
  // portal, LabPlot's sandbox may not see it otherwise
  const QString flatpak = program("flatpak");
  const QStringList installs{ QDir::home().filePath(".local/share/flatpak/app/org.kde.labplot"),
                              "/var/lib/flatpak/app/org.kde.labplot" };
  if (!flatpak.isEmpty())
    for (const QString &dir : installs)
      if (QFileInfo(dir).isDir())
        return { flatpak, "run", "--file-forwarding", "org.kde.labplot", "@@", file, "@@" };
#endif
  return {};
}

bool LabPlot::open(const QString &file, const QString &configured, QString *error)
{
  auto fail = [error](const QString &why)
  {
    if (error)
      *error = why;
    return false;
  };
  // QtDMM's own Flatpak sees no programs of the system
  if (QFileInfo::exists("/.flatpak-info"))
    return fail(tr("QtDMM runs as Flatpak and cannot start LabPlot; open %1 in LabPlot")
                  .arg(QDir::toNativeSeparators(file)));
  QStringList cmd = command(QDir::toNativeSeparators(file), configured);
  if (cmd.isEmpty())
    return fail(configured.trimmed().isEmpty()
                  ? tr("LabPlot not found; set it under Settings, General, or open %1 in LabPlot")
                      .arg(QDir::toNativeSeparators(file))
                  : tr("LabPlot not found: %1").arg(configured));
  const QString exe = cmd.takeFirst();
  if (!QProcess::startDetached(exe, cmd))
    return fail(tr("LabPlot does not start: %1").arg(QDir::toNativeSeparators(exe)));
  return true;
}
