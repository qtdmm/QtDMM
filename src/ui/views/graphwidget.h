//======================================================================
// File:		dmmgraph.h
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:43:46 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================

#pragma once

#include <array>

#include <QtGui>
#include <QtWidgets>
#include <QPrinter>
#include <QChartView>
#include <QChart>
#include <QLineSeries>
#include <QScatterSeries>
#include <QValueAxis>
#include <QCategoryAxis>
#include <limits>
#include <QGraphicsLineItem>

#include "ui/views/gapline.h"
#include "recording/recordingstore.h"
#include "core/valueref.h"
#include <QPointer>
#include <functional>

class GraphFlagItem;
class GraphLegend;
class GraphStateStrip;
class GraphMinimap;
class Settings;
struct Recording;

/// The recorder's view: plots the curve a RecordingStore keeps.
///
/// The store does the recording (every reading with its time, triggers,
/// integral, marks); the graph shows a window of it in time (scrollable),
/// scales the y axis, and draws the integral with its scale and offset, the
/// cursor and threshold lines and the marks. The x axis is the time since
/// the start of the recording; more readings than pixel columns are thinned
/// to the minimum and maximum of each column. Rendering uses Qt Charts; the
/// cursor and threshold lines are QGraphicsLineItems on top of the chart.
/// The recorder settings (setSampleTime(), setMode(), setThresholds(), ...)
/// are passed on to the store, so the graph can still be driven as a whole.
/// Data can be exported/imported as CSV and printed.
///
/// The store's state shows: in Live the window follows the newest reading
/// (the scroll bar rests), while recording "● REC 0:42 / 5:00" stands top
/// left in the plot, in View where the curve comes from (the start of the
/// recording or the file).
///
/// The sample time is in tenths of a second, like the settings: the grid
/// of the export, not the rate the graph shows.
class GraphWidget : public QWidget
{
  Q_OBJECT
public:
  /// How recording is started.
  enum SampleMode
  {
    Manual = 0,   ///< Start button
    Time,         ///< at setStartTime()
    Raising,      ///< when the reading rises through the raising threshold
    Falling       ///< when the reading falls through the falling threshold
  };

  /// Marker drawn at each sample.
  enum PointMode
  {
    NoPoint = 0,
    Circle,
    Square,
    Diamond,
    X,
    LargeCircle,
    LargeSquare,
    LargeDiamond,
    LargeX
  };

  /// Line style between samples.
  enum LineMode
  {
    NoLine = 0,
    Solid,
    Dot
  };

  /// Which horizontal line the mouse is dragging.
  enum CursorMode
  {
    NoCursor = 0,
    Trigger,      ///< recording start threshold
    Integration,  ///< integration threshold
    Reference,    ///< the reference line
    TimeCursorA,  ///< the time cursor set with a click
    TimeCursorB,  ///< the one set with Shift+click
    ExtraReference ///< an added reference line (m_dragExtra)
  };

  /// What the reference line stands at: 0 (the default), a value of its
  /// own (dragged there), the mean, minimum or maximum of the window shown
  /// - those follow the readings - or an alarm threshold; or no line.
  enum ReferenceKind
  {
    RefZero = 0,
    RefValue,
    RefMean,
    RefMin,
    RefMax,
    RefAlarm,
    RefOff
  };

  /// The graph's colours (context menu "Graph colours", Graph/variant).
  /// Each brings background, grid, lettering and curve colours; a curve
  /// colour chosen in the settings stays. Custom is the colours from the
  /// settings page, as before the variants.
  enum ColorVariant
  {
    Neutral,         ///< follows the window design
    ScopeBlue,       ///< oscilloscope, blue screen
    PhosphorGreen,   ///< green phosphor tube: one colour, 10 x 8 divisions
    PhosphorAmber,   ///< the same in amber
    ChartRecorder,   ///< paper and ink
    ColourBlind,     ///< white paper, the Okabe-Ito hues: apart with red-green or blue-yellow weakness
    HighContrast,    ///< black, white and pure hues, thicker lines
    Custom           ///< the colours from the settings page
  };
  /// Every variant, in the order of the menu and the settings page.
  static QList<ColorVariant> colorVariants();

  /// Entries of the context menu.
  enum PopupID
  {
    IDConnect = 1,
    IDDisconnect,
    IDStopRecorder,
    IDStartRecorder,
    IDClearGraph,
    IDConfigure,
    IDImportData,
    IDCopyImage,
    IDExportImage,
    IDColorVariant,  ///< property "variant" says which
    IDLive,
    IDReference,     ///< properties "kind" and "value"
    IDClearCursors,
    IDMinimap,
    IDEnvelope,
    IDNoteHere,      ///< property "t": the time clicked (ms)
    IDNoteNow,
    IDEditMark,      ///< property "mark": its index in RecordingStore::marks()
    IDRemoveMark,    ///< property "mark"
    IDCursorHere,    ///< properties "which" (0 = A, 1 = B) and "t" (ms)
    IDResetZoom,
    IDAxisAuto,      ///< property "unit": its right axis automatic again
    IDAxisFixed,     ///< property "unit": asks for its fixed range
    IDStats,         ///< the statistics beside the plot on or off
    IDViewButtons,   ///< the buttons beside the time buttons on or off
    IDStates,        ///< the strip of the meter's states on or off
    IDPatterns,      ///< patterned lines for the other meters' curves on or off
    IDAddReference,  ///< another reference line, property "value"
    IDRemoveReference, ///< an added reference line, property "index"
    IDCrossings      ///< the marks where the curve crosses a reference line
  };

