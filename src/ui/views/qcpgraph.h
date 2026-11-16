// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/qcpplotbase.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QPointer>
#include <functional>

#include "core/valueref.h"

class GraphTimeBar;
class QCPAxis;
class QCPGraph;
class QMouseEvent;
class QRubberBand;
class QScrollBar;
class QWheelEvent;
class RecordingSeries;
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
/// It draws values (setValues()) of any meters: each is a curve from the
/// store of its meter (setSources()), on the time of the first value's
/// store. A store given with setStore() is drawn instead - a file loaded.
///
/// The value axes are a list, one per unit: the left one first, the others
/// on the right, outwards. A value goes to the axis chosen for it or, on
/// Auto, to the axis of its unit, else to the next free one. kMaxAxes says
/// how many there are; nothing else knows the number. A unit that finds no
/// axis is not drawn (notice()).
///
/// So far: lines with gaps, the time axis, the value axes fitted to what is
/// shown or fixed by unit, the legend, the time buttons and the state above
/// the plot, the scrollbar below, zoom, hold and the crosshair.
///
/// The code that names QCustomPlot's types is in qcpgraph_<part>.cpp
/// (curves, axes, input); nothing outside knows them, for the move to NeoQCP.
class QcpGraph : public QcpPlotBase
{
  Q_OBJECT
public:
  explicit QcpGraph(QWidget *parent = nullptr);

  /// The most value axes: one on the left, the others on the right.
  static constexpr int kMaxAxes = 3;
  /// GraphModel::kAxisAuto.
  static constexpr int kAxisAuto = -1;

  /// @name What is drawn
  /// @{
  /// Where the values' stores come from: the store of a meter by its source
  /// (nullptr: not open) and the meter's name for the legend.
  void        setSources(std::function<RecordingStore *(const QString &source)> storeOf,
                         std::function<QString(const QString &source)> nameOf);
  /// A meter opened or closed: the stores are asked for again.
  void        sourcesChanged();
  /// The values drawn, each a curve; the first one whose meter is open gives the time.
  void        setValues(const QList<ValueRef> &values);
  QList<ValueRef> values() const { return m_values; }
  /// The axes chosen for values (place in the list, 0 the left one); the others are on Auto.
  void        setValueAxes(const QHash<ValueRef, int> &axes);
  /// Fixed ranges of the axes by base unit; a unit without one fits its values.
  void        setFixedRanges(const QMap<QString, QPair<double, double>> &ranges);
  /// A recording of its own drawn instead of the values' (its main value);
  /// nullptr: the values again.
  void        setStore(RecordingStore *store);
  /// The store that gives the time: setStore()'s, else the first value's; nullptr: an empty graph.
  RecordingStore *store() const { return m_store; }
  /// A unit that found no axis ("Hz: no free axis"); empty when all are drawn.
  QString     notice() const { return m_notice; }
  /// @}
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
  int         curveCount() const { return int(m_curves.size()); }
  /// The points of curve @p i as thinned (x in s since the start, y in the
  /// base unit; a gap is NaN), all the store keeps.
  QList<QPointF> curvePoints(int i = 0) const;
  /// The points handed to the plot: those of the time shown, y in its axis' unit.
  QList<QPointF> drawnPoints(int i = 0) const;
  /// The place of curve @p i's axis in the list, -1 while it is not drawn.
  int         curveAxis(int i) const;
  QColor      curveColor(int i) const;
  /// The axes in use, the left one included also without a unit.
  int         axisCount() const;
  /// Axis @p i: its title ("[mV]") and its limits in that unit.
  QString     axisTitle(int i) const;
  double      axisMin(int i) const;
  double      axisMax(int i) const;
  QString     legendText() const;
  /// The time axis, s since the start of the recording.
  double      timeAxisStart() const;
  double      timeAxisEnd() const;
  QString     leftAxisTitle() const { return axisTitle(0); }
  double      leftAxisMin() const { return axisMin(0); }
  double      leftAxisMax() const { return axisMax(0); }
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
  /// One value drawn.
  struct Curve
  {
    ValueRef    value;
    QPointer<RecordingStore> store;
    QString     meter;           ///< its meter's name, for the legend
    QString     unit;            ///< base unit, of its newest series
    QString     label;           ///< "Voltage DC (V)"
    int         axis = -1;       ///< place in m_axes; -1: not drawn
    double      shift = 0;       ///< its store's time 0 on the graph's time, s
    const RecordingSeries *newest = nullptr;   ///< the series a reading goes to
    QCPGraph   *graph = nullptr;
    QList<QPointF> points;       ///< thinned, all the store keeps, on the graph's time
    int         tail = 0;        ///< how many of them are the newest bucket's
  };
  /// One value axis: the unit it has now, and the frame zoomed on.
  struct Axis
  {
    QCPAxis    *axis = nullptr;
    QString     unit;            ///< base unit; empty: free
    double      factor = 1;      ///< base unit per unit of the axis
    double      zoomMin = 0;     ///< while m_zoomed, base unit
    double      zoomMax = 0;
  };

