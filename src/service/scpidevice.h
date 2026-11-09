// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

#include "core/limittest.h"
#include "core/sampletypes.h"

/// One meter as the SCPI server shows it: its readings, whether it is
/// connected and recording, where the QTDMM: queries get their answers
/// (Sources), and the requests of a client to it (signals). The
/// MeterController keeps one and registers it with the ScpiServer
/// (ScpiServer::addDevice()); with several, INSTrument:SELect chooses the
/// one the commands go to.
class ScpiDevice : public QObject
{
  Q_OBJECT
public:
  /// What the meter last delivered, in SI base units.
  struct Reading
  {
    double value = 0;
    QString unit;      ///< "V", "A", "Ohm" ... without prefix
    PortKey port;      ///< what is measured: voltage.dc, current.ac, voltage.diode, ...
    QString range;     ///< "AUTO", "MANU" or the meter's range text
    bool overload = false;
    bool hold = false;
    bool valid = false;
    qint64 msecs = 0;
  };
  /// A min/max memory (CALCulate:AVERage), in SI base units.
  struct Stats
  {
    int count = 0;      ///< readings since the last reset; 0: min, max and mean are NaN
    double min = 0;
    double max = 0;
    double mean = 0;
  };
  /// What the graph is doing (QTDMM:RECord?).
  struct RecordingState
  {
    enum Mode { Idle, Live, Record, View };
    Mode mode = Idle;
    double seconds = 0;   ///< from the first to the last reading
    int count = 0;        ///< readings held
    QString unit;         ///< base unit, "V"
  };
  /// One reading of the recording (QTDMM:RECord:DATA?).
  struct Point
  {
    double seconds = 0;    ///< since the start of the recording
    double value = 0;      ///< SI base units; NaN = a gap
    bool overload = false;
  };
  /// One alarm of the meter (QTDMM:ALARm:LIST?).
  struct AlarmState
  {
    enum Level { Off, Pending, Raised, Acknowledged };
    QString name;
    Level level = Off;
  };
  /// Where the QTDMM: queries get what the server does not hold itself. A
  /// source not set answers as if there were nothing.
  struct Sources
  {
    /// The display's min/max memory, of its primary value (CALCulate:AVERage
    /// without a channel).
    std::function<Stats()> stats;
    /// The min/max memory of @p channel (1 the main value) of its own, since
    /// the last reset (CALCulate:AVERage (@n), CALCulate2:AVERage).
    std::function<Stats(int channel)> channelStats;
    /// Resets the memory of @p channel alone (CALCulate:AVERage:CLEar (@n)).
    std::function<void(int channel)> clearChannelStats;
    std::function<RecordingState()> recording;
    /// The last @p count readings of @p channel (1 the main value) in the
    /// recording, oldest first.
    std::function<QList<Point>(int count, int channel)> recordingData;
    /// Writes the recording to the file @p name (QTDMM:RECord:SAVE): empty,
    /// or the SCPI error, "<code>,<text>".
    std::function<QString(const QString &name, bool raw)> saveRecording;
    std::function<QList<AlarmState>()> alarms;
    /// The meter's keys QtDMM can press ("hold", "range", ...), empty for a
    /// meter without.
    std::function<QStringList()> keys;
    /// Presses @p key; false when it could not (the meter not connected).
    std::function<bool(const QString &key)> pressKey;
    /// The PASS/FAIL limit test (CALCulate:LIMit), its verdict on the last
    /// reading of its value and the verdicts counted; setLimitTest() keeps
    /// it as the dialog does.
    std::function<LimitTest()> limitTest;
    std::function<void(const LimitTest &test)> setLimitTest;
    std::function<LimitTest::Verdict()> verdict;
    std::function<LimitTest::Count()> limitCount;
    std::function<void()> clearLimitCount;
  };
  void setSources(Sources sources) { m_sources = std::move(sources); }

  explicit ScpiDevice(QObject *parent = nullptr) : QObject(parent) {}

  /// What INSTrument:CATalog? calls it: the "My devices" name, else the
  /// instance id; the server makes an SCPI name of it ("UT61E_BENCH").
  void setName(const QString &name) { m_name = name; }
  QString name() const { return m_name; }
  /// The meter shown in *IDN? ("UNI-T UT61E"); the manufacturer is QtDMM.
  void setModel(const QString &model) { m_model = model; }
  /// Reading of value @p id (0 = main, 1 = second ...) as it arrives:
  /// channel id + 1 of the queries.
  void setReading(int id, const Reading &reading)
  {
    if (id >= 0)
      m_readings[id] = reading;
  }
  /// Another meter was taken over: the readings of the previous one go.
  void clearReadings() { m_readings.clear(); }
  /// The values the meter has (its Device page): channels 1..@p count of
  /// READ? (@<n>) and the like. Channels 1 and 2 are always there (READ2?
  /// on a meter without a second value answers as stale), and so is one
  /// that a reading came for.
  void setChannelCount(int count) { m_channelCount = count; }
  int  channelCount() const
  {
    const int seen = m_readings.isEmpty() ? 0 : m_readings.lastKey() + 1;
    return qMax(qMax(2, m_channelCount), seen);
  }
  /// The meter sends readings: false while connecting, after a timeout and
  /// while a lost connection is reopened (STAT:QUES? bit 2, INPut?).
  void setConnected(bool on) { m_connected = on; }
  void setRecording(bool on) { m_recording = on; }
  bool connected() const { return m_connected; }
  bool recording() const { return m_recording; }

Q_SIGNALS:
  /// INITiate / ABORt: the client wants the recorder started or stopped.
  void startRecording();
  void stopRecording();
  /// INPut ON|OFF: the client wants the meter connected or disconnected.
  void connectRequested(bool on);
  /// CALCulate:AVERage:CLEar: start the min/max memory afresh (F4).
  void resetMinMaxRequested();
  /// QTDMM:ALARm:ACKnowledge
  void acknowledgeAlarmsRequested();
  /// QTDMM:MARK "title"[,"text"]: a note in the graph and a mark in the
  /// readings table.
  void markRequested(const QString &title, const QString &note);

private:
  friend class ScpiServer;

  QString m_name;
  QString m_scpiName;   ///< the server's name for it, made of m_scpiFrom
  QString m_scpiFrom;
  QString m_model;
  QMap<int, Reading> m_readings;   ///< by value id (channel - 1)
  int m_channelCount = 2;
  bool m_connected = false;
  bool m_recording = false;
  Sources m_sources;
};
