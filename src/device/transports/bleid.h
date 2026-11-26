// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QBluetoothAddress>
#include <QBluetoothDeviceInfo>
#include <QUuid>

/// The identifier Qt uses for a BLE device: CoreBluetooth's UUID on macOS,
/// the Bluetooth address on platforms that expose it.
inline QString bleDeviceId(const QBluetoothDeviceInfo &info)
{
#ifdef Q_OS_MACOS
  return info.deviceUuid().toString(QUuid::WithoutBraces).toUpper();
#else
  return info.address().toString().toUpper();
#endif
}

/// True when @p id is a usable BLE device identifier on this platform.
inline bool isValidBleId(const QString &id)
{
#ifdef Q_OS_MACOS
  return !QUuid(id).isNull();
#else
  return !QBluetoothAddress(id).isNull();
#endif
}
