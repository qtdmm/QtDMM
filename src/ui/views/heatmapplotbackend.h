#pragma once

#include "ui/views/colormap.h"

#include <QBrush>
#include <QColor>
#include <QString>
#include <QWidget>

/// Backend-neutral API for heatmap renderers.
class HeatmapPlotBackend : public QWidget
{
  Q_OBJECT
public:
  using QWidget::QWidget;
  virtual void setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels) = 0;
  virtual void setAxisUnits(const QString &x, const QString &y, const QString &z) = 0;
  virtual void setLegend(const QString &legend) = 0;
  virtual void setColorScheme(ColorMap::Scheme scheme) = 0;
  virtual ColorMap::Scheme colorScheme() const = 0;
  virtual void setCellCount(int nx, int ny) = 0;
  virtual void addPoint(double x, double y, double z) = 0;
  virtual void clear() = 0;
  virtual void setHold(bool hold) = 0;
  virtual bool hold() const = 0;
};
