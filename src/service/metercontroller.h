// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <array>
#include <functional>
#include <optional>

#include "core/minmaxmemory.h"
#include "core/alarm.h"
#include "core/limittest.h"
#include "core/readingadapter.h"
#include "core/reading.h"
#include "core/sampletypes.h"
#include "core/stalerule.h"
#include "service/servicehost.h"

class MeterConnection;
struct MeterSetup;
class QProcess;
class ScpiDevice;
class RecordingStore;

/// The meter session without any user interface: the connection (MeterConnection), the
/// min/max memory, the alarms, its part in the SCPI server (ScpiDevice in a ServiceHost),
/// and the alarms' programs.
///
/// Views (LCD, analog meter, graph, readings table) and the window hang off
/// its signals; none of them talks to another. Whatever needs a question to
/// the user (a message box, a beep, raising the window) is left to the UI:
/// the controller reports it by a signal.
class MeterController : public QObject
{
  Q_OBJECT
public:
  /// The meter takes part in the network services of @p host (MainWindow's);
  /// without one it keeps services of its own.
  explicit MeterController(QObject *parent = nullptr, ServiceHost *host = nullptr);
  ~MeterController() override;

  /// The connection; the settings page configures its port and protocol.
  MeterConnection        *dmm() const { return m_dmm; }
  /// The recorder: the readings feed it from here on, views (the graph)
  /// show it. SCPI learns when it starts or stops.
  RecordingStore *recorder() const { return m_recorder; }
  /// Whether a start nobody clicked (an alarm, SCPI) may begin a recording: not while one runs - it goes on
  /// instead of starting afresh - and not over one viewed and not saved
  /// yet, which nobody could be asked about. False says why in @p why.
  bool        mayStartRecording(QString *why = nullptr) const;
  /// The connection as @p setup says (model, port, protocol, frames per
  /// reading) and the display count; takes effect at the next connect.
  void        configure(const MeterSetup &setup);
  /// A meter of DeviceSession without a window: its recording follows the
  /// window's, which DeviceSession starts and stops it with; a start of its
  /// own (SCPI INITiate) goes there through recordingRequested(). One not
  /// saved yet does not stop a start - nobody to ask - and is overwritten.
  void        setFollowsWindow(bool follows) { m_followsWindow = follows; }
  bool        followsWindow() const { return m_followsWindow; }
  /// A start or stop from elsewhere (DeviceSession: a meter without a
  /// window was started): recordingRequested(), as an alarm's.
  void        requestRecording(bool start) { Q_EMIT recordingRequested(start); }
  /// The folder QTDMM:RECord:SAVE writes into (Documents/QtDMM at first).
  void        setSaveDir(const QString &dir) { m_saveDir = dir; }
  /// Writes the recording to @p name in that folder (SCPI QTDMM:RECord:SAVE):
  /// a file name only, a new one; CSV without a suffix. Empty, or the SCPI
  /// error ("<code>,<text>", English) why not.
  QString     saveRecording(const QString &name, bool raw);
  /// Writes the recording with all values of this meter and of those that
  /// recorded with it to @p path (CSV, .xlsx, .ods); they count as saved then.
  bool        writeRecording(const QString &path, bool raw, QString *error = nullptr);
  /// The meters whose recordings saveRecording() writes along (DeviceSession:
  /// those that recorded with this one), their names before the columns.
  void        setRecordedWith(std::function<QList<MeterController *>()> meters) { m_recordedWith = std::move(meters); }
  /// The meter's display count (DMMInfo::display), for the range and the
  /// bar graph of the readings.
  void        setDisplayCounts(int counts) { m_adapter.setCounts(counts); }
  /// How many values the meter has (DMMInfo::valueCount()): the SCPI channels.
  void        setValueCount(int count);
  /// This instance's id (--config-id), part of the mDNS name.
  void        setInstanceId(const QString &id);
  /// Which of "My devices" the meter is (DMM/my-device) and its name, both
  /// empty when none; the name is what INSTrument:CATalog? calls it.
  void        setDevice(const QString &id, const QString &name);
  /// Where its readings come from (Reading::source): the "My devices" entry,
  /// else this instance, "device:<id>" or "instance:<id>".
  QString     source() const;
  /// The last reading of the main value (a calculated value's input), an
  /// empty Reading before the first.
  const Reading &lastReading() const { return m_lastMain; }
  /// The last reading of value @p id (Reading::id), empty before the first.
  Reading     lastReading(int id) const { return m_lastReadings.value(id); }
  /// The name of value @p id from its last reading ("Voltage DC (V)", the
  /// meter's own name of it); empty before the first.
  QString     valueName(int id) const;
  /// The values seen since the meter was connected: the highest id + 1, at
  /// least those the meter has (DMMInfo::valueCount()).
  int         valueCount() const;
  /// The entry of My devices setDevice() gave, empty for none.
  QString     deviceId() const { return m_deviceId; }
  /// The name setDevice() gave, empty for none.
  QString     deviceName() const { return m_deviceName; }
  /// The configured meter's name, for SCPI *IDN? and mDNS.
  void        setModel(const QString &model);
  QString     model() const { return m_model; }
  /// The meter's alarms; a list other than before is announced (alarmsChanged()).
  void        setAlarms(const QList<Alarm> &alarms);
  const AlarmManager *alarms() const { return m_alarms; }
  /// The colour of alertColorChanged(), invalid when none.
  QColor      alertColor() const { return m_alertColor; }
  /// The limit test (PASS/FAIL); a new one starts without a verdict.
  void        setLimitTest(const LimitTest &test);
  const LimitTest &limitTest() const { return m_limitTest; }
  LimitTest::Verdict verdict() const { return m_verdict; }
  /// The verdicts counted since the limit test was set or the count reset
  /// (resetLimitCount(), resetMinMax()).
  const LimitTest::Count &limitCount() const { return m_limitCount; }
  void        resetLimitCount();

