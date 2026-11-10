// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QString>
#include <QTime>
#include <QVector>
#include <cmath>
#include <functional>

#include "core/reading.h"
#include "core/valueref.h"
#include "recording/recordingfile.h"
#include "recording/recordingseries.h"
#include "core/sampletypes.h"

/// A step of the grid: the recording at the sample time, for the export.
struct GridPoint
{
  qint64  t = 0;            ///< ms since the start of the recording, a multiple of the step
  double  value = 0;        ///< the time-weighted mean over the step (the value at t for the first); NaN = none
  double  integral = 0;     ///< the integral at t; NaN in a gap
  Quality quality = Quality::Valid;   ///< the worst in the step
  quint32 limit = 0;        ///< the limit test's verdicts in the step (SampleFlag::Limit*)
};

/// One row of the readings series: a reading as the meter delivered it,
/// whichever value it was and whether or not a recording runs.
struct LoggedReading
{
  qint64  when = 0;         ///< ms since the epoch, wall clock
  double  value = 0;        ///< SI base units; NaN for an overload
  Quality quality = Quality::Valid;
  quint32 flags = 0;        ///< SampleFlag
  QString text;             ///< as the meter showed it ("-006.52", "OL")
  QString unit;             ///< as shown, with the SI prefix ("mV")
  PortKey port;             ///< what was measured: voltage.dc, resistance, ...
  QString range;            ///< "AUTO", "MANU" or the meter's range text
  int     id = 0;           ///< 0 = main value, 1.. = secondary values
  quint32 alarmArgb = 0;    ///< an alarm raised on this reading (QColor::rgba()), 0 = none
  QString alarmName;
  QString source;           ///< Reading::source: the meter it came from

  bool    hold() const { return flags & SampleFlag::Hold; }
};

/// The recorder without a widget: the recorded readings and everything that
/// decides what goes into them.
///
/// While recording it keeps every main reading (id 0) with its time, as a
/// RecordingSeries of RawPoint: setReading() from the MeterController. The
/// other values of the meter (a second display, id > 0) go into series of
/// their own on the same time axis (extra()), with their integral too; the
/// length, the marks and the export are the main value's, the triggers
/// watch the main value or the one setTriggerValue() chose. An
/// overload is a point without a value, and so is the moment the value went
/// stale (setStale()). The sample time is no longer the rate it records at
/// but the grid of the export: grid() gives the time-weighted mean of every
/// step. The integral is one over time (unit x s) of the values above the
/// integration threshold.
///
/// The store is in one of three states (State): Live, the readings of the
/// last liveKept() running through without being a recording; Record,
/// cleared at the start and kept until it stops; View, standing still - a
/// recording that ended, or a file loaded. Live shows what the meter does
/// right after connecting; a recording starts from it and ends in View.
///
/// Recording starts by hand, at a clock time or when the value crosses a
/// threshold (StartMode), and stops by hand or after the recording length.
/// The clock time and the thresholds start it from Live only: a recording
/// in View is not overwritten by a trigger. The store keeps all of a
/// recording, up to kMaxPoints readings: beyond that the oldest go
/// (appended() says so). The alarm marks live here too.
///
/// Next to the recording the store keeps a second series, the readings:
/// every reading of every value at full resolution, with its own capacity and
/// its own pause, independent of whether a recording runs (the readings
/// table). Its signals come in pairs around every change, so a table model
/// can turn them into begin/end calls one to one.
///
/// GraphWidget shows the store and follows its signals; it keeps only what is
/// a matter of display (window, zoom, scale, colours, the integral's scale
/// and offset). Only QtCore, so it can be tested on its own.
///
/// Sample time and recording length are in tenths of a second, like the
/// settings: a sample time of 5 is a grid of 0.5 s.
class RecordingStore : public QObject
{
  Q_OBJECT
public:
  /// How recording is started; the same numbers as GraphWidget::SampleMode.
  enum StartMode
  {
    Manual = 0,   ///< start()
    Time,         ///< at setStartTime()
    Raising,      ///< when the value rises through the raising threshold
    Falling       ///< when the value falls through the falling threshold
  };

  /// What the store does with the main readings.
  enum State
  {
    View = 0,    ///< stands: a recording that ended, a file loaded, or nothing yet
    Live,        ///< the last liveKept() of readings, running through
    Record       ///< recording since start(), until stop() or the length
  };
  Q_ENUM(State)

