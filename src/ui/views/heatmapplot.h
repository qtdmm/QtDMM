// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/colormap.h"

#include <QBrush>
#include <QColor>
#include <QImage>
#include <QVector>
#include <QWidget>


/// A live heatmap: X/Y position, color = third value.
class HeatmapPlot : public QWidget
{
  Q_OBJECT

public:
  explicit HeatmapPlot(QWidget *parent = nullptr);

  void setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels);
  void setAxisUnits(const QString &x, const QString &y, const QString &z);
  void setLegend(const QString &legend);
  void setColorScheme(ColorMap::Scheme scheme);
  ColorMap::Scheme colorScheme() const { return m_scheme; }
  void setCellCount(int nx, int ny);   // grid resolution, default 32 x 32

  void addPoint(double x, double y, double z);
  void clear();
  void setHold(bool hold);
  bool hold() const { return m_hold; }

protected:
  void paintEvent(QPaintEvent *) override;

private:
  struct Sample { double x, y, z; };
  struct Cell   { double sum = 0; int n = 0; };

  QVector<Sample>  m_samples;
  QVector<Sample>  m_shown;
  bool              m_hold = false;
  int              m_sampleHead = 0;
  QImage           m_colorBar;
  ColorMap::Scheme m_colorBarScheme = ColorMap::SchemeCount;
  ColorMap::Scheme m_scheme = ColorMap::Rainbow;
  int              m_nx = 32;
  int              m_ny = 32;
  QString   m_xUnit, m_yUnit, m_zUnit, m_legend;
  QBrush    m_background;
  QColor    m_grid, m_labels;
};
