// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/colormap.h"

#include <QBrush>
#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

class QCPColorMap;
class QCPColorScale;
class QCPItemText;
class QCPTextElement;
class QCustomPlot;

/// A live heatmap drawn with QCustomPlot: X/Y position, colour = the third
/// value, averaged per cell.
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

private:
  struct Sample { double x, y, z; };
  void refreshMap();
  void applyGradient();

  QCustomPlot *m_plot = nullptr;
  QCPColorMap *m_map = nullptr;
  QCPColorScale *m_scale = nullptr;
  QCPTextElement *m_title = nullptr;
  QCPItemText *m_noDataText = nullptr;
  QCPItemText *m_holdText = nullptr;
  QVector<Sample> m_samples, m_shown;
  int m_sampleHead = 0;
  int m_nx = 32, m_ny = 32;
  bool m_hold = false;
  ColorMap::Scheme m_scheme = ColorMap::Rainbow;
  QString m_xUnit, m_yUnit, m_zUnit, m_legend;
  QColor m_grid, m_labels;
};
