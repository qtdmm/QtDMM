#pragma once

#include "ui/views/heatmapplotbackend.h"

#include <QVector>

class QCPColorMap;
class QCPColorScale;
class QCPItemText;
class QCPTextElement;
class QCustomPlot;

/// QCustomPlot renderer for the live heatmap.
class QCPHeatmapPlot : public HeatmapPlotBackend
{
  Q_OBJECT
public:
  explicit QCPHeatmapPlot(QWidget *parent = nullptr);

  void setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels) override;
  void setAxisUnits(const QString &x, const QString &y, const QString &z) override;
  void setLegend(const QString &legend) override;
  void setColorScheme(ColorMap::Scheme scheme) override;
  ColorMap::Scheme colorScheme() const override { return m_scheme; }
  void setCellCount(int nx, int ny) override;
  void addPoint(double x, double y, double z) override;
  void clear() override;
  void setHold(bool hold) override;
  bool hold() const override { return m_hold; }

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
