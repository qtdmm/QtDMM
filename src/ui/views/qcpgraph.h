// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/qcpplotbase.h"

#include <QList>
#include <QPointF>
#include <QPointer>

class QCPGraph;
class RecordingStore;

/// The graph of a recording drawn with QCustomPlot: the successor of
/// GraphWidget, growing next to it until it can do all of it.
///
/// It draws from a RecordingStore and keeps no readings itself, only the
/// thinned points of the curve (SeriesDecimator): a new reading replaces the
/// points of its bucket, the plot is drawn at the pace of QcpPlotBase. It
/// never changes the store: Record, Stop, Live and Clear are the session's.
///
/// So far: the main value of one meter as a line with gaps, the time axis
/// following the newest reading over the window, the left axis fitted to
/// what is shown, hold and the crosshair.
///
/// The code that names QCustomPlot's types is in qcpgraph_<part>.cpp
/// (curves, axes); nothing outside knows them, for the move to NeoQCP.
class QcpGraph : public QcpPlotBase
{
  Q_OBJECT
public:
  explicit QcpGraph(QWidget *parent = nullptr);

  /// The recording drawn; nullptr: none, an empty graph.
  void        setStore(RecordingStore *store);
  RecordingStore *store() const { return m_store; }
  /// The time shown, seconds (at least 1).
  void        setWindowSeconds(int seconds);
  int         windowSeconds() const { return m_windowSeconds; }

  void        setTheme(const PlotTheme &theme) override;
  void        setHold(bool hold) override;
  /// Draws the store anew; the readings are the store's and stay.
  void        clear() override;

  /// @name For tests
  /// @{
  /// The curve's points as thinned (x in s since the start, y in the base
  /// unit; a gap is NaN), all the store keeps.
  QList<QPointF> curvePoints() const { return m_points; }
  /// The points handed to the plot: those of the time shown, y in the left axis' unit.
  QList<QPointF> drawnPoints() const;
  /// The time axis, s since the start of the recording.
  double      timeAxisStart() const;
  double      timeAxisEnd() const;
  /// The left axis' title ("mV").
  QString     leftAxisTitle() const;
  /// Draws what is pending now instead of at the drawing pace.
  void        refreshNow();
  /// @}

protected:
  void        refreshPlot() override;
  QString     crosshairText(double x, double y) const override;
  void        resizeEvent(QResizeEvent *event) override;

private:
  // qcpgraph_curves.cpp
  void        setupCurve();
  /// The bucket for the time shown on the plot's width.
  qint64      bucketNow() const;
  /// All points anew.
  void        rebuildPoints();
  /// The newest reading: a new bucket adds its points, one still filling replaces them.
  void        appendPoint(bool shifted);
  /// What the store did other than a reading at the end.
  void        storeChanged();
  void        drawCurve(double from, double to, double factor);
  // qcpgraph_axes.cpp
  void        setupAxes();
  /// The time shown, ms since the start: the window up to the newest.
  qint64      windowStart() const;
  /// The lowest and highest value of the points from @p from to @p to (s); false: none.
  bool        valueSpan(double from, double to, double &lo, double &hi) const;
  void        updateAxes(double &from, double &to, double &factor);

  QPointer<RecordingStore> m_store;
  QCPGraph   *m_curve = nullptr;
  QList<QPointF> m_points;       ///< thinned, the whole store
  int         m_tail = 0;        ///< how many of them are the newest bucket's
  qint64      m_bucket = 1;      ///< the bucket they were thinned with, ms
  bool        m_dirty = true;    ///< m_points is to be built anew
  int         m_windowSeconds = 600;
};
