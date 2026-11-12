// heatmapview.h
#pragma once

#include <QObject>
#include <array>

#include "core/reading.h"
#include "core/valueref.h"

class DeviceSession;
class ValueFeed;
class HeatmapPlotBackend;

/// What one heatmap window shows: X, Y and color value of any open meters.
class HeatmapView : public QObject
{
  Q_OBJECT
public:
  HeatmapView(DeviceSession *session, HeatmapPlotBackend *plot, QObject *parent);

  HeatmapPlotBackend *plot() const { return m_plot; }
  void         setValues(const ValueRef &x, const ValueRef &y, const ValueRef &z);
  ValueRef     value(int axis) const { return m_ref[axis]; }

private:
  enum Axis { X = 0, Y = 1, Z = 2 };
  void feed(int axis, const Reading &reading);
  void updateLegend();
  void updateAxisUnits();
  void tryEmit(int triggerAxis);

  HeatmapPlotBackend *m_plot;
  std::array<ValueFeed *, 3> m_feed{};
  std::array<ValueRef, 3>    m_ref;
  std::array<double, 3>      m_last{qQNaN(), qQNaN(), qQNaN()};
  std::array<qint64, 3>      m_lastT{-1, -1, -1};
  std::array<QString, 3>     m_source;
  std::array<QString, 3>     m_name;
  std::array<QString, 3>     m_units;
  qint64 m_plottedT = -1;
};
