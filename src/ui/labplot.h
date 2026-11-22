// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>

/// LabPlot (labplot.org), started with a file so its import dialog takes it.
/// QtDMM writes the file (RecordingFile, `*.labplot.csv`), LabPlot does the
/// analysis. The program is the one set under Settings → General
/// (Programs/labplot), else searched: `labplot`/`labplot2` in the PATH, the
/// Flatpak org.kde.labplot, the usual place on Windows and macOS.
namespace LabPlot
{
/// The command and its arguments that start LabPlot with @p file; @p
/// configured the program set by the user (a name is searched in the PATH),
/// empty: searched. Empty when there is none.
QStringList command(const QString &file, const QString &configured = QString());
/// Starts LabPlot with @p file, detached. False and the reason in @p error
/// when LabPlot is not found or does not start.
bool open(const QString &file, const QString &configured, QString *error = nullptr);
}
