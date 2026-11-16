// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/qcpplotbase.h"

#include <QList>
#include <QPointF>
#include <QPointer>

class GraphTimeBar;
class QCPGraph;
class QMouseEvent;
class QRubberBand;
class QScrollBar;
class QWheelEvent;
class RecordingStore;

/// The graph of a recording drawn with QCustomPlot: the successor of
/// GraphWidget, growing next to it until it can do all of it.
///
/// It draws from a RecordingStore and keeps no readings itself, only the
/// thinned points of the curve (SeriesDecimator): a new reading replaces the
/// points of its bucket, the plot is drawn at the pace of QcpPlotBase. It
/// never changes the store: Record, Stop, Live and Clear are the session's.
///
/// The time shown is a window of windowSeconds(): at the newest reading
/// while it follows (Live, Record), or where it was put (showFrom(): the
/// scrollbar, a drag, the keys) - in Live the store runs on meanwhile. "All"
/// (setFollowAll()) is a window over the whole recording that grows with it.
/// A frame drawn with Shift zooms time and left axis; Escape takes it back.
///
/// So far: the main value of one meter as a line with gaps, the time axis,
/// the left axis fitted to what is shown, the time buttons and the state
/// above the plot, the scrollbar below, zoom, hold and the crosshair.
///
/// The code that names QCustomPlot's types is in qcpgraph_<part>.cpp
/// (curves, axes, input); nothing outside knows them, for the move to NeoQCP.
class QcpGraph : public QcpPlotBase
{
  Q_OBJECT
public:
  explicit QcpGraph(QWidget *parent = nullptr);

  /// The recording drawn; nullptr: none, an empty graph.
  void        setStore(RecordingStore *store);
  RecordingStore *store() const { return m_store; }
  /// The name of the file the store was loaded from, for the state text; empty: a meter's.
  void        setSourceName(const QString &name);

  /// @name The time shown
  /// @{
  /// The window, seconds (at least 1); another one ends "All".
  void        setWindowSeconds(int seconds);
  int         windowSeconds() const { return m_windowSeconds; }
  /// "All": the window is the whole recording and grows with it.
  void        setFollowAll(bool on);
  bool        followAll() const { return m_followAll; }
  /// The window from @p start (ms since the start of the recording), within
  /// what the store keeps; at its end it follows again.
  void        showFrom(qint64 start);
  /// The window at the newest reading, following it.
  void        followEnd();
  /// Whether the window is at the newest reading and stays there.
  bool        following() const { return m_followEnd; }
  /// The start of the window, ms since the start of the recording.
  qint64      windowStart() const;
  /// The window narrower (@p factor > 1) or wider; ends "All".
  void        zoomBy(double factor);
  /// The window over the whole recording, once (not growing as "All").
  void        zoomFit();
  /// The frame @p rect (plot widget pixels): its time as the window, its
  /// values as the left axis, until endZoom().
  void        zoomToRect(const QRect &rect);
  /// The left axis fits again; with @p restoreWindow the time as before the first frame.
  void        endZoom(bool restoreWindow);
  bool        zoomed() const { return m_zoomed; }
  /// @}

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
  /// The left axis: its title ("[mV]") and its limits in that unit.
  QString     leftAxisTitle() const;
  double      leftAxisMin() const;
  double      leftAxisMax() const;
  QString     stateText() const;
  QScrollBar *scrollBar() const { return m_scrollBar; }
  GraphTimeBar *timeBar() const { return m_timeBar; }
  /// The rectangle of the axes in the plot widget, for zoomToRect().
  QRect       plotRect() const;
  /// Draws what is pending now instead of at the drawing pace.
  void        refreshNow();
  /// @}

Q_SIGNALS:
  /// The window or "All" changed, by the graph itself too (zoom, "All" growing).
  void        windowChanged(int seconds);
  void        followAllChanged(bool on);

protected:
  void        refreshPlot() override;
  QString     crosshairText(double x, double y) const override;
  void        resizeEvent(QResizeEvent *event) override;
  void        keyPressEvent(QKeyEvent *event) override;

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
  /// The window without telling "All" (it sets its own).
  void        applyWindow(int seconds);
  /// "All": the window of the whole recording; with @p grow only when the recording outgrew it.
  void        fitAll(bool grow);
  /// The latest start of the window: its end at the newest reading.
  qint64      lastStart() const;
  /// The lowest and highest value of the points from @p from to @p to (s); false: none.
  bool        valueSpan(double from, double to, double &lo, double &hi) const;
  void        updateAxes(double &from, double &to, double &factor);
  void        updateScrollBar();
  void        updateTimeBar();
  /// After the window moved: the picture now, unless it is held.
  void        redraw();
  // qcpgraph_input.cpp
  void        setupInput();
  void        mousePressed(QMouseEvent *event);
  void        mouseMoved(QMouseEvent *event);
  void        mouseReleased(QMouseEvent *event);
  void        wheelTurned(QWheelEvent *event);
  /// The window moved by @p fraction of itself.
  void        pan(double fraction);

  QPointer<RecordingStore> m_store;
  QString     m_sourceName;
  QCPGraph   *m_curve = nullptr;
  GraphTimeBar *m_timeBar = nullptr;
  QScrollBar *m_scrollBar = nullptr;
  QRubberBand *m_zoomBand = nullptr;
  QList<QPointF> m_points;       ///< thinned, the whole store
  int         m_tail = 0;        ///< how many of them are the newest bucket's
  qint64      m_bucket = 1;      ///< the bucket they were thinned with, ms
  bool        m_dirty = true;    ///< m_points is to be built anew
  int         m_windowSeconds = 600;
  bool        m_followAll = false;
  bool        m_followEnd = true;   ///< the window ends at the newest reading
  qint64      m_start = 0;          ///< its start while it does not, ms
  double      m_factor = 1;         ///< base unit per unit of the left axis
  /// A frame was zoomed on: the left axis' limits (base unit) and the window before.
  bool        m_zoomed = false;
  double      m_yZoomMin = 0;
  double      m_yZoomMax = 0;
  struct { qint64 start = 0; int seconds = 0; bool all = false; bool atEnd = true; } m_zoomBefore;
  /// The mouse: a drag moves the time, with Shift it draws the zoom frame.
  enum class Drag { None, Pan, Frame };
  Drag        m_drag = Drag::None;
  QPoint      m_pressPos;
  qint64      m_pressStart = 0;
};
