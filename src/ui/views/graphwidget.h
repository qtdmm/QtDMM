// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/qcpplotbase.h"

#include <QDateTime>
#include <QImage>
#include <QPicture>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QPointer>
#include <functional>

#include "core/valueref.h"
#include "recording/seriesdecimator.h"
#include "ui/views/graphlook.h"
#include "ui/views/graphreference.h"
#include "ui/views/graphoutliers.h"
#include "ui/views/graphstates.h"
#include "ui/views/graphstats.h"

class GraphMinimap;
class GraphStateStrip;
class GraphStatsPanel;
class GraphTimeBar;
class QCPAbstractItem;
class QCPAxis;
class QCPGraph;
class QCPItemStraightLine;
class QCPPainter;
class QMouseEvent;
class QPrinter;
class QRubberBand;
class QPrinter;
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
/// Its look is a colour variant (GraphLook) on the window design, with the
/// settings' line, points and width: setTheme() gives the design,
/// setVariant() and setLookSettings() the rest.
///
/// To read it: the crosshair under the mouse sits on the reading of the
/// left axis' value that holds there; a click sets the time cursor A,
/// Shift and a click B (lines one can drag, each with the level of that
/// value); the statistics column (GraphStats) has the values at the
/// cursors, what the curves did from A to B and over the time shown.
///
/// A reference line (GraphReference) lies on the left axis' value: at 0,
/// at a value it was dragged to, at an alarm threshold, or following the
/// mean, minimum or maximum of the time shown; up to four lines more at
/// fixed values. Where the curve crosses a fixed line a small triangle
/// stands, the line's label counts them. The limit test's band and the
/// start threshold of the recording are drawn on the axis of their value.
///
/// The marks of the recordings shown - alarms, the trigger of a
/// pre-triggered recording, the user's notes - are dashed lines at their
/// time, each with a flag in the row of the time labels (GraphFlags), the
/// time cursors' letters among them. A note is set with N (not in a file only looked at), edited with a
/// double click on its flag and dragged along the time.
///
/// Below the plot the overview (GraphMinimap) of the left axis' value over
/// the whole recording takes the scrollbar's place; at the bottom of the
/// plot the states its meter reported lie as a strip (GraphStateStrip). A
/// curve with more points than pixel columns is drawn as a band from its
/// lowest to its highest value per column (GraphBand); the min/max band
/// behind the left axis' curve is the same over a few columns more.
///
/// So far: lines with gaps, the time axis, the value axes fitted to what is
/// shown or fixed by unit, the legend, the time buttons and the state above
/// the plot, the scrollbar below, zoom, hold and the crosshair.
///
/// The code that names QCustomPlot's types is in graphwidget_<part>.cpp
/// (curves, axes, input, theme, overlays, marks, overview); nothing outside knows them, for the move to NeoQCP.
class GraphWidget : public QcpPlotBase
{
  Q_OBJECT
public:
  explicit GraphWidget(QWidget *parent = nullptr);

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
  /// The seconds the time axis spans: the window; on "All" at the end of a
  /// recording shorter than the window only the recording (at least 10 s),
  /// so it fills the width and grows with it.
  double      shownSeconds() const;
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

  /// @name The look
  /// @{
  /// The window design's colours: what the Neutral variant shows.
  void        setTheme(const PlotTheme &theme) override;
  void        setVariant(GraphLook::Variant variant);
  GraphLook::Variant variant() const { return m_variant; }
  /// The settings page's colours, line, points and width.
  void        setLookSettings(const GraphLook::Settings &settings);
  /// Each curve after the first with a dash pattern of its own.
  void        setPatternedLines(bool on);
  bool        patternedLines() const { return m_patterns; }
  const GraphLook::Look &look() const { return m_look; }
  /// @}

  /// @name Reading it
  /// @{
  /// The time cursor @p which (0: A, 1: B) at @p t, ms since the start of
  /// the recording, within what the store keeps; negative: none.
  void        setTimeCursor(int which, qint64 t);
  qint64      timeCursor(int which) const { return m_cursorT[which & 1]; }
  void        clearTimeCursors();
  /// The statistics column next to the plot.
  void        setStatsVisible(bool on);
  bool        statsVisible() const { return m_statsOn; }
  /// @}

