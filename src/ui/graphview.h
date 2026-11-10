// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>

class DeviceSession;
class GraphWidget;
class MeterController;
class RecordingStore;

/// What one graph window shows of the session: the store of the meter of
/// its first value (its base), the other meters' values from their stores,
/// an empty store of its own without a meter or for a file loaded. The
/// recording is the session's, so Record, Stop, Live and Clear of any graph
/// go there. InstanceWidget keeps one per GraphWidget; MainWindow owns the
/// windows.
class GraphView : public QObject
{
  Q_OBJECT
public:
  /// @param key  the graph's name in the settings ("graph", "graph-2", ...)
  GraphView(DeviceSession *session, GraphWidget *graph, const QString &key, QObject *parent);

  GraphWidget *graph() const { return m_graph; }
  QString     key() const { return m_key; }
  /// The limit test's band: of the meter whose value is on the left axis,
  /// when it tests that value.
  void        applyLimitBand();
  /// The meter that recorded, when its recording is in the graph; else none.
  QPointer<MeterController> recordingLead() const;

private:
  DeviceSession *m_session;
  GraphWidget *m_graph;
  QString     m_key;
  /// The graph's store while no meter has it: empty, Live (time axis only).
  RecordingStore *m_emptyStore;
  QPointer<MeterController> m_base;   ///< the meter of the graph's store, nullptr: m_emptyStore
  MeterController *m_closing = nullptr;   ///< the meter closing now (DeviceSession::closing())
  QMetaObject::Connection m_unit;        ///< the base's unit to the graph
  QMetaObject::Connection m_alarms;      ///< its alarms: the reference line's thresholds
  QMetaObject::Connection m_limitLink;   ///< of the meter of the graph's left axis
  /// The graph shows none of its store's meter's values: the meter of its first.
  void        followBase();
  /// The graph's store: of @p ctl, or the empty one for nullptr.
  void        setBase(MeterController *ctl);
  /// The alarms' thresholds of the graph's meter, for its reference line to snap to.
  void        applyAlarmThresholds();
  /// A file loaded without a device is no value of any meter: the graph
  /// starts afresh when the session records, goes live or clears.
  void        leaveFile();
};
