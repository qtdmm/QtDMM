// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

class FunctionGenerator;
class PowerSupply;

/// The devices with outputs that are open now - power supplies and
/// function generators, simulated or real - for the emergency off: one
/// key (MainWindow's ALL OFF in the status bar, Ctrl+Pause) switches every
/// output off at once. A device is added where it starts to run (the
/// simulated ones by their NodeRunner, which lives as long as the device
/// is open), not where it is only made: a copy in a settings dialog is no
/// device with outputs. It leaves on its own when it is destroyed.
class OutputRegistry : public QObject
{
  Q_OBJECT
public:
  /// The one of the process.
  static OutputRegistry &instance();

  void    add(PowerSupply *supply);
  void    add(FunctionGenerator *generator);
  void    remove(QObject *device);
  /// Devices with outputs open now.
  int     count() const;
  /// Every output off: each device's allOff() - the channels shown, also
  /// those set from outside (EXT); channels hidden in the settings stay
  /// untouched, as always. Returns how many devices were asked.
  int     allOff();

Q_SIGNALS:
  /// A device came or went.
  void    changed();

private:
  OutputRegistry() = default;
  void    forget(QObject *device);

  QList<QPointer<PowerSupply>> m_supplies;
  QList<QPointer<FunctionGenerator>> m_generators;
};
