// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>

class DeviceSession;
class GraphModel;
class MeterController;
class QcpGraph;

/// What one window of the graph on QCustomPlot shows of the session: the
/// store of the meter of its first value, none while that meter is not
/// open. What it shows is in its GraphModel; the recording is the session's.
/// The counterpart of GraphView for the graph that is to replace GraphWidget.
class QcpGraphView : public QObject
{
  Q_OBJECT
public:
  /// @param key  the graph's name in the settings ("qgraph-2", ...)
  QcpGraphView(DeviceSession *session, QcpGraph *graph, const QString &key, QObject *parent);

  QcpGraph   *graph() const { return m_graph; }
  GraphModel *model() const { return m_model; }
  QString     key() const { return m_key; }

private:
  /// The store of the meter of the first value, when it is open.
  void        followBase();

  DeviceSession *m_session;
  QcpGraph   *m_graph;
  GraphModel *m_model;
  QString     m_key;
  QPointer<MeterController> m_base;      ///< the meter whose store the graph draws
  MeterController *m_closing = nullptr;  ///< the meter closing now (DeviceSession::closing())
};
