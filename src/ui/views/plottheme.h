// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QBrush>
#include <QColor>
#include <QList>
#include <QPalette>

class QCPAxis;
class QCustomPlot;

/// The colours of a side plot, from the window design (MainWindow::applyPlotColors()).
/// The System design leaves the colours invalid: the palette's then (resolved()).
struct PlotTheme
{
  QBrush background;   ///< Qt::NoBrush: the palette's base
  QColor grid;         ///< invalid: the palette's mid
  QColor labels;       ///< invalid: the palette's text
  QColor data;         ///< the curve or bars, invalid: the palette's highlight

  /// This theme with the colours left invalid taken from @p pal: on a dark
  /// platform palette (Windows dark mode) black lines would vanish.
  PlotTheme resolved(const QPalette &pal) const;

  /// Background, axes and grid of @p plot; @p axes beyond x and y (a colour scale), else those two.
  void applyTo(QCustomPlot *plot, const QList<QCPAxis *> &axes = {}) const;
};
