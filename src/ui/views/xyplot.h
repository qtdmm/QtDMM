// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/qcpgraphcommon.h"
#include "ui/views/ringbuffer.h"

#include <QPointF>
#include <QVector>

class QCPGraph;

/// A live scatter plot of one value against another (XYView feeds it): the
/// newest kMaxPoints pairs, both axes in the prefix of their range.
class XYPlot : public QcpGraphCommon
{
  Q_OBJECT
public:
  explicit XYPlot(QWidget *parent = nullptr);

  void        setTheme(const PlotTheme &theme) override;
  /// The base units of the two values.
  void        setAxisUnits(const QString &x, const QString &y);
  void        addPoint(double x, double y);

private:
  static constexpr int kMaxPoints = 2000;
  void refreshPlot() override;
  void holdChanged(bool hold) override;
  void clearSamples() override;

  QCPGraph            *m_graph = nullptr;
  RingBuffer<QPointF>  m_points{ kMaxPoints };
  QVector<QPointF>     m_held;   ///< the points when hold froze the picture
  QString              m_xUnit;
  QString              m_yUnit;
};