  // qcpgraph_curves.cpp
  /// The curves anew from values and sources: stores, plot graphs, connections.
  void        makeCurves();
  /// The series of @p curve's value, oldest first.
  QList<const RecordingSeries *> seriesOf(const Curve &curve) const;
  /// The bucket for the time shown on the plot's width.
  qint64      bucketNow() const;
  /// All points anew, and which axis each curve has.
  void        rebuildPoints();
  void        rebuildCurve(Curve &curve);
  /// The newest reading of @p series went into @p curve: a new bucket adds
  /// its points, one still filling replaces them.
  void        appendTail(Curve &curve, const RecordingSeries &series);
  void        mainAppended(RecordingStore *store, bool shifted);
  void        extraAppended(RecordingStore *store, int index);
  /// What a store did other than a reading at the end.
  void        storeChanged();
  void        drawCurves(double from, double to);
  QColor      colorOf(int curve) const;
  void        updateLegend();
  // qcpgraph_axes.cpp
  void        setupAxes();
  /// Each curve on the axis chosen for it or of its unit, else the next free one.
  void        assignAxes();
  /// The window without telling "All" (it sets its own).
  void        applyWindow(int seconds);
  /// "All": the window of the whole recording; with @p grow only when the recording outgrew it.
  void        fitAll(bool grow);
  /// The latest start of the window: its end at the newest reading.
  qint64      lastStart() const;
  /// The lowest and highest value on axis @p axis from @p from to @p to (s); false: none.
  bool        valueSpan(int axis, double from, double to, double &lo, double &hi) const;
  void        updateAxes(double &from, double &to);
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

  std::function<RecordingStore *(const QString &source)> m_storeOf;
  std::function<QString(const QString &source)> m_nameOf;
  QList<ValueRef> m_values;
  QHash<ValueRef, int> m_valueAxes;
  QMap<QString, QPair<double, double>> m_fixedRanges;
  QPointer<RecordingStore> m_ownStore;   ///< setStore()
  QPointer<RecordingStore> m_store;      ///< the store of the time
  QList<QPointer<RecordingStore>> m_watched;   ///< the stores connected
  QString     m_sourceName;
  QString     m_notice;
  QList<Curve> m_curves;
  QList<Axis> m_axes;                    ///< kMaxAxes of them, the left one first
  GraphTimeBar *m_timeBar = nullptr;
  QScrollBar *m_scrollBar = nullptr;
  QRubberBand *m_zoomBand = nullptr;
  qint64      m_bucket = 1;      ///< the bucket the points were thinned with, ms
  bool        m_dirty = true;    ///< the points are to be built anew
  int         m_windowSeconds = 600;
  bool        m_followAll = false;
  bool        m_followEnd = true;   ///< the window ends at the newest reading
  qint64      m_start = 0;          ///< its start while it does not, ms
  /// A frame was zoomed on: the axes keep its values (Axis::zoomMin), and the window before.
  bool        m_zoomed = false;
  struct { qint64 start = 0; int seconds = 0; bool all = false; bool atEnd = true; } m_zoomBefore;
  /// The mouse: a drag moves the time, with Shift it draws the zoom frame.
  enum class Drag { None, Pan, Frame };
  Drag        m_drag = Drag::None;
  QPoint      m_pressPos;
  qint64      m_pressStart = 0;
};
