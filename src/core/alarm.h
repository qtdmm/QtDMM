// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include "core/valueref.h"

/// One alarm as the user set it up: a condition on a value of the meter (the
/// main one at first) and what to do while it holds. Kept as JSON in the
/// settings ([Alarms] list).
struct Alarm
{
  enum Condition { Below, Above, Outside, Inside, Overload, NoReadings };
  enum RecorderAction { RecorderNone, RecorderStart, RecorderStop };
  /// How often the sound plays, a second from start to start; never longer
  /// than the alarm is raised and not acknowledged.
  enum Repeat { Once, WhileRaised, Times };

  QString name;
  bool enabled = true;
  /// Which value it watches: of the meter's values (Reading::id) 0 the main
  /// one, 1.. the others; with a source, only that meter's. "No readings" is
  /// the meter's silence: always the main value.
  ValueRef value;
  Condition condition = Below;
  double a = 0;              ///< threshold, or lower bound of the range, in base units
  double b = 0;              ///< upper bound (Outside/Inside)
  double seconds = 0;        ///< condition must hold this long before the alarm raises; NoReadings: the silence
  double hysteresis = 0;     ///< base units the reading must come back by before the alarm clears

  // actions
  QString message;           ///< shown in the banner; empty = the condition in words
  QColor color = QColor(0xd8, 0x22, 0x22);
  bool banner = true;
  bool tintDisplay = true;   ///< the display and the analog meter take the colour while it is raised
  /// "" none, "beep" the system's beep, a name of the built-in sounds
  /// (alarm_warn ...), or a WAV file's path.
  QString sound = QStringLiteral("beep");
  Repeat repeat = Once;
  int repeatCount = 3;       ///< Times: how often
  bool clearSound = false;   ///< the "OK" sound once when the alarm clears
  bool popup = false;
  bool raiseWindow = false;
  QString command;           ///< program to run, %v value %u unit %n name; empty = none
  bool disconnect = false;   ///< close the port while the program runs, open it again after
  RecorderAction recorder = RecorderNone;
  bool markGraph = true;
  bool markTable = true;

  QJsonObject toJson() const;
  static Alarm fromJson(const QJsonObject &o);
  static QList<Alarm> listFromJson(const QString &json);
  static QString listToJson(const QList<Alarm> &alarms);
  /// The alarms of @p json from a file of someone else (a workspace): no
  /// program, the port kept open; @p removed tells whether one had a program.
  static QString listWithoutPrograms(const QString &json, bool *removed = nullptr);

  /// How messages name an alarm: "Alarm High voltage" for a name of its
  /// own, but just "Alarm 1" for the default names, which already start
  /// with the word (not "Alarm Alarm 1").
  static QString title(const QString &name);

  /// The condition in words: "below 5 V for 3 s". @p unit is the base unit.
  QString describe(const QString &unit) const;
  /// Whether @p value (base units; @p overload for OL) satisfies the condition
  /// itself, without duration or hysteresis. NoReadings is time-based and
  /// always false here.
  bool matches(double value, bool overload) const;
  /// Whether the alarm may clear again: the reading has moved back beyond
  /// the hysteresis band.
  bool released(double value, bool overload) const;
};

/// Runs the alarms against the readings: each alarm is off, pending (the
/// condition holds but not yet for its minimum duration), raised, or raised
/// and acknowledged. Time comes in as milliseconds with every call, so the
/// tests can drive it. Pure Qt Core.
class AlarmManager : public QObject
{
  Q_OBJECT
public:
  enum State { Off, Pending, Raised, Acknowledged };

  explicit AlarmManager(QObject *parent = nullptr);

  void setAlarms(const QList<Alarm> &alarms);
  const QList<Alarm> &alarms() const { return m_alarms; }
  State state(int index) const { return m_state.value(index, Off); }
  bool anyRaised() const;
  /// The colour for the display and the analog meter: that of the alarm
  /// raised last among those still raised (acknowledged or not) with
  /// Alarm::tintDisplay; invalid when there is none.
  QColor displayColor() const;

  /// A reading of value @p id arrived (Reading::id) from the meter @p source
  /// (Reading::source): @p value in base units, @p overload for OL. The
  /// alarms on that value look at it; the main value's readings also end a
  /// silence.
  void feed(double value, bool overload, qint64 nowMs, int id = 0, const QString &source = QString());
  /// The meter was connected or disconnected. "No readings" is silence of a
  /// connected meter, so it is counted from the connection, not from the
  /// start of the program: without this call a NoReadings alarm never
  /// raises. Disconnecting clears the ones it had raised.
  void setConnected(bool on, qint64 nowMs);
  /// Time passes without a reading (call once a second or so).
  void tick(qint64 nowMs);
  /// The user saw it: the banner may go, the alarm stays raised until the
  /// condition clears, and raises again only after that.
  void acknowledge(int index);
  void acknowledgeAll();

Q_SIGNALS:
  /// The alarm's condition has held long enough.
  void raised(int index, const Alarm &alarm, double value);
  /// The condition no longer holds (hysteresis respected).
  void cleared(int index, const Alarm &alarm);

private:
  void evaluate(int i, bool matches, bool releasedNow, double value, qint64 nowMs);

  QList<Alarm> m_alarms;
  QList<State> m_state;
  QList<qint64> m_since;       ///< when the condition started to hold (Pending)
  qint64 m_lastReading = -1;   ///< when the last reading arrived, -1 for none
  qint64 m_silenceFrom = -1;   ///< when the meter was connected, -1 while it is not
  double m_lastValue = 0;
  bool m_lastOverload = false;
  QStringList m_raiseOrder;    ///< names in the order they raised, last = newest
};
