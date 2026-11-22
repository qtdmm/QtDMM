// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>

class DeviceSession;
class GraphModel;
class MeterController;
class GraphWidget;
class RecordingStore;

/// What one graph window shows of the session: the
/// stores of the meters of its values, on the time of the first one that is
/// open. What it shows is in its GraphModel; the recording is the session's.
/// A file loaded (loadFile()) lies in a store of the view's own and stays
/// until a meter records or goes live, or closeFile().
/// The counterpart of GraphView for the graph that is to replace GraphWidget.
class GraphView : public QObject
{
  Q_OBJECT
public:
  /// @param key  the graph's name in the settings ("graphView-2", ...)
  GraphView(DeviceSession *session, GraphWidget *graph, const QString &key, QObject *parent);

  GraphWidget   *graph() const { return m_graph; }
  GraphModel *model() const { return m_model; }
  QString     key() const { return m_key; }
  /// The settings' colour variant by its name; the model's own goes before it.
  void        setDefaultVariant(const QString &name);

  /// Shows the recording in @p path instead of the meter's; false and
  /// @p error when it cannot be read.
  bool        loadFile(const QString &path, QString *error = nullptr);
  bool        showsFile() const { return m_showsFile; }
  /// Back to the meter of the first value.
  void        closeFile();

Q_SIGNALS:
  /// A file is shown now, or no longer (loadFile(), closeFile()).
  void        fileShownChanged(bool shown);

private:
  /// The meters open now: the graph asks for their stores again.
  void        followBase();
  /// A meter that records or goes live ends the look at a file.
  void        watch(const MeterController *ctl);
  void        applyLook();
  /// Another value is on the left axis: its meter's alarms and limit test are followed.
  void        followLeftValue();
  void        applyLeftValue();

  DeviceSession *m_session;
  GraphWidget   *m_graph;
  GraphModel *m_model;
  QString     m_key;
  QString     m_defaultVariant;
  MeterController *m_closing = nullptr;  ///< the meter closing now (DeviceSession::closing())
  RecordingStore *m_fileStore = nullptr; ///< the file loaded, made with the first one
  bool        m_showsFile = false;
  QMetaObject::Connection m_alarms;
  QMetaObject::Connection m_limits;
};
