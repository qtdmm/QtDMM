// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

#include "core/valueref.h"

class DeviceSession;
class PoincarePlot;
class ValueFeed;

/// What one Poincaré plot window shows: a value of any open meter
/// (ValueFeed), with where it comes from and why there is none yet.
/// InstanceWidget keeps one per PoincarePlot; MainWindow owns the windows.
class PoincareView : public QObject
{
  Q_OBJECT
public:
  PoincareView(DeviceSession *session, PoincarePlot *plot, QObject *parent);

  PoincarePlot *plot() const { return m_plot; }
  /// The value plotted; another one starts afresh.
  void        setValue(const ValueRef &value);
  ValueRef    value() const { return m_value; }

private:
  PoincarePlot *m_plot;
  ValueFeed    *m_feed;
  ValueRef      m_value;
};