  GraphWidget(QWidget *parent, Settings *settings);
  GraphWidget(QWidget *parent = nullptr);
  ~GraphWidget();
  /// The window from the settings, in seconds (also how wide Live is
  /// without a recording length). A window other than the last one ends
  /// "All"; the same one keeps it, and the window of "All" stays.
  void             setGraphSize(int size);
  /// The window last set with setGraphSize(), in seconds (0: none yet).
  int              windowSetting() const { return m_savedWindow; }
  /// The visible window in seconds: the settings' one, or that of "All".
  int              windowSeconds() const { return m_windowSeconds; }
  /// The recorder behind the graph.
  RecordingStore  *store() const { return m_store; }
  /// Shows @p store instead of the graph's own one (the MeterController's
  /// recorder). The graph does not take ownership; the store has to outlive it.
  void             setStore(RecordingStore *store);
  /// Unit of the recorded quantity for the axis label; the SI prefix is
  /// stripped because values arrive in base units (see DmmDecoder::DmmChannel).
  void             setUnit(const QString &);
  /// Sample time in tenths of a second.
  /// The window and the length are kept in seconds: a new sample time
  /// recounts them in samples, so the order of the two calls does not matter.
  void             setSampleTime(int v);
  /// Recording duration in tenths of a second after which recording stops
  /// on its own (0 = until stopped).
  void             setSampleLength(int v);
  /// Clock time for SampleMode::Time.
  void             setStartTime(const QTime &time) { m_store->setStartTime(time); }
  void             setMode(GraphWidget::SampleMode mode);
  /// Prints the curve with title and comment.
  void             print(QPrinter *prt, const QString &, const QString &);
  /// Thresholds for the Raising/Falling start modes.
  void             setThresholds(double falling, double raising);
  /// Y axis: automatic (optionally always including zero) or fixed min/max.
  void             setScale(bool autoScale, bool includeZero, double min, double max);
  void             setColors(const QColor &bg, const QColor &grid,
                             const QColor &data, const QColor &cursor,
                             const QColor &start,
                             const QColor &integration, const QColor &intThreshold);
  /// Colours of the window design (background, grid, axis lettering); a
  /// background of Qt::NoBrush means the colours from the settings. @p data
  /// replaces the curve colour only while that is the default blue.
  void             setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels,
                                  const QColor &data = QColor());
  /// The colours in use: @p variant for this graph (context menu), or
  /// the default from the settings page when @p variant is -1.
  void             setColorVariant(ColorVariant defaultVariant, int variant = -1);
  ColorVariant     colorVariant() const { return m_variant; }
  int              colorOverride() const { return m_variantOverride; }
  /// "neutral", "scope", "phosphor-green", "phosphor-amber", "recorder", "custom"
  static QString   variantName(ColorVariant variant);
  static ColorVariant variantFromName(const QString &name);
  /// The variant's name in the menus ("Scope blue").
  static QString   variantTitle(ColorVariant variant);
  /// The smallest time step (..., 1, 2, 5, 10, 15, 30 s, 1, 2, 5, 10, 15,
  /// 30 min, 1, 2, 3, 6, 12 h, days) that is at least @p v seconds: the
  /// x division. Public for the tests.
  static double    timeStep(double v);
  /// Puts the reference line at @p kind; @p value for RefValue and
  /// RefAlarm (a value of 0 is RefZero).
  void             setReference(ReferenceKind kind, double value = 0);
  ReferenceKind    referenceKind() const { return m_referenceKind; }
  /// Where the line is now, in base units; NaN when the mean, minimum or
  /// maximum has no reading in the window.
  double           referenceValue() const { return m_reference; }
  /// Further reference lines at fixed values (base units of the left axis),
  /// besides the one above: added in its menu, dragged, removed there; at
  /// most kMaxExtraReferences. Kept per window (Windows/<graph>-reference-lines).
  void             setExtraReferences(const QList<double> &values);
  QList<double>    extraReferences() const { return m_extraRefs; }
  static constexpr int kMaxExtraReferences = 4;
  /// Marks where the curve of the left axis crosses a reference line in the
  /// window shown, up or down; the line's label counts them (View, Crossings).
  void             setCrossingsVisible(bool on);
  bool             crossingsVisible() const { return m_crossingsOn; }
  /// The crossings counted for line @p line: -1 the reference line, 0.. the added ones.
  int              crossingCount(int line) const;
  /// Whether the line is drawn: in the scale, and its value (referenceId()) shown.
  bool             referenceShown() const;
  /// The reference as kept per window (Windows/graph-reference, the
  /// workspace): "" for 0, "mean", "min", "max", "alarm <value>", "<value>",
  /// "off".
  QString          referenceSetting() const;
  void             setReferenceSetting(const QString &text);
  /// Fixed ranges of the right axes by unit ("Hz" 49.5 .. 50.5): the axis
  /// of that unit covers at least them, on round ticks; a unit without one
  /// scales itself. The left axis has its range in the settings.
  void             setFixedRange(const QString &unit, double min, double max);
  void             clearFixedRange(const QString &unit);
  bool             hasFixedRange(const QString &unit) const { return m_fixedRanges.contains(unit); }
  /// The ranges as kept with the window layout: {"Hz":[49.5,50.5]}.
  QString          fixedRangesSetting() const;
  void             setFixedRangesSetting(const QString &text);
  /// The y axis under @p pos (in the view): 0 left, 1 right, 2 third, -1 none.
  int              axisSideAt(const QPoint &pos) const;
  /// The alarms' thresholds (base units): the reference line snaps to them.
  /// A line on a threshold that went stays where it is.
  void             setAlarmThresholds(const QList<double> &values);
  /// The limit test's band (PASS/FAIL) of the value @p value, base units:
  /// two dashed green lines at @p low and @p high, faintly green between,
  /// while that value is the left axis' (leftValue()). An id of -1
  /// takes it away; an open side (NaN or infinite) has no line, the band
  /// reaches the edge. The reference line snaps to the lines.
  void             setLimits(const ValueRef &value, double low = 0, double high = 0);
  void             setLimits(int id, double low = 0, double high = 0) { setLimits(ValueRef(id), low, high); }
  /// @name Time cursors
  /// A click into the graph sets cursor A, Shift+click cursor B; both can
  /// be dragged, Esc and the context menu take them away. A alone says the
  /// time and the value there; with B the graph says the time and the
  /// change of the value between them, their mean and the integral (Ah for
  /// a current, Wh for a power).
  /// @{
  /// Puts cursor @p which (0 = A, 1 = B) at @p t, ms since the start of
  /// the recording; -1 takes it away.
  void             setTimeCursor(int which, qint64 t);
  qint64           timeCursor(int which) const { return m_cursorT[which & 1]; }
  /// The statistics beside the plot: the values at the cursors, between
  /// them and over the window shown; on at first, kept with the layout.
  void             setStatsVisible(bool on);
  bool             statsVisible() const { return m_statsOn; }
  /// The statistics left of the plot instead of right (Settings, Graph;
  /// the same for every graph).
  void             setStatsLeft(bool left);
  /// The buttons beside the time buttons: overview, band, reference line,
  /// remove the cursors, a note now; kept with the layout.
  /// The strip at the bottom of the plot with the states the meter reported
  /// for the value on the left axis (HOLD, MIN/MAX/PEAK, REL, OL); on at
  /// first, kept with the graph.
  void             setStatesVisible(bool on);
  bool             statesVisible() const { return m_statesOn; }
  /// Each curve of another meter dashed in a pattern of its own, so the
  /// curves are told apart without their colours (View, Patterned lines).
  void             setPatternedLines(bool on);
  bool             patternedLines() const { return m_patternsOn; }
  /// The states in the window as text, "HOLD 5.0-10.0 | OL 12.0-13.0"
  /// (seconds; for the tests), empty without any or while off.
  QString          statesText() const;
  void             setViewButtonsVisible(bool on);
  bool             viewButtonsVisible() const { return m_viewButtonsOn; }
  /// What they say, as plain text (also while hidden).
  QString          statsText() const;
  /// Zoomed on a frame (Shift + drag): "Reset zoom" undoes it.
  bool             zoomed() const { return m_zoomed; }
  /// "Reset zoom": the window of before the zoom, the axes as before.
  void             resetZoom() { endZoom(true); }
  void             clearTimeCursors();
  /// @}
  /// @name The overview strip below the graph (GraphMinimap)
  /// All the store keeps, the window between two brackets; in place of
  /// the scroll bar while it is shown (context menu "Overview", kept per
  /// window as Windows/graph-minimap). Moved back from the newest reading
  /// in Live, the window stays where it is ("Live paused") until the
  /// strip, a time button or Live bring it back.
  /// @{
  void             setMinimapVisible(bool on);
  /// Entries of the window around the graph ("Hide window"), first in its
  /// context menu; not owned.
  void             setWindowActions(const QList<QAction *> &actions) { m_windowActions = actions; }
  /// Printing in the context menu, after Export image (the window's action).
  void             setPrintAction(QAction *action) { m_printAction = action; }
  bool             minimapVisible() const { return m_minimapOn; }
  GraphMinimap    *minimap() const { return m_minimap; }
  /// @}
  /// How often the curves were built anew from the whole store (for the tests).
  int              rebuilds() const { return m_rebuilds; }
  /// Where the time axis starts, s since the start of the recording (for the tests).
  double           timeAxisStart() const;
  /// @name The min/max band behind the curve
  /// Per pixel column the lowest and the highest value of the columns
  /// within kEnvelopeColumns either side: a band as wide on screen whatever
  /// the window, which shows the spread around the curve. Off by default
  /// (context menu "Min/max band", kept per window as
  /// Windows/graph-envelope).
  /// @{
  void             setEnvelopeVisible(bool on);
  bool             envelopeVisible() const { return m_envelopeOn; }
  /// The band's outline in scene coordinates, empty while off. For the tests.
  QPainterPath     envelopePath() const;
  static constexpr int kEnvelopeColumns = 4;
  /// @}
  /// @name The meter's other values
  /// A curve for each series of the store's other values
  /// (RecordingStore::extra(), a second display), in a colour of its own.
  /// A value in the main value's unit shares its axis; the first other unit
  /// gets an axis on the right with a scale of its own, a third unit a third
  /// axis outside it; values in a fourth unit are not drawn. The titles of
  /// the right axes are in the colour of their first curve. A legend names
  /// the curves. They are off until
  /// switched on (the sidebar's value menu, kept per window as
  /// Windows/graph-shown-values): the graph shows the main value, as it
  /// always did. The store records them all the same.
  /// Reference line, band and cursor levels are of the value of the left
  /// axis (referenceId()), so is the overview (minimapValue()); the
  /// triggers stay the main value's.
  /// @{
  int              extraCurveCount() const { return int(m_extraCurves.size()); }
  /// The first segment of the curve of extra(@p i) (for the tests).
  QLineSeries     *extraCurve(int i) const { return m_extraCurves[size_t(i)].series; }
  bool             extraCurveShown(int i) const { return m_extraCurves[size_t(i)].shown; }
  /// Drawn as a band, too dense for a line (updateDenseBands()).
  bool             extraCurveBanded(int i) const { return m_extraCurves[size_t(i)].band->isVisible(); }
  bool             extraCurveOnRight(int i) const { return m_extraCurves[size_t(i)].side == 1; }
  /// The axis of curve @p i: 0 left, 1 right, 2 the third axis.
  int              extraCurveSide(int i) const { return m_extraCurves[size_t(i)].side; }
  QValueAxis      *rightAxis() const { return m_right[0].axis; }
  /// The third axis, outside the right one.
  QValueAxis      *thirdAxis() const { return m_right[1].axis; }
  /// The left axis' title, "[mV]".
  QString          leftAxisTitle() const { return m_yTitle->text(); }
  /// Its title, "[°C]" (empty while it is hidden).
  QString          rightAxisTitle() const { return sideTitle(1); }
  QString          thirdAxisTitle() const { return sideTitle(2); }
  /// The colour of the right (1) or third (2) axis' title.
  QColor           sideTitleColor(int side) const { return m_right[size_t(side - 1)].title->brush().color(); }
  /// The legend's text, empty while there is only the main value.
  QString          legendText() const;
  /// Value @p id on or off (0 the main value; the sidebar's value menu).
  /// Each change sets the view afresh: axes, scales and prefixes from the
  /// curves shown; the recording is not touched. At most two units are
  /// drawn: a value in a fourth one waits until another goes off, or until
  /// an axis is chosen for it (setValueAxis()).
  void             setValueShown(int id, bool shown);
  /// The axis of a value (the sidebar's value menu): Left, Right or Third
  /// puts it there - a unit the axis has already joins it, another one takes
  /// the axis; the curves of the old unit on it move to a free right axis
  /// when they are on Auto, else they go off (notice()). Auto, for the
  /// layouts from before: the first unit left, the second right, the third
  /// on the third axis, a unit there already on its axis. The main value is
  /// always on the left.
  enum ValueAxis { AxisAuto, AxisLeft, AxisRight, AxisThird };
  void             setValueAxis(int id, ValueAxis axis);
  ValueAxis        valueAxis(int id) const { return id <= 0 ? AxisLeft : m_valueAxes.value(id, AxisAuto); }
  /// Whether value @p id is drawn: switched on and its unit on an axis.
  bool             valueDrawn(int id) const;
  /// The colour of value @p id's curve (also while it is off).
  QColor           valueColor(int id) const { return id <= 0 ? dataColor() : extraColor(id); }
  /// The axis value @p id is drawn on, AxisAuto while it is not drawn.
  ValueAxis        valueSide(int id) const;
  /// The value of this meter the overview draws: the one of the left axis
  /// (referenceId()), with none there the lowest on the right, with none
  /// drawn the main value. Another meter's value on the left axis comes
  /// before one of this meter's on the right (leftValue()).
  int              minimapValue() const;
  /// The value of the left axis: this meter's (referenceId()), with none
  /// of them there the first other meter's curve on it; id -1 with none.
  /// Reference line, band, cursor levels, the limit band and the overview
  /// go by it.
  ValueRef         leftValue() const;
  /// The other values shown (ids > 0).
  QList<int>       shownValues() const;

  /// Start, stop, Live and Clear of a recording of several meters
  /// (DeviceSession): the graph's store may follow another one. Without
  /// them they act on the graph's store.
  struct Control
  {
    std::function<void()> start, stop, live, clear;
  };
  void             setControl(const Control &control) { m_control = control; }

  /// @name Values of other meters
  /// Curves of meters open beside the window's (DeviceSession), each from
  /// that meter's own store, shifted to this store's time by their starts
  /// (RecordingStore::offsetOf()). On the left axis in its unit, else on
  /// the right one while that is free or has its unit; otherwise not
  /// drawn. A value of a meter not open is kept without a curve. Minimap,
  /// envelope, integral and the cursors' figures stay with this store's.
  /// A file viewed (importCsvFile()) shows the other meters in it instead,
  /// as curves of their own, until Live or a recording.
  /// @{
  /// Where they are: the store of a meter by its source (Reading::source),
  /// nullptr while it is not open, and its name for the legend.
  void             setOtherStores(std::function<RecordingStore *(const QString &source)> store,
                                  std::function<QString(const QString &source)> name);
  /// The vendors' names ("Uni-Trend"): the legend leaves one at the start
  /// of a meter's name out first when its row has no room.
  void             setVendorNames(const QStringList &vendors) { m_vendors = vendors; }
  void             setOtherValues(const QList<ValueRef> &values);
  QList<ValueRef>  otherValues() const;
  /// Shows or hides one of them (at the end of otherValues()).
  void             setOtherValueShown(const ValueRef &value, bool shown);
  /// The axis chosen for another meter's value; AxisAuto: left in its unit,
  /// else right while free. A chosen axis with another unit leaves the curve
  /// out, with a notice().
  void             setOtherValueAxis(const ValueRef &value, ValueAxis axis);
  ValueAxis        otherValueAxis(const ValueRef &value) const { return m_otherAxes.value(value, AxisAuto); }
  /// Where it is drawn now; AxisAuto while it is not.
  ValueAxis        otherValueSide(const ValueRef &value) const;
  /// Its curve's colour while it is drawn, else invalid.
  QColor           otherValueColor(const ValueRef &value) const;
  /// A meter was opened or closed: the stores anew.
  void             otherStoresChanged();
  int              otherCurveCount() const { return int(m_otherCurves.size()); }
  bool             otherCurveShown(int i) const { return m_otherCurves[size_t(i)].shown; }
  bool             otherCurveOnRight(int i) const { return m_otherCurves[size_t(i)].side == 1; }
  int              otherCurveSide(int i) const { return m_otherCurves[size_t(i)].side; }
  /// The points of curve @p i as drawn, in this store's seconds.
  QList<QPointF>   otherCurvePoints(int i) const { return m_otherCurves[size_t(i)].points; }
  QPen             otherCurvePen(int i) const { return m_otherCurves[size_t(i)].line->pen(); }
  bool             otherCurveBanded(int i) const { return m_otherCurves[size_t(i)].band->isVisible(); }
  /// @}
  bool             mainShown() const { return m_mainShown; }

  /// @name The values, of any meter (ValueRef)
  /// One list for the graph's own meter and the others: a value of the
  /// meter of the store (baseSource(); an empty source is that meter too)
  /// is its main curve or one of its other values, one of another meter a
  /// curve from that meter's store. The graph does not choose its store:
  /// the caller makes the meter of the first value its base (setStore(),
  /// setBaseSource()).
  /// @{
  void             setBaseSource(const QString &source) { m_baseSource = source; }
  /// @p value with the meter an empty source means: the base.
  ValueRef         resolved(const ValueRef &value) const
  {
    return value.source.isEmpty() ? ValueRef(value.id, m_baseSource) : value;
  }
  /// Another meter's store becomes the graph's (@p source its meter): the
  /// values and their axes stay, its values become the main curve and the
  /// other values, the ones of the meter before curves of another meter.
  void             setBase(RecordingStore *store, const QString &source);
  QString          baseSource() const { return m_baseSource; }
  bool             isBase(const ValueRef &value) const { return resolved(value).source == m_baseSource; }
  /// The values shown, the base meter's first (with its source), in order.
  QList<ValueRef>  values() const;
  void             setValues(const QList<ValueRef> &values);
  bool             valueShown(const ValueRef &value) const;
  void             setValueShown(const ValueRef &value, bool shown);
  ValueAxis        valueAxis(const ValueRef &value) const;
  void             setValueAxis(const ValueRef &value, ValueAxis axis);
  /// Where it is drawn now; AxisAuto while it is not.
  ValueAxis        valueSide(const ValueRef &value) const;
  /// Its curve's colour while it is drawn, else invalid.
  QColor           drawnColor(const ValueRef &value) const;
  /// @}
  /// The unit of the left axis: the main value's, or while it is hidden
  /// the one of the lowest value shown (its newest series).
  QString          leftUnit() const;
  /// The value of the left axis, which the reference line, the band and the
  /// cursor levels are of: 0 (the main one), while it is hidden the lowest
  /// value shown, -1 with none. The trigger line stays the main value's.
  int              referenceId() const;
  /// @}
  /// @{
  /// Live, but the window held where the overview put it.
  bool             livePaused() const { return m_liveHold; }
  /// @}
  /// Line widths of the data and the integration curve.
  void             setLine(int d, int i);
  /// Draws a vertical mark at the current sample (an alarm raised); marks
  /// move with the data and go with clearSLOT().
  void             addMark(const QColor &color, const QString &name);
  int              markCount() const { return int(m_marks.size()); }
  /// Asks for a note (NoteDlg) at @p t (ms of the recording; -1: now, or
  /// on the newest reading) and sets it; its index in the marks, -1 when
  /// cancelled.
  int              askNote(qint64 t = -1);
  /// Sets a note without asking (QTDMM:MARK); index as askNote(). No
  /// noteAdded(): the caller marks the table itself.
  int              addNote(qint64 t, const QString &title, const QString &note);
  /// Asks for the new title and text of mark @p index.
  bool             editMark(int index);
  /// The mark whose flag is at @p viewPos (in the chart view), -1: none.
  int              markAt(const QPoint &viewPos) const;
  /// Shows mark @p index's title, time and text in a bubble at @p globalPos.
  void             showMarkNote(int index, const QPoint &globalPos);
  /// Unsaved recorded data in memory.
  bool             dirty() const { return m_store->dirty(); }
  void             setAlertUnsaved(bool on) { m_alertUnsaved = on; }
  void             setCrosshair(bool on) { m_crosshair = on; }
  /// LineMode and PointMode for the data and the integration curve.
  void             setLineStyle(int, int, int, int);
  /// Integration curve: shown, scale factor, threshold (values at or below
  /// it reset the sum) and offset.
  void             setIntegration(bool, double, double, double);
  /// Which value of the meter the integration curve integrates: 0 the main
  /// value, 1 the second display (Graph/int-value). Without that value the
  /// curve is not drawn; it goes on the axis its value is on.
  void             setIntegrationValue(int id);
  int              integrationValue() const { return m_intValueId; }
  void             setSettings(Settings *settings) { m_cfg = settings; }
  /// Since 26.2 the integral is one over time (unit x s), no longer the sum
  /// of the samples. Once per settings file Graph/int-scale is divided by
  /// the sample time in seconds, so the curve stays as it was;
  /// Graph/int-scale-per-second marks it done. True when the scale changed.
  static bool      migrateIntegralScale(Settings *cfg);