  /// @name Lines on the left axis' value
  /// @{
  /// The value on the left axis, which reference line, crossings, limit
  /// band and cursor levels go by; an id of -1: none (a file's is none too).
  ValueRef    leftValue() const;
  void        setReference(const GraphReference::Line &line);
  GraphReference::Line reference() const { return m_reference; }
  /// Further lines at fixed values (base unit), GraphReference::kMaxLines at most.
  void        setReferenceLines(const QList<double> &values);
  QList<double> referenceLines() const { return m_referenceLines; }
  /// The triangles where the curve crosses a fixed line, and their count in its label.
  void        setCrossingsVisible(bool on);
  bool        crossingsVisible() const { return m_crossingsOn; }
  /// The alarm thresholds on the left value (base unit): the dragged line snaps to them.
  /// A line on a threshold that went stays where it is.
  void        setAlarmThresholds(const QList<double> &values);
  QList<double> alarmThresholds() const { return m_alarmThresholds; }
  /// The limit test's band of @p value: two dashed green lines at @p low
  /// and @p high, faintly green between, while that value is the left
  /// one. An open side (NaN or infinite) has no line, the band reaches the
  /// edge. An id of -1 takes it away.
  void        setLimits(const ValueRef &value, double low = 0, double high = 0);
  /// The left axis' value at @p globalPos on a round step, for a line added
  /// there; outside the plot the middle of the axis.
  double      roundedValueAt(const QPoint &globalPos) const;
  /// "1.234 mV": a value of the left axis as its numbers.
  QString     levelText(double value) const;
  /// @}

  /// @name Marks and notes
  /// @{
  /// The marks of all recordings shown, in the order of their time; a
  /// mark is named by its place in it.
  int         markCount() const { return int(m_marks.size()); }
  /// Whether notes can be set and marks changed: not in a recording that
  /// is only looked at and not kept (a file loaded into the view).
  void        setMarksEditable(bool on);
  bool        marksEditable() const { return m_marksEditable; }
  /// The mark whose flag is at @p pos (plot widget pixels), or the note whose line is there; -1: none.
  int         markAt(const QPoint &pos) const;
  int         markAtGlobal(const QPoint &globalPos) const;
  bool        markIsNote(int mark) const;
  /// Asks for a note (NoteDlg) at @p t (ms on the graph's time; -1: now,
  /// or the newest reading of a recording stopped) and sets it in the
  /// recording of the left axis' value. Its place in the marks, -1 when
  /// the dialog was left or there is no recording. Says noteAdded().
  int         askNote(qint64 t = -1);
  /// Sets a note without asking; as askNote(), without noteAdded().
  /// @p photo: the photo it shows (PhotoStore::relative()).
  int         addNote(qint64 t, const QString &title, const QString &note, const QString &photo = QString());
  /// Asks for another title and text of @p mark.
  bool        editMark(int mark);
  void        removeMark(int mark);
  /// Moves the note @p mark to @p t (ms on the graph's time, within its
  /// recording); its new place in the marks.
  int         moveMark(int mark, qint64 t);
  /// Title, time and text of @p mark as the tooltip shows them (HTML).
  QString     markTip(int mark) const;
  /// The time at @p globalPos, ms on the graph's time; -1 outside the plot.
  qint64      timeAt(const QPoint &globalPos) const;
  /// @}

  /// @name Overview, states, bands
  /// @{
  /// The overview strip below the plot instead of the scrollbar.
  void        setMinimapVisible(bool on);
  bool        minimapVisible() const { return m_minimapOn; }
  /// The strip of the meter's states at the bottom of the plot.
  void        setStatesVisible(bool on);
  bool        statesVisible() const { return m_statesOn; }
  /// The min/max band behind the curve of the left axis' value.
  void        setEnvelopeVisible(bool on);
  bool        envelopeVisible() const { return m_envelopeOn; }
  /// @}

  /// @name Outliers
  /// @{
  /// Readings not drawn and left out of the statistics; the store keeps them.
  void        setOutliers(const GraphOutliers::Rule &rule);
  GraphOutliers::Rule outliers() const { return m_outliers; }
  /// How many readings in the time shown are hidden (counted at most four times a second).
  int         outliersHidden() const { return m_outliersHidden; }
  /// Counts them now (the tests).
  void        countOutliers();
  /// Asks for the rule's figures (Jump and Limits in the left axis' unit) and sets it.
  void        askOutliers(GraphOutliers::Mode mode);
  /// @}

