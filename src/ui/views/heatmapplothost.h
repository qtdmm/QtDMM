#pragma once

#include "ui/views/heatmapplotbackend.h"

class QStackedWidget;
class HeatmapPlot;
class QCPHeatmapPlot;

/// Keeps heatmap renderers synchronized while allowing live backend switching.
class HeatmapPlotHost : public HeatmapPlotBackend
{
  Q_OBJECT
public:
  enum Backend { Legacy, QCustomPlot };
  Q_ENUM(Backend)

  explicit HeatmapPlotHost(QWidget *parent = nullptr);
  Backend backend() const { return m_backend; }
  void setBackend(Backend backend);

  void setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels) override;
  void setAxisUnits(const QString &x, const QString &y, const QString &z) override;
  void setLegend(const QString &legend) override;
  void setColorScheme(ColorMap::Scheme scheme) override;
  ColorMap::Scheme colorScheme() const override;
  void setCellCount(int nx, int ny) override;
  void addPoint(double x, double y, double z) override;
  void clear() override;
  void setHold(bool hold) override;
  bool hold() const override;

private:
  QStackedWidget *m_stack = nullptr;
  HeatmapPlot *m_legacy = nullptr;
  QCPHeatmapPlot *m_qcp = nullptr;
  Backend m_backend = Legacy;
};
