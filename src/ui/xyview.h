// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <array>

#include "core/reading.h"
#include "core/valueref.h"

class DeviceSession;
class ValueFeed;
class XYPlot;

/// What one XY plot window shows: two values of any open meters.
class XYView : public QObject
{
  Q_OBJECT
public:
  XYView(DeviceSession *session, XYPlot *plot, QObject *parent);

  XYPlot     *plot() const { return m_plot; }
  void        setValues(const ValueRef &x, const ValueRef &y);
  ValueRef    xValue() const { return m_x; }
  ValueRef    yValue() const { return m_y; }

private:
  void        feedX(const Reading &reading);
  void        feedY(const Reading &reading);
  void        updateLegend();
  void        updateAxisUnits();

  XYPlot     *m_plot;
  ValueFeed  *m_xFeed;
  ValueFeed  *m_yFeed;
  ValueRef    m_x;
  ValueRef    m_y;
  double      m_lastX = qQNaN();
  double      m_lastY = qQNaN();
  qint64      m_lastXT = -1;
  qint64      m_lastYT = -1;
  qint64      m_plottedT = -1;
  QString     m_xSource;
  QString     m_ySource;
  QString     m_xName;
  QString     m_yName;
  std::array<QString, 2> m_units;
};
