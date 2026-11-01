// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include "core/reading.h"
#include "core/valueref.h"

class DeviceSession;
class MeterController;

/// What a view needs of one value of any meter of a DeviceSession: the
/// readings, staleness and min/max of the meter that has it (value()), from
/// whichever controller that is now. A view filters the readings by
/// Reading::id as it always did; the feed picks the meter.
///
/// It follows the session: a meter opened later is found, one closed leaves
/// the view with "no value" (a reading of Quality::NoValue for the value)
/// and the value is kept, so that it shows again when the meter comes back.
class ValueFeed : public QObject
{
  Q_OBJECT
public:
  explicit ValueFeed(DeviceSession *session, QObject *parent = nullptr);

  /// Follows @p value; nothing for an empty source.
  void        setValue(const ValueRef &value);
  ValueRef    value() const { return m_value; }
  /// The meter that has value() now, nullptr while it is not open.
  MeterController *controller() const { return m_ctl; }

Q_SIGNALS:
  /// MeterController::reading of the meter, all its values.
  void        reading(const Reading &reading);
  void        staleChanged(bool stale);
  void        minimumChanged(double value, const QString &text, const QString &unit);
  void        maximumChanged(double value, const QString &text, const QString &unit);
  void        minMaxReset();
  void        valueMinMaxChanged(int id, double minimum, double maximum);
  /// Another meter (or none): the view starts afresh.
  void        controllerChanged(MeterController *ctl);

private:
  /// Connects to the meter that has value() now, if it changed.
  void        attach();

  DeviceSession *m_session;
  ValueRef    m_value;
  QPointer<MeterController> m_ctl;
  QList<QMetaObject::Connection> m_links;
};
