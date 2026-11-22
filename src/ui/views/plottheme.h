// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QBrush>
#include <QColor>
#include <QList>

class QCPAxis;
class QCustomPlot;

/// The colours of a side plot, from the window design (MainWindow::applyPlotColors()).
struct PlotTheme
{
  QBrush background;   ///< Qt::NoBrush: the palette's base
  QColor grid;
  QColor labels;
  QColor data;         ///< the curve or bars; invalid where the plot has its own (heatmap)

  /// Background, axes and grid of @p plot; @p axes beyond x and y (a colour scale), else those two.
  void applyTo(QCustomPlot *plot, const QList<QCPAxis *> &axes = {}) const;
};
