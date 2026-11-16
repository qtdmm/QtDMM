// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>

#include "core/valueref.h"

/// What a graph window shows, without anything that draws: its values and
/// its time window, with the texts they have in the settings and benches
/// (the formats of the graph on QtCharts, under keys of its own). The graph
/// on QCustomPlot shows what its model says; it grows with that graph.
class GraphModel : public QObject
{
  Q_OBJECT
public:
  static constexpr int kDefaultWindowSeconds = 600;

  explicit GraphModel(QObject *parent = nullptr);

  /// The values drawn, the first one on the left axis.
  QList<ValueRef> values() const { return m_values; }
  void        setValues(const QList<ValueRef> &values);
  /// "0@device:a 1@device:b", as the settings keep the values.
  QString     valuesSetting() const;
  void        setValuesSetting(const QString &text);

  /// The time the graph shows, seconds; at least 1.
  int         windowSeconds() const { return m_windowSeconds; }
  void        setWindowSeconds(int seconds);
  /// "All": the window is the whole recording and grows with it.
  bool        followAll() const { return m_followAll; }
  void        setFollowAll(bool on);
  /// The window "All" takes for @p recordedSeconds of recording, seconds:
  /// at least 10, whole minutes from one minute on, at most @p limitSeconds
  /// (0: no limit). With @p grow a quarter more, so the window does not
  /// change with every reading.
  static int  allTarget(double recordedSeconds, bool grow, int limitSeconds);

Q_SIGNALS:
  void        valuesChanged();
  void        windowChanged(int seconds);
  void        followAllChanged(bool on);

private:
  QList<ValueRef> m_values;
  int         m_windowSeconds = kDefaultWindowSeconds;
  bool        m_followAll = false;
};
