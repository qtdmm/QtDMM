// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariant>
#include <QVariantMap>

#include "device/dmmdecoder.h"
#include "device/frameformat.h"

/// How to open a meter, read from its keys (Settings::meterKeys(), an entry
/// of "My devices") without the settings page: the model's row of the table
/// (or the manual settings), the port string and the protocol - what
/// MeterConnection is given (setDmmInfo(), setDevice(), setFormat()).
///
/// DeviceSettings shows the same keys in widgets and asks this for what it
/// derives from them (device(), valueCount()), so the page and a meter opened
/// without it agree.
struct MeterSetup
{
  /// The model's row, or the manual settings ("custom"), with externalSetup
  /// and sigrokExe filled in.
  DmmDecoder::DMMInfo info;
  /// The port string for MeterConnection::setDevice(): "/dev/ttyUSB0",
  /// "ble <address> <key> <fields>", "SIGROK <driver>:conn=...".
  QString device;
  FrameFormat::DataFormat format = FrameFormat::Invalid;
  /// How many values the meter has (DMMInfo::valueCount(), the chosen
  /// Victron fields).
  int     valueCount = 1;
  bool    knownModel = false;   ///< a model of the table, not manual settings

  /// Reads @p keys; @p sigrokExe is the sigrok-cli path ("Port settings/sigrok_exe",
  /// which is not one of the meter's keys).
  static MeterSetup fromKeys(const QVariantMap &keys, const QString &sigrokExe = QStringLiteral("sigrok-cli"));

  /// The persisted protocol: the name (FrameFormat::toString) since the
  /// protocol table exists, the combo index (= enum value) in older files.
  static FrameFormat::DataFormat formatFromSetting(const QVariant &value);

  /// The kinds of meter that have no serial port, as the settings page tells
  /// them apart. All false for manual settings.
  static bool isSigrok(const DmmDecoder::DMMInfo &info) { return info.protocol == FrameFormat::Sigrok && !info.sigrokDriver.isEmpty(); }
  static bool isGatt(const DmmDecoder::DMMInfo &info);
  static bool isBluetooth(const DmmDecoder::DMMInfo &info);
};