Q_SIGNALS:
  /// The user works in this graph (a click, a key, its menu): with several
  /// graphs the one the window's actions are for.
  void             used();
  /// values() changed (setValues(), setValueShown()).
  void             valuesChanged();
  /// Status bar text: sample time, window and remaining length.
  void             info(const QString &);
  void             error(const QString &);
  /// Recording started/stopped.
  void             running(bool);
  /// The store's state changed (RecordingStore::State).
  void             stateChanged(int state);
  /// A short hint for the status bar ("Current off: the left axis shows V now").
  void             notice(const QString &);
  /// Window/total size changed by zooming (seconds).
  /// Sample time changed by a CSV import (tenths of a second).
  void             sampleTime(int);
  /// The context menu chose this graph's colours: a ColorVariant, or -1
  /// for the default from the settings page.
  void             colorVariantChanged(int variant);
  void             zoomIn(double);
  void             zoomOut(double);
  /// Show the whole recording (key 0).
  /// A time button asks for this visible window (seconds).
  void             windowRequested(int seconds);
  /// A threshold line was dragged with the mouse.
  void             thresholdChanged(GraphWidget::CursorMode, double);
  /// The reference line was dragged or chosen in the context menu.
  void             referenceChanged();
  /// leftValue() is another one (the limit band to show).
  void             leftValueChanged(const ValueRef &value);
  /// A note was set at @p when (askNote()): the readings table marks its reading.
  void             noteAdded(const QDateTime &when, const QString &title);
  /// A note was dragged from @p from to @p to: the readings table moves its mark.
  void             noteMoved(const QDateTime &from, const QDateTime &to, const QString &title);
  /// @name Context menu requests, handled by InstanceWidget
  /// @{
  void             connectDMM(bool);
  void             configure();
  /// "Clear graph" in the context menu: the window asks about unsaved data
  /// first (InstanceWidget::clearSLOT()).
  void             clearRequested();
  /// "Start recording" (true) or "Stop recording" in the context menu: the
  /// window starts it as its Record button does (MainWindow::startSLOT()).
  void             recordRequested(bool start);
  /// "Live" in the context menu (InstanceWidget::liveSLOT()).
  void             liveRequested();
  void             importData();
  /// importCsvFile() loaded a file: it wants to be seen.
  void             fileImported();
  /// @}