  /// What set a mark.
  enum class MarkKind
  {
    Alarm,     ///< an alarm, a fault, a function changed (MeterController::markRequested())
    Trigger,   ///< where a pre-triggered recording was triggered
    Note       ///< a note of the user's (addNote(), QTDMM:MARK)
  };
  /// A mark at a time of the recording: a line in the graph with a flag.
  struct Mark
  {
    qint64  t;       ///< ms since the start of the recording
    quint32 color;   ///< ARGB, as QColor::rgba()
    QString name;    ///< the title, on the flag
    QString note = QString();   ///< longer text, may be empty
    MarkKind kind = MarkKind::Alarm;
  };

  /// The most readings a series keeps; the oldest go beyond it.
  static constexpr int kMaxPoints = 2000000;
  /// The most Live keeps when the recording has no length (ms): an hour.
  static constexpr qint64 kLiveMaxMs = 3600 * 1000;
  /// The colour of the mark where a pre-triggered recording was triggered.
  static constexpr quint32 kTriggerColor = 0xff2e9b3a;
  /// The colour of a note's mark.
  static constexpr quint32 kNoteColor = RecordingFile::kNoteColor;

  explicit RecordingStore(QObject *parent = nullptr);

  /// @name Settings
  /// @{
  /// The grid of the export in tenths of a second (>= 1).
  void        setSampleTime(int tenths);
  int         sampleTime() const { return m_sampleTime; }
  /// How wide Live is when the recording has no length (seconds, >= 1):
  /// the graph's visible window.
  void        setLiveWidth(int seconds);
  /// The most readings a series keeps (kMaxPoints; smaller for tests).
  void        setMaxPoints(int points);
  /// Recording duration in tenths of a second after which recording stops
  /// on its own (0 = until stopped).
  void        setSampleLength(int tenths);
  /// Pre-trigger in ms (0 = off): a recording started by a threshold
  /// (Raising, Falling) reaches back this far - into the live window, which
  /// keeps at least that much - and marks where the trigger came. The
  /// recording length counts from the trigger.
  void        setPreTrigger(int ms);
  int         preTrigger() const { return m_preMs; }
  void        setStartMode(StartMode mode) { m_mode = mode; }
  StartMode   startMode() const { return m_mode; }
  /// Clock time for StartMode::Time.
  void        setStartTime(const QTime &time) { m_startTime = time; }
  void        setThresholds(double falling, double raising);
  void        setFallingThreshold(double v) { m_fallingThreshold = v; }
  void        setRaisingThreshold(double v) { m_raisingThreshold = v; }
  double      fallingThreshold() const { return m_fallingThreshold; }
  double      raisingThreshold() const { return m_raisingThreshold; }
  /// The value the thresholds watch: the main one at first, or another of
  /// the meter's values (a recording of a voltage started by its current).
  void        setTriggerValue(const ValueRef &value);
  ValueRef    triggerValue() const { return m_triggerValue; }
  /// The recording settings of a session (the Recording page), for all its
  /// stores at once (DeviceSession::setRecordingSetup()).
  struct Setup
  {
    int       sampleTime = 10;   ///< tenths of a second
    int       sampleLength = 0;  ///< tenths, 0 = until stopped
    int       preTrigger = 0;    ///< ms
    StartMode mode = Manual;
    QTime     startTime;
    double    falling = 0;
    double    raising = 0;
    ValueRef  trigger;           ///< the value the thresholds watch
  };
  /// Takes @p setup. The store that leads (its meter has the trigger value)
  /// takes its start too; the others start with it (startAt()) and never on
  /// their own: Manual.
  void        applySetup(const Setup &setup, bool lead);
  /// Values at or below it reset the integral to 0.
  void        setIntegrationThreshold(double v) { m_integrationThreshold = v; }
  double      integrationThreshold() const { return m_integrationThreshold; }
  /// Unit of the recorded values, without SI prefix ("V"). While the store
  /// holds samples their unit stays; a new one applies from the next
  /// start() or clear() on.
  void        setUnit(const QString &baseUnit);
  QString     unit() const { return m_unit; }
  /// How long a value holds before it is stale (ms, StaleRule): the gap
  /// setStale() adds starts this long after the last value.
  void        setStaleAfter(int ms) { m_staleMs = ms; }
  /// The clocks the store reads: a monotonic one in ms (the core clock,
  /// Sample::now()) and the wall clock. Tests set their own.
  void        setClock(std::function<qint64()> monotonic, std::function<QDateTime()> wall);
  /// @}

