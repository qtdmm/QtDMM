// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class DeviceSettings;
class QLabel;

/// A device at a glance: the picture of its kind of instrument in its
/// maker's colours, and its name, model, type, protocol and connection.
/// Page 4 of the assistant and the Info tab of a device of My devices.
class DeviceSummary : public QWidget
{
  Q_OBJECT
public:
  explicit DeviceSummary(QWidget *parent = nullptr);

  /// Shows the device @p settings describe, called @p name.
  void        show(const DeviceSettings *settings, const QString &name);
  /// Shows a logic node called @p name, of @p model ("QtDMM Formula").
  void        showNode(const QString &name, const QString &model);
  /// How it is reached: "/dev/ttyUSB0, 19200 7O1", "USB-HID /dev/hidraw3",
  /// "Bluetooth LE, AA:BB:..." (never a key), "simulated in QtDMM".
  static QString connection(const DeviceSettings *settings);

private:
  QLabel     *m_symbol;
  QLabel     *m_info;
};