public Q_SLOTS:
  /// Discards the recorded data.
  void             clearSLOT();
  /// @name Keyboard zoom/pan, also reachable from MainWindow's shortcuts
  /// @{
  void             zoomInSLOT()  { leaveAll(); Q_EMIT zoomIn(1.25); }
  void             zoomOutSLOT() { leaveAll(); Q_EMIT zoomOut(1.25); }
  /// The window as long as what there is (kept in the settings, unlike "All").
  void             zoomFitSLOT() { m_followAll = false; Q_EMIT windowRequested(allTarget(false)); }
  /// Shifts the visible window by a fraction of its width (negative = back).
  void             pan(double fraction);
  void             scrollToStart();
  void             scrollToEnd();
  /// Puts a picture of the graph on the clipboard.
  void             copyImageSLOT();
  /// Writes the graph to a file chosen in a dialog (SVG, PDF, PNG).
  bool             exportImageSLOT();
  /// @}
  void             startSLOT();
  void             stopSLOT();
  /// The store goes Live (RecordingStore::live()), the curve viewed goes.
  void             liveSLOT();
  /// The recording settings of the store changed (DeviceSession): the time
  /// buttons and the threshold lines anew.
  void             recordingSetupChanged();
  /// Export with a file dialog; returns false when cancelled or failed.
  bool             exportDataSLOT();
  /// The export's file dialog: the file chosen (with its suffix), empty when
  /// cancelled; @p raw for "every reading".
  QString          askExportFile(bool *raw);
  /// A file is viewed (importCsvFile()), not a recording.
  bool             showsFile() const { return !m_sourceFile.isEmpty(); }
  void             importDataSLOT();
  void             connectSLOT(bool on) { m_connected = on; }

  /// File-path-driven, non-interactive halves of export/importDataSLOT (no QFileDialog),
  /// split out so the CSV parsing/writing logic can be exercised from tests.
  bool             exportCsvFile(const QString &fileName, bool raw = false);
  bool             importCsvFile(const QString &fileName);
  /// The text top left in the plot: "● REC 0:42 / 5:00", the source of what
  /// is viewed, or empty (Live). Public for the tests.
  QString          stateText() const;
  /// Writes the graph to @p fileName, format taken from the suffix (svg, pdf,
  /// png, jpg, bmp). SVG and PDF keep the curve, the axes and their labels as
  /// vectors; @p size is the drawing size in points, the widget's own when
  /// empty. Non-interactive half of exportImageSLOT(), for the test.
  bool             exportImageFile(const QString &fileName, QSize size = QSize());

