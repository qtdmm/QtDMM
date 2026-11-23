// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore>
#include <QSerialPort>

#include "device/dmmdecoder.h"


/// A meter on a real (or USB-to-serial) serial port.
///
/// Thin QSerialPort wrapper that knows the meter's line settings from its
/// DMMInfo and applies them in init(), including the DTR/RTS levels many
/// optically isolated cables draw their power from.
class SerialDevice : public QSerialPort {
    Q_OBJECT
public:
    explicit SerialDevice(const DmmDecoder::DMMInfo &info, QString device, QObject *p = nullptr) : QSerialPort(p), m_dmmInfo(info) {
        setPortName(device);
    };

    /// Appends the system's serial port names to @p portlist.
    static bool availablePorts(QStringList &portlist);
    /// The name of @p port that stays when the cable is plugged in again:
    /// on Linux the link in @p byIdDir (/dev/serial/by-id) that points to
    /// it - ttyUSB0 may be ttyUSB1 tomorrow. Else, or when there is none,
    /// @p port as it is.
    static QString stablePortName(const QString &port, const QString &byIdDir = QStringLiteral("/dev/serial/by-id"));
    /// A device string ("SERIAL /dev/ttyUSB0") with the stable port name,
    /// a USB-HID one ("HID 0x1a86:0xe008 /dev/hidraw3", HIDHOLTEK) with its
    /// udev link /dev/qtdmm/... when there is one; any other kind as it is.
    static QString stableDevice(const QString &device, const QString &hidLinkDir = QStringLiteral("/dev/qtdmm"));
    /// Sets baud rate, data/stop bits, parity and the DTR/RTS lines from
    /// the DMMInfo. Must be called on an open port.
    bool init();

protected:
    DmmDecoder::DMMInfo m_dmmInfo;
};