  /// @name The recording
  /// @{
  /// The readings of the live window, of the recording or of what is viewed.
  const RecordingSeries &series() const { return m_series; }
  /// The readings kept (series().count()).
  int         count() const { return m_series.count(); }
  /// The series of the meter's other values (Reading::id > 0), the one
  /// used last at the end: one per value and port. When a value measures
  /// something else (a second display from Hz to %), its series ends with a
  /// gap and a new one begins, or the one of that port goes on and moves to
  /// the end; the recording goes on. Their readings count from the
  /// main value's first one; a display that is off is a gap. Their points
  /// carry the integral as the main value's do (the graph may draw it).
  int         extraCount() const { return int(m_extra.size()); }
  const RecordingSeries &extra(int i) const { return m_extra.at(i); }
  /// ms since the start the recording covers: up to now while it runs (and
  /// in Live), up to the stop, or up to the last reading of a loaded one.
  qint64      duration() const;
  /// The start of what the store still keeps (ms since the start of the
  /// recording): 0 until older readings had to go.
  qint64      origin() const;
  /// What the readings did between two times (graph cursors, the
  /// reference line on the mean of the window).
  struct Stats
  {
    int     count = 0;           ///< readings whose value holds for a while in the span
    double  min = std::nan("");
    double  max = std::nan("");
    double  mean = std::nan(""); ///< over time: each value for as long as it held
    double  sigma = std::nan("");///< standard deviation, weighted over time as the mean
    double  integral = 0;        ///< value x s over the span; gaps count nothing
    qint64  covered = 0;         ///< ms of the span with a value
  };
  /// Stats from @p from to @p to (ms since the start, either order). A
  /// value holds until the next reading, the last one up to duration(); in a
  /// loaded file it holds for no time but counts. A loaded file on its own
  /// grid weighs each row over the step before it.
  Stats       stats(qint64 from, qint64 to) const { return statsOf(m_series, from, to); }
  /// stats() of @p series: the main one or one of extra(), on the same time
  /// axis.
  Stats       statsOf(const RecordingSeries &series, qint64 from, qint64 to) const;
  /// When the recording started (for an import: the file's first time stamp;
  /// in Live: when the live window started).
  QDateTime   startDateTime() const { return m_start; }
  /// The start on the core clock (ms, Sample::now()), for startAt().
  qint64      startClock() const { return m_t0; }
  /// Where the time 0 of @p other lies on this store's time, ms: on the
  /// core clock (the meters of one process record on one time base), by
  /// the wall clock when either came from a file.
  qint64      offsetOf(const RecordingStore &other) const;
  State       state() const { return m_state; }
  /// Recording (not Live).
  bool        isRunning() const { return m_state == Record; }
  /// How wide Live is (ms): the recording length, or the graph's window
  /// when it records until stopped, then at most kLiveMaxMs; at least the
  /// pre-trigger.
  qint64      liveWindow() const;
  /// How much Live keeps (ms): with a recording length liveWindow(), else
  /// kLiveMaxMs (or the longer pre-trigger) - a shorter window for a while
  /// loses nothing that "All" shows.
  qint64      liveKept() const;
  /// How long the graph can get (ms, 0 = open): Live's window (an hour
  /// without a recording length), a recording's length plus its pre-trigger, what a
  /// stopped recording or a file holds.
  qint64      lengthLimit() const;
  /// Recorded data not exported yet; never in Live.
  bool        dirty() const { return m_dirty; }
  /// Tenths of a second left until the recording length is reached (0 when
  /// not recording).
  int         remainingLength() const;
  QList<Mark> marks() const { return m_marks; }
  /// The recording on a grid of @p tenths (the sample time): step k is the
  /// time-weighted mean over the step that ends at k x step, step 0 the value
  /// at the start. A value holds until the next reading; a step without one
  /// is NaN. A loaded recording that is on the grid already comes back as it
  /// was.
  QVector<GridPoint> grid(int tenths) const;
  /// A value of another meter for the export, a column group of its own
  /// (RecordingColumn::device).
  struct Foreign
  {
    RecordingStore *store = nullptr;
    int     id = 0;    ///< its value: 0 the main one, else extra() of that id
    QString device;    ///< the meter's name
  };
  /// The values for the CSV/spreadsheet export: the grid of the sample time,
  /// or with @p raw every reading at its time (a gap a NaN of its own),
  /// starting at the first.
  /// With @p extras (indices of extra()) the other values too, a column
  /// group each on the same rows: on the grid their mean over the step, raw
  /// a row at every reading of any of them, each value holding until its
  /// next. Before a value began its cells are empty (CellState::None).
  /// With @p foreign values of other meters the same way, on this time
  /// (offsetOf()). @p device names this meter's columns (Recording::device).
  /// Recording::meta: what a loaded QtDMM recording had (its devices), the
  /// marks, then @p info over it (MeterController::recordingInfo()).
  Recording   toRecording(bool raw = false, const QList<int> &extras = {}, const QList<Foreign> &foreign = {},
                          const QString &device = QString(), const QJsonObject &info = {}) const;
  /// Writes the recording (CSV, .xlsx or .ods by suffix), on the grid or
  /// with @p raw every reading; clears dirty(), also of the @p foreign
  /// stores. In Live: the live window up to now.
  bool        write(const QString &path, QString *error = nullptr, bool raw = false, const QList<int> &extras = {},
                    const QList<Foreign> &foreign = {}, const QString &device = QString(),
                    const QJsonObject &info = {});
  /// Replaces the recording by @p rec: start, sample time and values, each
  /// at its time, and its other values as extra() (value 2 is id 1 ...);
  /// the store is in View then. A recording on the grid of its
  /// sample time stays one: it exports as it came.
  void        load(const Recording &rec);
  /// @}