  /// Connects (true) or disconnects the meter. False when the port could
  /// not be opened.
  bool        connectMeter(bool on);
  /// Presses the meter's key @p key (ControlBar); false when it could not be
  /// sent (no such key, not connected).
  bool        pressKey(const QString &key);
  /// The recorder started or stopped (SCPI reports it).
  void        setRecording(bool on);

  /// The network services this meter takes part in.
  ServiceHost *serviceHost() const { return m_host; }
  /// ServiceHost::apply() with this instance and model.
  void        applyScpi(const ScpiConfig &config);
  /// The SCPI server, HiSLIP and VXI-11, running or not (ServiceHost::services()).
  QList<ServiceState> services() const;
  /// Source of the SCPI screen dump (ServiceHost::setScreenshotSource()).
  void        setScreenshotSource(std::function<QByteArray(const QByteArray &)> source);

  /// Min/max memory in SI base units; +-1e20 when empty.
  /// Which value of the meter the min/max memory follows (Reading::id; the
  /// main value, 0, at first): the LCD's primary value. Another one clears it.
  void        setMinMaxValue(int id);
  int         minMaxValue() const { return m_minMaxId; }
  double      minimum() const { return m_minMax.minimum(); }
  double      maximum() const { return m_minMax.maximum(); }
  /// The min/max memory of value @p id of its own (SCPI CALCulate:AVERage
  /// (@n)), whatever the display follows; resetMinMax() clears it too.
  const MinMaxMemory &valueMinMax(int id) const { return m_valueMinMax.at(size_t(qBound(0, id, kMaxValues - 1))); }
  /// The unit of the current reading's value ("mV": prefix and base unit,
  /// as the display shows it but for Victron's time to go in h), and its
  /// base unit ("V").
  QString     unit() const { return m_lastUnit; }
  QString     baseUnit() const { return m_baseUnit; }
  /// The base unit of value @p id (Reading::id) from its last reading; the
  /// main value's is baseUnit().
  QString     valueUnit(int id) const;
  bool        overload() const { return m_overload; }

public Q_SLOTS:
  /// Clears the min/max memory, the display's and those of every value.
  void        resetMinMax();
  /// Another meter was taken over: the unit of the last reading is
  /// forgotten (empty until the next one), so that the next reading
  /// announces its unit even when it is the same.
  void        forgetUnit();
  /// The user acknowledged the alarm banner.
  void        acknowledgeAlarms();

Q_SIGNALS:
  /// A reading, after min/max; overload and prefix already worked out.
  void        reading(const Reading &reading);
  /// A new minimum (@p value in SI base units, @p text and @p unit as the
  /// meter showed them).
  void        minimumChanged(double value, const QString &text, const QString &unit);
  /// A new maximum, like minimumChanged().
  void        maximumChanged(double value, const QString &text, const QString &unit);
  /// The min/max memory was cleared: the Reset key, another value to follow,
  /// or its value moved to another port (MinMaxMemory) - not a range change
  /// (mV -> V).
  void        minMaxReset();
  /// The min/max memory of value @p id changed (valueMinMax()): a new
  /// extreme, afresh on another port, or cleared (NaN, NaN). SI base units.
  void        valueMinMaxChanged(int id, double minimum, double maximum);
  /// The unit of the main value changed ("mV" -> "V"), unit(); empty after
  /// forgetUnit().
  void        unitChanged(const QString &unit);
  /// The main value's base unit changed ("V" -> "A"); empty after forgetUnit().
  void        baseUnitChanged(const QString &baseUnit);
  /// The main value went stale (StaleRule) or is current again; the views
  /// fade it. Only on a change.
  void        staleChanged(bool stale);
  /// For the status bar's connection field (MeterConnection messages, alarms, SCPI errors).
  void        error(const QString &);
  /// For the status bar's info field.
  void        info(const QString &);

