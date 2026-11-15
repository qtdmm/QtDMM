// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/reading.h"
#include "ui/views/qcpgraphcommon.h"
#include "ui/views/ringbuffer.h"

#include <QVector>

class QCPBars;

/// A live histogram of one value's newest kMaxValues readings (HistogramView
/// feeds it): 5 to 40 bars, the square root of the count.
class HistogramPlot : public QcpGraphCommon
{
  Q_OBJECT
public:
  explicit HistogramPlot(QWidget *parent = nullptr);

  void        setTheme(const PlotTheme &theme) override;
  /// The value counted; another one starts afresh.
  void        setValueId(int id);

public Q_SLOTS:
  /// A reading of the value counted; another unit starts afresh, overload,
  /// HOLD and readings without a number do not count.
  void        addReading(const Reading &reading);
  /// Why there is nothing to draw (no device, not connected); empty: no readings yet.
  void        setNotice(const QString &notice);

private:
  static constexpr int kMaxValues = 1000;
  void refreshPlot() override;
  void holdChanged(bool hold) override;
  void clearSamples() override;
  QString crosshairText(double x, double y) const override;

  QCPBars           *m_bars = nullptr;
  RingBuffer<double> m_values{ kMaxValues };
  QVector<double>    m_held;   ///< the values when hold froze the picture
  int                m_valueId = 0;
  QString            m_unit;
};