  /// What a value measures, for the UI: "Voltage DC (V)", "Temperature (°F)".
  static QString describe(const PortKey &port, const QString &baseUnit);

  /// @name The readings series
  /// @{
  int         readingCount() const { return m_readings.size(); }
  /// Reading @p i, 0 = the oldest.
  const LoggedReading &readingAt(int i) const { return m_readings.at(i); }
  /// What one last digit of value @p id is worth now, in SI base units: from
  /// the newest numbers shown (0.001 for "1.234 V" and "0.012 kV" is 1). 0 when
  /// the readings have none, a file loaded for one.
  double      displayStep(int id) const;
  /// Readings to keep (at least 1); the oldest go when the series shrinks.
  void        setReadingCapacity(int rows);
  int         readingCapacity() const { return m_readingCapacity; }
  /// Paused: readings pass by without being logged. The recording is not
  /// affected, nor does its state touch the pause.
  void        setReadingsPaused(bool paused) { m_readingsPaused = paused; }
  bool        readingsPaused() const { return m_readingsPaused; }
  /// An alarm raised on the newest reading: the row gets the colour.
  void        markLastReading(quint32 argb, const QString &name);
  /// The same on the reading nearest to @p wall (ms since the epoch): a
  /// note set at a time of the graph.
  void        markReadingAt(qint64 wall, quint32 argb, const QString &name);
  /// Takes the mark named @p name off the reading nearest to @p wall (a
  /// note dragged away); another mark there stays.
  void        unmarkReadingAt(qint64 wall, const QString &name);
  /// @}

public Q_SLOTS:
  /// Adds @p reading to the readings series only (dropped while paused).
  void        logReading(const Reading &reading);
  /// Adds a row as another store logged it (dropped while paused).
  void        logRow(const LoggedReading &row);
  /// Adds @p rows at once, one insert for the table; the newest when they
  /// are more than the capacity (dropped while paused).
  void        logRows(const QList<LoggedReading> &rows);
  /// Empties the readings series.
  void        clearReadings();
  /// A reading from the MeterController: every one goes into the readings
  /// series; the main value (id 0) into the live window or the recording, the
  /// triggers and the check that a recording keeps its function (in Live
  /// another function starts the window anew).
  void        setReading(const Reading &reading);
  /// The main value went stale (MeterController::staleChanged()): the live
  /// window or the recording gets a gap from where the last value stopped
  /// holding.
  void        setStale(bool stale);
  /// The clock: the start at a clock time and the recording length.
  /// A timer calls it every second.
  void        poll();
  /// Clears the recording and starts it; from Live with the pre-trigger's
  /// readings when a threshold started it.
  void        start();
  /// Starts as another store's recording did (DeviceSession: the meters of
  /// one process on one time base): at @p t0 on the core clock, wall clock
  /// @p start, from Live with what it holds since then. The time does not
  /// move to the first reading.
  void        startAt(qint64 t0, const QDateTime &start);
  /// The time of the recording that runs stays where it is, also when the
  /// first reading comes later: others started with startAt() share it.
  void        holdOrigin() { m_holdOrigin = true; }
  /// Ends a recording: View. Nothing in Live or View.
  void        stop();
  /// Live: the live window starts anew, empty but for the value the meter
  /// shows. Ends a recording too (the UI does not offer that).
  void        live();
  /// Another meter: the value shown so far (and the other displays') is
  /// not this one's, so live() and a recording do not start with it.
  void        forgetReading();
  /// Discards the recorded readings and marks; a running recording goes on,
  /// and so does Live.
  void        clear();
  /// A mark at the newest reading (now, while recording).
  void        addMark(quint32 argb, const QString &name);
  /// A note at @p t (ms of the recording; -1: where addMark() would put
  /// it) titled @p title; the marks stay in the order of their time.
  /// Returns its index in marks().
  int         addNote(qint64 t, const QString &title, const QString &note);
  /// The time a mark set now gets: now while recording or live, else the
  /// newest reading.
  qint64      markTime() const;
  /// New title and text of mark @p index.
  void        setMarkText(int index, const QString &title, const QString &note);
  void        removeMark(int index);
  /// Moves mark @p index to @p t (ms of the recording); the marks stay in
  /// the order of their time. Returns its new index in marks().
  int         setMarkTime(int index, qint64 t);

Q_SIGNALS:
  /// A reading was stored (series().last()); @p shifted: older ones went
  /// for it, the indices moved down.
  void        appended(bool shifted);
  /// The readings were discarded (clear(), start()).
  void        cleared();
  /// load() replaced the recording.
  void        loaded();
  /// A reading was stored in extra(@p index) (its last point).
  void        extraAppended(int index);
  /// The extra() series changed other than by a reading at the end: one
  /// began or went on again, or older points went (appended(true) says that
  /// when the main series lost points too).
  void        extrasChanged();
  void        runningChanged(bool running);
  /// View, Live or Record now.
  void        stateChanged(RecordingStore::State state);
  /// Duration, remaining length or running state changed (status bar).
  void        progressChanged();
  /// A mark came or went.
  void        marksChanged();
  /// A trigger started or the length stopped the recording: the UI beeps.
  void        alert();
  /// The meter measures something else than the recording now (@p from
  /// "Voltage DC (V)", @p to "Resistance (Ω)", or °F after °C), so the
  /// recording stopped: one recording never mixes two quantities or units.
  void        functionChanged(const QString &from, const QString &to);

