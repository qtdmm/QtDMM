// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/colormap.h"

#include <QBrush>
#include <QColor>
#include <QVector>
#include <QWidget>


class QContextMenuEvent;

/// A live heatmap: X/Y position, color = third value.
class HeatmapPlot : public QWidget
{
  Q_OBJECT

public:
  explicit HeatmapPlot(QWidget *parent = nullptr);

  void setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels);
  void setAxisCaptions(const QString &x, const QString &y, const QString &z);
  void setLegend(const QString &legend);
  void setColorScheme(ColorMap::Scheme scheme);
  ColorMap::Scheme colorScheme() const { return m_scheme; }
  void setCellCount(int nx, int ny);   // grid resolution, default 32 x 32

  void addPoint(double x, double y, double z);
  void clear();

signals:
  void colorSchemeChanged(ColorMap::Scheme scheme);

protected:
  void paintEvent(QPaintEvent *) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

private:
  struct Sample { double x, y, z; };
  struct Cell   { double sum = 0; int n = 0; };

  void rebuildGrid(double xmin, double xmax, double ymin, double ymax,
                   QVector<Cell> &cells) const;

  QVector<Sample>  m_samples;
  ColorMap::Scheme m_scheme = ColorMap::Rainbow;
  int       m_nx = 32;
  int       m_ny = 32;
  QString   m_xCaption, m_yCaption, m_zCaption, m_legend;
  QBrush    m_background;
  QColor    m_grid, m_labels;
};
