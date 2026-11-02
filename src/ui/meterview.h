// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QObject>

#include "core/valueref.h"
#include "ui/views/analogmeter.h"

class DeviceSession;
class MeterController;
class QTimer;
class ValueFeed;

/// What one analog meter window shows: a value of any open meter
/// (ValueFeed), with that meter's scale, its alarm colour (blinking if
/// wanted) and its PASS/FAIL band. InstanceWidget keeps one per AnalogMeter
/// and hands them the settings; MainWindow owns the windows.
class MeterView : public QObject
{
  Q_OBJECT
public:
  MeterView(DeviceSession *session, AnalogMeter *meter, QObject *parent);

  AnalogMeter *meter() const { return m_meter; }
  /// The value shown; "no value" while its meter is not connected.
  void        setValue(const ValueRef &value);
  ValueRef    value() const { return m_value; }
  /// Its own style (0 dark studio, 1 classic ivory), -1 for the settings'.
  void        setStyle(int style);
  int         style() const { return m_style; }
  /// The style it is drawn in: its own, else the settings'.
  int         shownStyle() const { return m_style >= 0 ? m_style : m_look.style; }

  /// The settings of the page Appearance and the alarms' blinking.
  struct Look
  {
    int  style = 0;          ///< for a meter without its own
    bool ballistics = true;  ///< needle inertia
    int  redZone = 90;       ///< percent of full scale
    AnalogMeter::ScaleMode scaleMode = AnalogMeter::Auto;
    bool blink = false;      ///< the alarm colour blinks
  };
  void        setLook(const Look &look);

private:
  /// Another meter has the value (or none): its scale, alarm colour and band.
  void        follow(const MeterController *ctl);
  void        applyLimits();
  void        applyStyle();
  /// The alarm's colour, or the meter's own in the dark phase of the blinking.
  void        showAlertColor();

  AnalogMeter *m_meter;
  ValueFeed   *m_feed;
  ValueRef     m_value;
  int          m_style = -1;
  Look         m_look;
  QColor       m_alertColor;   ///< the raised alarm's colour, invalid when none
  bool         m_alertPhase = true;   ///< blinking: the colour shown now
  QTimer      *m_alertTimer;
  QMetaObject::Connection m_alertLink;   ///< the alarm colour of the meter shown
  QMetaObject::Connection m_limitLink;   ///< its limit test
};