  /// An alarm went off. @p shown is the value as text ("12.3V" or "OL"),
  /// @p text its message, @p index its place in the list. The controller
  /// already ran its program; the sound, popup and raising the window are
  /// up to the UI.
  void        alarmRaised(int index, const Alarm &alarm, const QString &shown, const QString &text);
  /// The alarm's condition no longer holds.
  void        alarmCleared(int index, const Alarm &alarm);
  /// setAlarms() with another list: kept with the device, the sounds of
  /// the list before stop (the places may have moved).
  void        alarmsChanged(const QList<Alarm> &alarms);
  /// The alarm banner: one line per raised, unacknowledged alarm, in the
  /// colour of the first; empty when nothing is raised.
  void        alarmBannerChanged(const QString &lines, const QColor &color);
  /// The colour the display and the analog meter take: that of the alarm
  /// raised last among those raised (acknowledged or not) with
  /// Alarm::tintDisplay; invalid when there is none.
  void        alertColorChanged(const QColor &color);
  /// The limit test judged a reading of its value @p id (Reading::id;
  /// every reading, for the deviation in percent, NaN without a nominal),
  /// or has no verdict (off, disconnected, no number). The views take it
  /// for the value they show.
  void        limitJudged(int id, LimitTest::Verdict verdict, double deviation);
  /// limitCount() has changed: a verdict more, or reset.
  void        limitCountChanged(const LimitTest::Count &count);
  /// setLimitTest() - from the window or a SCPI client (CALCulate:LIMit).
  void        limitTestChanged(const LimitTest &test);
  /// An alarm wants the recorder started (true) or stopped.
  void        recordingRequested(bool start);
  /// A mark in the graph and/or the readings table: an alarm ("Alarm
  /// Overvoltage"), a change of function, a SCPI client (QTDMM:MARK);
  /// @p name is the text shown.
  void        markRequested(const QColor &color, const QString &name, bool graph, bool table);

  /// A SCPI client asked to connect/disconnect the meter.
  void        connectRequested(bool on);
  /// An alarm's program with "Disconnect first" takes the port (true) or
  /// has ended and gives it back (false). A recording goes on; the time
  /// without readings is a gap in it.
  void        portReleased(bool released);
  /// SCPI server state as a sentence for the settings page; services()
  /// changed with it.
  void        scpiStatusChanged(const QString &detail);

private:
  /// valueMinMaxChanged() with the memory of value @p id as it is.
  void        emitValueMinMax(int id);
  void        responseSLOT(const DmmDecoder::DmmResponse &response);
  /// One value as the views get it: min/max, alarms, SCPI.
  void        publish(const Reading &in);
  void        onAlarmRaised(int index, const Alarm &alarm, double value);
  void        updateBanner();
  /// The name INSTrument:CATalog? shows: the device's, else the instance id.
  void        updateScpiName();
  /// What the QTDMM: and CALCulate queries read (ScpiServer::Sources).
  void        installScpiSources();
  void        timerEvent(QTimerEvent *) override;

  MeterConnection                *m_dmm;
  AlarmManager       *m_alarms;
  RecordingStore     *m_recorder;
  ReadingAdapter      m_adapter;
  ScpiDevice         *m_scpiDevice;   ///< this meter in the SCPI server
  ServiceHost        *m_host;
  // a SCPI start or (dis)connect checked but not run yet (it runs after the
  // command): the next command's check counts it, so "INIT;INIT" starts once
  bool                m_startPending = false;
  bool                m_followsWindow = false;
  QString             m_saveDir;
  std::function<QList<MeterController *>()> m_recordedWith;
  std::optional<bool> m_connectPending;
  QProcess           *m_external;     ///< an alarm's program that wanted the port: one at a time
  bool                m_reopenAfter = false;   ///< it closed an open port: open it again when it ends
  bool                m_externalPending = false;   ///< planned, not started yet
  QString             m_instanceId = "default";
  QString             m_deviceId;
  QString             m_deviceName;
  QString             m_model;

  MinMaxMemory m_minMax;   ///< min/max of value m_minMaxId, SI, per port
  int         m_minMaxId = 0;
  std::array<MinMaxMemory, kMaxValues> m_valueMinMax;   ///< per value id, valueMinMax()
  QString     m_lastUnit;
  Reading     m_lastMain;   ///< lastReading()
  QMap<int, Reading> m_lastReadings;   ///< lastReading(int), by id
  QString     m_baseUnit;
  QMap<int, QString> m_valueUnits;   ///< base unit by value id, valueUnit()
  QMap<int, bool> m_valueOverload;   ///< the last reading of each value was OL
  QMap<int, QString> m_valueFault;   ///< the fault word each value shows now, empty for none
  bool        m_overload = false;
  QColor      m_alertColor;   ///< the colour last sent with alertColorChanged()
  LimitTest   m_limitTest;
  LimitTest::Verdict m_verdict = LimitTest::NoVerdict;
  LimitTest::Count m_limitCount;
  QElapsedTimer m_clock;   ///< the stale check's time base, monotonic
  StaleRule   m_stale;     ///< when the main value is too old to stand for the present
  bool        m_staleShown = false;   ///< what staleChanged() said last
};
