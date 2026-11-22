// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/colormap.h"
#include "ui/views/qcpgraphcommon.h"
#include "ui/views/ringbuffer.h"

#include <QVector>

class QCPColorMap;
class QCPColorScale;

/// A live heatmap (HeatmapView feeds it): X/Y position, the colour the
/// third value averaged over the samples in a cell, the newest kMaxSamples.
class HeatmapPlot : public QcpGraphCommon
{
  Q_OBJECT
public:
  explicit HeatmapPlot(QWidget *parent = nullptr);

  void             setTheme(const PlotTheme &theme) override;
  /// The base units of X, Y and the colour.
  void             setAxisUnits(const QString &x, const QString &y, const QString &z);
  void             setColorScheme(ColorMap::Scheme scheme);
  ColorMap::Scheme colorScheme() const { return m_scheme; }
  /// The grid, 2 to kMaxGridDimension cells each way; 32 x 32 at first.
  void             setCellCount(int nx, int ny);
  void             addPoint(double x, double y, double z);

private:
  struct Sample
  {
    double x, y, z;
  };
  static constexpr int kMaxSamples = 5000;
  static constexpr int kMaxGridDimension = 256;
  void    refreshPlot() override;
  void    holdChanged(bool hold) override;
  void    clearSamples() override;
  QString crosshairText(double x, double y) const override;
  void    applyGradient();
  /// The colour scale's width fixed to what it needs (fonts of the theme),
  /// and the plot's least width with it: a narrow window narrows the plot,
  /// not the scale.
  void    fixScaleWidth();

  QCPColorMap       *m_map = nullptr;
  QCPColorScale     *m_scale = nullptr;
  RingBuffer<Sample> m_samples{ kMaxSamples };
  QVector<Sample>    m_held;   ///< the samples when hold froze the picture
  int                m_nx = 32;
  int                m_ny = 32;
  ColorMap::Scheme   m_scheme = ColorMap::Rainbow;
  QString            m_xUnit;
  QString            m_yUnit;
  QString            m_zUnit;
};
