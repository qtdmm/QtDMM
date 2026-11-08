// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

#include <utility>

struct Recording;

/// A recording as a LabPlot project (`*.lml`, labplot.org): it opens in
/// LabPlot without its import dialog, with the numbers and a graph of them.
///
/// The project, named like the file, holds a spreadsheet "Data" with
/// `time[s]` (the x column) and a column per value as in the CSV for
/// LabPlot (RecordingFile::numberColumns: "Sim A: value [V]", NaN where
/// there is no number, which LabPlot leaves as a gap), and a worksheet
/// "Graph" with a plot per unit, one below the other on the same time
/// axis: a value with another unit never shares a scale. A plot of more
/// than one value has a legend.
///
/// LabPlot reads a missing attribute as 0, not as its default, so the
/// parts come from templates LabPlot wrote itself (assets/labplot/, in the
/// resources at :/labplot). The axes' ranges are set here, rounded out to
/// a step of 1, 2, 2.5 or 5 x 10^n: LabPlot does not scale them itself
/// when it loads a project. The XML is not compressed; LabPlot reads it
/// either way.
namespace LabPlotProject
{
/// The XML of the project of @p recording named @p name; empty when the
/// templates are missing (resources not linked).
QByteArray xml(const Recording &recording, const QString &name);
/// Writes the project of @p recording to @p path, named after the file;
/// false with @p error (translated) when there is nothing to write or the
/// file cannot be written.
bool write(const Recording &recording, const QString &path, QString *error = nullptr);
/// The range [@p lo, @p hi] widened to whole steps of 1, 2, 2.5 or 5 x 10^n,
/// about five of them; a single value gets a range around it.
std::pair<double, double> niceRange(double lo, double hi);
}