  /// The readings series, before and after each change: rows @p first to
  /// @p last are about to come or go (the oldest ones when the series is full).
  void        readingsAboutToInsert(int first, int last);
  void        readingsInserted();
  void        readingsAboutToRemove(int first, int last);
  void        readingsRemoved();
  void        readingsAboutToClear();
  void        readingsCleared();
  /// Row @p row got an alarm mark.
  void        readingMarked(int row);

private:
  /// The row of m_readings nearest to @p wall, -1 when there is none.
  int         readingNearest(qint64 wall) const;
  /// ms since the start of the recording, now.
  qint64      elapsed() const { return m_monotonic() - m_t0; }
  /// Adds a point with the integral up to it.
  void        appendPoint(RawPoint p);
  /// A gap in the other values not heard of for the stale limit at @p now.
  void        closeSilentExtras(qint64 now);
  /// appendPoint(), then the oldest points beyond the limits go; the signals.
  void        append(const RawPoint &p);
  /// Drops what is older than the store keeps; true when anything of the
  /// main series went (the other series alone say so with extrasChanged()).
  bool        trim();
  /// trim() for a change of the limits: extrasChanged() also when the main
  /// series lost points (no reading's appended() says it).
  void        trimForLimits();
  bool        trimMain();
  /// The start triggers on a new value of triggerValue().
  bool        trigger(double value);
  /// Stops at the recording length when @p t (ms since the start) reached it.
  bool        lengthReached(qint64 t);
  /// Starts a recording; with @p preMs (from Live) it reaches back into
  /// the live window.
  void        begin(qint64 preMs);
  /// What Live holds from @p from (ms of its time) on: the reading that
  /// held then and those after it, of every value.
  struct LivePart
  {
    QVector<RawPoint> main;
    QList<RecordingSeries> extra;
    qint64  t0 = 0;   ///< Live's start, which their times count from
  };
  LivePart    livePart(qint64 from) const;
  /// Appends @p part to the recording, its times counted from m_t0.
  void        appendLivePart(const LivePart &part);
  /// The signals at the start of a recording, in state @p was before.
  void        started(State was);
  /// The value the meter shows, as the first point at the start (when it
  /// is not stale); the other values too. @p atItsTime: at the time it
  /// came, not at the start (startAt() reaching back without Live).
  void        appendCurrent(bool atItsTime = false);
  /// A reading of another value (id > 0) into its series.
  void        appendExtra(const Reading &reading);
  /// Appends @p p to extra(@p i), at the earliest at its last point.
  void        appendExtraPoint(int i, RawPoint p);
  /// The newest series of value @p id, -1 when there is none.
  int         extraIndex(int id) const;
  /// Drops the extra points older than the main series keeps, and series
  /// that ended before it; true when anything went.
  bool        trimExtras();
  /// grid() of @p series.
  QVector<GridPoint> gridOf(const RecordingSeries &series, int tenths) const;
  /// An overload has no value: NaN, with the sign of -OL that the
  /// ReadingAdapter put on @p value.
  static double overloadValue(double value) { return std::copysign(qQNaN(), value); }
  /// What a reading is as a point: an overload, no number or a fault word
  /// shown (both a gap), else a value.
  static Quality pointQuality(const Reading &r)
  {
    return r.overload ? Quality::Overload : r.noNumber() ? r.quality : Quality::Valid;
  }