  /// The units of the value axes drawn, the left one first.
  QStringList axisUnits() const;
  /// The axis key of values that have readings but no unit (a
  /// microcontroller's numbers): they get an axis of their own like a unit.
  static inline const QString kNoUnit = QStringLiteral("1");
  /// @p unit of an axis as shown: SiPrefix::displayText(), "no unit" for kNoUnit.
  static QString unitText(const QString &unit);
  /// Asks for a fixed range of the axis of @p unit (base unit), with
  /// @p min and @p max at first; without them what the axis shows now.
  /// False when cancelled.
  bool        askFixedRange(const QString &unit, double &min, double &max, bool given);

  void        setHold(bool hold) override;
  /// Draws the store anew; the readings are the store's and stay.
  void        clear() override;

  /// @name Pictures
  /// What a picture of the graph shows: the legend, the plot with its axes,
  /// lines and flags, and the states' strip; not the buttons, the overview
  /// or the statistics. SVG and PDF keep the plot as vectors.
  /// @{
  /// The picture's natural size: the plot's, with the legend's row above it.
  QSize       pictureSize() const;
  /// The picture into @p target of @p painter, scaled to it, its proportions kept.
  void        paintPicture(QCPPainter *painter, const QRectF &target);
  /// The picture, recorded at the screen's resolution, to be played anywhere.
  QPicture    picture();
  /// The picture as an image at its natural size.
  QImage      pictureImage();
  /// The picture as a file, by its suffix: svg, pdf, png or jpg. False: not written.
  bool        exportImageFile(const QString &fileName);
  /// The picture on the clipboard.
  void        copyImage();
  /// A page: @p title, when the recording started, its sample time, @p comment, the picture below.
  void        print(QPrinter *printer, const QString &title, const QString &comment);
  /// The colour @p value is drawn in; invalid while it is not drawn (not
  /// shown, or no axis for its unit).
  QColor      drawnColor(const ValueRef &value) const;
  /// @}

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
  /// The pen curve @p i is drawn with, and whether its readings have markers now.
  QPen        curvePen(int i) const;
  bool        curveMarkers(int i) const;
  /// The pen it is drawn with now: solid where its points are too close for dashes.
  QPen        drawnPen(int i) const;
  /// The colour of axis @p i's numbers: its curve's when it has one alone.
  QColor      axisColor(int i) const;
  /// The grid lines of axis @p i now (10 x 8 divisions: 9 across the plot).
  int         axisTickCount(int i) const;
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
  /// The statistics as plain text, as they are now (not waiting for the column's pace).
  QString     statsText() const;
  /// What the crosshair says for the mouse at @p pos (plot widget pixels), and where its lines are.
  QString     crosshairTextAt(const QPoint &pos);
  /// The level line of cursor @p which: its value on the left axis (that axis' unit); NaN while it has none.
  double      cursorLevel(int which) const;
  /// The x of cursor @p which in the plot widget, -1 while it is not shown.
  int         cursorPixel(int which) const;
  /// Line @p line (-1 the reference line, 0.. the added ones): its value now
  /// (base unit; NaN while it is not drawn), its label, its crossings, its
  /// y in the plot widget (-1 while it is not drawn).
  double      referenceValue(int line = -1) const;
  QString     referenceLabel(int line = -1) const;
  int         crossingCount(int line = -1) const;
  /// The pixel row of line @p line (as referenceValue(), or kTriggerLine); -1: not drawn.
  int         referencePixel(int line = -1) const;
  /// The limit band drawn: its edges (base unit, infinite for an open side); false: none.
  bool        limitBand(double *low, double *high) const;
  /// The start threshold's line (base unit); NaN: none.
  double      triggerLevel() const { return m_triggerLevel; }
  /// referencePixel() and levelAt(): no line, and the start threshold's.
  static constexpr int kNoLine = -2;
  static constexpr int kTriggerLine = -3;
  /// Mark @p mark: its time on the graph's time (ms), its title, its
  /// colour, the x of its line in the plot widget (-1 while it is not
  /// shown), its flag below the plot (empty while it has none) and what
  /// the flag says.
  qint64      markTime(int mark) const;
  QString     markName(int mark) const;
  /// Its photo (RecordingStore::Mark::photo), empty for none.
  QString     markPhoto(int mark) const;
  QColor      markColor(int mark) const;
  int         markPixel(int mark) const;
  /// The marks sharing the flag of @p mark (several where their lines fall
  /// together, a series of photos; the first one has the flag); empty while
  /// it is not shown.
  QList<int>  markGroup(int mark) const;
  QRect       markFlagRect(int mark) const;
  QString     markFlagText(int mark) const;
  /// The flag of cursor @p which below the plot; empty while it has none.
  QRect       cursorFlagRect(int which) const;
  GraphMinimap *minimap() const { return m_minimap; }
  /// The states in the time shown ("HOLD 3.0-5.0 | OL 8.0-9.0"), whether the strip is shown or not.
  QString     statesText() const;
  /// Whether curve @p i is drawn as a band now.
  bool        curveBanded(int i) const;
  /// The band of curve @p i (-1: the min/max band) at @p t (ms on the
  /// graph's time): its lowest and highest value, base unit; false where it has none.
  bool        bandAt(int i, qint64 t, double *lo, double *hi) const;
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
  void        statsVisibleChanged(bool on);
  void        cursorsChanged();
  /// The reference line, the added lines or the crossings' switch changed, by a drag too.
  void        referenceChanged();
  /// Another value is on the left axis.
  void        leftValueChanged();
  /// Overview, states or the min/max band were switched.
  void        overviewChanged();
  /// The rule for outliers changed (setOutliers(), askOutliers()).
  void        outliersChanged();
  /// The start threshold was dragged to @p value (base unit): the settings take it.
  void        startThresholdDragged(double value);
  /// A note was set at @p when (askNote()): the readings table marks its reading.
  void        noteAdded(const QDateTime &when, const QString &title);
  /// A note was dragged from @p from to @p to: the readings table moves its mark.
  void        noteMoved(const QDateTime &from, const QDateTime &to, const QString &title);
  /// A click on the flag of a note with a photo: @p photo, its path in the
  /// project (RecordingStore::Mark::photo).
  void        photoClicked(const QString &photo);

protected:
  void        refreshPlot() override;
  CrosshairAt crosshairAt(double x, double y) override;
  void        resizeEvent(QResizeEvent *event) override;
  void        keyPressEvent(QKeyEvent *event) override;
  /// As narrow as the row above still holds its buttons and one short
  /// legend entry; the arrangement of the windows keeps to it.
  QSize       minimumSizeHint() const override;

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
    QPen        pen;             ///< its line; Qt::NoPen: none (styleCurves())
    QList<QPointF> points;       ///< thinned, all the store keeps, on the graph's time
    int         tail = 0;        ///< how many of them are the newest bucket's
    bool        banded = false;  ///< too dense for a line: drawn as a band (drawCurves())
    bool        unitless = false; ///< readings, but no unit: on the axis of kNoUnit
  };
  /// One value axis: the unit it has now, and the frame zoomed on.
  struct Axis
  {
    QCPAxis    *axis = nullptr;
    QString     unit;            ///< base unit; empty: free
    double      factor = 1;      ///< base unit per unit of the axis
    double      zoomMin = 0;     ///< while m_zoomed, base unit
    double      zoomMax = 0;
    bool        divided = false; ///< its ticker is the one of the 8 divisions
  };

  // graphwidget_curves.cpp
  /// The curves anew from values and sources: stores, plot graphs, connections.
  void        makeCurves();
  /// The series of @p curve's value, oldest first.
  QList<const RecordingSeries *> seriesOf(const Curve &curve) const;
  /// How often @p curve turns between @p from and @p to (s, graph time) by
  /// at least GraphBand::kTurnPixels on its axis as it stands now.
  int         turnsIn(const Curve &curve, double from, double to) const;
  /// The bucket for the time shown on the plot's width.
  qint64      bucketNow() const;
  /// All points anew, and which axis each curve has.
  void        rebuildPoints();
  void        rebuildCurve(Curve &curve);
  /// The newest reading of @p series went into @p curve: a new bucket adds
  /// its points, one still filling replaces them.
  void        appendTail(Curve &curve, const RecordingSeries &series);
  /// The decimator's options for @p series of @p curve: its hidden outliers.
  SeriesDecimator::Options decimation(const Curve &curve, const RecordingSeries &series) const;
  /// countOutliers() after a picture, at most four times a second and a tenth of the time.
  void        scheduleOutlierCount();
  void        mainAppended(RecordingStore *store, bool shifted);
  void        extraAppended(RecordingStore *store, int index);
  /// What a store did other than a reading at the end.
  void        storeChanged();
  void        drawCurves(double from, double to);
  void        updateLegend();
  /// A store is gone: its curves go, without asking the sources.
  void        storeGone(QObject *store);
  // graphwidget_theme.cpp
  void        setupTheme();
  /// The look from variant, design and settings, onto the plot and the curves.
  void        applyLook();
  /// Which curve @p curve is for its colour and dashes: its place among the values, its meter.
  GraphLook::CurveKey keyOf(int curve) const;
  QColor      colorOf(int curve) const;
  /// Pens of the curves and the colours of the axes' numbers.
  void        styleCurves();
  /// Curve @p curve with @p shown points on the plot's width: markers
  /// while they are few; dense, neither smoothing nor dashes - a band of
  /// strokes shows neither, and both cost (a dashed or smoothed wide line
  /// of thousands of strokes takes seconds).
  void        styleDensity(const Curve &curve, int shown);
  /// The 8 divisions of an axis showing @p lo .. @p hi (its unit): the range widened to whole 1-2-5 steps.
  static void divide(double &lo, double &hi, double &step);
  // graphwidget_axes.cpp
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
  // graphwidget_overlays.cpp
  void        setupOverlays();
  /// A dialog for a minimum and a maximum in @p unit (base unit), titled
  /// @p title; false when cancelled.
  bool        askMinMax(const QString &title, const QString &unit, double &min, double &max);
  /// The curve the crosshair, the cursors' levels and later the reference line go by: the first on the left axis; -1: none.
  int         leftCurve() const;
  GraphStats::Curve statsCurve(int curve) const;
  GraphStats::Input statsInput() const;
  /// The cursors' lines, flags and levels where the axes are now.
  void        updateCursors();
  /// The statistics soon, not with every reading.
  void        scheduleStats();
  void        updateStats();
  /// The column at its side, as wide as it wants; the rest for the plot.
  void        layoutStats();
  /// Reference line, added lines, crossings, limit band and start threshold where the axes are now.
  void        updateLevels();
  /// The value the reference line follows (mean, minimum, maximum of the time shown); NaN: none.
  double      followedValue();
  /// The line at @p pos one can drag: -1 the reference line, 0.. an added one, kTriggerLine; kNoLine: none.
  int         levelAt(const QPoint &pos) const;
  /// The line dragged to @p y (plot widget pixels): the reference line snaps, an added one and
  /// the start threshold take a round value.
  void        dragLevel(int line, double y);
  /// The cursor whose line is at @p pos (plot widget pixels): 0, 1, or -1.
  int         cursorAt(const QPoint &pos) const;
  // graphwidget_overview.cpp
  void        setupOverview();
  /// The overview's store, value and window, the strip, the bands: as the curves are now.
  void        updateOverview();
  /// The strip at the bottom of the axes' rectangle.
  void        placeStates();
  // graphwidget_marks.cpp
  void        setupMarks();
  /// The marks anew from the stores, and the flags of marks and cursors.
  void        updateMarks();
  /// The recording a note goes to - the left value's, else the one of the
  /// time - and its time 0 on the graph's time (ms); nullptr: none.
  RecordingStore *noteStore(qint64 *shift = nullptr) const;
  /// The place of mark @p index of @p store in the marks; -1: not there.
  int         markOf(const RecordingStore *store, int index) const;
  /// The mark pressed on, in its store's marks now - other marks may have
  /// come meanwhile; -1: it is gone.
  int         draggedIndex() const;
  /// Ends the drag of a note: it stays (noteMoved()) or goes back (@p cancel).
  void        endMarkDrag(bool cancel);
  /// The tooltip of the flag of @p mark: its note, or a line per mark of its group.
  QString     flagTip(int mark) const;
  /// A menu of the marks of one flag; the one chosen shows its photo or its note.
  void        chooseMark(const QList<int> &group, const QPoint &globalPos);
  // graphwidget_input.cpp
  void        setupInput();
  void        mouseDoubleClicked(QMouseEvent *event);
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
  PlotTheme   m_design;                  ///< setTheme(): the window design
  GraphLook::Variant m_variant = GraphLook::Neutral;
  GraphLook::Settings m_lookSettings;
  GraphLook::Look m_look;
  bool        m_patterns = false;
  QCPAbstractItem *m_centreTicks = nullptr;   ///< the phosphor tubes' fine ticks
  GraphTimeBar *m_timeBar = nullptr;
  QScrollBar *m_scrollBar = nullptr;
  QRubberBand *m_zoomBand = nullptr;
  GraphStatsPanel *m_statsPanel = nullptr;
  QTimer      m_statsTimer;
  bool        m_statsOn = false;
  qint64      m_cursorT[2] = { -1, -1 };   ///< ms since the start of the recording, -1: none
  QCPItemStraightLine *m_cursorLine[2] = {};
  QCPItemStraightLine *m_cursorLevel[2] = {};
  /// A mark of a store shown.
  struct Mark
  {
    QPointer<RecordingStore> store;
    int         index = 0;       ///< in the store's marks
    qint64      shift = 0;       ///< the store's time 0 on the graph's time, ms
    qint64      t = 0;           ///< on the graph's time, ms
    quint32     color = 0;
    QString     name;
    QString     note;
    QString     meter;           ///< its meter's name while several are shown
    bool        isNote = false;
    QString     photo;           ///< a note's photo, its path in the project
  };
  GraphMinimap *m_minimap = nullptr;
  qint64      m_minimapShift = 0;        ///< its store's time 0 on the graph's time, ms
  GraphStates::Cache m_statesCache;   ///< the strip's spans, each reading scanned once
  GraphStateStrip *m_stateStrip = nullptr;
  QCPAbstractItem *m_bands = nullptr;    ///< dense curves and the min/max band
  bool        m_minimapOn = true;
  bool        m_statesOn = true;
  bool        m_envelopeOn = false;
  QList<Mark> m_marks;                   ///< by time
  QCPAbstractItem *m_markItem = nullptr; ///< their lines and the flags
  QPointer<RecordingStore> m_dragStore;  ///< Drag::Mark: the note dragged, its store,
  int         m_dragIndex = -1;          ///< its place in the store's marks
  qint64      m_dragFrom = 0;            ///< and its time before, the store's
  qint64      m_dragT = 0;               ///< where it is now, and its title: it is known by them
  QString     m_dragName;
  bool        m_marksEditable = true;
  int         m_tipMark = -1;            ///< the mark whose tooltip is up
  GraphReference::Line m_reference;
  QList<double> m_referenceLines;
  bool        m_crossingsOn = true;
  GraphOutliers::Rule m_outliers;
  int         m_outliersHidden = 0;
  bool        m_outlierCountPending = false;
  qint64      m_outlierCountCost = 0;   ///< ms the last count took
  QList<double> m_alarmThresholds;
  ValueRef    m_limitValue{ -1 };
  double      m_limitLow = 0;
  double      m_limitHigh = 0;
  ValueRef    m_lastLeft{ -1 };          ///< leftValueChanged() was said for it
  QCPAbstractItem *m_levels = nullptr;      ///< band and lines, under the curves
  QCPAbstractItem *m_levelMarks = nullptr;  ///< labels and crossings, over them
  /// A line drawn: which (-1 the reference line, 0.. added), its value, its label, its crossings.
  struct Level { int line = -1; double value = 0; QString label; int crossings = 0; };
  QList<Level> m_levelsDrawn;
  bool        m_bandDrawn = false;
  double      m_triggerLevel = qQNaN();
  int         m_triggerCurve = -1;       ///< the curve whose store's start threshold is drawn
  double      m_followed = qQNaN();      ///< followedValue(), kept while it costs
  QElapsedTimer m_followedAge;
  qint64      m_followedCost = 0;        ///< ms the last one took
  qint64      m_bucket = 1;      ///< the bucket the points were thinned with, ms
  bool        m_dirty = true;    ///< the points are to be built anew
  int         m_windowSeconds = 600;
  bool        m_followAll = true;
  bool        m_followEnd = true;   ///< the window ends at the newest reading
  qint64      m_start = 0;          ///< its start while it does not, ms
  /// A frame was zoomed on: the axes keep its values (Axis::zoomMin), and the window before.
  bool        m_zoomed = false;
  struct { qint64 start = 0; int seconds = 0; bool all = false; bool atEnd = true; } m_zoomBefore;
  /// The mouse: a drag moves the time, with Shift it draws the zoom frame.
  enum class Drag { None, Pan, Frame, Cursor, Level, Mark };
  Drag        m_drag = Drag::None;
  QPoint      m_pressPos;
  qint64      m_pressStart = 0;
  bool        m_moved = false;      ///< the mouse went more than a slip since the press: no click
  int         m_dragCursor = 0;     ///< Drag::Cursor: which one
  int         m_dragLevel = -1;     ///< Drag::Level: which line (levelAt())
};
