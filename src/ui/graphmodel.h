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

Q_SIGNALS:
  void        valuesChanged();
  void        windowChanged(int seconds);

private:
  QList<ValueRef> m_values;
  int         m_windowSeconds = kDefaultWindowSeconds;
};