  RecordingSeries m_series;
  QList<RecordingSeries> m_extra;   ///< the other values, extra()
  QMap<int, Reading> m_extraReading; ///< the newest reading of each other value
  qint64      m_t0 = 0;           ///< the monotonic clock at the start
  qint64      m_stopT = -1;       ///< ms since the start when it stopped; -1 = running or never ran
  qint64      m_liveWidthMs = 600 * 1000;
  qint64      m_floorMs = 0;      ///< a recording beyond m_maxPoints: the time of the first reading kept
  int         m_maxPoints = kMaxPoints;
  bool        m_fromFile = false;   ///< load(): the time base is the file's
  int         m_loadedGrid = 0;   ///< load(): the points are the steps of this grid (tenths); 0 = readings
  QList<Mark> m_marks;
  /// load(): what the file's meta.json said beyond the marks (its devices),
  /// written along again
  QJsonObject m_fileInfo;
  QTimer      m_poll;

  int         m_sampleTime = 1;
  int         m_sampleLength = 0;
  int         m_preMs = 0;
  bool        m_holdOrigin = false;   ///< holdOrigin(), startAt(): the start stays
  qint64      m_preUsed = 0;          ///< ms the current recording reaches back before its trigger
  double      m_integral = 0;   ///< the running integral, carried over gaps
  State       m_state = View;
  bool        m_dirty = false;
  QDateTime   m_start;
  QString     m_unit;

  StartMode   m_mode = Manual;
  QTime       m_startTime;
  double      m_raisingThreshold = 0;
  double      m_fallingThreshold = 0;
  ValueRef    m_triggerValue;
  double      m_lastVal = 0;
  bool        m_lastValValid = false;
  double      m_integrationThreshold = 0;

  /// The newest main reading: the first value of a recording that starts.
  Reading     m_reading;
  bool        m_haveReading = false;
  QString     m_nextUnit;        ///< setUnit() while readings are held
  bool        m_unitPending = false;
  PortKey     m_recordPort;      ///< what this recording (or the live window) measures, from its first value
  QString     m_recordBaseUnit;  ///< and in which unit (°C or °F)
  int         m_staleMs = 3000;

  std::function<qint64()>    m_monotonic;
  std::function<QDateTime()> m_wall;

  QList<LoggedReading> m_readings;
  int         m_readingCapacity = 10000;
  bool        m_readingsPaused = false;
  void        removeOldestReadings(int count);
};
