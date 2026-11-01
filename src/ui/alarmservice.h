// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <functional>

#include "core/alarm.h"
#include "core/limittest.h"

class AlarmSound;
class DeviceSession;
class MeterController;
class QWidget;

/// What the alarms and the PASS/FAIL tests of every open meter need from
/// the desktop: their sounds (each rule its own, AlarmSound), the popup,
/// raising the window, and one banner for all of them. The controllers
/// judge the readings and run the programs; this follows each one the
/// session has (DeviceSession::opened).
class AlarmService : public QObject
{
  Q_OBJECT
public:
  /// @p window is raised by an alarm that wants it and parents the popups.
  AlarmService(DeviceSession *session, QWidget *window, QObject *parent = nullptr);
  /// Instead of playing: the tests count what would have sounded.
  void        setPlayer(const std::function<void(const QString &)> &player);
  /// Whether a sound of alarm @p index of @p ctl is still to come.
  bool        sounding(const MeterController *ctl, int index) const;
  /// The banner's lines as banner() sends them.
  QString     bannerText() const;

public Q_SLOTS:
  /// Acknowledge on the banner: the alarms of every meter.
  void        acknowledgeAll();

Q_SIGNALS:
  /// The banner: one line per raised, unacknowledged alarm of every meter,
  /// with the meter's name in front when more than one is open, on the
  /// colour of the first; empty when nothing is raised.
  void        bannerChanged(const QString &lines, const QColor &color);

private:
  struct Sounds
  {
    AlarmSound *alarm = nullptr;
    AlarmSound *limit = nullptr;   ///< the limit test's, by verdict
    LimitTest::Verdict last = LimitTest::NoVerdict;   ///< only a change sounds
    QString     banner;            ///< the meter's banner lines
    QColor      color;
  };
  void        attach(MeterController *ctl);
  void        alarmRaised(MeterController *ctl, int index, const Alarm &alarm, const QString &shown,
                          const QString &text);
  void        limitJudged(MeterController *ctl, LimitTest::Verdict verdict);
  void        updateBanner();

  DeviceSession *m_session;
  QPointer<QWidget> m_window;
  QHash<const MeterController *, Sounds> m_sounds;
  std::function<void(const QString &)> m_player;
};