protected Q_SLOTS:
  void             popupSLOT(QAction *action);

protected:
  /// exportImageFile() without hiding the crosshair.
  bool             writeImage(const QString &fileName, QSize size);

  QScrollBar      *scrollbar;
  GraphMinimap    *m_minimap = nullptr;
  bool             m_minimapOn = true;
  bool             m_envelopeOn = false;
  QGraphicsPathItem *m_envelope = nullptr;
  /// The curve's points (pointsOf(), gaps as NaN) for the band
  QList<QPointF>   m_curvePts;
  void             updateEnvelope();
  /// The band over the plot's pixel columns of @p pts, from the lowest to
  /// the highest the curve reaches in each column and the @p k columns
  /// either side, for the y range @p y0 .. @p y1; empty when there is
  /// nothing to draw. The min/max band (k = kEnvelopeColumns) and the
  /// dense curves (kDenseColumns) use it.
  QPainterPath     columnBand(const QList<QPointF> &pts, double y0, double y1, int k) const;
  /// A value of the meter or of another meter with more points in the
  /// window than kDensePixels per pixel column allow is drawn as its band
  /// (columnBand(), k = kDenseColumns): a faint fill and the outline in the curve's
  /// pen, so every extreme stays and the dashes still tell it apart - the
  /// line's zigzag would be a solid area in its colour.
  void             updateDenseBands();
  /// One curve's band: shown when @p points are dense, the line then hidden.
  void             updateDenseBand(QGraphicsPathItem *band, GapLine *line, bool shown, const QList<QPointF> &points,
                                   int side, const QPen &pen);
  QGraphicsPathItem *newBand();
  static constexpr double kDensePixels = 2.0;
  /// A column of the dense band takes its neighbours' extremes too: no
  /// notches where a column got only a maximum or only a minimum.
  static constexpr int kDenseColumns = 1;
  bool             m_liveHold = false;   ///< Live, the window held at m_holdStart
  qint64           m_holdStart = 0;      ///< ms since the start of the recording
  /// The chart, and below it the overview or the scroll bar.
  void             layoutParts();
  /// The overview asks for the window to start at @p start.
  void             showFrom(qint64 start);
  /// The window follows the newest reading again (Live), or goes to the end.
  void             followEnd();
  /// The overview asks for a window of @p width ms from @p start.
  void             showSpan(qint64 start, qint64 width, bool atEnd);
  /// Tells the overview the window shown.
  void             updateMinimapView();
  qint64           m_bucket = 1;    ///< ms per drawn min/max pair (see bucketSize())
  int              m_tailData = 0;  ///< points the newest bucket gave the data line (1 or 2; a gap is 1)
  int              m_tailInt = 0;   ///< the same for the integral
  int              m_windowSeconds = 600;   ///< the visible window, applyWindow()
  int              m_savedWindow = 0;       ///< the settings' window, setGraphSize(); 0 = none yet
  int              m_rebuilds = 0;          ///< rebuildSeries() calls, see onAppended()
  /// @name Time buttons (All / 1 min / 5 min / 30 min) top right in the graph
  /// @{
  QWidget         *m_timeBar = nullptr;
  QList<QToolButton *> m_timeButtons;   ///< property "seconds": 0 = All
  /// Time and value under the crosshair, fixed top left above the plot (a
  /// tooltip window trailed behind the mouse).
  QLabel          *m_cursorLabel = nullptr;
  void             hideCrosshair();
  bool             m_followAll = false;   ///< "All": the window grows with the recording
  void             timeButtonClicked(int seconds);
  /// "All": a window that holds the recording so far (plus room to grow
  /// when @p grow), at least 10 s and at most the recording length.
  int              allTarget(bool grow) const;
  /// "All": shows allTarget() - in the graph only, the settings keep
  /// their window (it would be saved with them).
  void             requestAll(bool grow);
  /// Shows a window of @p seconds (setGraphSize(), requestAll()).
  void             applyWindow(int seconds);
  /// A zoom from "All" starts from the window shown: it goes to the
  /// settings first.
  void             leaveAll();
  void             updateTimeButtons();
  /// The longest window worth a time button, in seconds (0 = none).
  int              lengthLimit() const;
  /// The scroll bar over what the store keeps.
  void             updateScrollRange();
  void             placeTimeBar();
  /// @}
  double           m_scaleMin;
  double           m_scaleMax;
  bool             m_autoScale;
  RecordingStore  *m_store;
  SampleMode       mode() const { return SampleMode(m_store->startMode()); }
  bool             m_connected;
  /// The store's signals: a new reading, discarded or replaced readings.
  void             connectStore();
  /// Where the window starts (ms since the start of the recording).
  qint64           windowStart() const;
  /// The start of a window that has the newest reading at its right edge.
  qint64           followStart() const;
  /// Whether the window moves on with the time: Live (not held by the
  /// overview), or a recording seen at its end.
  bool             following() const;
  /// FrameClock: wanted while the graph is shown and Live or recording.
  void             updateClockWant();
  /// A tick of the FrameClock: the time axis on to now, between readings.
  void             frameTick();
  qint64           bucketSize() const;
  int              bucketStart(int i) const;
  void             appendToSeries();
  void             appendIntegral(const RecordingSeries &series, int first, int last, bool filling);
  void             onAppended(bool shifted);
  void             onCleared();
  void             onStateChanged();
  /// @name The state top left in the plot (stateText())
  /// @{
  QLabel          *m_stateLabel = nullptr;
  QColor           m_stateColor;          ///< the lettering colour, for View
  QString          m_sourceFile;          ///< the file viewed (importCsvFile()), empty = a recording
  QString          m_sourceDevice;        ///< the meter of its main value (Recording::device)
  void             updateStateLabel();
  /// @}
  /// The mark lines anew from the store's marks.
  void             syncMarks();
  QPoint           m_mpos;
  bool             m_mouseDown;
  bool             m_mousePan;
  CursorMode       m_cursorMode;
  QColor           m_bgColor;
  QColor           m_gridColor;
  QColor           m_dataColor;
  QColor           m_cursorColor;
  QColor           m_startColor;
  QColor           m_intColor;
  QColor           m_intThresholdColor;
  int              m_lineWidth;
  /// The width of the main curve: m_lineWidth, thicker in HighContrast.
  double           lineWidth() const;
  int              m_intLineWidth;
  bool             m_alertUnsaved;
  bool             m_crosshair;
  PointMode        m_pointMode;
  PointMode        m_intPointMode;
  LineMode         m_lineMode;
  LineMode         m_intLineMode;
  double           m_integrationScale;
  double           m_integrationOffset;
  bool             m_showIntegration;
  bool             m_includeZero;
  QMenu           *m_popup;
  QList<QAction *> m_windowActions;   ///< setWindowActions()
  QAction         *m_printAction = nullptr;   ///< setPrintAction()

  // Qt Charts based rendering (core curve + axes, plus the integration curve
  // and the cursor/threshold overlays reintroduced as QGraphicsLineItems on
  // top of the chart scene).
  QChartView      *m_chartView = nullptr;
  QChart          *m_chart;
  QLineSeries     *m_dataSeries;   ///< the data line's first segment (positions map with it)
  QScatterSeries  *m_dataPoints;
  QLineSeries     *m_intSeries;
  QScatterSeries  *m_intPoints;
  std::unique_ptr<GapLine> m_dataLine;   ///< the data line, split at the gaps
  std::unique_ptr<GapLine> m_intLine;
  /// A curve of one of the store's other series; kept for reuse when there
  /// are fewer.
  struct ExtraCurve
  {
    QLineSeries *series = nullptr;   ///< the first segment
    std::unique_ptr<GapLine> line;
    int  tail = 0;                   ///< points the newest bucket gave (see m_tailData)
    int  side = 0;                   ///< its axis: 0 left, 1 right, 2 third
    bool shown = false;              ///< drawn: an axis has its unit, not hidden
    QList<QPointF> points;           ///< as drawn, for the band (m_curvePts)
    QGraphicsPathItem *band = nullptr;   ///< too dense for a line (updateDenseBands()), the chart's child
  };
  std::vector<ExtraCurve> m_extraCurves;
  /// A value of another meter (setOtherValues()).
  struct OtherCurve
  {
    ValueRef     value;
    QPointer<RecordingStore> store;   ///< nullptr while its meter is not open
    QLineSeries *series = nullptr;
    std::unique_ptr<GapLine> line;
    int          side = 0;   ///< as ExtraCurve::side
    bool         shown = false;
    QString      unit;
    QString      name;    ///< in the legend: the meter's name and the value's
    QString      meter;   ///< the meter's name alone, empty without one
    QString      label;   ///< the value's name alone (curveName())
    QList<QPointF> points;
    QGraphicsPathItem *band = nullptr;   ///< as ExtraCurve::band
  };
  std::vector<OtherCurve> m_otherCurves;
  QHash<ValueRef, ValueAxis> m_otherAxes;   ///< setOtherValueAxis(), missing: AxisAuto
  QList<QPointer<RecordingStore>> m_otherConnected;   ///< the stores connected to
  std::function<RecordingStore *(const QString &)> m_otherStore;
  std::function<QString(const QString &)> m_otherName;
  QStringList      m_vendors;   ///< setVendorNames()
  /// The other meters of a file viewed (importCsvFile()), a store each, and
  /// their values among the curves (source "file:<index>"), not in otherValues().
  QList<RecordingStore *> m_fileStores;
  QStringList      m_fileDevices;   ///< their names, as m_fileStores
  QList<ValueRef>  m_fileValues;
  /// The curves of @p values anew.
  void             makeOtherCurves(const QList<ValueRef> &values);
  /// The store of another meter's value: of the file viewed, or open beside
  /// (not while a file is viewed).
  RecordingStore  *otherStore(const QString &source) const;
  QString          otherName(const QString &source) const;
  /// The other meters of the file viewed go (Live, a recording, another file).
  void             dropFileStores();
  /// The other meters of the file @p rec, as curves.
  void             setFileStores(const Recording &rec);
  QTimer           m_otherTimer;   ///< their new readings, drawn together
  /// Their stores, connections and curves anew.
  void             attachOtherStores();
  /// Their points and axes anew, after rebuildExtras() (the right axis
  /// may be free); true when the left scale grew.
  bool             rebuildOtherCurves();
  /// New readings of theirs (m_otherTimer): their curves, axes, legend.
  void             onOtherReadings();
  QColor           otherColor(int i) const;
  QPen             otherPen(int i) const;
  /// The axes on the right: [0] the right one, the first unit other than
  /// the main one; [1] the third, outside it, the next unit.
  struct SideAxis
  {
    /// A category axis: its labels are the tick values scaled to the
    /// prefix of the range shown (12 [kHz], not 1.2e+04 [Hz]), set by
    /// updateRightAxis() at the ticks of a value axis
    QCategoryAxis           *axis = nullptr;
    QString                  unit;              ///< empty while it is hidden, the base unit
    QString                  prefix;            ///< of its labels and title, from the range shown
    QGraphicsSimpleTextItem *title = nullptr;   ///< above it, as m_yTitle
    double                   scaleMin = 1e40;
    double                   scaleMax = -1e40;
  };
  std::array<SideAxis, 2> m_right;
  /// The y axis of side @p side (0 left, 1 right, 2 third).
  QValueAxis      *yAxisOf(int side) const { return side == 0 ? m_yAxis : m_right[size_t(side - 1)].axis; }
  /// Grows the scale of the right axis @p side by @p v; true when it grew.
  bool             growSide(int side, double v);
  void             resetSideScales();
  QString          sideTitle(int side) const;
  /// The colour of the first curve on side @p side: this meter's lowest
  /// value there, else the first other meter's curve; the labels' without.
  QColor           sideColor(int side) const;
  static int       sideOf(ValueAxis axis) { return axis == AxisRight ? 1 : axis == AxisThird ? 2 : 0; }
  static ValueAxis axisOfSide(int side) { return side == 1 ? AxisRight : side == 2 ? AxisThird : AxisLeft; }
  QSet<int>        m_shownValues;        ///< the meter's other values switched on
  QMap<int, ValueAxis> m_valueAxes;      ///< setValueAxis(), missing: AxisAuto
  bool             m_mainShown = true;   ///< the main value's curve, setValueShown(0)
  int              m_intValueId = 0;     ///< setIntegrationValue()
  GraphLegend     *m_legend = nullptr;
  GraphStateStrip *m_stateStrip = nullptr;
  bool             m_statesOn = true;
  bool             m_patternsOn = false;
  /// A state of the left axis' value from @p from to @p to (ms).
  struct StateSpan
  {
    int     kind = 0;   ///< GraphStateStrip::Kind
    QString label;
    qint64  from = 0;
    qint64  to = 0;
    Qt::BrushStyle pattern = Qt::SolidPattern;
  };
  /// Why a gap is one, in a word, and the hatching of that cause.
  static std::pair<QString, Qt::BrushStyle> gapCause(const RawPoint &p);
  /// The states in the window shown, each kind's stretches joined.
  QList<StateSpan> stateSpans() const;
  /// The states of a reading's flags and quality as words ("HOLD REL"), for the hover line.
  static QString   stateWords(quint32 flags, Quality quality);
  /// Places the strip at the bottom of the plot and asks it to paint anew.
  void             updateStateStrip();
  QColor           extraColor(int i) const;
  /// The line of value @p id (1 = value 2 ...): its colour, valueStyle().
  QPen             extraPen(int id) const;
  static Qt::PenStyle valueStyle(int id);
  void             rebuildExtras();
  void             onExtraAppended(int i);
  void             rebuildOtherIntegral();
  /// The integration curve (line and points) on @p axis.
  void             integralOnAxis(QAbstractAxis *axis);
  void             updateLegend();
  /// The right axes from their scales, with the left axis' tick count;
  /// their titles in the colour of their first curve.
  void             updateRightAxis();
  /// The drawn points of readings first..last of @p series, thinned per bucket
  /// (see the comment at the definition); returns the number of points.
  int              pointsOf(const RecordingSeries &series, int first, int last, bool integral, QList<QPointF> &out) const;
  QValueAxis      *m_xAxis;
  QValueAxis      *m_yAxis;
  /// The y axis title ("[V]"), written horizontally above the axis: turned
  /// by 90 degrees a "V" reads like ">".
  QGraphicsSimpleTextItem *m_yTitle;
  QBrush           m_themeBackground;   ///< Qt::NoBrush: m_bgColor
  QColor           m_themeGrid;
  QColor           m_themeLabels;
  QColor           m_themeData;         ///< proposed curve colour, invalid = none
  QColor           dataColor() const;   ///< the curve colour in use
  /// The unit of value @p id, its newest series with one; empty before one came.
  QString          valueUnitOf(int id) const;
  /// Before value @p id goes onto the axis @p axis: the curves of another
  /// unit on it go off (@p chosenOnly: those with that axis chosen), with a notice().
  void             makeRoom(int id, ValueAxis axis, bool chosenOnly = false);
  /// After a value went on or off or to another axis: the view afresh.
  void             refreshValues();
  /// The overview draws minimapValue(), in its curve's colour.
  void             updateMinimapValue();
  QColor           intColor() const;    ///< the integration curve colour in use
  ColorVariant     m_variant = Neutral;          ///< in use
  ColorVariant     m_defaultVariant = Neutral;   ///< from the settings page
  int              m_variantOverride = -1;       ///< this graph's choice, -1 = default
  /// Scope, phosphor and recorder: 10 x 8 divisions of 1-2-5 steps.
  bool             divisions() const;
  bool             phosphor() const { return m_variant == PhosphorGreen || m_variant == PhosphorAmber; }
  /// The y range, widened to 8 whole divisions of 1-2-5 steps when divisions().
  void             setYRange(double min, double max);
  /// Phosphor: 5 fine ticks per division on the reference line (else the
  /// bottom edge) and on the trigger's mark (else the left edge).
  QGraphicsPathItem *m_centreTicks;
  void             updateCentreTicks();
  /// @name The reference line
  /// Across the plot at the reference value (0) while that lies inside the
  /// y range, with the value small at its right end ("0 A"): a current
  /// that charges and discharges reads against it; without the value in
  /// range there is no line. Phosphor draws it with its fine ticks
  /// (updateCentreTicks()).
  /// @{
  QGraphicsLineItem       *m_referenceLine = nullptr;
  QGraphicsSimpleTextItem *m_referenceLabel = nullptr;
  struct ExtraRefItem
  {
    QGraphicsLineItem       *line = nullptr;   ///< the chart's children
    QGraphicsSimpleTextItem *label = nullptr;
  };
  QList<double>             m_extraRefs;
  std::vector<ExtraRefItem> m_extraRefItems;
  int                       m_dragExtra = -1;   ///< the added line dragged
  /// Where the context menu was opened, as a value of the left axis: an
  /// added line starts there (NaN: in the middle)
  double                    m_refMenuValue = std::numeric_limits<double>::quiet_NaN();
  bool                      m_crossingsOn = true;
  QGraphicsPathItem        *m_crossMarks = nullptr;
  QList<int>                m_crossCounts;      ///< [0] the reference line, then the added ones
  void             updateExtraReferences();
  void             updateCrossings();
  /// The crossings of @p value by the left axis' curve in the window: time
  /// (s) and whether it rose; crossings closer than 3 pixels count once.
  QList<std::pair<double, bool>> crossingsOf(double value) const;
  /// The value of the left axis at scene height @p y, rounded to a step of
  /// a hundredth of the axis.
  double           roundedValueAt(double y) const;
  QString          referenceText(double value) const;
  /// @p label with @p text at the right end of the plot, above the line at
  /// @p y unless that leaves the plot.
  void             placeReferenceLabel(QGraphicsSimpleTextItem *label, double y, const QString &text);
  QString          m_referenceText;   ///< the reference line's label without the crossings
  double           m_reference = 0;
  ReferenceKind    m_referenceKind = RefZero;
  QList<double>    m_alarmThresholds;
  /// The statistics of the window shown, for RefMean, RefMin, RefMax.
  RecordingStore::Stats windowStats() const;
  /// The line dragged to @p y in the scene: it snaps to 0, the window's
  /// mean, minimum and maximum, the alarm thresholds and the limits within a few
  /// pixels, else takes the value there, rounded to a hundredth of the
  /// scale.
  void             dragReference(double y);
  /// The reference line's y in the scene, NaN when the value is not
  /// inside the y range (on its edge the axis is the line).
  double           referenceY() const;
  void             updateReferenceLine();
  /// @}
  /// @name The limit test's band (setLimits())
  /// @{
  QGraphicsRectItem *m_limitBand = nullptr;
  QGraphicsLineItem *m_limitLines[2] = { nullptr, nullptr };   ///< low, high
  Control          m_control;   ///< setControl()
  QString          m_baseSource;   ///< setBaseSource()
  ValueRef         m_limitValue{ -1 };
  ValueRef         m_lastReference{ -1 };   ///< for leftValueChanged()
  qint64           m_minimapShift = 0;      ///< where the overview's store begins on this one's time (ms)
  double           m_limitLow = 0;
  double           m_limitHigh = 0;
  void             updateLimitBand();
  /// @}
  /// @name Time cursors (setTimeCursor())
  /// @{
  qint64           m_cursorT[2] = { -1, -1 };      ///< ms since the start, -1 = none
  QGraphicsLineItem *m_cursorLine[2] = {};
  GraphFlagItem    *m_cursorName[2] = {};   ///< "A" and "B" as flags under the lines
  /// across the plot at the value under each cursor: the change is the
  /// distance between the two
  QGraphicsLineItem *m_cursorLevel[2] = {};
  /// @name The statistics beside the plot (statsText())
  /// @{
  QScrollArea     *m_statsPanel = nullptr;
  QLabel          *m_statsLabel = nullptr;
  QToolButton     *m_statsButton = nullptr;   ///< on the time bar: on or off
  QTimer           m_statsTimer;               ///< gathers updates: at most 4 a second
  bool             m_statsOn = false;
  bool             m_statsLeft = true;
  mutable int      m_statsWant = 0;   ///< the width the widest row of statsHtml() wants
  /// The statistics column's width: 0 while off.
  int              statsWidth() const;
  bool             m_viewButtonsOn = true;
  QWidget         *m_viewButtons = nullptr;   ///< beside the time buttons
  QToolButton     *m_overviewButton = nullptr;
  QToolButton     *m_envelopeButton = nullptr;
  QToolButton     *m_referenceButton = nullptr;
  QToolButton     *m_clearCursorsButton = nullptr;
  /// The buttons as the graph is: pressed, greyed.
  void             updateViewButtons();
  /// The reference line's entries (the menu's submenu and its button).
  void             fillReferenceMenu(QMenu *menu);
  /// Updates the statistics soon (m_statsTimer), not with every reading.
  void             scheduleStats();
  /// "1.234 mV", "--" for none.
  static QString   withUnit(double v, const QString &unit);
  /// One prefix and one number of decimals for the numbers of a value in
  /// the statistics, so they stand below each other: four significant
  /// digits for the largest of @p values; the smallest of @p fine (a σ, a
  /// peak-peak) keeps two, within six decimals.
  struct ColumnScale
  {
    double  factor = 1;
    QString unit;
    int     decimals = 0;
    QString text(double v) const;
  };
  static ColumnScale columnScale(const QList<double> &values, const QList<double> &fine, const QString &unit);
  /// A time of the recording as the time axis counts it ("16.874 s", "2.281 min").
  QString          axisTimeText(qint64 t) const;
  /// What a unit of the time axis is in seconds: 1, 60 or 3600.
  double           timeAxisUnit() const;
  /// The values drawn at @p t: id (0 the main one) and the series that holds then.
  QList<std::pair<int, const RecordingSeries *>> drawnSeries(qint64 t) const;
  QString          seriesUnit(const RecordingSeries *s) const;
  /// A curve for the statistics: of this graph's meter or of another one
  /// (then on its store's time, @p shift ms from this one's).
  struct StatCurve
  {
    int     id = 0;            ///< the value's id; -1 for another meter's
    const RecordingSeries *series = nullptr;
    const RecordingStore *store = nullptr;
    qint64  shift = 0;
    QString name;
    QString unit;
    QColor  colour;
  };
  /// The curves drawn at @p t: the main one first, the meter's other
  /// values, then the other meters' in their order.
  QList<StatCurve> statCurves(qint64 t) const;
  void             updateStats();
  /// The statistics as HTML: the cursors, from A to B, the window shown.
  QString          statsHtml() const;
  /// @}
  QPoint           m_pressPos;                      ///< where the left button went down
  bool             m_clickCandidate = false;        ///< a press that may become a click: sets a cursor
  int              m_pressedMark = -1;              ///< the mark whose flag the left button went down on
  bool             m_markDragging = false;          ///< m_pressedMark is a note being dragged along the time axis
  qint64           m_markDragFrom = 0;              ///< its time before the drag, for Esc and noteMoved()
  /// Moves the note pressed on (m_pressedMark) to the time under @p pos once
  /// the mouse went far enough; false when it is no note or not yet dragged.
  bool             dragMark(const QPoint &pos);
  /// Ends a drag: the note stays (noteMoved()) or goes back (@p cancel).
  void             endMarkDrag(bool cancel);
  /// True when mark @p index can be dragged: a note.
  bool             markDraggable(int index) const;
  bool             m_pressShift = false;
  /// Places the lines and the text anew.
  void             updateTimeCursors();
  /// The value of @p series that holds at @p t; NaN in a gap, outside or
  /// without a series. @p store: the one it belongs to, null this one.
  double           valueAt(const RecordingSeries *series, qint64 t, const RecordingStore *store = nullptr) const;
  /// The series of value @p id (> 0) drawn that holds at @p t (Hz, then %:
  /// the one of the time), with @p leftOnly one on the left axis; -1 with none.
  int              drawnExtra(int id, qint64 t, bool leftOnly) const;
  /// The series of leftValue() at @p t, its store and where that
  /// store's time 0 lies on this one's (ms); no series with none.
  struct RefSeries
  {
    const RecordingStore *store = nullptr;
    const RecordingSeries *series = nullptr;
    qint64 shift = 0;
  };
  RefSeries        refSeries(qint64 t) const;
  /// The curve of another meter that is leftValue(), -1 with none.
  int              referenceOther() const;
  /// The points of leftValue()'s curve as drawn, for the band.
  QList<QPointF>   referencePoints() const;
  /// @}
  QColor           m_defaultAxisLine;   ///< the chart's own axis line colour
  void             applyThemeColors();
  void             placeYTitle();
  void             showUnit();
  /// @name x labels in s, min or h
  /// Qt's axis can only print the seconds; its own labels are kept for the
  /// layout but drawn invisible, these are drawn over them at every tick.
  /// @{
  QList<QGraphicsSimpleTextItem *> m_xLabels;
  double           m_xStep = 0;         ///< seconds between the x ticks
  QColor           m_xLabelColor;
  void             updateXLabels();
  /// @}
  /// @name y labels with the SI prefix of the reading
  /// The same for the y axis: Qt's labels can only print base units
  /// (0.35 V), these print them with the prefix the meter shows (350 mV).
  /// @{
  QList<QGraphicsSimpleTextItem *> m_yLabels;
  QString          m_yPrefix;           ///< the prefix the y labels use now
  /// The meter's prefix while it measures what the store holds; else the
  /// one that suits the axis range.
  QString          yPrefix() const;
  QString          m_livePrefix;        ///< the prefix of the meter's unit (setUnit()), for an axis without readings
  QString          m_liveBase;          ///< and its base unit
  void             updateYLabels();
  /// @}
  QGraphicsLineItem *m_crosshairVLine;
  QGraphicsLineItem *m_crosshairHLine;
  QGraphicsLineItem *m_triggerLine;
  QGraphicsLineItem *m_integrationLine;
  /// Alarm marks: a vertical line at the sample the alarm raised on, one
  /// per RecordingStore::marks() in the same order.
  QList<QGraphicsLineItem *> m_marks;
  QList<GraphFlagItem *> m_markFlags;   ///< number and name under each mark
  /// The flags under the plot, the cursors' and the marks', in one row.
  void             updateFlags();
  void             updateMarkPositions();
  /// @name Zoom on a frame (Shift + drag) and "Reset zoom"
  /// @{
  bool             m_zoomDragging = false;          ///< Shift + left button drags a frame
  QGraphicsRectItem *m_zoomBand = nullptr;          ///< the frame while it is dragged
  bool             m_zoomed = false;                ///< zoomToRect(): the axes show m_yZoom
  std::array<std::pair<double, double>, 3> m_yZoom; ///< min and max per side while m_zoomed
  struct ZoomBefore
  {
    qint64 start = 0;
    int    seconds = 0;
    bool   atEnd = false;
    bool   all = false;
  } m_zoomBefore;                                   ///< the window before the first zoom
  /// Shows the plot's part @p frame (scene coordinates): its time as the
  /// window, its height on every y axis.
  void             zoomToRect(const QRectF &frame);
  /// The axes automatic again (or fixed), @p restoreWindow: the window of
  /// before the first zoom.
  void             endZoom(bool restoreWindow);
  void             endZoomDrag();
  /// @}
  /// The right-click menu at @p pos (in the view; a point outside the plot:
  /// no entries for a time).
  void             showMenu(const QPoint &pos, const QPoint &globalPos);
  /// The hover line (m_cursorLabel) above the plot, or inside it at the bottom when the buttons leave no room.
  void             placeCursorLabel();
  /// The menu of a right axis: automatic or a fixed range (@p side 1 or 2).
  void             addAxisEntries(QMenu *menu, int side);
  /// "Right axis [Hz]", "Third axis [Hz]".
  QString          axisMenuTitle(int side) const;
  /// Asks for the fixed range of the axis of @p unit.
  void             askFixedRange(const QString &unit);
  QMap<QString, std::pair<double, double>> m_fixedRanges;   ///< setFixedRange()

  void             resizeEvent(QResizeEvent *)Q_DECL_OVERRIDE;
  void             showEvent(QShowEvent *) override;
  void             hideEvent(QHideEvent *) override;
  bool             eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;

  void             handleChartMousePress(QMouseEvent *);
  void             handleChartMouseMove(QMouseEvent *);
  void             handleChartMouseRelease(QMouseEvent *);
  void             handleChartWheel(QWheelEvent *);
  /// Returns true when the key was used.
  bool             handleChartKey(QKeyEvent *);

  void             emitInfo();
  void             rebuildSeries();
  /// Whether the points of the curve and of the integral are drawn. Hidden
  /// ones get no points: Qt Charts lays out a scatter series anew with every
  /// change, also a hidden one (that was most of the time of a reading).
  bool             pointsShown() const { return m_pointMode != NoPoint; }
  bool             intPointsShown() const { return m_showIntegration && m_intPointMode != NoPoint; }
  /// The time axis, what depends on the window's readings too (mean line,
  /// band).
  void             updateXAxisRange();
  /// The time axis alone: range, ticks, labels, time cursors, the
  /// overview's window. Cheap enough for each FrameClock tick.
  void             updateTimeAxis();
  /// "All" while it runs and the window is not full: the axis ends now and
  /// stretches with the clock (stretchedSeconds() wide, at least 10 s), the
  /// curve reaches the right edge; the window grows in steps for the buckets.
  bool             stretching() const;
  double           stretchedSeconds() const;
  /// The lines get the points around the time axis' range, and the hold
  /// to now while the window follows the readings (GapLine).
  void             updateLineSpans();
  void             updateSeriesAppearance();
  void             updateThresholdLinesVisibility();
  void             updateThresholdLinePositions();
  QString          formatEngineeringValue(double value, QString *unit = nullptr) const;

private:
  Qt::PenStyle     penStyle(LineMode);
  Settings        *m_cfg;

};

