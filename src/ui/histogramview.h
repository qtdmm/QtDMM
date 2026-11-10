// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

#include "core/valueref.h"

class DeviceSession;
class HistogramPlot;
class ValueFeed;

/// What one histogram window shows: a value of any open meter.
class HistogramView : public QObject
{
  Q_OBJECT
public:
  HistogramView(DeviceSession *session, HistogramPlot *plot, QObject *parent);

  HistogramPlot *plot() const { return m_plot; }
  void           setValue(const ValueRef &value);
  ValueRef       value() const { return m_value; }

private:
  void           updateCaption();

  HistogramPlot *m_plot;
  ValueFeed     *m_feed;
  ValueRef       m_value;
  QString        m_sourceCaption;
  QString        m_valueName;
};
